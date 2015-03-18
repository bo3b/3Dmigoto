#include "internal_includes/tokens.h"
#include "internal_includes/structs.h"
#include "internal_includes/decode.h"
#include "stdlib.h"
#include "stdio.h"
#include "internal_includes/reflect.h"
#include "internal_includes/debug.h"

#define FOURCC(a, b, c, d) ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24 ))
static enum {FOURCC_DXBC = FOURCC('D', 'X', 'B', 'C')}; //DirectX byte code
static enum {FOURCC_SHDR = FOURCC('S', 'H', 'D', 'R')}; //Shader model 4 code
static enum {FOURCC_SHEX = FOURCC('S', 'H', 'E', 'X')}; //Shader model 5 code
static enum {FOURCC_RDEF = FOURCC('R', 'D', 'E', 'F')}; //Resource definition (e.g. constant buffers)
static enum {FOURCC_ISGN = FOURCC('I', 'S', 'G', 'N')}; //Input signature
static enum {FOURCC_IFCE = FOURCC('I', 'F', 'C', 'E')}; //Interface (for dynamic linking)
static enum {FOURCC_OSGN = FOURCC('O', 'S', 'G', 'N')}; //Output signature

static enum {FOURCC_ISG1 = FOURCC('I', 'S', 'G', '1')}; //Input signature with Stream and MinPrecision
static enum {FOURCC_OSG1 = FOURCC('O', 'S', 'G', '1')}; //Output signature with Stream and MinPrecision

typedef struct DXBCContainerHeaderTAG
{
	unsigned fourcc;
	uint32_t unk[4];
	uint32_t one;
	uint32_t totalSize;
	uint32_t chunkCount;
} DXBCContainerHeader;

typedef struct DXBCChunkHeaderTAG
{
	unsigned fourcc;
	unsigned size;
} DXBCChunkHeader;

#ifdef _DEBUG
static uint64_t operandID = 0;
static uint64_t instructionID = 0;
#endif

#if defined(_WIN32)
#define osSprintf(dest, size, src) sprintf_s(dest, size, src)
#else
#define osSprintf(dest, size, src) sprintf(dest, src)
#endif

void DecodeNameToken(const uint32_t* pui32NameToken, Operand* psOperand)
{
    psOperand->eSpecialName = DecodeOperandSpecialName(*pui32NameToken);
	switch(psOperand->eSpecialName)
	{
        case NAME_UNDEFINED:
        {
            psOperand->specialName = "undefined";
            break;
        }
        case NAME_POSITION:
        {
            psOperand->specialName = "position";
            break;
        }
        case NAME_CLIP_DISTANCE:
        {
            psOperand->specialName = "clipDistance";
            break;
        }
        case NAME_CULL_DISTANCE:
        {
            psOperand->specialName = "cullDistance";
            break;
        }
        case NAME_RENDER_TARGET_ARRAY_INDEX:
        {
            psOperand->specialName = "renderTargetArrayIndex";
            break;
        }
        case NAME_VIEWPORT_ARRAY_INDEX:
        {
            psOperand->specialName = "viewportArrayIndex";
            break;
        }
        case NAME_VERTEX_ID:
        {
            psOperand->specialName = "vertexID";
            break;
        }
        case NAME_PRIMITIVE_ID:
        {
            psOperand->specialName = "primitiveID";
            break;
        }
        case NAME_INSTANCE_ID:
        {
            psOperand->specialName = "instanceID";
            break;
        }
        case NAME_IS_FRONT_FACE:
        {
            psOperand->specialName = "isFrontFace";
            break;
        }
        case NAME_SAMPLE_INDEX:
        {
            psOperand->specialName = "sampleIndex";
            break;
        }
        //For the quadrilateral domain, there are 6 factors (4 sides, 2 inner).
		case NAME_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR:
		case NAME_FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR: 
		case NAME_FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR: 
		case NAME_FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR:
		case NAME_FINAL_QUAD_U_INSIDE_TESSFACTOR:
		case NAME_FINAL_QUAD_V_INSIDE_TESSFACTOR:

        //For the triangular domain, there are 4 factors (3 sides, 1 inner)
		case NAME_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR:
		case NAME_FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR:
		case NAME_FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR:
		case NAME_FINAL_TRI_INSIDE_TESSFACTOR:

        //For the isoline domain, there are 2 factors (detail and density).
		case NAME_FINAL_LINE_DETAIL_TESSFACTOR:
		case NAME_FINAL_LINE_DENSITY_TESSFACTOR:
        {
            psOperand->specialName = "tessFactor";
            break;
        }
        default:
        {
            ASSERT(0);
            break;
        }
    }

    return;
}

// Find the declaration of the texture described by psTextureOperand and
// mark it as a shadow type. (e.g. accessed via sampler2DShadow rather than sampler2D)
void MarkTextureAsShadow(ShaderInfo* psShaderInfo, std::vector<Declaration> &psDeclList, const Operand* psTextureOperand)
{
    ResourceBinding* psBinding = 0;
	int found;

	ASSERT(psTextureOperand->eType == OPERAND_TYPE_RESOURCE);

    found = GetResourceFromBindingPoint(RTYPE_TEXTURE, psTextureOperand->ui32RegisterNumber, psShaderInfo, &psBinding);

	if(found)
	{
		for (std::vector<Declaration>::iterator psDecl = psDeclList.begin(); psDecl != psDeclList.end(); ++psDecl)
		{
			if(psDecl->eOpcode == OPCODE_DCL_RESOURCE)
			{
				if(psDecl->asOperands[0].eType == OPERAND_TYPE_RESOURCE &&
					psDecl->asOperands[0].ui32RegisterNumber == psTextureOperand->ui32RegisterNumber)
				{
					psDecl->ui32IsShadowTex = 1;
					break;
				}
			}
		}
	}
}

uint32_t DecodeOperand (const uint32_t *pui32Tokens, Operand* psOperand)
{
    int i;
	uint32_t ui32NumTokens = 1;
    OPERAND_NUM_COMPONENTS eNumComponents;

#ifdef _DEBUG
    psOperand->id = operandID++;
#endif

    //Some defaults
    psOperand->iWriteMaskEnabled = 1;
    psOperand->iGSInput = 0;
	psOperand->aeDataType[0] = SVT_FLOAT;
	psOperand->aeDataType[1] = SVT_FLOAT;
	psOperand->aeDataType[2] = SVT_FLOAT;
	psOperand->aeDataType[3] = SVT_FLOAT;

    psOperand->iExtended = DecodeIsOperandExtended(*pui32Tokens);


    psOperand->eModifier = OPERAND_MODIFIER_NONE;
    psOperand->psSubOperand[0] = 0;
    psOperand->psSubOperand[1] = 0;
    psOperand->psSubOperand[2] = 0;

	/* Check if this instruction is extended.  If it is,
	 * we need to print the information first */
	if (psOperand->iExtended)
	{
		/* OperandToken1 is the second token */
		ui32NumTokens++;

        if(DecodeExtendedOperandType(pui32Tokens[1]) == EXTENDED_OPERAND_MODIFIER)
        {
            psOperand->eModifier = DecodeExtendedOperandModifier(pui32Tokens[1]);
            psOperand->eMinPrecision = (OPERAND_MIN_PRECISION) DecodeOperandMinPrecision(pui32Tokens[1]);
        }

	}

	psOperand->iIndexDims = DecodeOperandIndexDimension(*pui32Tokens);
    psOperand->eType = DecodeOperandType(*pui32Tokens);

    psOperand->ui32RegisterNumber = 0;

    eNumComponents = DecodeOperandNumComponents(*pui32Tokens);

    switch(eNumComponents)
    {
        case OPERAND_1_COMPONENT:
        {
            psOperand->iNumComponents = 1;
            break;
        }
        case OPERAND_4_COMPONENT:
        {
            psOperand->iNumComponents = 4;
            break;
        }
        default:
        {
            psOperand->iNumComponents = 0;
            break;
        }
    }

    if(psOperand->iWriteMaskEnabled &&
       psOperand->iNumComponents == 4)
    {
        psOperand->eSelMode = DecodeOperand4CompSelMode(*pui32Tokens);

        if(psOperand->eSelMode == OPERAND_4_COMPONENT_MASK_MODE)
        {
            psOperand->ui32CompMask = DecodeOperand4CompMask(*pui32Tokens);
        }
        else
        if(psOperand->eSelMode == OPERAND_4_COMPONENT_SWIZZLE_MODE)
        {
            psOperand->ui32Swizzle = DecodeOperand4CompSwizzle(*pui32Tokens);

            if(psOperand->ui32Swizzle != NO_SWIZZLE)
            {
                psOperand->aui32Swizzle[0] = DecodeOperand4CompSwizzleSource(*pui32Tokens, 0);
                psOperand->aui32Swizzle[1] = DecodeOperand4CompSwizzleSource(*pui32Tokens, 1);
                psOperand->aui32Swizzle[2] = DecodeOperand4CompSwizzleSource(*pui32Tokens, 2);
                psOperand->aui32Swizzle[3] = DecodeOperand4CompSwizzleSource(*pui32Tokens, 3);
            }
        }
        else
        if(psOperand->eSelMode == OPERAND_4_COMPONENT_SELECT_1_MODE)
        {
            psOperand->aui32Swizzle[0] = DecodeOperand4CompSel1(*pui32Tokens);
        }
    }

	//Set externally to this function based on the instruction opcode.
	psOperand->iIntegerImmediate = 0;

    if(psOperand->eType == OPERAND_TYPE_IMMEDIATE32)
    {
        for(i=0; i< psOperand->iNumComponents; ++i)
        {
            psOperand->afImmediates[i] = *((float*)(&pui32Tokens[ui32NumTokens]));
            ui32NumTokens ++;
        }
    }
    else
    if(psOperand->eType == OPERAND_TYPE_IMMEDIATE64)
    {
        for(i=0; i< psOperand->iNumComponents; ++i)
        {
            psOperand->adImmediates[i] = *((double*)(&pui32Tokens[ui32NumTokens]));
            ui32NumTokens +=2;
        }
    }

    for(i=0; i <psOperand->iIndexDims; ++i)
    {
        OPERAND_INDEX_REPRESENTATION eRep = DecodeOperandIndexRepresentation(i ,*pui32Tokens);

        psOperand->eIndexRep[i] = eRep;

        psOperand->aui32ArraySizes[i] = 0;
        psOperand->ui32RegisterNumber = 0;

        switch(eRep)
        {
            case OPERAND_INDEX_IMMEDIATE32:
            {
                psOperand->ui32RegisterNumber = *(pui32Tokens+ui32NumTokens);
                psOperand->aui32ArraySizes[i] = psOperand->ui32RegisterNumber;
                break;
            }
            case OPERAND_INDEX_RELATIVE:
            {
                psOperand->psSubOperand[i] = new Operand();
                    DecodeOperand(pui32Tokens+ui32NumTokens, psOperand->psSubOperand[i]);

                    ui32NumTokens++;
                break;
            }
			case OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE:
			{
                psOperand->ui32RegisterNumber = *(pui32Tokens+ui32NumTokens);
                psOperand->aui32ArraySizes[i] = psOperand->ui32RegisterNumber;

                ui32NumTokens++;

                psOperand->psSubOperand[i] = new Operand();
                    DecodeOperand(pui32Tokens+ui32NumTokens, psOperand->psSubOperand[i]);

				ui32NumTokens++;
				break;
			}
            default:
            {
                ASSERT(0);
                break;
            }
        }

        ui32NumTokens++;
    }

	psOperand->specialName.clear();

    return ui32NumTokens;
}

const uint32_t* DecodeDeclaration(Shader* psShader, const uint32_t* pui32Token, Declaration* psDecl)
{
    uint32_t ui32TokenLength = DecodeInstructionLength(*pui32Token);
    const uint32_t bExtended = DecodeIsOpcodeExtended(*pui32Token);
    const OPCODE_TYPE eOpcode = DecodeOpcodeType(*pui32Token);
    uint32_t ui32OperandOffset = 1;

    psDecl->eOpcode = eOpcode;

	psDecl->ui32IsShadowTex = 0;

    if(bExtended)
    {
        ui32OperandOffset = 2;
    }

    switch (eOpcode)
    {
        case OPCODE_DCL_RESOURCE: // DCL* opcodes have
        {
            psDecl->value.eResourceDimension = DecodeResourceDimension(*pui32Token);
            psDecl->ui32NumOperands = 1;
            DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);
            break;
        }
        case OPCODE_DCL_CONSTANT_BUFFER: // custom operand formats.
        {
            psDecl->ui32NumOperands = 1;
            DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);
            break;
        }
        case OPCODE_DCL_SAMPLER:
        {
            break;
        }
        case OPCODE_DCL_INDEX_RANGE:
        {
            psDecl->ui32NumOperands = 1;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);
            psDecl->value.ui32IndexRange = pui32Token[ui32OperandOffset];

            if(psDecl->asOperands[0].eType == OPERAND_TYPE_INPUT)
            {
                uint32_t i;
                const uint32_t indexRange = psDecl->value.ui32IndexRange;
                const uint32_t reg = psDecl->asOperands[0].ui32RegisterNumber;

                psShader->aIndexedInput[reg] = indexRange;
                psShader->aIndexedInputParents[reg] = reg;

                //-1 means don't declare this input because it falls in
                //the range of an already declared array.
                for(i=reg+1; i<reg+indexRange; ++i)
                {
                    psShader->aIndexedInput[i] = -1;
                    psShader->aIndexedInputParents[i] = reg;
                }
            }

            if(psDecl->asOperands[0].eType == OPERAND_TYPE_OUTPUT)
            {
                psShader->aIndexedOutput[psDecl->asOperands[0].ui32RegisterNumber] = psDecl->value.ui32IndexRange;
            }
            break;
        }
        case OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY:
        {
            psDecl->value.eOutputPrimitiveTopology = DecodeGSOutputPrimitiveTopology(*pui32Token);
            break;
        }
        case OPCODE_DCL_GS_INPUT_PRIMITIVE:
        {
            psDecl->value.eInputPrimitive = DecodeGSInputPrimitive(*pui32Token);
            break;
        }
        case OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT:
        {
            psDecl->value.ui32MaxOutputVertexCount = pui32Token[1];
            break;
        }
        case OPCODE_DCL_TESS_PARTITIONING:
        {
            psDecl->value.eTessPartitioning = DecodeTessPartitioning(*pui32Token);
            break;
        }
        case OPCODE_DCL_TESS_DOMAIN:
        {
            psDecl->value.eTessDomain = DecodeTessDomain(*pui32Token);
            break;
        }
        case OPCODE_DCL_TESS_OUTPUT_PRIMITIVE:
        {
            psDecl->value.eTessOutPrim = DecodeTessOutPrim(*pui32Token);
            break;
        }
        case OPCODE_DCL_THREAD_GROUP:
        {
            psDecl->value.aui32WorkGroupSize[0] = pui32Token[1];
            psDecl->value.aui32WorkGroupSize[1] = pui32Token[2];
            psDecl->value.aui32WorkGroupSize[2] = pui32Token[3];
            break;
        }
        case OPCODE_DCL_INPUT:
        {
            psDecl->ui32NumOperands = 1;
            DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);
            break;
        }
        case OPCODE_DCL_INPUT_SIV:
        {
            psDecl->ui32NumOperands = 1;
            DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);
            if(psShader->eShaderType == PIXEL_SHADER)
            {
                psDecl->value.eInterpolation = DecodeInterpolationMode(*pui32Token);
                
            }
            break;
        }
        case OPCODE_DCL_INPUT_PS:
        {
            psDecl->ui32NumOperands = 1;
            psDecl->value.eInterpolation = DecodeInterpolationMode(*pui32Token);
            DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);
            break;
        }
        case OPCODE_DCL_INPUT_SGV:
        case OPCODE_DCL_INPUT_PS_SGV:
        {
            psDecl->ui32NumOperands = 1;
            DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);
            DecodeNameToken(pui32Token + 3, &psDecl->asOperands[0]);
            break;
        }
        case OPCODE_DCL_INPUT_PS_SIV:
        {
            psDecl->value.eInterpolation = DecodeInterpolationMode(*pui32Token);
            break;
        }
        case OPCODE_DCL_OUTPUT:
        {
            psDecl->ui32NumOperands = 1;
            DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);
            break;
        }
        case OPCODE_DCL_OUTPUT_SGV:
        {
            break;
        }
        case OPCODE_DCL_OUTPUT_SIV:
        {
            psDecl->ui32NumOperands = 1;
            DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);
            DecodeNameToken(pui32Token + 3, &psDecl->asOperands[0]);
            break;
        }
        case OPCODE_DCL_TEMPS:
        {
            psDecl->value.ui32NumTemps = *(pui32Token+ui32OperandOffset);
            break;
        }
        case OPCODE_DCL_INDEXABLE_TEMP:
        {
            break;
        }
        case OPCODE_DCL_GLOBAL_FLAGS:
        {
            psDecl->value.ui32GlobalFlags = DecodeGlobalFlags(*pui32Token);
            break;
        }
        case OPCODE_DCL_INTERFACE:
        {
            uint32_t func = 0, numClassesImplementingThisInterface, arrayLen, interfaceID;
            interfaceID = pui32Token[ui32OperandOffset];
            ui32OperandOffset++;
            psDecl->ui32TableLength = pui32Token[ui32OperandOffset];
            ui32OperandOffset++;

            numClassesImplementingThisInterface = DecodeInterfaceTableLength(*(pui32Token+ui32OperandOffset));
            arrayLen = DecodeInterfaceArrayLength(*(pui32Token+ui32OperandOffset));

            ui32OperandOffset++;

            psDecl->value.interface.ui32InterfaceID = interfaceID;
            psDecl->value.interface.ui32NumFuncTables = numClassesImplementingThisInterface;
            psDecl->value.interface.ui32ArraySize = arrayLen;

            psShader->funcPointer[interfaceID].ui32NumBodiesPerTable = psDecl->ui32TableLength;

            for(;func < numClassesImplementingThisInterface; ++func)
            {
                uint32_t ui32FuncTable = *(pui32Token+ui32OperandOffset);
                psShader->aui32FuncTableToFuncPointer[ui32FuncTable] = interfaceID;

                psShader->funcPointer[interfaceID].aui32FuncTables[func] = ui32FuncTable;
                ui32OperandOffset++;
            }

            break;
        }
        case OPCODE_DCL_FUNCTION_BODY:
        {
            psDecl->ui32NumOperands = 1;
            DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);
            break;
        }
        case OPCODE_DCL_FUNCTION_TABLE:
        {
            uint32_t ui32Func;
            const uint32_t ui32FuncTableID = pui32Token[ui32OperandOffset++];
            const uint32_t ui32NumFuncsInTable = pui32Token[ui32OperandOffset++];

            for(ui32Func=0; ui32Func<ui32NumFuncsInTable;++ui32Func)
            {
                const uint32_t ui32FuncBodyID = pui32Token[ui32OperandOffset++];

                psShader->aui32FuncBodyToFuncTable[ui32FuncBodyID] = ui32FuncTableID;

                psShader->funcTable[ui32FuncTableID].aui32FuncBodies[ui32Func] = ui32FuncBodyID;

            }

// OpcodeToken0 is followed by a DWORD that represents the function table
// identifier and another DWORD (TableLength) that gives the number of
// functions in the table.
//
// This is followed by TableLength DWORDs which are function body indices.
//

            break;
        }
		case OPCODE_DCL_INPUT_CONTROL_POINT_COUNT:
		{
			break;
		}
		case OPCODE_HS_DECLS:
		{
			break;
		}
		case OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT:
		{
			psDecl->value.ui32MaxOutputVertexCount = DecodeOutputControlPointCount(*pui32Token);
			break;
		}
		case OPCODE_HS_JOIN_PHASE:
		case OPCODE_HS_FORK_PHASE:
		case OPCODE_HS_CONTROL_POINT_PHASE:
		{
			break;
		}
		case OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT:
		{
            ASSERT(psShader->ui32ForkPhaseCount != 0);//Check for wrapping when we decrement.
            psDecl->value.aui32HullPhaseInstanceInfo[0] = psShader->ui32ForkPhaseCount-1;
            psDecl->value.aui32HullPhaseInstanceInfo[1] = pui32Token[1];
			break;
		}
		case OPCODE_CUSTOMDATA:
		{
			ui32TokenLength = pui32Token[1];
			{
				int iTupleSrc = 0, iTupleDest = 0;
				//const uint32_t ui32ConstCount = pui32Token[1] - 2;
				//const uint32_t ui32TupleCount = (ui32ConstCount / 4);
				CUSTOMDATA_CLASS eClass = DecodeCustomDataClass(pui32Token[0]);

				const uint32_t ui32NumVec4 = (ui32TokenLength - 2) / 4;
				uint32_t uIdx = 0;

				ICBVec4 const *pVec4Array = (ICBVec4 const *) (pui32Token + 2);

				//The buffer will contain at least one value, but not more than 4096 scalars/1024 vec4's.
				ASSERT(ui32NumVec4 < MAX_IMMEDIATE_CONST_BUFFER_VEC4_SIZE);
		
				/* must be a multiple of 4 */
				ASSERT(((ui32TokenLength - 2) % 4) == 0);

				for (uIdx = 0; uIdx < ui32NumVec4; uIdx++)
				{
					psDecl->asImmediateConstBuffer[uIdx] = pVec4Array[uIdx];
				}

				psDecl->ui32NumOperands = ui32NumVec4;
			}
			break;
		}
        case OPCODE_DCL_HS_MAX_TESSFACTOR:
        {
            psDecl->value.fMaxTessFactor = *((float*)&pui32Token[1]);
            break;
        }
        case OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED:
        {
            psDecl->ui32NumOperands = 1;
            psDecl->value.eResourceDimension = DecodeResourceDimension(*pui32Token);
            psDecl->sUAV.ui32GloballyCoherentAccess = DecodeAccessCoherencyFlags(*pui32Token);
			psDecl->sUAV.bCounter = 0;
			psDecl->sUAV.ui32BufferSize = 0;
            DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);
            break;
        }
        case OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW:
        {
            ResourceBinding* psBinding = NULL;
            ConstantBuffer* psBuffer = NULL;

            psDecl->ui32NumOperands = 1;
            psDecl->sUAV.ui32GloballyCoherentAccess = DecodeAccessCoherencyFlags(*pui32Token);
			psDecl->sUAV.bCounter = 0;
			psDecl->sUAV.ui32BufferSize = 0;
            DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);
			// This should be a RTYPE_UAV_RWBYTEADDRESS buffer. It is memory backed by
			// a shader storage buffer whose is unknown at compile time.
			psDecl->sUAV.ui32BufferSize = 0;
            break;
        }
        case OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED:
        {
            ResourceBinding* psBinding = NULL;
            ConstantBuffer* psBuffer = NULL;

            psDecl->ui32NumOperands = 1;
            psDecl->sUAV.ui32GloballyCoherentAccess = DecodeAccessCoherencyFlags(*pui32Token);
			psDecl->sUAV.bCounter = 0;
			psDecl->sUAV.ui32BufferSize = 0;
            DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);

            if(GetResourceFromBindingPoint(RTYPE_UAV_RWSTRUCTURED, psDecl->asOperands[0].ui32RegisterNumber, psShader->sInfo, &psBinding))
            {
                GetUAVBufferFromBindingPoint(psBinding->ui32BindPoint, psShader->sInfo, &psBuffer);
                psDecl->sUAV.ui32BufferSize = psBuffer->ui32TotalSizeInBytes;
            }
            else if(GetResourceFromBindingPoint(RTYPE_UAV_RWSTRUCTURED_WITH_COUNTER, psDecl->asOperands[0].ui32RegisterNumber, psShader->sInfo, &psBinding) ||
				GetResourceFromBindingPoint(RTYPE_UAV_APPEND_STRUCTURED, psDecl->asOperands[0].ui32RegisterNumber, psShader->sInfo, &psBinding) ||
				GetResourceFromBindingPoint(RTYPE_UAV_CONSUME_STRUCTURED, psDecl->asOperands[0].ui32RegisterNumber, psShader->sInfo, &psBinding))
            { 
                GetUAVBufferFromBindingPoint(psBinding->ui32BindPoint, psShader->sInfo, &psBuffer);
                psDecl->sUAV.ui32BufferSize = psBuffer->ui32TotalSizeInBytes;
				psDecl->sUAV.bCounter = 1;
            }
            break;
        }
        case OPCODE_DCL_RESOURCE_STRUCTURED:
        {
            psDecl->ui32NumOperands = 1;
            DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);
            break;
        }
        case OPCODE_DCL_RESOURCE_RAW:
        {
            psDecl->ui32NumOperands = 1;
            DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);
            break;
        }
        case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED:
        {
            ResourceBinding* psBinding = NULL;
            ConstantBuffer* psBuffer = NULL;

            psDecl->ui32NumOperands = 1;
            psDecl->sUAV.ui32GloballyCoherentAccess = 0;

            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);

            psDecl->sTGSM.ui32Stride = pui32Token[ui32OperandOffset++];
            psDecl->sTGSM.ui32Count = pui32Token[ui32OperandOffset++];
            break;
        }
        case OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW:
        {
            ResourceBinding* psBinding = NULL;
            ConstantBuffer* psBuffer = NULL;

            psDecl->ui32NumOperands = 1;
            psDecl->sUAV.ui32GloballyCoherentAccess = 0;

            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psDecl->asOperands[0]);

            psDecl->sTGSM.ui32Stride = 4;
            psDecl->sTGSM.ui32Count = pui32Token[ui32OperandOffset++];
            break;
        }
        default:
        {
            //Reached end of declarations
            return 0;
        }
    }

    return pui32Token + ui32TokenLength;
}

const uint32_t* DeocdeInstruction(const uint32_t* pui32Token, Instruction* psInst, Shader* psShader)
{
    uint32_t ui32TokenLength = DecodeInstructionLength(*pui32Token);
    const uint32_t bExtended = DecodeIsOpcodeExtended(*pui32Token);
    const OPCODE_TYPE eOpcode = DecodeOpcodeType(*pui32Token);
    uint32_t ui32OperandOffset = 1;

#ifdef _DEBUG
    psInst->id = instructionID++;
#endif

    psInst->eOpcode = eOpcode;

    psInst->bSaturate = DecodeInstructionSaturate(*pui32Token);

    psInst->bAddressOffset = 0;

    if(bExtended)
    {
        do {
            const uint32_t ui32ExtOpcodeToken = pui32Token[ui32OperandOffset];
            const EXTENDED_OPCODE_TYPE eExtType = DecodeExtendedOpcodeType(ui32ExtOpcodeToken);

            if(eExtType == EXTENDED_OPCODE_SAMPLE_CONTROLS)
            {
                psInst->bAddressOffset = 1;

                psInst->iUAddrOffset = DecodeImmediateAddressOffset(
							    IMMEDIATE_ADDRESS_OFFSET_U, ui32ExtOpcodeToken);
			    psInst->iVAddrOffset = DecodeImmediateAddressOffset(
							    IMMEDIATE_ADDRESS_OFFSET_V, ui32ExtOpcodeToken);
			    psInst->iWAddrOffset = DecodeImmediateAddressOffset(
							    IMMEDIATE_ADDRESS_OFFSET_W, ui32ExtOpcodeToken);
            }

			ui32OperandOffset++;
		}
		while(DecodeIsOpcodeExtended(pui32Token[ui32OperandOffset-1]));
    }

    switch (eOpcode)
    {
        //no operands
        case OPCODE_CUT:
        case OPCODE_EMIT:
        case OPCODE_EMITTHENCUT:
        case OPCODE_RET:
        case OPCODE_LOOP:
        case OPCODE_ENDLOOP:
        case OPCODE_BREAK:
        case OPCODE_ELSE:
        case OPCODE_ENDIF:
        case OPCODE_CONTINUE:
        case OPCODE_DEFAULT:
        case OPCODE_ENDSWITCH:
        case OPCODE_NOP:
		case OPCODE_HS_CONTROL_POINT_PHASE:
		case OPCODE_HS_FORK_PHASE:
		case OPCODE_HS_JOIN_PHASE:
        {
            psInst->ui32NumOperands = 0;
            break;
        }
		case OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT:
		{
            psInst->ui32NumOperands = 0;
			break;
		}
        case OPCODE_SYNC:
        {
            psInst->ui32NumOperands = 0;
            psInst->ui32SyncFlags = DecodeSyncFlags(*pui32Token);
            break;
        }

        //1 operand
        case OPCODE_EMIT_STREAM:
        case OPCODE_CUT_STREAM:
        case OPCODE_EMITTHENCUT_STREAM:
        case OPCODE_CASE:
        case OPCODE_SWITCH:
        case OPCODE_LABEL:
        {
            psInst->ui32NumOperands = 1;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[0]);

			if(eOpcode == OPCODE_CASE)
			{
				psInst->asOperands[0].iIntegerImmediate = 1;
			}
            break;
        }

        case OPCODE_INTERFACE_CALL:
        {
            psInst->ui32NumOperands = 1;
            psInst->ui32FuncIndexWithinInterface = pui32Token[ui32OperandOffset];
            ui32OperandOffset++;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[0]);
            
            break;
        }

	/* Floating point instruction decodes */

        //Instructions with two operands go here
        case OPCODE_MOV:
        {
            psInst->ui32NumOperands = 2;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[0]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[1]);

            //Mov with an integer dest. If src is an immediate then it must be encoded as an integer.
            if(psInst->asOperands[0].eMinPrecision == OPERAND_MIN_PRECISION_SINT_16 ||
                psInst->asOperands[0].eMinPrecision == OPERAND_MIN_PRECISION_UINT_16)
            {
                psInst->asOperands[1].iIntegerImmediate = 1;
            }
            break;
        }
		case OPCODE_LOG:
		case OPCODE_RSQ:
		case OPCODE_EXP:
		case OPCODE_SQRT:
        case OPCODE_ROUND_PI:
		case OPCODE_ROUND_NI:
		case OPCODE_ROUND_Z:
		case OPCODE_ROUND_NE:
		case OPCODE_FRC:
		case OPCODE_FTOU:
		case OPCODE_FTOI:
        case OPCODE_UTOF:
		case OPCODE_ITOF:
        case OPCODE_INEG:
        case OPCODE_IMM_ATOMIC_ALLOC:
        case OPCODE_IMM_ATOMIC_CONSUME:
        case OPCODE_DMOV:
        case OPCODE_DTOF:
        case OPCODE_FTOD:
        case OPCODE_DRCP:
        case OPCODE_COUNTBITS:
        case OPCODE_FIRSTBIT_HI:
        case OPCODE_FIRSTBIT_LO:
        case OPCODE_FIRSTBIT_SHI:
        case OPCODE_BFREV:
		case OPCODE_F32TOF16:
		case OPCODE_F16TOF32:
		case OPCODE_RCP:
		case OPCODE_NOT:
		case OPCODE_DERIV_RTX_COARSE:
		case OPCODE_DERIV_RTY_COARSE:
		case OPCODE_DERIV_RTX_FINE:
		case OPCODE_DERIV_RTY_FINE:
		case OPCODE_DERIV_RTX:
		case OPCODE_DERIV_RTY:
        {
            psInst->ui32NumOperands = 2;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[0]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[1]);
            break;
        }

        //Instructions with three operands go here
        case OPCODE_SINCOS:
		case OPCODE_MIN:
		case OPCODE_UMIN:		// missing opcode
		case OPCODE_IMAX:
		case OPCODE_IMIN:
		case OPCODE_MAX:
		case OPCODE_MUL:
		case OPCODE_DIV:
		case OPCODE_ADD:
		case OPCODE_DP2:
		case OPCODE_DP3:
		case OPCODE_DP4:
        case OPCODE_NE:
        case OPCODE_OR:
        case OPCODE_LT:
        case OPCODE_IEQ:
        case OPCODE_IADD:
        case OPCODE_AND:
        case OPCODE_GE:
        case OPCODE_IGE:
		case OPCODE_EQ:
		case OPCODE_USHR:
		case OPCODE_ISHL:
		case OPCODE_ISHR:
		case OPCODE_LD:
		case OPCODE_IMUL:
		case OPCODE_ILT:
        case OPCODE_INE:
        case OPCODE_UGE:
        case OPCODE_ULT:
        case OPCODE_ATOMIC_AND:
        case OPCODE_ATOMIC_IADD:
        case OPCODE_ATOMIC_OR:
        case OPCODE_ATOMIC_XOR:
        case OPCODE_ATOMIC_IMAX:
        case OPCODE_ATOMIC_IMIN:
        case OPCODE_ATOMIC_UMAX:
        case OPCODE_ATOMIC_UMIN:
        case OPCODE_DADD:
        case OPCODE_DMAX:
        case OPCODE_DMIN:
        case OPCODE_DMUL:
        case OPCODE_DEQ:
        case OPCODE_DGE:
        case OPCODE_DLT:
        case OPCODE_DNE:
        case OPCODE_DDIV:
		case OPCODE_XOR:
		case OPCODE_SAMPLE_POS:		// bo3b: added for WatchDogs
        {
            psInst->ui32NumOperands = 3;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[0]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[1]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[2]);
            break;
        }
        //Instructions with four operands go here
		case OPCODE_MAD:
        case OPCODE_MOVC:
		case OPCODE_IMAD:
		case OPCODE_UDIV:
        case OPCODE_LOD:
        case OPCODE_SAMPLE:
        case OPCODE_GATHER4:
        case OPCODE_LD_MS:
        case OPCODE_UBFE:
        case OPCODE_IBFE:
        case OPCODE_ATOMIC_CMP_STORE:
        case OPCODE_IMM_ATOMIC_IADD:
        case OPCODE_IMM_ATOMIC_AND:
        case OPCODE_IMM_ATOMIC_OR:
        case OPCODE_IMM_ATOMIC_XOR:
        case OPCODE_IMM_ATOMIC_EXCH:
        case OPCODE_IMM_ATOMIC_IMAX:
        case OPCODE_IMM_ATOMIC_IMIN:
        case OPCODE_IMM_ATOMIC_UMAX:
        case OPCODE_IMM_ATOMIC_UMIN:
        case OPCODE_DMOVC:
        case OPCODE_DFMA:
		{
            psInst->ui32NumOperands = 4;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[0]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[1]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[2]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[3]);
            break;
		}
        case OPCODE_GATHER4_PO:
        case OPCODE_SAMPLE_L:
        case OPCODE_BFI:
        case OPCODE_SWAPC:
        case OPCODE_IMM_ATOMIC_CMP_EXCH:
        {
            psInst->ui32NumOperands = 5;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[0]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[1]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[2]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[3]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[4]);
            break;
        }
        case OPCODE_GATHER4_C:
		case OPCODE_SAMPLE_C:
		case OPCODE_SAMPLE_C_LZ:
        case OPCODE_SAMPLE_B:
		{
            psInst->ui32NumOperands = 5;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[0]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[1]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[2]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[3]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[4]);

			MarkTextureAsShadow(psShader->sInfo, psShader->psDecl, &psInst->asOperands[2]);
            break;
		}
        case OPCODE_GATHER4_PO_C:
        case OPCODE_SAMPLE_D:
        {
            psInst->ui32NumOperands = 6;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[0]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[1]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[2]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[3]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[4]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[5]);

			MarkTextureAsShadow(psShader->sInfo, psShader->psDecl, &psInst->asOperands[2]);
            break;
        }
        case OPCODE_IF:
        case OPCODE_BREAKC:
        case OPCODE_CALLC:
        case OPCODE_CONTINUEC:
        case OPCODE_RETC:
        case OPCODE_DISCARD:
        {
            psInst->eBooleanTestType = DecodeInstrTestBool(*pui32Token);
            psInst->ui32NumOperands = 2;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[0]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[1]);
            break;
        }
		case OPCODE_CUSTOMDATA:
		{
            psInst->ui32NumOperands = 0;
			ui32TokenLength = pui32Token[1];
			break;
		}
        case OPCODE_EVAL_CENTROID:
        {
            psInst->ui32NumOperands = 2;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[0]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[1]);
            break;
        }
        case OPCODE_EVAL_SAMPLE_INDEX:
        case OPCODE_EVAL_SNAPPED:
        {
            psInst->ui32NumOperands = 3;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[0]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[1]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[2]);
            break;
        }
        case OPCODE_STORE_UAV_TYPED:
        case OPCODE_LD_UAV_TYPED:
        case OPCODE_LD_RAW:
        case OPCODE_STORE_RAW:
        {
            psInst->ui32NumOperands = 3;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[0]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[1]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[2]);
            break;
        }
        case OPCODE_STORE_STRUCTURED:
        case OPCODE_LD_STRUCTURED:
        {
            psInst->ui32NumOperands = 4;
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[0]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[1]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[2]);
            ui32OperandOffset += DecodeOperand(pui32Token+ui32OperandOffset, &psInst->asOperands[3]);
            break;
        }
		// Added to decode ResInfo variants. 
		// TODO: update to newest version of CrossCompiler.
		case OPCODE_RESINFO:
		{
			psInst->ui32NumOperands = 3;

			psInst->eResInfoReturnType = DecodeResInfoReturnType(pui32Token[0]);

			ui32OperandOffset += DecodeOperand(pui32Token + ui32OperandOffset, &psInst->asOperands[0]);
			ui32OperandOffset += DecodeOperand(pui32Token + ui32OperandOffset, &psInst->asOperands[1]);
			ui32OperandOffset += DecodeOperand(pui32Token + ui32OperandOffset, &psInst->asOperands[2]);
			break;
		}
		case OPCODE_MSAD:
        default:
        {
			ASSERT(0);
            break;
        }
    }

	UpdateOperandReferences(psShader, psInst);

    return pui32Token + ui32TokenLength;
}

void UpdateOperandReferences(Shader* psShader, Instruction* psInst)
{
    uint32_t ui32Operand;
    const uint32_t ui32NumOperands = psInst->ui32NumOperands;
    for(ui32Operand = 0; ui32Operand < ui32NumOperands; ++ui32Operand)
    {
        Operand* psOperand = &psInst->asOperands[ui32Operand];
        if(psOperand->eType == OPERAND_TYPE_INPUT || 
            psOperand->eType == OPERAND_TYPE_INPUT_CONTROL_POINT)
        {
            if(psOperand->iIndexDims == INDEX_2D)
            {
                if(psOperand->aui32ArraySizes[1] != 0)//gl_in[].gl_Position
                {
                    psShader->abInputReferencedByInstruction[psOperand->ui32RegisterNumber] = 1;
                }
            }
            else
            {
                psShader->abInputReferencedByInstruction[psOperand->ui32RegisterNumber] = 1;
            }
        }
    }
}

const uint32_t* DecodeHullShaderJoinPhase(const uint32_t* pui32Tokens, Shader* psShader)
{
	const uint32_t* pui32CurrentToken = pui32Tokens;
	const uint32_t ui32ShaderLength = psShader->ui32ShaderLength;

//Declarations
	Declaration* psDecl;
    psDecl = new Declaration[ui32ShaderLength];
    psShader->psHSJoinPhaseDecl = psDecl;
    psShader->ui32HSJoinDeclCount = 0;

    while(1) //Keep going until we reach the first non-declaration token, or the end of the shader.
    {
        const uint32_t* pui32Result = DecodeDeclaration(psShader, pui32CurrentToken, psDecl);

        if(pui32Result)
        {
            pui32CurrentToken = pui32Result;
            psShader->ui32HSJoinDeclCount++;
            psDecl++;

            if(pui32CurrentToken >= (psShader->pui32FirstToken + ui32ShaderLength))
            {
                break;
            }
        }
        else
        {
            break;
        }
    }


//Instructions
	std::vector<Instruction> &psInst = psShader->psHSJoinPhaseInstr;
	psInst.clear();

    while (pui32CurrentToken < (psShader->pui32FirstToken + ui32ShaderLength))
    {
		Instruction instr;
        const uint32_t* nextInstr = DeocdeInstruction(pui32CurrentToken, &instr, psShader);
		psInst.push_back(instr);

#ifdef _DEBUG
        if(nextInstr == pui32CurrentToken)
        {
            ASSERT(0);
            break;
        }
#endif

        pui32CurrentToken = nextInstr;
    }

	return pui32CurrentToken;
}

const uint32_t* DecodeHullShaderForkPhase(const uint32_t* pui32Tokens, Shader* psShader)
{
	const uint32_t* pui32CurrentToken = pui32Tokens;
	const uint32_t ui32ShaderLength = psShader->ui32ShaderLength;
    const uint32_t ui32ForkPhaseIndex = psShader->ui32ForkPhaseCount;

	Instruction* psInst;

//Declarations
	Declaration* psDecl;
    psDecl = new Declaration[ui32ShaderLength];

    ASSERT(ui32ForkPhaseIndex < MAX_FORK_PHASES);

    psShader->ui32ForkPhaseCount++;

    psShader->apsHSForkPhaseDecl[ui32ForkPhaseIndex] = psDecl;
    psShader->aui32HSForkDeclCount[ui32ForkPhaseIndex] = 0;

    while(1) //Keep going until we reach the first non-declaration token, or the end of the shader.
    {
        const uint32_t* pui32Result = DecodeDeclaration(psShader, pui32CurrentToken, psDecl);

        if(pui32Result)
        {
            pui32CurrentToken = pui32Result;
            psShader->aui32HSForkDeclCount[ui32ForkPhaseIndex]++;
            psDecl++;

            if(pui32CurrentToken >= (psShader->pui32FirstToken + ui32ShaderLength))
            {
                break;
            }
        }
        else
        {
            break;
        }
    }


//Instructions
    psInst = new Instruction[ui32ShaderLength];
    psShader->apsHSForkPhaseInstr[ui32ForkPhaseIndex] = psInst;
    psShader->aui32HSForkInstrCount[ui32ForkPhaseIndex] = 0;

    while (pui32CurrentToken < (psShader->pui32FirstToken + ui32ShaderLength))
    {
        const uint32_t* nextInstr = DeocdeInstruction(pui32CurrentToken, psInst, psShader);

#ifdef _DEBUG
        if(nextInstr == pui32CurrentToken)
        {
            ASSERT(0);
            break;
        }
#endif

        pui32CurrentToken = nextInstr;

		if(psInst->eOpcode == OPCODE_HS_FORK_PHASE)
		{
			pui32CurrentToken = DecodeHullShaderForkPhase(pui32CurrentToken, psShader);
			return pui32CurrentToken;
		}

        psShader->aui32HSForkInstrCount[ui32ForkPhaseIndex]++;
        psInst++;
    }

	return pui32CurrentToken;
}

const uint32_t* DecodeHullShaderControlPointPhase(const uint32_t* pui32Tokens, Shader* psShader)
{
	const uint32_t* pui32CurrentToken = pui32Tokens;
	const uint32_t ui32ShaderLength = psShader->ui32ShaderLength;

	Instruction* psInst;

//TODO one block of memory for instructions and declarions to reduce memory usage and number of allocs.
//malloc max(sizeof(declaration), sizeof(instruction) * shader length; or sizeof(DeclInst) - unifying both structs.

//Declarations
	Declaration* psDecl;
    psDecl = new Declaration[ui32ShaderLength];
    psShader->psHSControlPointPhaseDecl = psDecl;
    psShader->ui32HSControlPointDeclCount = 0;

    while(1) //Keep going until we reach the first non-declaration token, or the end of the shader.
    {
        const uint32_t* pui32Result = DecodeDeclaration(psShader, pui32CurrentToken, psDecl);

        if(pui32Result)
        {
            pui32CurrentToken = pui32Result;
            psShader->ui32HSControlPointDeclCount++;
            psDecl++;

            if(pui32CurrentToken >= (psShader->pui32FirstToken + ui32ShaderLength))
            {
                break;
            }
        }
        else
        {
            break;
        }
    }


//Instructions
    psInst = new Instruction[ui32ShaderLength];
    psShader->psHSControlPointPhaseInstr = psInst;
    psShader->ui32HSControlPointInstrCount = 0;

    while (pui32CurrentToken < (psShader->pui32FirstToken + ui32ShaderLength))
    {
        const uint32_t* nextInstr = DeocdeInstruction(pui32CurrentToken, psInst, psShader);

#ifdef _DEBUG
        if(nextInstr == pui32CurrentToken)
        {
            ASSERT(0);
            break;
        }
#endif

        pui32CurrentToken = nextInstr;

		if(psInst->eOpcode == OPCODE_HS_FORK_PHASE)
		{
			pui32CurrentToken = DecodeHullShaderForkPhase(pui32CurrentToken, psShader);
			return pui32CurrentToken;
		}
		if(psInst->eOpcode == OPCODE_HS_JOIN_PHASE)
		{
			pui32CurrentToken = DecodeHullShaderJoinPhase(pui32CurrentToken, psShader);
			return pui32CurrentToken;
		}
        psInst++;
        psShader->ui32HSControlPointInstrCount++;
    }

	return pui32CurrentToken;
}

const uint32_t* DecodeHullShader(const uint32_t* pui32Tokens, Shader* psShader)
{
	const uint32_t* pui32CurrentToken = pui32Tokens;
	const uint32_t ui32ShaderLength = psShader->ui32ShaderLength;
	Declaration* psDecl;
    psDecl = new Declaration[ui32ShaderLength];
    psShader->psHSDecl = psDecl;
    psShader->ui32HSDeclCount = 0;

    while(1) //Keep going until we reach the first non-declaration token, or the end of the shader.
    {
        const uint32_t* pui32Result = DecodeDeclaration(psShader, pui32CurrentToken, psDecl);

        if(pui32Result)
        {
            pui32CurrentToken = pui32Result;

			if(psDecl->eOpcode == OPCODE_HS_CONTROL_POINT_PHASE)
			{
				pui32CurrentToken = DecodeHullShaderControlPointPhase(pui32CurrentToken, psShader);
				return pui32CurrentToken;
			}
			if(psDecl->eOpcode == OPCODE_HS_FORK_PHASE)
			{
				pui32CurrentToken = DecodeHullShaderForkPhase(pui32CurrentToken, psShader);
				return pui32CurrentToken;
			}
			if(psDecl->eOpcode == OPCODE_HS_JOIN_PHASE)
			{
				pui32CurrentToken = DecodeHullShaderJoinPhase(pui32CurrentToken, psShader);
				return pui32CurrentToken;
			}

            psDecl++;
            psShader->ui32HSDeclCount++;

            if(pui32CurrentToken >= (psShader->pui32FirstToken + ui32ShaderLength))
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

	return pui32CurrentToken;
}

void Decode(const uint32_t* pui32Tokens, Shader* psShader)
{
	const uint32_t* pui32CurrentToken = pui32Tokens;
    const uint32_t ui32ShaderLength = pui32Tokens[1];
	psShader->ui32MajorVersion = DecodeProgramMajorVersion(*pui32CurrentToken);
	psShader->ui32MinorVersion = DecodeProgramMinorVersion(*pui32CurrentToken);
	psShader->eShaderType = DecodeShaderType(*pui32CurrentToken);

	pui32CurrentToken++;//Move to shader length
	psShader->ui32ShaderLength = ui32ShaderLength;
    pui32CurrentToken++;//Move to after shader length (usually a declaration)

    psShader->pui32FirstToken = pui32Tokens;

	if(psShader->eShaderType == HULL_SHADER)
	{
		pui32CurrentToken = DecodeHullShader(pui32CurrentToken, psShader);
		return;
	}

    //Using ui32ShaderLength as the instruction count
    //will allocate more than enough memory. Avoids having to
    //traverse the entire shader just to get the real instruction count.
	std::vector<Instruction> &psInst = psShader->psInst;

	std::vector<Declaration> &psDecl = psShader->psDecl;

    while(1) //Keep going until we reach the first non-declaration token, or the end of the shader.
    {
		Declaration decl;
        const uint32_t* pui32Result = DecodeDeclaration(psShader, pui32CurrentToken, &decl);
        
        if(pui32Result)
        {
            pui32CurrentToken = pui32Result;
			psDecl.push_back(decl);

            if(pui32CurrentToken >= (psShader->pui32FirstToken + ui32ShaderLength))
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    while (pui32CurrentToken < (psShader->pui32FirstToken + ui32ShaderLength))
    {
		Instruction inst;
        const uint32_t* nextInstr = DeocdeInstruction(pui32CurrentToken, &inst, psShader);

#ifdef _DEBUG
        if(nextInstr == pui32CurrentToken)
        {
            ASSERT(0);
            break;
        }
#endif

        pui32CurrentToken = nextInstr;
		psInst.push_back(inst);
    }
}

Shader* DecodeDXBC(uint32_t* data)
{
    Shader* psShader;
	DXBCContainerHeader* header = (DXBCContainerHeader*)data;
	uint32_t i;
	uint32_t chunkCount;
	uint32_t* chunkOffsets;
    ReflectionChunks refChunks;

	if(header->fourcc != FOURCC_DXBC)
	{
        //Could be SM1/2/3. If the shader type token
        //looks valid then we continue
        uint32_t type = DecodeShaderTypeDX9(data[0]);

		if (type != INVALID_SHADER)
		{
			return DecodeDX9BC(data);
		}
		return 0;
	}

    refChunks.pui32Inputs = NULL;
    refChunks.pui32Interfaces = NULL;
    refChunks.pui32Outputs = NULL;
    refChunks.pui32Resources = NULL;
	refChunks.pui32Inputs11 = NULL;
	refChunks.pui32Outputs11 = NULL;

	chunkOffsets = (uint32_t*)(header + 1);

	chunkCount = header->chunkCount;

	for(i = 0; i < chunkCount; ++i)
	{
		uint32_t offset = chunkOffsets[i];

		DXBCChunkHeader* chunk = (DXBCChunkHeader*)((char*)data + offset);

        switch(chunk->fourcc)
        {
            case FOURCC_ISGN:
            {
                refChunks.pui32Inputs = (uint32_t*)(chunk + 1);
                break;
            }
			case FOURCC_ISG1:
			{
                refChunks.pui32Inputs11 = (uint32_t*)(chunk + 1);
                break;
			}
            case FOURCC_RDEF:
            {
                refChunks.pui32Resources = (uint32_t*)(chunk + 1);
                break;
            }
            case FOURCC_IFCE:
            {
                refChunks.pui32Interfaces = (uint32_t*)(chunk + 1);
                break;
            }
            case FOURCC_OSGN:
            {
                refChunks.pui32Outputs = (uint32_t*)(chunk + 1);
                break;
            }
			case FOURCC_OSG1:
            {
                refChunks.pui32Outputs11 = (uint32_t*)(chunk + 1);
                break;
            }
            case FOURCC_SHDR:
            case FOURCC_SHEX:
            {
                uint32_t ui32MajorVersion;
                uint32_t ui32MinorVersion;

                psShader = new Shader();

                ui32MajorVersion = DecodeProgramMajorVersion(*(uint32_t*)(chunk + 1));
                ui32MinorVersion = DecodeProgramMinorVersion(*(uint32_t*)(chunk + 1));

                LoadShaderInfo(ui32MajorVersion,
                    ui32MinorVersion,
                    &refChunks,
                    psShader->sInfo);
			    Decode((uint32_t*)(chunk + 1), psShader);
			    return psShader;
            }
            default:
            {
                break;
            }
        }
	}
    return 0;
}

