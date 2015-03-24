#include "internal_includes/reflect.h"
#include "internal_includes/debug.h"
#include "internal_includes/decode.h"
#include <stdlib.h>

#include <string>

static void ReadStringFromTokenStream(const uint32_t* tokens, std::string &str)
{
    char* charTokens = (char*) tokens;
    char nextCharacter = *charTokens++;
	str.clear();

    //Add each individual character until
    //a terminator is found.
    while(nextCharacter != 0) {

        str.push_back(nextCharacter);

        nextCharacter = *charTokens++;
    }
}

static void ReadInputSignatures(const uint32_t* pui32Tokens,
                        ShaderInfo* psShaderInfo,
						const int extended)
{
    uint32_t i;

    InOutSignature* psSignatures;
    const uint32_t* pui32FirstSignatureToken = pui32Tokens;
    const uint32_t ui32ElementCount = *pui32Tokens++;
    const uint32_t ui32Key = *pui32Tokens++;

    psSignatures = new InOutSignature[ui32ElementCount];
    psShaderInfo->psInputSignatures = psSignatures;
    psShaderInfo->ui32NumInputSignatures = ui32ElementCount;

    for(i=0; i<ui32ElementCount; ++i)
    {
        uint32_t ui32ComponentMasks;
        InOutSignature* psCurrentSignature = psSignatures + i;
        uint32_t ui32SemanticNameOffset;

		psCurrentSignature->ui32Stream = 0;
		psCurrentSignature->eMinPrec = D3D_MIN_PRECISION_DEFAULT;

		if(extended)
			psCurrentSignature->ui32Stream = *pui32Tokens++;

		ui32SemanticNameOffset = *pui32Tokens++;
        psCurrentSignature->ui32SemanticIndex = *pui32Tokens++;
        psCurrentSignature->eSystemValueType = (SPECIAL_NAME) *pui32Tokens++;
        psCurrentSignature->eComponentType = (INOUT_COMPONENT_TYPE) *pui32Tokens++;
        psCurrentSignature->ui32Register = *pui32Tokens++;
        
        ui32ComponentMasks = *pui32Tokens++;
        psCurrentSignature->ui32Mask = ui32ComponentMasks & 0x7F;
        //Shows which components are read
        psCurrentSignature->ui32ReadWriteMask = (ui32ComponentMasks & 0x7F00) >> 8;

		if(extended)
			psCurrentSignature->eMinPrec = (MIN_PRECISION) *pui32Tokens++;

        ReadStringFromTokenStream((const uint32_t*)((const char*)pui32FirstSignatureToken+ui32SemanticNameOffset), psCurrentSignature->SemanticName);
    }
}

static void ReadOutputSignatures(const uint32_t* pui32Tokens,
                        ShaderInfo* psShaderInfo,
						const int extended)
{
    uint32_t i;

    InOutSignature* psSignatures;
    const uint32_t* pui32FirstSignatureToken = pui32Tokens;
    const uint32_t ui32ElementCount = *pui32Tokens++;
    const uint32_t ui32Key = *pui32Tokens++;

    psSignatures = new InOutSignature[ui32ElementCount];
    psShaderInfo->psOutputSignatures = psSignatures;
    psShaderInfo->ui32NumOutputSignatures = ui32ElementCount;

    for(i=0; i<ui32ElementCount; ++i)
    {
        uint32_t ui32ComponentMasks;
        InOutSignature* psCurrentSignature = psSignatures + i;
        uint32_t ui32SemanticNameOffset;

		psCurrentSignature->ui32Stream = 0;
		psCurrentSignature->eMinPrec = D3D_MIN_PRECISION_DEFAULT;

		if(extended)
			psCurrentSignature->ui32Stream = *pui32Tokens++;

		ui32SemanticNameOffset = *pui32Tokens++;
        psCurrentSignature->ui32SemanticIndex = *pui32Tokens++;
        psCurrentSignature->eSystemValueType = (SPECIAL_NAME)*pui32Tokens++;
        psCurrentSignature->eComponentType = (INOUT_COMPONENT_TYPE) *pui32Tokens++;
        psCurrentSignature->ui32Register = *pui32Tokens++;

        ui32ComponentMasks = *pui32Tokens++;
        psCurrentSignature->ui32Mask = ui32ComponentMasks & 0x7F;
        //Shows which components are NEVER written.
        psCurrentSignature->ui32ReadWriteMask = (ui32ComponentMasks & 0x7F00) >> 8;

		if(extended)
			psCurrentSignature->eMinPrec = (MIN_PRECISION) *pui32Tokens++;

        ReadStringFromTokenStream((const uint32_t*)((const char*)pui32FirstSignatureToken+ui32SemanticNameOffset), psCurrentSignature->SemanticName);
    }
}

static const uint32_t* ReadResourceBinding(const uint32_t* pui32FirstResourceToken, const uint32_t* pui32Tokens, ResourceBinding* psBinding)
{
    uint32_t ui32NameOffset = *pui32Tokens++;

    ReadStringFromTokenStream((const uint32_t*)((const char*)pui32FirstResourceToken+ui32NameOffset), psBinding->Name);

    psBinding->eType = (ResourceType) *pui32Tokens++;
    psBinding->ui32ReturnType = *pui32Tokens++;
    psBinding->eDimension = (REFLECT_RESOURCE_DIMENSION)*pui32Tokens++;
    psBinding->ui32NumSamples = *pui32Tokens++;
    psBinding->ui32BindPoint = *pui32Tokens++;
    psBinding->ui32BindCount = *pui32Tokens++;
    psBinding->ui32Flags = *pui32Tokens++;

    return pui32Tokens;
}

//Read D3D11_SHADER_TYPE_DESC
static void ReadShaderVariableType(const uint32_t ui32MajorVersion,
								   const uint32_t* pui32FirstConstBufToken,
								   const uint32_t* pui32tokens, ShaderVarType* varType)
{
    const uint16_t* pui16Tokens = (const uint16_t*) pui32tokens;
    uint16_t ui32MemberCount;
    uint32_t ui32MemberOffset;
	const uint32_t* pui32MemberTokens;
	uint32_t i;

    varType->Class = (SHADER_VARIABLE_CLASS)pui16Tokens[0];
    varType->Type = (SHADER_VARIABLE_TYPE)pui16Tokens[1];
    varType->Rows = pui16Tokens[2];
    varType->Columns = pui16Tokens[3];
    varType->Elements = pui16Tokens[4];

    varType->MemberCount = ui32MemberCount = pui16Tokens[5];
	varType->Members = 0;

	if(varType->ParentCount)
	{
		varType->FullName = varType->Parent->FullName +
			"." +
			varType->Name;
	}

	if (ui32MemberCount)
	{
		varType->Members = (ShaderVarType*)malloc(sizeof(ShaderVarType)*ui32MemberCount);

		ui32MemberOffset = pui32tokens[3];

		pui32MemberTokens = (const uint32_t*)((const char*)pui32FirstConstBufToken + ui32MemberOffset);

		for (i = 0; i< ui32MemberCount; ++i)
		{
			// It's not completely clear why this is necessary, but the reason we build blank
			// C++ object here is to create default structure for the object and construct it.
			// This prevents a crash in ReadStringFromTokenStream.
			ShaderVarType member;
			memcpy(&varType->Members[i], &member, sizeof(ShaderVarType));

			uint32_t ui32NameOffset = *pui32MemberTokens++;
			uint32_t ui32MemberTypeOffset = *pui32MemberTokens++;

			varType->Members[i].Parent = varType;
			varType->Members[i].ParentCount = varType->ParentCount + 1;

			varType->Members[i].Offset = *pui32MemberTokens++;

			// Same fix here, where the uninitialized struct/object would crash upon string assignment.
			// We did not understand the code, but this fixes the crash for AC3 shaders.
			std::string temp;
			ReadStringFromTokenStream((const uint32_t*)((const char*)pui32FirstConstBufToken + ui32NameOffset), temp);
			varType->Members[i].Name = temp;

			ReadShaderVariableType(ui32MajorVersion, pui32FirstConstBufToken,
				(const uint32_t*)((const char*)pui32FirstConstBufToken + ui32MemberTypeOffset), &varType->Members[i]);
		}
	}
}

static const uint32_t* ReadConstantBuffer(ShaderInfo* psShaderInfo,
    const uint32_t* pui32FirstConstBufToken, const uint32_t* pui32Tokens, ConstantBuffer* psBuffer)
{
    uint32_t i;
    uint32_t ui32NameOffset = *pui32Tokens++;
    uint32_t ui32VarCount = *pui32Tokens++;
    uint32_t ui32VarOffset = *pui32Tokens++;
    const uint32_t* pui32VarToken = (const uint32_t*)((const char*)pui32FirstConstBufToken+ui32VarOffset);

    ReadStringFromTokenStream((const uint32_t*)((const char*)pui32FirstConstBufToken+ui32NameOffset), psBuffer->Name);

    for(i=0; i<ui32VarCount; ++i)
    {
        //D3D11_SHADER_VARIABLE_DESC
		ShaderVar sVar;

        uint32_t ui32Flags;
        uint32_t ui32TypeOffset;
        uint32_t ui32DefaultValueOffset;

        ui32NameOffset = *pui32VarToken++;

        ReadStringFromTokenStream((const uint32_t*)((const char*)pui32FirstConstBufToken+ui32NameOffset), sVar.Name);

        sVar.ui32StartOffset = *pui32VarToken++;
        sVar.ui32Size = *pui32VarToken++;
        ui32Flags = *pui32VarToken++;
        ui32TypeOffset = *pui32VarToken++;

		sVar.sType.Name = sVar.Name;
		sVar.sType.FullName = sVar.Name;
		sVar.sType.Parent = 0;
		sVar.sType.ParentCount = 0;
		sVar.sType.Offset = 0;

        ReadShaderVariableType(psShaderInfo->ui32MajorVersion, pui32FirstConstBufToken, 
			(const uint32_t*)((const char*)pui32FirstConstBufToken+ui32TypeOffset), &sVar.sType);

        ui32DefaultValueOffset = *pui32VarToken++;


		if (psShaderInfo->ui32MajorVersion  >= 5)
		{
			uint32_t StartTexture = *pui32VarToken++;
			uint32_t TextureSize = *pui32VarToken++;
			uint32_t StartSampler = *pui32VarToken++;
			uint32_t SamplerSize = *pui32VarToken++;
		}

		sVar.haveDefaultValue = false;

        if(ui32DefaultValueOffset)
        {
			uint32_t i = 0;
			const uint32_t ui32NumDefaultValues = sVar.ui32Size / 4;
			const uint32_t* pui32DefaultValToken = (const uint32_t*)((const char*)pui32FirstConstBufToken+ui32DefaultValueOffset);

			//Always a sequence of 4-bytes at the moment.
			//bool const becomes 0 or 0xFFFFFFFF int, int & float are 4-bytes.
			ASSERT(sVar.ui32Size%4 == 0);

			sVar.haveDefaultValue = true;

			for(i=0; i<ui32NumDefaultValues;++i)
			{
				sVar.pui32DefaultValues.push_back(pui32DefaultValToken[i]);
			}
        }

		psBuffer->asVars.push_back(sVar);
    }


    {
        uint32_t ui32Flags;
        uint32_t ui32BufferType;

        psBuffer->ui32TotalSizeInBytes = *pui32Tokens++;
        ui32Flags = *pui32Tokens++;
        ui32BufferType = *pui32Tokens++;
    }

    return pui32Tokens;
}

static void ReadResources(const uint32_t* pui32Tokens,//in
                   ShaderInfo* psShaderInfo)//out
{
    ResourceBinding* psResBindings;
    ConstantBuffer* psConstantBuffers;
    const uint32_t* pui32ConstantBuffers;
    const uint32_t* pui32ResourceBindings;
    const uint32_t* pui32FirstToken = pui32Tokens;
    uint32_t i, k;

    const uint32_t ui32NumConstantBuffers = *pui32Tokens++;
    const uint32_t ui32ConstantBufferOffset = *pui32Tokens++;

    uint32_t ui32NumResourceBindings = *pui32Tokens++;
    uint32_t ui32ResourceBindingOffset = *pui32Tokens++;
    uint32_t ui32ShaderModel = *pui32Tokens++;
    uint32_t ui32CompileFlags = *pui32Tokens++;//D3DCompile flags? http://msdn.microsoft.com/en-us/library/gg615083(v=vs.85).aspx

    //Resources
    pui32ResourceBindings = (const uint32_t*)((const char*)pui32FirstToken + ui32ResourceBindingOffset);

    psResBindings = new ResourceBinding[ui32NumResourceBindings];

    psShaderInfo->ui32NumResourceBindings = ui32NumResourceBindings;
    psShaderInfo->psResourceBindings = psResBindings;

    k=0;
    for(i=0; i < ui32NumResourceBindings; ++i)
    {
        pui32ResourceBindings = ReadResourceBinding(pui32FirstToken, pui32ResourceBindings, psResBindings+i);

        switch(psResBindings[i].eType)
        {
            case RTYPE_CBUFFER:
            {
                ASSERT(k < MAX_CBUFFERS);
                psShaderInfo->aui32ConstBufferBindpointRemap[psResBindings[i].ui32BindPoint] = k++;
                break;
            }
            case RTYPE_UAV_RWBYTEADDRESS:
            case RTYPE_UAV_RWSTRUCTURED:
			case RTYPE_UAV_RWTYPED:
			case RTYPE_UAV_APPEND_STRUCTURED:
			case RTYPE_UAV_CONSUME_STRUCTURED:
			case RTYPE_UAV_RWSTRUCTURED_WITH_COUNTER:
            {
                ASSERT(k < MAX_UAV);
                psShaderInfo->aui32UAVBindpointRemap[psResBindings[i].ui32BindPoint] = k++;
            }
            default:
            {
                break;
            }
        }
    }

    //Constant buffers
    pui32ConstantBuffers = (const uint32_t*)((const char*)pui32FirstToken + ui32ConstantBufferOffset);

    psConstantBuffers = new ConstantBuffer[ui32NumConstantBuffers];

    psShaderInfo->ui32NumConstantBuffers = ui32NumConstantBuffers;
    psShaderInfo->psConstantBuffers = psConstantBuffers;

    for(i=0; i < ui32NumConstantBuffers; ++i)
    {
        pui32ConstantBuffers = ReadConstantBuffer(psShaderInfo, pui32FirstToken, pui32ConstantBuffers, psConstantBuffers+i);
    }
}

static const uint16_t* ReadClassType(const uint32_t* pui32FirstInterfaceToken, const uint16_t* pui16Tokens, ClassType* psClassType)
{
    const uint32_t* pui32Tokens = (const uint32_t*)pui16Tokens;
    uint32_t ui32NameOffset = *pui32Tokens;
    pui16Tokens+= 2;

    psClassType->ui16ID = *pui16Tokens++;
    psClassType->ui16ConstBufStride = *pui16Tokens++;
    psClassType->ui16Texture = *pui16Tokens++;
    psClassType->ui16Sampler = *pui16Tokens++;

    ReadStringFromTokenStream((const uint32_t*)((const char*)pui32FirstInterfaceToken+ui32NameOffset), psClassType->Name);

    return pui16Tokens;
}

static const uint16_t* ReadClassInstance(const uint32_t* pui32FirstInterfaceToken, const uint16_t* pui16Tokens, ClassInstance* psClassInstance)
{
    uint32_t ui32NameOffset = *pui16Tokens++ << 16;
    ui32NameOffset |= *pui16Tokens++;

    psClassInstance->ui16ID = *pui16Tokens++;
    psClassInstance->ui16ConstBuf = *pui16Tokens++;
    psClassInstance->ui16ConstBufOffset = *pui16Tokens++;
    psClassInstance->ui16Texture = *pui16Tokens++;
    psClassInstance->ui16Sampler = *pui16Tokens++;

    ReadStringFromTokenStream((const uint32_t*)((const char*)pui32FirstInterfaceToken+ui32NameOffset), psClassInstance->Name);

    return pui16Tokens;
}


static void ReadInterfaces(const uint32_t* pui32Tokens,
                        ShaderInfo* psShaderInfo)
{
    uint32_t i;
    uint32_t ui32StartSlot;
    const uint32_t* pui32FirstInterfaceToken = pui32Tokens;
    const uint32_t ui32ClassInstanceCount = *pui32Tokens++;
    const uint32_t ui32ClassTypeCount = *pui32Tokens++;
    const uint32_t ui32InterfaceSlotRecordCount = *pui32Tokens++;
    const uint32_t ui32InterfaceSlotCount = *pui32Tokens++;
    const uint32_t ui32ClassInstanceOffset = *pui32Tokens++;
    const uint32_t ui32ClassTypeOffset = *pui32Tokens++;
    const uint32_t ui32InterfaceSlotOffset = *pui32Tokens++;

    const uint16_t* pui16ClassTypes = (const uint16_t*)((const char*)pui32FirstInterfaceToken + ui32ClassTypeOffset);
    const uint16_t* pui16ClassInstances = (const uint16_t*)((const char*)pui32FirstInterfaceToken + ui32ClassInstanceOffset);
    const uint32_t* pui32InterfaceSlots = (const uint32_t*)((const char*)pui32FirstInterfaceToken + ui32InterfaceSlotOffset);

    const uint32_t* pui32InterfaceSlotTokens = pui32InterfaceSlots;

    ClassType* psClassTypes;
    ClassInstance* psClassInstances;

    psClassTypes = new ClassType[ui32ClassTypeCount];
    for(i=0; i<ui32ClassTypeCount; ++i)
    {
        pui16ClassTypes = ReadClassType(pui32FirstInterfaceToken, pui16ClassTypes, psClassTypes+i);
        psClassTypes[i].ui16ID = (uint16_t)i;
    }

    psClassInstances = new ClassInstance[ui32ClassInstanceCount];
    for(i=0; i<ui32ClassInstanceCount; ++i)
    {
        pui16ClassInstances = ReadClassInstance(pui32FirstInterfaceToken, pui16ClassInstances, psClassInstances+i);
    }

    //Slots map function table to $ThisPointer cbuffer variable index
    ui32StartSlot = 0;
    for(i=0; i<ui32InterfaceSlotRecordCount;++i)
    {
        uint32_t k;
        
        const uint32_t ui32SlotSpan = *pui32InterfaceSlotTokens++;
        const uint32_t ui32Count = *pui32InterfaceSlotTokens++;
        const uint32_t ui32TypeIDOffset = *pui32InterfaceSlotTokens++;
        const uint32_t ui32TableIDOffset = *pui32InterfaceSlotTokens++;

        const uint16_t* pui16TypeID = (const uint16_t*)((const char*)pui32FirstInterfaceToken+ui32TypeIDOffset);
        const uint32_t* pui32TableID = (const uint32_t*)((const char*)pui32FirstInterfaceToken+ui32TableIDOffset);

        for(k=0; k < ui32Count; ++k)
        {
            psShaderInfo->aui32TableIDToTypeID[*pui32TableID++] = *pui16TypeID++;
        }

        ui32StartSlot += ui32SlotSpan;
    }

    psShaderInfo->ui32NumClassInstances = ui32ClassInstanceCount;
    psShaderInfo->psClassInstances = psClassInstances;

    psShaderInfo->ui32NumClassTypes = ui32ClassTypeCount;
    psShaderInfo->psClassTypes = psClassTypes;
}

void GetConstantBufferFromBindingPoint(const uint32_t ui32BindPoint, ShaderInfo* psShaderInfo, ConstantBuffer** ppsConstBuf)
{
    uint32_t index;
    
    ASSERT(ui32BindPoint < MAX_CBUFFERS);
    
    index = psShaderInfo->aui32ConstBufferBindpointRemap[ui32BindPoint]; 
    
    ASSERT(index < psShaderInfo->ui32NumConstantBuffers);
    
    *ppsConstBuf = psShaderInfo->psConstantBuffers + index;
}

void GetUAVBufferFromBindingPoint(const uint32_t ui32BindPoint, ShaderInfo* psShaderInfo, ConstantBuffer** ppsConstBuf)
{
    uint32_t index;
    
    ASSERT(ui32BindPoint < MAX_UAV);
    
    index = psShaderInfo->aui32UAVBindpointRemap[ui32BindPoint]; 
    
	// Bo3b: changed to <= from <, as this assert seems to fire often, with no harm.
    ASSERT(index <= psShaderInfo->ui32NumConstantBuffers);
    
    *ppsConstBuf = psShaderInfo->psConstantBuffers + index;
}

int GetResourceFromBindingPoint(ResourceType eType, uint32_t ui32BindPoint, ShaderInfo* psShaderInfo, ResourceBinding** ppsOutBinding)
{
    uint32_t i;
    const uint32_t ui32NumBindings = psShaderInfo->ui32NumResourceBindings;
    ResourceBinding* psBindings = psShaderInfo->psResourceBindings;

    for(i=0; i<ui32NumBindings; ++i)
    {
        if(psBindings[i].eType == eType)
        {
			if(ui32BindPoint >= psBindings[i].ui32BindPoint && ui32BindPoint < (psBindings[i].ui32BindPoint + psBindings[i].ui32BindCount))
			{
				*ppsOutBinding = psBindings + i;
				return 1;
			}
        }
    }
    return 0;
}

// bo3b: fix the assignment warning for x64, because the data will never be expected to be larger than 4G.
//  Might be better to get latest version of James-Jones, but this has been modified to all be in C++
#pragma warning(push)
#pragma warning(disable : 4267)
int GetInterfaceVarFromOffset(uint32_t ui32Offset, ShaderInfo* psShaderInfo, ShaderVar** ppsShaderVar)
{
    uint32_t i;
    ConstantBuffer* psThisPointerConstBuffer = psShaderInfo->psThisPointerConstBuffer;

    const uint32_t ui32NumVars = psThisPointerConstBuffer->asVars.size();

    for(i=0; i<ui32NumVars; ++i)
    {
        if(ui32Offset >= psThisPointerConstBuffer->asVars[i].ui32StartOffset && 
            ui32Offset < (psThisPointerConstBuffer->asVars[i].ui32StartOffset + psThisPointerConstBuffer->asVars[i].ui32Size))
	    {
		    *ppsShaderVar = &psThisPointerConstBuffer->asVars[i];
		    return 1;
	    }
    }
    return 0;
}
#pragma warning(pop)

void LoadShaderInfo(const uint32_t ui32MajorVersion,
    const uint32_t ui32MinorVersion,
    const ReflectionChunks* psChunks,
    ShaderInfo* psInfo)
{
    const uint32_t* pui32Inputs = psChunks->pui32Inputs;
	const uint32_t* pui32Inputs11 = psChunks->pui32Inputs11;
    const uint32_t* pui32Resources = psChunks->pui32Resources;
    const uint32_t* pui32Interfaces = psChunks->pui32Interfaces;
    const uint32_t* pui32Outputs = psChunks->pui32Outputs;
	const uint32_t* pui32Outputs11 = psChunks->pui32Outputs11;

    psInfo->eTessOutPrim = TESSELLATOR_OUTPUT_UNDEFINED;
    psInfo->eTessPartitioning = TESSELLATOR_PARTITIONING_UNDEFINED;

    psInfo->ui32MajorVersion = ui32MajorVersion;
    psInfo->ui32MinorVersion = ui32MinorVersion;


    if(pui32Inputs)
        ReadInputSignatures(pui32Inputs, psInfo, 0);
    if(pui32Inputs11)
        ReadInputSignatures(pui32Inputs11, psInfo, 1);
    if(pui32Resources)
        ReadResources(pui32Resources, psInfo);
    if(pui32Interfaces)
        ReadInterfaces(pui32Interfaces, psInfo);
    if(pui32Outputs)
        ReadOutputSignatures(pui32Outputs, psInfo, 0);
    if(pui32Outputs11)
        ReadOutputSignatures(pui32Outputs11, psInfo, 1);

    {
        uint32_t i;
        for(i=0; i<psInfo->ui32NumConstantBuffers;++i)
        {
            std::string cbufName = psInfo->psConstantBuffers[i].Name;
            std::string cbufThisPointer = "$ThisPointer";
            if(cbufName.compare(cbufThisPointer) == 0)
            {
                psInfo->psThisPointerConstBuffer = &psInfo->psConstantBuffers[i];
            }
        }
    }
}

void FreeShaderInfo(ShaderInfo* psShaderInfo)
{
    delete[] psShaderInfo->psInputSignatures; psShaderInfo->psInputSignatures = 0;
    delete[] psShaderInfo->psResourceBindings; psShaderInfo->psResourceBindings = 0;
    delete[] psShaderInfo->psConstantBuffers; psShaderInfo->psConstantBuffers = 0;
    delete[] psShaderInfo->psClassTypes; psShaderInfo->psClassTypes = 0;
    delete[] psShaderInfo->psClassInstances; psShaderInfo->psClassInstances = 0;
    delete[] psShaderInfo->psOutputSignatures; psShaderInfo->psOutputSignatures = 0;

    psShaderInfo->ui32NumInputSignatures = 0;
    psShaderInfo->ui32NumResourceBindings = 0;
    psShaderInfo->ui32NumConstantBuffers = 0;
    psShaderInfo->ui32NumClassTypes = 0;
    psShaderInfo->ui32NumClassInstances = 0;
    psShaderInfo->ui32NumOutputSignatures = 0;
}

struct ConstantTableD3D9
{
    uint32_t size;
    uint32_t creator;
    uint32_t version;
    uint32_t constants;
    uint32_t constantInfos;
    uint32_t flags;
    uint32_t target;
};

// These enums match those in d3dx9shader.h.
enum RegisterSet
{
    RS_BOOL,
    RS_INT4,
    RS_FLOAT4,
    RS_SAMPLER,
};

enum TypeClass
{
    CLASS_SCALAR,
    CLASS_VECTOR,
    CLASS_MATRIX_ROWS,
    CLASS_MATRIX_COLUMNS,
    CLASS_OBJECT,
    CLASS_STRUCT,
};

enum Type
{
    PT_VOID,
    PT_BOOL,
    PT_INT,
    PT_FLOAT,
    PT_STRING,
    PT_TEXTURE,
    PT_TEXTURE1D,
    PT_TEXTURE2D,
    PT_TEXTURE3D,
    PT_TEXTURECUBE,
    PT_SAMPLER,
    PT_SAMPLER1D,
    PT_SAMPLER2D,
    PT_SAMPLER3D,
    PT_SAMPLERCUBE,
    PT_PIXELSHADER,
    PT_VERTEXSHADER,
    PT_PIXELFRAGMENT,
    PT_VERTEXFRAGMENT,
    PT_UNSUPPORTED,
};
typedef struct ConstantInfoD3D9_TAG
{
    uint32_t name;
    uint16_t registerSet;
    uint16_t registerIndex;
    uint16_t registerCount;
    uint16_t reserved;
    uint32_t typeInfo;
    uint32_t defaultValue;
} ConstantInfoD3D9;

typedef struct TypeInfoD3D9_TAG
{
    uint16_t typeClass;
    uint16_t type;
    uint16_t rows;
    uint16_t columns;
    uint16_t elements;
    uint16_t structMembers;
    uint32_t structMemberInfos;
} TypeInfoD3D9;

typedef struct StructMemberInfoD3D9_TAG
{
    uint32_t name;
    uint32_t typeInfo;
} StructMemberInfoD3D9;

void LoadD3D9ConstantTable(const char* data,
    ShaderInfo* psInfo)
{
    ConstantTableD3D9* ctab;
    uint32_t constNum;
    ConstantInfoD3D9* cinfos;
    ConstantBuffer* psConstantBuffer;
    uint32_t ui32ConstantBufferSize = 0;
	uint32_t numResourceBindingsNeeded = 0;

    ctab = (ConstantTableD3D9*)data;

    cinfos = (ConstantInfoD3D9*) (data + ctab->constantInfos);

    psInfo->ui32NumConstantBuffers++;

    //Only 1 Constant Table in d3d9
    ASSERT(psInfo->ui32NumConstantBuffers==1);

    psConstantBuffer = new ConstantBuffer();

    psInfo->psConstantBuffers = psConstantBuffer;

    psConstantBuffer->Name = "$Globals";

	//Determine how many resource bindings to create
	for(constNum = 0; constNum < ctab->constants; ++constNum)
	{
		if(cinfos[constNum].registerSet == RS_SAMPLER)
		{
			++numResourceBindingsNeeded;
		}
	}

	psInfo->psResourceBindings = new ResourceBinding[numResourceBindingsNeeded];

    for(constNum = 0; constNum < ctab->constants; ++constNum)
    {
		TypeInfoD3D9* typeInfo = (TypeInfoD3D9*) (data + cinfos[constNum].typeInfo);
		ShaderVar var;

		if(cinfos[constNum].registerSet != RS_SAMPLER)
		{
	        var.Name = data + cinfos[constNum].name;
    	    var.ui32Size = cinfos[constNum].registerCount * 16;
       		var.ui32StartOffset = cinfos[constNum].registerIndex * 16;
			var.haveDefaultValue = false;

			if(ui32ConstantBufferSize < (var.ui32Size + var.ui32StartOffset))
			{
				ui32ConstantBufferSize = var.ui32Size + var.ui32StartOffset;
			}

			var.sType.Rows = typeInfo->rows;
			var.sType.Columns = typeInfo->columns;
			var.sType.Elements = typeInfo->elements;
			var.sType.MemberCount = typeInfo->structMembers;
			var.sType.Members = 0;
			var.sType.Offset = 0;
			var.sType.FullName = var.Name;
			var.sType.Parent = 0;
			var.sType.ParentCount = 0;

			switch(typeInfo->typeClass)
			{
				case CLASS_SCALAR:
				{
					var.sType.Class = SVC_SCALAR;
					break;
				}
				case CLASS_VECTOR:
				{
					var.sType.Class = SVC_VECTOR;
					break;
				}
				case CLASS_MATRIX_ROWS:
				{
					var.sType.Class = SVC_MATRIX_ROWS;
					break;
				}
				case CLASS_MATRIX_COLUMNS:
				{
					var.sType.Class = SVC_MATRIX_COLUMNS;
					break;
				}
				case CLASS_OBJECT:
				{
					var.sType.Class = SVC_OBJECT;
					break;
				}
				case CLASS_STRUCT:
				{
					var.sType.Class = SVC_STRUCT;
					break;
				}
			}

			switch(cinfos[constNum].registerSet)
			{
				case RS_BOOL:
				{
					var.sType.Type = SVT_BOOL;
					break;
				}
				case RS_INT4:
				{
					var.sType.Type = SVT_INT;
					break;
				}
				case RS_FLOAT4:
				{
					var.sType.Type = SVT_FLOAT;
					break;
				}
			}
			psConstantBuffer->asVars.push_back(var);
		}
		else
		{
			// Create a resource if it is sampler in order to replicate the d3d10+
			// method of separating samplers from general constants.
			uint32_t ui32ResourceIndex = psInfo->ui32NumResourceBindings++;
			ResourceBinding* res = &psInfo->psResourceBindings[ui32ResourceIndex];

			res->Name = data + cinfos[constNum].name;

			res->ui32BindPoint = cinfos[constNum].registerIndex;
			res->ui32BindCount = cinfos[constNum].registerCount;
			res->ui32Flags = 0;
			res->ui32NumSamples = 1;
			res->ui32ReturnType = 0;

			res->eType = RTYPE_TEXTURE;

			switch(typeInfo->type)
			{
			case PT_SAMPLER:
			case PT_SAMPLER1D:
				res->eDimension = REFLECT_RESOURCE_DIMENSION_TEXTURE1D;
				break;
			case PT_SAMPLER2D:
				res->eDimension = REFLECT_RESOURCE_DIMENSION_TEXTURE2D;
				break;
			case PT_SAMPLER3D:
				res->eDimension = REFLECT_RESOURCE_DIMENSION_TEXTURE2D;
				break;
			case PT_SAMPLERCUBE:
				res->eDimension = REFLECT_RESOURCE_DIMENSION_TEXTURECUBE;
				break;
			}
		}
    }
    psConstantBuffer->ui32TotalSizeInBytes = ui32ConstantBufferSize;
}
