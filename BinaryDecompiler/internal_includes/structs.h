#pragma once

#include <map>
#include <vector>
#include <string>
#include "hlslcc.h"

#include "internal_includes/tokens.h"
#include "internal_includes/reflect.h"

static enum{ MAX_SUB_OPERANDS = 3};

struct Operand
{
    int iExtended;
    OPERAND_TYPE eType;
    OPERAND_MODIFIER eModifier;
    OPERAND_MIN_PRECISION eMinPrecision;
    int iIndexDims;
    int indexRepresentation[4];
    int writeMask;
    int iGSInput;
    int iWriteMaskEnabled;

    int iNumComponents;

    OPERAND_4_COMPONENT_SELECTION_MODE eSelMode;
    uint32_t ui32CompMask;
    uint32_t ui32Swizzle;
    uint32_t aui32Swizzle[4];

    uint32_t aui32ArraySizes[3];
    uint32_t ui32RegisterNumber;
    //If eType is OPERAND_TYPE_IMMEDIATE32
    float afImmediates[4];
    //If eType is OPERAND_TYPE_IMMEDIATE64
    double adImmediates[4];

	int iIntegerImmediate;

    SPECIAL_NAME eSpecialName;
    std::string specialName;

    OPERAND_INDEX_REPRESENTATION eIndexRep[3];

    Operand* psSubOperand[MAX_SUB_OPERANDS];

	//One type for each component.
	SHADER_VARIABLE_TYPE aeDataType[4];
#ifdef _DEBUG
    uint64_t id;
#endif
};

struct Instruction
{
    OPCODE_TYPE eOpcode;
    INSTRUCTION_TEST_BOOLEAN eBooleanTestType;
	COMPARISON_DX9 eDX9TestType;
    uint32_t ui32SyncFlags;
    uint32_t ui32NumOperands;
    Operand asOperands[6];
    uint32_t bSaturate;
    uint32_t ui32FuncIndexWithinInterface;

	RESINFO_RETURN_TYPE eResInfoReturnType;		// added for ResInfo parse

    int bAddressOffset;
    int iUAddrOffset;
    int iVAddrOffset;
    int iWAddrOffset;

#ifdef _DEBUG
    uint64_t id;
#endif
};

static enum{ MAX_IMMEDIATE_CONST_BUFFER_VEC4_SIZE = 1024};

struct ICBVec4 {
	uint32_t a;
	uint32_t b;
	uint32_t c;
	uint32_t d;
};

struct Declaration
{
    OPCODE_TYPE eOpcode;

    uint32_t ui32NumOperands;

    Operand asOperands[2];

	ICBVec4 asImmediateConstBuffer[MAX_IMMEDIATE_CONST_BUFFER_VEC4_SIZE];
    //The declaration can set one of these
    //values depending on the opcode.
    union {
        uint32_t ui32GlobalFlags;
        uint32_t ui32NumTemps;
        RESOURCE_DIMENSION eResourceDimension;
        INTERPOLATION_MODE eInterpolation;
        PRIMITIVE_TOPOLOGY eOutputPrimitiveTopology;
        PRIMITIVE eInputPrimitive;
        uint32_t ui32MaxOutputVertexCount;
        TESSELLATOR_DOMAIN eTessDomain;
        TESSELLATOR_PARTITIONING eTessPartitioning;
        TESSELLATOR_OUTPUT_PRIMITIVE eTessOutPrim;
        uint32_t aui32WorkGroupSize[3];
        //Fork phase index followed by the instance count.
        uint32_t aui32HullPhaseInstanceInfo[2];
        float fMaxTessFactor;
        uint32_t ui32IndexRange;

        struct Interface_TAG
        {
            uint32_t ui32InterfaceID;
            uint32_t ui32NumFuncTables;
            uint32_t ui32ArraySize;
        } interface;
    } value;

    struct UAV_TAG
    {
        uint32_t ui32GloballyCoherentAccess;
        uint32_t ui32BufferSize;
		uint8_t bCounter;
    } sUAV;

    struct TGSM
    {
        uint32_t ui32Stride;
        uint32_t ui32Count;
    } sTGSM;

    uint32_t ui32TableLength;

	uint32_t ui32IsShadowTex;

};

struct Shader
{
    uint32_t ui32MajorVersion;
    uint32_t ui32MinorVersion;
    SHADER_TYPE eShaderType;

    GLLang eTargetLanguage;

	int fp64;

    //DWORDs in program code, including version and length tokens.
    uint32_t ui32ShaderLength;

    std::vector<Declaration> psDecl;

    //Instruction* functions;//non-main subroutines

    std::map<int, uint32_t> aui32FuncTableToFuncPointer;
    std::map<int, uint32_t> aui32FuncBodyToFuncTable;

    struct FuncTable {
        std::map<int, uint32_t> aui32FuncBodies;
    };
	std::map<int, FuncTable> funcTable;

    struct FuncPointer {
        std::map<int, uint32_t> aui32FuncTables;
        uint32_t ui32NumBodiesPerTable;
    };
	std::map<int, FuncPointer> funcPointer;

    std::vector<uint32_t> ui32NextClassFuncName;

    std::vector<Instruction> psInst;

    const uint32_t* pui32FirstToken;//Reference for calculating current position in token stream.

	//Hull shader declarations and instructions.
	//psDecl, psInst are null for hull shaders.
	uint32_t ui32HSDeclCount;
	Declaration* psHSDecl;

	uint32_t ui32HSControlPointDeclCount;
	Declaration* psHSControlPointPhaseDecl;

	uint32_t ui32HSControlPointInstrCount;
	Instruction* psHSControlPointPhaseInstr;

    uint32_t ui32ForkPhaseCount;

	uint32_t aui32HSForkDeclCount[MAX_FORK_PHASES];
	Declaration* apsHSForkPhaseDecl[MAX_FORK_PHASES];

	uint32_t aui32HSForkInstrCount[MAX_FORK_PHASES];
	Instruction* apsHSForkPhaseInstr[MAX_FORK_PHASES];

	uint32_t ui32HSJoinDeclCount;
	Declaration* psHSJoinPhaseDecl;

	std::vector<Instruction> psHSJoinPhaseInstr;

    ShaderInfo *sInfo;

	std::vector<int> abScalarInput;

    std::map<int, int> aIndexedOutput;

    std::map<int, int> aIndexedInput;
    std::map<int, int> aIndexedInputParents;

    std::vector<RESOURCE_DIMENSION> aeResourceDims;

    std::vector<int> aiInputDeclaredSize;

    std::vector<int> aiOutputDeclared;

    //Does not track built-in inputs.
    std::map<int, int> abInputReferencedByInstruction;

	//int aiOpcodeUsed[NUM_OPCODES];

	Shader() :
		ui32MajorVersion(0),
		ui32MinorVersion(0),
		ui32ShaderLength(0),
		pui32FirstToken(0),
		ui32HSDeclCount(0),
		psHSDecl(0),
		ui32HSControlPointDeclCount(0),
		psHSControlPointPhaseDecl(0),
		ui32HSControlPointInstrCount(0),
		psHSControlPointPhaseInstr(0),
		ui32ForkPhaseCount(0),
		ui32HSJoinDeclCount(0),
		psHSJoinPhaseDecl(0),
		psDecl(),
		aui32FuncTableToFuncPointer(),
		aui32FuncBodyToFuncTable(),
		funcTable(),
		funcPointer(),
		ui32NextClassFuncName(),
		psInst(),
		psHSJoinPhaseInstr(),
		abScalarInput(),
		aIndexedOutput(),
		aIndexedInput(),
		aIndexedInputParents(),
		aeResourceDims(),
		aiInputDeclaredSize(),
		aiOutputDeclared(),
		abInputReferencedByInstruction()
	{
		for (int i = 0; i < MAX_FORK_PHASES; ++i)
		{
			aui32HSForkDeclCount[i] = 0;
			apsHSForkPhaseDecl[i] = 0;
			aui32HSForkInstrCount[i] = 0;
			apsHSForkPhaseInstr[i] = 0;
		}
		sInfo = new ShaderInfo();
	}

	~Shader()
	{
		delete sInfo;
		sInfo = 0;
	}
};

static const uint32_t MAIN_PHASE = 0;
static const uint32_t HS_FORK_PHASE = 1;
static const uint32_t HS_CTRL_POINT_PHASE = 2;
static const uint32_t HS_JOIN_PHASE = 3;
static enum{ NUM_PHASES = 4};

