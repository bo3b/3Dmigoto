#include "internal_includes/toHLSLInstruction.h"
#include "internal_includes/toHLSLOperand.h"
#include "internal_includes/languages.h"
#include "cbstring/bstrlib.h"
#include "stdio.h"
#include <stdlib.h>
#include "internal_includes/debug.h"

static void AddIndentation(HLSLCrossCompilerContext* psContext)
{
	// 3DMigoto: Variable indentation handled in DecompileHLSL::appendOutput
	bcatcstr(*psContext->currentGLSLString, "  ");
}

// This function prints out the destination name, possible destination writemask, assignment operator
// and any possible conversions needed based on the eSrcType+ui32SrcElementCount (type and size of data expected to be coming in)
// As an output, pNeedsParenthesis will be filled with the amount of closing parenthesis needed
// and pSrcCount will be filled with the number of components expected
// ui32CompMask can be used to only write to 1 or more components (used by MOVC)
static void AddOpAssignToDestWithMask(HLSLCrossCompilerContext* psContext, const Operand* psDest,
	SHADER_VARIABLE_TYPE eSrcType, uint32_t ui32SrcElementCount, const char *szAssignmentOp, int *pNeedsParenthesis, uint32_t ui32CompMask)
{
	uint32_t ui32DestElementCount = GetNumSwizzleElementsWithMask(psDest, ui32CompMask);
	bstring glsl = *psContext->currentGLSLString;
	SHADER_VARIABLE_TYPE eDestDataType = GetOperandDataType(psContext, psDest);
	ASSERT(pNeedsParenthesis != NULL);

	*pNeedsParenthesis = 0;

	TranslateOperandWithMask(psContext, psDest, TO_FLAG_DESTINATION, ui32CompMask);

	// Simple path: types match.
	if (eDestDataType == eSrcType)
	{
		// Cover cases where the HLSL language expects the rest of the components to be default-filled
		// eg. MOV r0, c0.x => Temp[0] = vec4(c0.x);
		if (ui32DestElementCount > ui32SrcElementCount)
		{
			bformata(glsl, " %s %s(", szAssignmentOp, GetConstructorForType(eDestDataType, ui32DestElementCount));
			*pNeedsParenthesis = 1;
		}
		else
			bformata(glsl, " %s ", szAssignmentOp);
		return;
	}

	switch (eDestDataType)
	{
	case SVT_INT:
		if (eSrcType == SVT_FLOAT && psContext->psShader->ui32MajorVersion > 3)
		{
#if 1
			// 3DMigoto: Not using bitcasts at the moment:
			bformata(glsl, " %s ", szAssignmentOp);
			(*pNeedsParenthesis)--;
#else
			bformata(glsl, " %s floatBitsToInt(", szAssignmentOp);
#endif
			// Cover cases where the HLSL language expects the rest of the components to be default-filled
			if (ui32DestElementCount > ui32SrcElementCount)
			{
				bformata(glsl, "%s(", GetConstructorForType(eSrcType, ui32DestElementCount));
				(*pNeedsParenthesis)++;
			}
		}
		else
			bformata(glsl, " %s %s(", szAssignmentOp, GetConstructorForType(eDestDataType, ui32DestElementCount));
		break;
	case SVT_UINT:
		if (eSrcType == SVT_FLOAT && psContext->psShader->ui32MajorVersion > 3)
		{
#if 1
			// 3DMigoto: Not using bitcasts at the moment:
			bformata(glsl, " %s ", szAssignmentOp);
			(*pNeedsParenthesis)--;
#else
			bformata(glsl, " %s floatBitsToUint(", szAssignmentOp);
#endif
			// Cover cases where the HLSL language expects the rest of the components to be default-filled
			if (ui32DestElementCount > ui32SrcElementCount)
			{
				bformata(glsl, "%s(", GetConstructorForType(eSrcType, ui32DestElementCount));
				(*pNeedsParenthesis)++;
			}
		}
		else
			bformata(glsl, " %s %s(", szAssignmentOp, GetConstructorForType(eDestDataType, ui32DestElementCount));
		break;

	case SVT_FLOAT:
		if (psContext->psShader->ui32MajorVersion > 3)
		{
#if 1
			// 3DMigoto: Not using bitcasts at the moment:
			bformata(glsl, " %s ", szAssignmentOp);
			(*pNeedsParenthesis)--;
#else
			if (eSrcType == SVT_INT)
				bformata(glsl, " %s intBitsToFloat(", szAssignmentOp);
			else
				bformata(glsl, " %s uintBitsToFloat(", szAssignmentOp);
#endif
			// Cover cases where the HLSL language expects the rest of the components to be default-filled
			if (ui32DestElementCount > ui32SrcElementCount)
			{
				bformata(glsl, "%s(", GetConstructorForType(eSrcType, ui32DestElementCount));
				(*pNeedsParenthesis)++;
			}

		}
		else
			bformata(glsl, " %s %s(", szAssignmentOp, GetConstructorForType(eDestDataType, ui32DestElementCount));
		break;
	default:
		// TODO: Handle bools?
		break;
	}
	(*pNeedsParenthesis)++;
	return;
}

static void AddAssignToDest(HLSLCrossCompilerContext* psContext, const Operand* psDest,
	SHADER_VARIABLE_TYPE eSrcType, uint32_t ui32SrcElementCount, int* pNeedsParenthesis)
{
	AddOpAssignToDestWithMask(psContext, psDest, eSrcType, ui32SrcElementCount, "=", pNeedsParenthesis, OPERAND_4_COMPONENT_MASK_ALL);
}

static void AddAssignPrologue(HLSLCrossCompilerContext *psContext, int numParenthesis)
{
	bstring glsl = *psContext->currentGLSLString;
	while (numParenthesis != 0)
	{
		bcatcstr(glsl, ")");
		numParenthesis--;
	}
	bcatcstr(glsl, ";\n");

}

static ShaderVarType* LookupStructuredVar(HLSLCrossCompilerContext* psContext,
	Operand* psResource,
	Operand* psByteOffset,
	uint32_t ui32Component)
{
	ConstantBuffer* psCBuf = NULL;
	ShaderVarType* psVarType = NULL;
	uint32_t aui32Swizzle[4] = { OPERAND_4_COMPONENT_X };
	int byteOffset = ((int*)psByteOffset->afImmediates)[0] + 4 * ui32Component;
	int vec4Offset = 0;
	int32_t index = -1;
	int32_t rebase = -1;
	int found;

	ASSERT(psByteOffset->eType == OPERAND_TYPE_IMMEDIATE32);
	//TODO: multi-component stores and vector writes need testing.

	//aui32Swizzle[0] = psInst->asOperands[0].aui32Swizzle[component];

	switch (byteOffset % 16)
	{
	case 0:
		aui32Swizzle[0] = 0;
		break;
	case 4:
		aui32Swizzle[0] = 1;
		break;
	case 8:
		aui32Swizzle[0] = 2;
		break;
	case 12:
		aui32Swizzle[0] = 3;
		break;
	}

	switch (psResource->eType)
	{
	case OPERAND_TYPE_RESOURCE:
		GetConstantBufferFromBindingPoint(RGROUP_TEXTURE, psResource->ui32RegisterNumber, psContext->psShader->sInfo, &psCBuf);
		break;
	case OPERAND_TYPE_UNORDERED_ACCESS_VIEW:
		GetConstantBufferFromBindingPoint(RGROUP_UAV, psResource->ui32RegisterNumber, psContext->psShader->sInfo, &psCBuf);
		break;
	case OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY:
	{
		//dcl_tgsm_structured defines the amount of memory and a stride.
		ASSERT(psResource->ui32RegisterNumber < MAX_GROUPSHARED);
		//return &psContext->psShader->sGroupSharedVarType[psResource->ui32RegisterNumber];
	}
	default:
		ASSERT(0);
		break;
	}

	found = GetShaderVarFromOffset(vec4Offset, aui32Swizzle, psCBuf, &psVarType, &index, &rebase);
	ASSERT(found);

	return psVarType;
}


static void TranslateShaderStorageStore(HLSLCrossCompilerContext* psContext, Instruction* psInst)
{
	bstring glsl = *psContext->currentGLSLString;
	ShaderVarType* psVarType = NULL;
	uint32_t ui32DataTypeFlag = TO_FLAG_INTEGER;
	int component;
	int srcComponent = 0;

	Operand* psDest = 0;
	Operand* psDestAddr = 0;
	Operand* psDestByteOff = 0;
	Operand* psSrc = 0;
	int structured = 0;
	int groupshared = 0;

	switch (psInst->eOpcode)
	{
	case OPCODE_STORE_STRUCTURED:
		psDest = &psInst->asOperands[0];
		psDestAddr = &psInst->asOperands[1];
		psDestByteOff = &psInst->asOperands[2];
		psSrc = &psInst->asOperands[3];
		structured = 1;

		break;
	case OPCODE_STORE_RAW:
		psDest = &psInst->asOperands[0];
		psDestByteOff = &psInst->asOperands[1];
		psSrc = &psInst->asOperands[2];
		break;
	}

	for (component = 0; component < 4; component++)
	{
		const char* swizzleString[] = { ".x", ".y", ".z", ".w" };
		ASSERT(psInst->asOperands[0].eSelMode == OPERAND_4_COMPONENT_MASK_MODE);
		if (psInst->asOperands[0].ui32CompMask & (1 << component))
		{
			SHADER_VARIABLE_TYPE eSrcDataType = GetOperandDataType(psContext, psSrc);

			if (structured && psDest->eType != OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY)
			{
				psVarType = LookupStructuredVar(psContext, psDest, psDestByteOff, component);
			}

			AddIndentation(psContext);

			if (structured && psDest->eType == OPERAND_TYPE_RESOURCE)
			{
				bformata(glsl, "StructuredRes%d", psDest->ui32RegisterNumber);
			}
			else
			{
				TranslateOperand(psContext, psDest, TO_FLAG_DESTINATION | TO_FLAG_NAME_ONLY);
			}
			bformata(glsl, "[");
			if (structured) //Dest address and dest byte offset
			{
				if (psDest->eType == OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY)
				{
					TranslateOperand(psContext, psDestAddr, TO_FLAG_INTEGER | TO_FLAG_UNSIGNED_INTEGER);
					bformata(glsl, "].value[");
					TranslateOperand(psContext, psDestByteOff, TO_FLAG_INTEGER | TO_FLAG_UNSIGNED_INTEGER);
					bformata(glsl, "/4u ");//bytes to floats
				}
				else
				{
					TranslateOperand(psContext, psDestAddr, TO_FLAG_INTEGER | TO_FLAG_UNSIGNED_INTEGER);
				}
			}
			else
			{
				TranslateOperand(psContext, psDestByteOff, TO_FLAG_INTEGER | TO_FLAG_UNSIGNED_INTEGER);
			}

			//RAW: change component using index offset
			if (!structured || (psDest->eType == OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY))
			{
				bformata(glsl, " + %d", component);
			}

			bformata(glsl, "]");

			if (structured && psDest->eType != OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY)
			{
				if (psVarType->Name.compare("$Element") != 0)
				{
					bformata(glsl, ".%s", psVarType->Name.c_str());
				}
			}

			if (structured)
			{
				uint32_t flags = TO_FLAG_UNSIGNED_INTEGER;
				if (psVarType)
				{
					if (psVarType->Type == SVT_INT)
					{
						flags = TO_FLAG_INTEGER;
					}
					else if (psVarType->Type == SVT_FLOAT)
					{
						flags = TO_FLAG_NONE;
					}
				}
				//TGSM always uint
				bformata(glsl, " = (");
				if (GetNumSwizzleElements(psSrc) > 1)
					TranslateOperandWithMask(psContext, psSrc, flags, 1 << (srcComponent++));
				else
					TranslateOperandWithMask(psContext, psSrc, flags, OPERAND_4_COMPONENT_MASK_X);

			}
			else
			{
				//Dest type is currently always a uint array.
				bformata(glsl, " = (");
				if (GetNumSwizzleElements(psSrc) > 1)
					TranslateOperandWithMask(psContext, psSrc, TO_FLAG_UNSIGNED_INTEGER, 1 << (srcComponent++));
				else
					TranslateOperandWithMask(psContext, psSrc, TO_FLAG_UNSIGNED_INTEGER, OPERAND_4_COMPONENT_MASK_X);
			}

			//Double takes an extra slot.
			if (psVarType && psVarType->Type == SVT_DOUBLE)
			{
				if (structured && psDest->eType == OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY)
					bcatcstr(glsl, ")");
				component++;
			}

			bformata(glsl, ");\n");
		}
	}
}
static void TranslateShaderStorageLoad(HLSLCrossCompilerContext* psContext, Instruction* psInst)
{
	bstring glsl = *psContext->currentGLSLString;
	ShaderVarType* psVarType = NULL;
	uint32_t aui32Swizzle[4] = { OPERAND_4_COMPONENT_X };
	uint32_t ui32DataTypeFlag = TO_FLAG_INTEGER;
	int component;
	int destComponent = 0;
	int numParenthesis = 0;
	Operand* psDest = 0;
	Operand* psSrcAddr = 0;
	Operand* psSrcByteOff = 0;
	Operand* psSrc = 0;
	int structured = 0;

	switch (psInst->eOpcode)
	{
	case OPCODE_LD_STRUCTURED:
		psDest = &psInst->asOperands[0];
		psSrcAddr = &psInst->asOperands[1];
		psSrcByteOff = &psInst->asOperands[2];
		psSrc = &psInst->asOperands[3];
		structured = 1;
		break;
	case OPCODE_LD_RAW:
		psDest = &psInst->asOperands[0];
		psSrcByteOff = &psInst->asOperands[1];
		psSrc = &psInst->asOperands[2];
		break;
	}

	if (psInst->eOpcode == OPCODE_LD_RAW)
	{
		int numParenthesis = 0;
		int firstItemAdded = 0;
		uint32_t destCount = GetNumSwizzleElements(psDest);
		uint32_t destMask = GetOperandWriteMask(psDest);
		AddIndentation(psContext);
		AddAssignToDest(psContext, psDest, SVT_UINT, destCount, &numParenthesis);
		if (destCount > 1)
		{
			bformata(glsl, "%s(", GetConstructorForType(SVT_UINT, destCount));
			numParenthesis++;
		}
		for (component = 0; component < 4; component++)
		{
			if (!(destMask & (1 << component)))
				continue;

			if (firstItemAdded)
				bcatcstr(glsl, ", ");
			else
				firstItemAdded = 1;

			bformata(glsl, "RawRes%d[((", psSrc->ui32RegisterNumber);
			TranslateOperand(psContext, psSrcByteOff, TO_FLAG_INTEGER);
			bcatcstr(glsl, ") >> 2)");
			if (psSrc->eSelMode == OPERAND_4_COMPONENT_SWIZZLE_MODE && psSrc->aui32Swizzle[component] != 0)
			{
				bformata(glsl, " + %d", psSrc->aui32Swizzle[component]);
			}
			bcatcstr(glsl, "]");
		}
		AddAssignPrologue(psContext, numParenthesis);
	}
	else
	{
		int numParenthesis = 0;
		int firstItemAdded = 0;
		uint32_t destCount = GetNumSwizzleElements(psDest);
		uint32_t destMask = GetOperandWriteMask(psDest);
		ASSERT(psInst->eOpcode == OPCODE_LD_STRUCTURED);
		AddIndentation(psContext);
		AddAssignToDest(psContext, psDest, SVT_UINT, destCount, &numParenthesis);
		if (destCount > 1)
		{
			bformata(glsl, "%s(", GetConstructorForType(SVT_UINT, destCount));
			numParenthesis++;
		}
		for (component = 0; component < 4; component++)
		{
			ShaderVarType *psVar = NULL;
			int addedBitcast = 0;
			if (!(destMask & (1 << component)))
				continue;

			if (firstItemAdded)
				bcatcstr(glsl, ", ");
			else
				firstItemAdded = 1;

			if (psSrc->eType == OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY)
			{
				// input already in uints
				TranslateOperand(psContext, psSrc, TO_FLAG_NAME_ONLY);
				bcatcstr(glsl, "[");
				TranslateOperand(psContext, psSrcAddr, TO_FLAG_INTEGER);
				bcatcstr(glsl, "].value[(");
				TranslateOperand(psContext, psSrcByteOff, TO_FLAG_UNSIGNED_INTEGER);
				bformata(glsl, " >> 2u) + %d]", psSrc->eSelMode == OPERAND_4_COMPONENT_SWIZZLE_MODE ? psSrc->aui32Swizzle[component] : component);
			}
			else
			{
				ConstantBuffer *psCBuf = NULL;
				ResourceGroup eGroup = RGROUP_UAV;
				if(OPERAND_TYPE_RESOURCE == psSrc->eType)
				{
					eGroup = RGROUP_TEXTURE;
				}
				psVar = LookupStructuredVar(psContext, psSrc, psSrcByteOff, psSrc->eSelMode == OPERAND_4_COMPONENT_SWIZZLE_MODE ? psSrc->aui32Swizzle[component] : component);
				GetConstantBufferFromBindingPoint(eGroup, psSrc->ui32RegisterNumber, psContext->psShader->sInfo, &psCBuf);

				if (psVar->Type == SVT_FLOAT)
				{
#if 0
					// 3DMigoto: Not using bitcasts at the moment:
					bcatcstr(glsl, "floatBitsToUint(");
					addedBitcast = 1;
#endif
				}
				else if (psVar->Type == SVT_DOUBLE)
				{
					bcatcstr(glsl, "unpackDouble2x32(");
					addedBitcast = 1;
				}
				if (psSrc->eType == OPERAND_TYPE_UNORDERED_ACCESS_VIEW)
				{
					bformata(glsl, "%s[", psCBuf->Name);
					TranslateOperand(psContext, psSrcAddr, TO_FLAG_INTEGER);
					bcatcstr(glsl, "]");
					if (psVar->Name.compare("$Element") != 0)
					{
						bcatcstr(glsl, ".");
						bcatcstr(glsl, psVar->Name.c_str());
					}
				}
				else
				{
					int byteOffset = ((int*)psSrcByteOff->afImmediates)[0] + 4 * component;
					int vec4Offset = byteOffset/16;

					bformata(glsl, "StructuredRes%d[", psSrc->ui32RegisterNumber);
					TranslateOperand(psContext, psSrcAddr, TO_FLAG_INTEGER);
					bcatcstr(glsl, "].");

					//StructuredBuffer<float4x4> WorldTransformData;
					//Becomes cbuf = WorldTransformData and var = $Element.
					if (psVar->Name.compare("$Element") != 0)
					{
						bcatcstr(glsl, psVar->Name.c_str());
					}
					else
					{
						bcatcstr(glsl, psCBuf->Name.c_str());
					}

					//Select component of matrix.
					if(psVar->Class == SVC_MATRIX_COLUMNS || psVar->Class == SVC_MATRIX_ROWS)
					{
						const char* swizzleString[] = { ".x", ".y", ".z", ".w" };
						bformata(glsl, "[%d]%s", vec4Offset, swizzleString[component]);
					}
				}

				if (addedBitcast)
					bcatcstr(glsl, ")");
				if (psVar->Type == SVT_DOUBLE)
					component++; // doubles take up 2 slots
			}

		}
		AddAssignPrologue(psContext, numParenthesis);

		return;

	}

#if 0

	//(int)GetNumSwizzleElements(&psInst->asOperands[0])
	for (component = 0; component < 4; component++)
	{
		const char* swizzleString[] = { ".x", ".y", ".z", ".w" };
		ASSERT(psDest->eSelMode == OPERAND_4_COMPONENT_MASK_MODE);
		if (psDest->ui32CompMask & (1 << component))
		{
			if (structured && psSrc->eType != OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY)
			{
				psVarType = LookupStructuredVar(psContext, psSrc, psSrcByteOff, psSrc->aui32Swizzle[component]);
			}

			AddIndentation(psContext);

			aui32Swizzle[0] = psSrc->aui32Swizzle[component];

			TranslateOperand(psContext, psDest, TO_FLAG_DESTINATION);
			if (GetNumSwizzleElements(psDest) > 1)
				bformata(glsl, swizzleString[destComponent++]);

			if (psVarType)
			{
				// TODO completely broken now after AddAssignToDest refactorings.
				AddAssignToDest(psContext, psDest, SVTTypeToFlag(psVarType->Type), GetNumSwizzleElements(psDest), &numParenthesis);
			}
			else
			{
				AddAssignToDest(psContext, psDest, TO_FLAG_NONE, GetNumSwizzleElements(psDest), &numParenthesis);
			}

			if (psSrc->eType == OPERAND_TYPE_RESOURCE)
			{
				if (structured)
					bformata(glsl, "(StructuredRes%d[", psSrc->ui32RegisterNumber);
				else
					bformata(glsl, "(RawRes%d[", psSrc->ui32RegisterNumber);
			}
			else
			{
				bformata(glsl, "(");
				TranslateOperand(psContext, psSrc, TO_FLAG_NAME_ONLY);
				bformata(glsl, "[");
				Translate
			}

			if (structured) //src address and src byte offset
			{
				if (psSrc->eType == OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY)
				{
					TranslateOperand(psContext, psSrcAddr, TO_FLAG_INTEGER | TO_FLAG_UNSIGNED_INTEGER);
					bformata(glsl, "].value[");
					TranslateOperand(psContext, psSrcByteOff, TO_FLAG_INTEGER | TO_FLAG_UNSIGNED_INTEGER);
					bformata(glsl, "/4u ");//bytes to floats
				}
				else
				{
					TranslateOperand(psContext, psSrcAddr, TO_FLAG_INTEGER | TO_FLAG_UNSIGNED_INTEGER);
				}
			}
			else
			{
				TranslateOperand(psContext, psSrcByteOff, TO_FLAG_INTEGER | TO_FLAG_UNSIGNED_INTEGER);
			}

			//RAW: change component using index offset
			if (!structured || (psSrc->eType == OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY))
			{
				bformata(glsl, " + %d", psSrc->aui32Swizzle[component]);
			}

			bformata(glsl, "]");
			if (structured && psSrc->eType != OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY)
			{
				if (strcmp(psVarType->Name, "$Element") != 0)
				{
					bformata(glsl, ".%s", psVarType->Name);
				}

				if (psVarType->Type == SVT_DOUBLE)
				{
					//Double takes an extra slot.
					component++;
				}
			}

			bformata(glsl, ");\n");
		}
	}
#endif
}

void TranslateInstruction(HLSLCrossCompilerContext* psContext, Instruction* psInst, Instruction* psNextInst)
{
	bstring glsl = *psContext->currentGLSLString;
	int numParenthesis = 0;

#ifdef _DEBUG
	AddIndentation(psContext);
	bformata(glsl, "//Instruction %d\n", psInst->id);
#if 0
	if(psInst->id == 73)
	{
		ASSERT(1); //Set breakpoint here to debug an instruction from its ID.
	}
#endif
#endif

	switch (psInst->eOpcode)
	{
	case OPCODE_LD_STRUCTURED:
	{
#ifdef _DEBUG
		AddIndentation(psContext);
		bcatcstr(glsl, "//LD_STRUCTURED\n");
#endif
		TranslateShaderStorageLoad(psContext, psInst);
		break;
	}
	case OPCODE_STORE_RAW:
	{
#ifdef _DEBUG
		AddIndentation(psContext);
		bcatcstr(glsl, "//STORE_RAW\n");
#endif
		TranslateShaderStorageStore(psContext, psInst);
		break;
	}
	case OPCODE_STORE_STRUCTURED:
	{
#ifdef _DEBUG
		AddIndentation(psContext);
		bcatcstr(glsl, "//STORE_STRUCTURED\n");
#endif
		TranslateShaderStorageStore(psContext, psInst);
		break;
	}
	case OPCODE_LD_RAW:
	{
#ifdef _DEBUG
		AddIndentation(psContext);
		bcatcstr(glsl, "//LD_RAW\n");
#endif

		TranslateShaderStorageLoad(psContext, psInst);
		break;
	}
	default:
	{
		ASSERT(0);
		break;
	}
	}

	if (psInst->bSaturate) //Saturate is only for floating point data (float opcodes or MOV)
	{
		int dstCount = GetNumSwizzleElements(&psInst->asOperands[0]);
		AddIndentation(psContext);
		AddAssignToDest(psContext, &psInst->asOperands[0], SVT_FLOAT, dstCount, &numParenthesis);
		bcatcstr(glsl, "clamp(");

		TranslateOperand(psContext, &psInst->asOperands[0], TO_AUTO_BITCAST_TO_FLOAT);
		bcatcstr(glsl, ", 0.0, 1.0)");
		AddAssignPrologue(psContext, numParenthesis);
	}
}
