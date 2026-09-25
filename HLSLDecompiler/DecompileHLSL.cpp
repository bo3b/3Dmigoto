// The goal with this file is too keep it as much C++ as possible.
// So we avoid adding Windows specific stuff if at all possible.
//
// To that end, we are also changing some fundamental use types from int/long to size_t
// where it makes sense as an actual size measurement.  We are doing this because the basic
// functions like strlen and sizeof return size_t, which varies in size from x32 to x64.
// Otherwise comparisons to strlen or assignments can report warnings of truncation.
// This also fixes all signed/unsigned mismatch warnings.
//
// Also using 'size_t' for some for loops here, because of the inherent problems of 'int'
// being signed.  I tried using 'auto', but it made poor choices that still had warnings.
// I changed all the variants that are simple and clear, and left the unusual, less clear variants as is.
// Changing to size_t fixes a large number of signed/unsigned mismatch warnings.
//
// We are also using the CRT_SECURE flags to automatically fix sprintf, sscanf into _s safe versions
// where it can. _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1 _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT = 1
//
// All sscanf that did not use %s or %c were switched to sscanf_s to avoid warnings.

#include <map>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <set>
#include <algorithm>

#include "DecompileHLSL.h"

#include "BinaryDecompiler\internal_includes\structs.h"
#include "BinaryDecompiler\internal_includes\decode.h"

#include <excpt.h>

#include "assert.h"
#include "log.h"
#include "version.h"

// MSVC insists we use MS's secure version of scanf, which in turn insists we
// pass the size of each string/char array as an unsigned integer. We want to
// use something like sizeof/_countof/ARRAYSIZE to make that safe even if we
// change the size of one of the buffers, but that returns a size_t, which does
// not match the type scanf is expecting to find on the stack on x64 so we need
// to cast it. That's a bit ugly, so we use this helper:
#define UCOUNTOF(...) (unsigned)_countof(__VA_ARGS__)

using namespace std;

enum DataType
{
	DT_bool,
	DT_bool4,
	DT_float,
	DT_float2,
	DT_float3,
	DT_float4,
	DT_float4x2,
	DT_float3x3,
	DT_float4x3,
	DT_float2x4,
	DT_float3x4,
	DT_float4x4,
	DT_uint4,
	DT_int4,
	DT_uint3,
	DT_int3,
	DT_uint2,
	DT_int2,
	DT_uint,
	DT_int,
	// FIXME: Missing types added for primitive type StructuredBuffers, but not yet rest of decompiler:
	DT_float3x2, DT_float2x3, DT_float2x2,
	DT_float4x1, DT_float3x1, DT_float2x1,
	DT_float1x4, DT_float1x3, DT_float1x2, DT_float1x1,
	DT_float1, DT_uint1, DT_int1, DT_bool1,
	DT_bool2, DT_bool3,
	DT_dword, // Same as uint (MS continues spreading the incorrect definition of a word), but has no vector or matrix variants
	DT_half4x4, DT_half4x3, DT_half4x2, DT_half4x1,
	DT_half3x4, DT_half3x3, DT_half3x2, DT_half3x1,
	DT_half2x4, DT_half2x3, DT_half2x2, DT_half2x1,
	DT_half1x4, DT_half1x3, DT_half1x2, DT_half1x1,
	DT_uint4x4, DT_uint4x3, DT_uint4x2, DT_uint4x1,
	DT_uint3x4, DT_uint3x3, DT_uint3x2, DT_uint3x1,
	DT_uint2x4, DT_uint2x3, DT_uint2x2, DT_uint2x1,
	DT_uint1x4, DT_uint1x3, DT_uint1x2, DT_uint1x1,
	DT_int4x4, DT_int4x3, DT_int4x2, DT_int4x1,
	DT_int3x4, DT_int3x3, DT_int3x2, DT_int3x1,
	DT_int2x4, DT_int2x3, DT_int2x2, DT_int2x1,
	DT_int1x4, DT_int1x3, DT_int1x2, DT_int1x1,
	DT_bool4x4, DT_bool4x3, DT_bool4x2, DT_bool4x1,
	DT_bool3x4, DT_bool3x3, DT_bool3x2, DT_bool3x1,
	DT_bool2x4, DT_bool2x3, DT_bool2x2, DT_bool2x1,
	DT_bool1x4, DT_bool1x3, DT_bool1x2, DT_bool1x1,
	// FIXME: Add support for double, doubleN, doubleNxM (shader model 5+)
	// FUTURE: Minimum precision types (Win8+)
	DT_Unknown
};
struct BufferEntry
{
	DataType bt;
	int matrixRow;
	bool isRowMajor;
	string Name;
};
// Key is register << 16 + offset
typedef map<int, BufferEntry> CBufferData;
typedef map<string, string> StringStringMap;

//dx9
struct ConstantValue
{
	string name;
	float x;
	float y;
	float z;
	float w;
};
//dx9

// Convenience routine to calculate just the number of swizzle components.
// Used for ibfe.  Inputs like 'o1.xy', return 2.

static string swizCount(char *operand)
{
	if (!operand)
		return "";

	size_t count = strlen(operand) - (strrchr(operand, '.') - operand) - 1;
	return (count == 1) ? "" : to_string(count);
}

class Decompiler
{
public:

	// Key is register << 16 + offset
	CBufferData mCBufferData;

	// Resources.
	map<string, int> mCBufferNames;

	map<int, string> mSamplerNames;
	map<int, int>    mSamplerNamesArraySize;
	map<int, string> mSamplerComparisonNames;
	map<int, int>    mSamplerComparisonNamesArraySize;

	map<int, string> mTextureNames;
	map<int, int>    mTextureNamesArraySize;
	map<int, string> mTextureType;

	map<int, string> mUAVNames;
	map<int, int>    mUAVNamesArraySize;
	map<int, string> mUAVType;

	map<string, string> mStructuredBufferTypes;
	set<string> mStructuredBufferUsedNames;
	map<string, string> mStructuredBufferDeclarations;
	map<string, vector<pair<int, int> > > mStructuredBufferFieldRanges;
	map<string, set<int> > mStructuredBufferIntegerOffsets;

	//dx9
	map<int, string> mUniformNames;
	map<int, string> mBoolUniformNames;
	map<int, ConstantValue> mConstantValues;
	map<int, string> mInputNames;
	//dx9

	// Output register tracking.
	map<string, string> mOutputRegisterValues;
	map<string, DataType> mOutputRegisterType;
	string mShaderType;
	string mSV_Position;
	bool mUsesProjection;
	Instruction *mLastStatement;
	string mMulOperand, mMulOperand2, mMulTarget;
	StringStringMap mCorrectedIndexRegisters;
	StringStringMap mRemappedOutputRegisters;
	vector<pair<string, string> > mRemappedInputRegisters;
	set<string> mBooleanRegisters;
	map<string, string> mIntegerAliases;
	map<string, string> mIntegerReadAliases;
	set<string> mDeclaredIntegerAliases;
	set<string> mIntegerBitPatternComponents;
	set<string> mIntegerReadBitPatternComponents;

	DecompilerSettings *G;

	vector<char> mOutput;
	size_t mCodeStartPos;		// Used as index into buffer, name misleadingly suggests pointer usage.
	bool mErrorOccurred;
	bool mPatched;
	int uuidVar;

	// Auto-indent of generated code
	const char* indent = "  ";
	int nestCount;

	Decompiler()
		: mLastStatement(0),
		uuidVar(0),
		nestCount(0)
	{}

	void logDecompileError(const string &err)
	{
		mErrorOccurred = true;
		LogInfo("    error parsing shader> %s\n", err.c_str());
	}

	DataType TranslateType(const char *name)
	{
		if (!strcmp(name, "float4x4")) return DT_float4x4;
		if (!strcmp(name, "float4x3")) return DT_float4x3;
		if (!strcmp(name, "float4x2")) return DT_float4x2;
		if (!strcmp(name, "float2x4")) return DT_float2x4;
		if (!strcmp(name, "float3x4")) return DT_float3x4;
		if (!strcmp(name, "float4")) return DT_float4;
		if (!strcmp(name, "float3x3")) return DT_float3x3;
		if (!strcmp(name, "float3")) return DT_float3;
		if (!strcmp(name, "float2")) return DT_float2;
		if (!strcmp(name, "float1")) return DT_float;
		if (!strcmp(name, "float")) return DT_float;
		if (!strcmp(name, "bool")) return DT_bool;
		if (!strcmp(name, "bool4")) return DT_bool4;
		if (!strcmp(name, "uint4")) return DT_uint4;
		if (!strcmp(name, "uint3")) return DT_uint3;
		if (!strcmp(name, "uint2")) return DT_uint2;
		if (!strcmp(name, "uint")) return DT_uint;
		if (!strcmp(name, "int4")) return DT_int4;
		if (!strcmp(name, "int3")) return DT_int3;
		if (!strcmp(name, "int2")) return DT_int2;
		if (!strcmp(name, "int")) return DT_int;
		// FIXME: Missing types added for primitive type StructuredBuffers, but not yet rest of decompiler:
#define DT(x) do { if (!strcmp(name, #x)) return DT_##x; } while (0)
		DT(float3x2); DT(float2x3); DT(float2x2);
		DT(float4x1); DT(float3x1); DT(float2x1);
		DT(float1x4); DT(float1x3); DT(float1x2); DT(float1x1);
		DT(float1); DT(uint1); DT(int1); DT(bool1);
		DT(bool2); DT(bool3);
		DT(dword);
		DT(half4x4); DT(half4x3); DT(half4x2); DT(half4x1);
		DT(half3x4); DT(half3x3); DT(half3x2); DT(half3x1);
		DT(half2x4); DT(half2x3); DT(half2x2); DT(half2x1);
		DT(half1x4); DT(half1x3); DT(half1x2); DT(half1x1);
		DT(uint4x4); DT(uint4x3); DT(uint4x2); DT(uint4x1);
		DT(uint3x4); DT(uint3x3); DT(uint3x2); DT(uint3x1);
		DT(uint2x4); DT(uint2x3); DT(uint2x2); DT(uint2x1);
		DT(uint1x4); DT(uint1x3); DT(uint1x2); DT(uint1x1);
		DT(int4x4); DT(int4x3); DT(int4x2); DT(int4x1);
		DT(int3x4); DT(int3x3); DT(int3x2); DT(int3x1);
		DT(int2x4); DT(int2x3); DT(int2x2); DT(int2x1);
		DT(int1x4); DT(int1x3); DT(int1x2); DT(int1x1);
		DT(bool4x4); DT(bool4x3); DT(bool4x2); DT(bool4x1);
		DT(bool3x4); DT(bool3x3); DT(bool3x2); DT(bool3x1);
		DT(bool2x4); DT(bool2x3); DT(bool2x2); DT(bool2x1);
		DT(bool1x4); DT(bool1x3); DT(bool1x2); DT(bool1x1);
#undef DT
		logDecompileError("Unknown data type: " + string(name));
		return DT_Unknown;
	}

	// Make this bump to new line slightly more clear by making it a convenience routine.
	static void NextLine(const char *c, size_t &pos, size_t max)
	{
		while (c[pos] != 0x0a && pos < max) 
			pos++; 
		pos++;
	}

	// Just take the current input line, and copy it to the output.
	// This is used when we aren't sure what to do with something, and gives us the lines
	// in the output as ASM for reference. Specifically modifying the input pos.
	void ASMLineOut(const char *c, size_t &pos, size_t max)
	{
		char buffer[256];

		const char *startPos = c + pos;
		const char *eolPos = strchr(startPos, '\n');
		std::string line(startPos, eolPos);
		sprintf(buffer, "%s\n", line.c_str());
		appendOutput(buffer);
	}


	// This is to fix a specific problem seen with Batman, where the fxc compiler
	// is aggressive and packs the input down to a single float4, where it should
	// be two float2's. This is going to fix only this specific case, because there
	// are other times when the packing is right.
	// We'll do this by reading ahead a full line, and seeing if the upcoming
	// sequence is of the form:
	// Name                 Index   Mask Register SysValue  Format   Used
	// -------------------- ----- ------ -------- -------- ------- ------
	// TEXCOORD                 0   xy          0     NONE   float   xy  
	// TEXCOORD                 1   xy          1     NONE   float   xy  
	//
	// The goal is to switch from outputing:
	//		float2 v0 : TEXCOORD0,
	//		float2 v1 : TEXCOORD1,
	// to:
	//		float4 v0 : TEXCOORD0,	// change packing to float4 if different texcoord
	//		float2 v1 : TEXCOORD1,
	//
	// This is only going to be done for TEXCOORD.  I've looked at examples of 
	// outputs like SV_TARGET, where fxc could have packed them, but it did not.
	// Adding the extra parts there creates spurious warnings, so for now, 
	// we'll only do TEXCOORD as a known problem.
	// This does also unfortunately create warnings for the TEXCOORD outputs, but
	// I was unable to find any other way to avoid the fxc packing optimization.

	bool SkipPacking(const char *c, map<string, DataType> inUse)
	{
		char name[256], mask[16], sysvalue[16], format[16];
		int index, reg1, reg2; format[0] = 0; mask[0] = 0;
		size_t pos = 0;

		int numRead = sscanf_s(c + pos, "// %s %d %s %d %s %s",
			name, UCOUNTOF(name), &index, mask, UCOUNTOF(mask), &reg1, sysvalue, UCOUNTOF(sysvalue), format, UCOUNTOF(format));
		if (numRead != 6)
			return false;
		name[sizeof(name) - 1] = '\0'; // Appease the static analysis gods
		if (strcmp(name, "TEXCOORD") != 0)
			return false;

		// Any v* register already active needs to be skipped, to allow the 
		// normal packing to succeed.  input:v1.xy + w1.zw. or output:o1.xy + p1.zw
		if (inUse.find(string("v" + to_string(reg1))) != inUse.end())
			return false;
		if (inUse.find(string("o" + to_string(reg1))) != inUse.end())
			return false;

		// skip pointer in order to parse next line
		while (c[pos] != 0x0a && pos < 200) pos++; pos++;

		numRead = sscanf_s(c + pos, "// %s %d %s %d %s %s",
			name, UCOUNTOF(name), &index, mask, UCOUNTOF(mask), &reg2, sysvalue, UCOUNTOF(sysvalue), format, UCOUNTOF(format));
		if (numRead != 6)
			return false;
		name[sizeof(name) - 1] = '\0'; // Appease the static analysis gods
		if (strcmp(name, "TEXCOORD") != 0)
			return false;

		string line1, line2;
		if (gLogDebug)
		{
			line1 = string(c + 0);
			line1 = line1.substr(0, line1.find('\n'));
			line2 = string(c + pos);
			line2 = line2.substr(0, line2.find('\n'));
		}

		// The key aspect is whether we are supposed to use a different Register.
		if (reg1 == reg2)
		{
			LogDebug("    SkipPacking false for v%d==v%d\n", reg1, reg2);
			LogDebug("      %s\n", line1.c_str());
			LogDebug("      %s\n", line2.c_str());
			return false;
		}

		LogDebug("    SkipPacking true for:\n");
		LogDebug("      %s\n", line1.c_str());
		LogDebug("      %s\n", line2.c_str());

		return true;
	}

	// Looking up the interpolation values for a given input, like 'centroid'
	// or 'linear centroid'.  These are not found in the text declaration for
	// Input Signature, so we are fetching them from the already parsed James-
	// Jones input.  Otherwise we'd need to parse the dcl_input_ps phrases.

	string GetInterpolation(Shader *shader, string vRegister)
	{
		string interpolation = "";

		for each(Declaration declaration in shader->asPhase[MAIN_PHASE].ppsDecl[0])
		{
			if (declaration.eOpcode == OPCODE_DCL_INPUT_PS)
			{
				int regOut = stoi(vRegister.substr(1));
				uint32_t binReg = declaration.asOperands[0].ui32RegisterNumber;
				if (regOut == binReg)
				{
					switch (declaration.value.eInterpolation)
					{
					case INTERPOLATION_CONSTANT:
						// The constant interpolation modifier in assembly means do not
						// interpolate, which in HLSL uses the nointerpolation modifier.
						// Seen in Arkham Knight
						interpolation = "nointerpolation ";
						break;
					// Let's skip adding this, as it's the default, and adds noise.
					//case INTERPOLATION_LINEAR:
					//	interpolation = "linear ";
					//	break;
					case INTERPOLATION_LINEAR_CENTROID:
						interpolation = "linear centroid ";
						break;
					case INTERPOLATION_LINEAR_NOPERSPECTIVE:
						interpolation = "linear noperspective ";
						break;
					case INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID:
						interpolation = "linear noperspective centroid ";
						break;
					case INTERPOLATION_LINEAR_SAMPLE:
						interpolation = "linear sample ";
						break;
					case INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE:
						interpolation = "linear noperspective sample ";
						break;

					default:
						break;
					}
				}
			}
		}

		return interpolation;
	}


	// Input signature:
	//
	// Name                 Index   Mask Register SysValue  Format   Used
	// -------------------- ----- ------ -------- -------- ------- ------
	// TEXCOORD                 0   xy          0     NONE   float   xy  
	// TEXCOORD                 1   xy          1     NONE   float   xy  
	// COLOR                    3   xyz         2     NONE   float   xyz 

	void ParseInputSignature(Shader *shader, const char *c, size_t size)
	{
		// DataType is not used here, just a convenience for calling SkipPacking.
		map<string, DataType> usedInputRegisters;

		mRemappedInputRegisters.clear();
		// Write header.  Extra space handles odd case for no input and no output sections.
		const char *inputHeader = "\nvoid main(\n";
		mOutput.insert(mOutput.end(), inputHeader, inputHeader + strlen(inputHeader));

		// Read until header.
		const char *headerid = "// Input signature:";
		size_t pos = 0;
		while (pos < size - strlen(headerid))
		{
			if (!strncmp(c + pos, headerid, strlen(headerid)))
				break;
			else
			{
				NextLine(c, pos, size);
			}
		}
		if (pos >= size - strlen(headerid)) return;
		// Skip header.
		for (int i = 0; i < 4; ++i)
		{
			NextLine(c, pos, size);
		}
		// Read list.
		while (pos < size)
		{
			char name[256], mask[16], format[16], format2[16];
			int index, slot; format[0] = 0; mask[0] = 0;
			if (!strncmp(c + pos, "// no Input", strlen("// no Input")))
				break;
			int numRead = sscanf_s(c + pos, "// %s %d %s %d %s %s",
				name, UCOUNTOF(name), &index, mask, UCOUNTOF(mask), &slot, format2, UCOUNTOF(format2), format, UCOUNTOF(format));
			if (numRead != 6)
			{
				logDecompileError("Error parsing input signature: " + string(c + pos, 80));
				return;
			}
			// finish type.
			if (SkipPacking(c + pos, usedInputRegisters))
				sprintf(format2, "%s%d", format, 4);				// force to float4
			else
				if (strlen(mask) > 1)
					sprintf(format2, "%s%d", format, (int)strlen(mask));	// e.g. float2
				else
					strcpy(format2, format);
			// Already used?
			char registerName[32];
			sprintf(registerName, "v%d", slot);
			string regNameStr = registerName;
			map<string, DataType>::iterator i = usedInputRegisters.find(regNameStr);
			if (i != usedInputRegisters.end())
			{
				sprintf(registerName, "w%d.", slot);
				const char *INDEX_MASK = "xyzw";
				string newName = registerName;
				for (size_t j = 0; j < strlen(mask); ++j)
					newName.push_back(INDEX_MASK[j]);
				mRemappedInputRegisters.push_back(pair<string, string>(regNameStr + "." + string(mask), newName));
				if (strlen(mask) > 1)
				{
					for (size_t j = 0; j < strlen(mask); ++j)
					{
						newName = registerName;
						newName.push_back(INDEX_MASK[j]);
						string oldName = regNameStr + ".";
						oldName.push_back(mask[j]);
						mRemappedInputRegisters.push_back(pair<string, string>(oldName, newName));
					}
				}
				sprintf(registerName, "w%d", slot);
				regNameStr = registerName;
			}
			else
			{
				usedInputRegisters[regNameStr] = TranslateType(format2);
			}

			// Now adding interpolation modifiers like 'centroid' as fetched from James-Jones input
			string modifier = GetInterpolation(shader, regNameStr);

			// Write, e.g.  centroid  float4 v4 : TEXCOORD2,
			char buffer[256];
			sprintf(buffer, "  %s%s %s : %s%d,\n", modifier.c_str(), format2, regNameStr.c_str(), name, index);
			mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			NextLine(c, pos, size);
			// End?
			if (!strncmp(c + pos, "//\n", 3) || !strncmp(c + pos, "//\r", 3))
				break;
		}
	}

	void ParseOutputSignature(const char *c, size_t size)
	{
		mOutputRegisterType.clear();
		mRemappedOutputRegisters.clear();
		mSV_Position.clear();
		// Read until header.
		const char *headerid = "// Output signature:";
		size_t pos = 0;
		while (pos < size - strlen(headerid))
		{
			if (!strncmp(c + pos, headerid, strlen(headerid)))
				break;
			else
			{
				NextLine(c, pos, size);
			}
		}
		if (pos >= size - strlen(headerid)) return;
		// Skip header.
		for (int i = 0; i < 4; ++i)
		{
			NextLine(c, pos, size);
		}
		// Read list.
		while (pos < size)
		{
			char name[256], mask[16], format[16], format2[16];
			int index, slot; format[0] = 0; mask[0] = 0;
			if (!strncmp(c + pos, "// no Output", strlen("// no Output")))
				break;

			// Name                 Index   Mask Register SysValue  Format   Used
			// -------------------- ----- ------ -------- -------- ------- ------
			// SV_Target                0   xyzw        0   TARGET   float   xyzw
			int numRead = sscanf_s(c + pos, "// %s %d %s %d %s %s",
				name, UCOUNTOF(name), &index, mask, UCOUNTOF(mask), &slot, format2, UCOUNTOF(format2), format, UCOUNTOF(format));
			if (numRead == 6)
			{
				// finish type.
				if (SkipPacking(c + pos, mOutputRegisterType))
					sprintf(format2, "%s%d", format, 4);				// force to float4
				else
					if (strlen(mask) > 1)
						sprintf(format2, "%s%d", format, (int)strlen(mask));	// e.g. float2
					else
						strcpy(format2, format);
				// Already used?
				char registerName[32];
				sprintf(registerName, "o%d", slot);
				string regNameStr = registerName;
				map<string, DataType>::iterator i = mOutputRegisterType.find(regNameStr);
				if (i != mOutputRegisterType.end())
				{
					sprintf(registerName, "p%d.", slot);
					const char *INDEX_MASK = "xyzw";
					string newName = registerName;
					for (size_t j = 0; j < strlen(mask); ++j)
						newName.push_back(INDEX_MASK[j]);
					mRemappedOutputRegisters[regNameStr + "." + string(mask)] = newName;
					if (strlen(mask) > 1)
					{
						for (size_t j = 0; j < strlen(mask); ++j)
						{
							newName = registerName;
							newName.push_back(INDEX_MASK[j]);
							string oldName = regNameStr + ".";
							oldName.push_back(mask[j]);
							mRemappedOutputRegisters[oldName] = newName;
						}
					}
					sprintf(registerName, "p%d", slot);
					regNameStr = registerName;
				}
				// Write.
				char buffer[256];
				sprintf(buffer, "  out %s %s : %s%d,\n", format2, regNameStr.c_str(), name, index);
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
				if (!strcmp(name, "SV_Position"))
					mSV_Position = regNameStr;
				mOutputRegisterType[regNameStr] = TranslateType(format2);
			}
			else if (numRead == 3)
			{
				char reg[64];
				char sysValue[64];
				char buffer[256];
				// Name                 Index   Mask Register SysValue  Format   Used
				// -------------------- ----- ------ -------- -------- ------- ------
				// SV_Depth                 0    N/A   oDepth    DEPTH   float    YES
				numRead = sscanf_s(c + pos, "// %s %d %s %s %s %s",
					name, UCOUNTOF(name), &index, mask, UCOUNTOF(mask), reg, UCOUNTOF(reg), sysValue, UCOUNTOF(sysValue), format, UCOUNTOF(format));
				sprintf(buffer, "  out %s %s : %s,\n", format, reg, name);
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
			if (numRead != 6)
			{
				logDecompileError("Error parsing output signature: " + string(c + pos, 80));
				break;
			}
			NextLine(c, pos, size);
			// End?
			if (!strncmp(c + pos, "//\n", 3) || !strncmp(c + pos, "//\r", 3))
				break;
		}
		// Write footer.
		mOutput.pop_back();
		mOutput.pop_back();
		const char *mainFooter = ")\n{\n";
		mOutput.insert(mOutput.end(), mainFooter, mainFooter + strlen(mainFooter));
	}

	void WriteZeroOutputSignature(const char *c, size_t size)
	{
		// Read until header.
		const char *headerid = "// Output signature:";
		size_t pos = 0;
		while (pos < size - strlen(headerid))
		{
			if (!strncmp(c + pos, headerid, strlen(headerid)))
				break;
			else
			{
				NextLine(c, pos, size);
			}
		}
		if (pos >= size - strlen(headerid)) return;
		// Skip header.
		for (int i = 0; i < 4; ++i)
		{
			NextLine(c, pos, size);
		}
		// Read list.
		set<string> outputRegister;
		while (pos < size)
		{
			char name[256], mask[16], format[16], format2[16];
			int index, slot; format[0] = 0; mask[0] = 0;
			if (!strncmp(c + pos, "// no Output", strlen("// no Output")))
				break;
			int numRead = sscanf_s(c + pos, "// %s %d %s %d %s %s",
				name, UCOUNTOF(name), &index, mask, UCOUNTOF(mask), &slot, format2, UCOUNTOF(format2), format, UCOUNTOF(format));
			if (numRead == 6)
			{
				// Already used?
				char registerName[32];
				sprintf(registerName, "o%d", slot);
				string regNameStr = registerName;
				set<string>::iterator i = outputRegister.find(regNameStr);
				if (i != outputRegister.end())
				{
					sprintf(registerName, "p%d", slot);
					regNameStr = registerName;
				}
				else
				{
					outputRegister.insert(regNameStr);
				}
				// Write.
				char buffer[256];
				sprintf(buffer, "  %s = 0;\n", regNameStr.c_str());
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
			else if (numRead == 3)
			{
				char sysValue[64];
				int numRead = sscanf_s(c + pos, "// %s %d %s %s %s %s",
					name, UCOUNTOF(name), &index, mask, UCOUNTOF(mask), sysValue, UCOUNTOF(sysValue), format2, UCOUNTOF(format2), format, UCOUNTOF(format));
				// Write.
				char buffer[256];
				sprintf(buffer, "  %s = 0;\n", sysValue);
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
			if (numRead != 6)
			{
				logDecompileError("Error parsing output signature: " + string(c + pos, 80));
				break;
			}
			NextLine(c, pos, size);
			// End?
			if (!strncmp(c + pos, "//\n", 3) || !strncmp(c + pos, "//\r", 3))
				break;
		}
	}

	//dx9
	size_t getLineEnd(const char * c, size_t size, size_t & pos, bool & foundLineEnd)
	{
		size_t lineStart = pos;
		while (pos < size)
		{
			if (pos < size - 1)
			{
				if (c[pos] == 0x0d && c[pos + 1] == 0x0a)
				{
					// This code path doesn't trigger for me (DarkStarSword).
					// Does this mean that the newline style output from the
					// disassembler can vary? If so, dependent on what?
					foundLineEnd = true;
					pos += 2;
					return pos - lineStart - 2;
					break;
				}
				else if (c[pos] == 0x0a)
				{
					foundLineEnd = true;
					pos += 1;
					return pos - lineStart - 1;
					break;
				}
			}
			pos++;
		}

		return pos;
	}

	void SplitString(const std::string& s, std::vector<std::string>& v, const std::string& c)
	{
		std::string::size_type pos1, pos2;
		pos2 = s.find(c);
		pos1 = 0;
		while (std::string::npos != pos2)
		{
			if (pos2 > pos1) //remove c
			{
				v.push_back(s.substr(pos1, pos2 - pos1));
			}

			pos1 = pos2 + c.size();
			pos2 = s.find(c, pos1);
		}
		if (pos1 != s.length())
			v.push_back(s.substr(pos1));
	}

	void ReadResourceBindingsDX9(const char *c, size_t size)
	{
		mCBufferNames.clear();
		mSamplerNames.clear();
		mSamplerNamesArraySize.clear();
		mSamplerComparisonNames.clear();
		mSamplerComparisonNamesArraySize.clear();
		mTextureNames.clear();
		mTextureNamesArraySize.clear();

		size_t pos = 0;
		bool parseParameters = false;
		bool parseRegisters = false;

		while (pos < size)
		{

			const char * lineStart = c + pos;
			bool foundLineEnd = false;
			size_t lineSize = getLineEnd(c, size, pos, foundLineEnd);


			if (lineSize < 2)
			{
				break;
			}

			//code start
			if (lineStart[0] != '/' || lineStart[1] != '/')
			{
				break;
			}

			const char * headerid = "// Parameters:";
			if (!strncmp(lineStart, headerid, strlen(headerid)))
			{
				parseParameters = true;
				parseRegisters = false;
			}

			headerid = "// Registers:";
			if (!strncmp(lineStart, headerid, strlen(headerid)))
			{
				parseParameters = false;
				parseRegisters = true;
				continue;
			}


			if (parseRegisters)
			{
				char * lineStr = new char[lineSize + 1];
				memcpy(lineStr, lineStart, lineSize);
				lineStr[lineSize] = 0;

				vector<string> result;
				SplitString(lineStr, result, " ");

				if (result.size() != 4)
				{
					delete[] lineStr;
					continue;
				}

				if (result[1] == "Name" || result[3] == "----")
				{
					delete[] lineStr;
					continue;
				}

				if (result[2].c_str()[0] == 's')
				{
					int slot = atoi(&result[2].c_str()[1]);
					mTextureNames[slot] = result[1];
					mTextureNamesArraySize[slot] = 1;
					mTextureType[slot] = "Texture2D<float4>";
				}
				else if (result[2].c_str()[0] == 'c')
				{
					int index = atoi(&result[2].c_str()[1]);
					mUniformNames[index] = result[1];
				}
				if (result[2].c_str()[0] == 'b')
				{
					int index = atoi(&result[2].c_str()[1]);
					mBoolUniformNames[index] = result[2];
				}
			}
		}
	}
	//dx9

	void ReadResourceBindings(const char *c, size_t size)
	{
		mCBufferNames.clear();
		mSamplerNames.clear();
		mSamplerNamesArraySize.clear();
		mSamplerComparisonNames.clear();
		mSamplerComparisonNamesArraySize.clear();
		mTextureNames.clear();
		mTextureNamesArraySize.clear();
		mUAVNames.clear();
		mUAVNamesArraySize.clear();
		// Read until header.
		const char *headerid = "// Resource Bindings:";
		size_t pos = 0;
		while (pos < size - strlen(headerid))
		{
			if (!strncmp(c + pos, headerid, strlen(headerid)))
				break;
			else
			{
				NextLine(c, pos, size);
			}
		}
		if (pos >= size - strlen(headerid)) return;
		// Skip header.
		for (int i = 0; i < 4; ++i)
		{
			NextLine(c, pos, size);
		}
		// Read list.
		while (pos < size)
		{
			char name[256], type[16], format[16], dim[16], bind[16];
			int arraySize;
			type[0] = 0;
			int numRead = sscanf_s(c + pos, "// %s %s %s %s %s %d",
				name, UCOUNTOF(name), type, UCOUNTOF(type), format, UCOUNTOF(format), dim, UCOUNTOF(dim),
				bind, UCOUNTOF(bind), &arraySize);

			if (numRead != 6)
				logDecompileError("Error parsing resource declaration: " + string(c + pos, 80));

			int slot = 0;
			for (int i = 0; i < sizeof(bind) && bind[i]; ++i)
			{
				if (bind[i] >= '0' && bind[i] <= '9')
				{
					slot = atoi(&bind[i]);
					break;
				}
			}

			if (!strcmp(type, "sampler"))
			{
				char *escapePos = strchr(name, '['); if (escapePos) *escapePos = '_';
				escapePos = strchr(name, ']'); if (escapePos) *escapePos = '_';
				string baseName = string(name) + "_s";
				mSamplerNames[slot] = baseName;
				mSamplerNamesArraySize[slot] = arraySize;
				if (arraySize > 1)
					for (int i = 0; i < arraySize; ++i)
					{
					sprintf(name, "%s[%d]", baseName.c_str(), i);
					mSamplerNames[slot + i] = name;
					}
			}
			else if (!strcmp(type, "sampler_c"))
			{
				char *escapePos = strchr(name, '['); if (escapePos) *escapePos = '_';
				escapePos = strchr(name, ']'); if (escapePos) *escapePos = '_';
				string baseName = string(name) + "_s";
				mSamplerComparisonNames[slot] = baseName;
				mSamplerComparisonNamesArraySize[slot] = arraySize;
				if (arraySize > 1)
					for (int i = 0; i < arraySize; ++i)
					{
					sprintf(name, "%s[%d]", baseName.c_str(), i);
					mSamplerComparisonNames[slot + i] = name;
					}
			}
			else if (!strcmp(type, "texture") || !strcmp(type, "UAV"))
			{
				char *escapePos = strchr(name, '['); if (escapePos) *escapePos = '_';
				escapePos = strchr(name, ']'); if (escapePos) *escapePos = '_';
				string baseName = string(name);
				map<int, string> *mNames = &mTextureNames;
				map<int, int>    *mNamesArraySize = &mTextureNamesArraySize;
				map<int, string> *mType = &mTextureType;
				std::string rw;

				if (!strcmp(type, "UAV")) {
					mNames = &mUAVNames;
					mNamesArraySize = &mUAVNamesArraySize;
					mType = &mUAVType;
					rw = "RW";
				}

				(*mNames)[slot] = baseName;
				(*mNamesArraySize)[slot] = arraySize;
				if (arraySize > 1)
					for (int i = 0; i < arraySize; ++i)
					{
					sprintf(name, "%s[%d]", baseName.c_str(), i);
					(*mNames)[slot + i] = name;
					}
				if (!strcmp(dim, "1d"))
					(*mType)[slot] = rw + "Texture1D<" + string(format) + ">";
				else if (!strcmp(dim, "1darray"))
					(*mType)[slot] = rw + "Texture1DArray<" + string(format) + ">";
				else if(!strcmp(dim, "2d"))
					(*mType)[slot] = rw + "Texture2D<" + string(format) + ">";
				else if (!strcmp(dim, "2darray"))
					(*mType)[slot] = rw + "Texture2DArray<" + string(format) + ">";
				else if (!strcmp(dim, "3d"))
					(*mType)[slot] = rw + "Texture3D<" + string(format) + ">";
				else if (!strcmp(dim, "cube"))
					(*mType)[slot] = rw + "TextureCube<" + string(format) + ">";
				else if (!strcmp(dim, "cubearray"))
					(*mType)[slot] = rw + "TextureCubeArray<" + string(format) + ">";
				else if (!strncmp(dim, "2dMS", 4))
				{
					// The documentation says it's not legal, but we see Texture 2DMS with no ending size in WatchDogs. 
					// Instead of "2dMS4", it's just "2dMS". If we set that to Texture2DMS<float4>, with no size, 
					// it generates the same code as the ASM.	 sscanf returns -1 as an error.
					//
					// LogInfo("--> Texture %s: sprintf=%d as %s\n", dim, error, buffer);
					int msnumber;
					int scanned;
					char buffer[256];
					scanned = sscanf_s(dim + 4, "%d", &msnumber);
					if (scanned == 1)
						sprintf(buffer, "Texture2DMS<%s,%d>", format, msnumber);
					else
						sprintf(buffer, "Texture2DMS<%s>", format);
					(*mType)[slot] = rw + buffer;
				}
				// Two new ones for Mordor.
				else if (!strcmp(dim, "buf"))
					(*mType)[slot] = rw + "Buffer<" + string(format) + ">";
				else if (!strcmp(format, "struct"))
					(*mType)[slot] = rw + "StructuredBuffer<" + mStructuredBufferTypes[name] + ">";
				else if (!strcmp(format, "byte"))
					(*mType)[slot] = rw + "ByteAddressBuffer";
				else
					logDecompileError("Unknown " + string(type) + " dimension: " + string(dim));
			}
			else if (!strcmp(type, "cbuffer"))
				mCBufferNames[name] = slot;
			NextLine(c, pos, size);
			// End?
			if (!strncmp(c + pos, "//\n", 3) || !strncmp(c + pos, "//\r", 3))
				break;
		}
	}

	void WriteResourceDefinitions()
	{
		char buffer[256];
		_snprintf_s(buffer, 256, 256, "\n");
		mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));

		for (map<int, string>::iterator i = mSamplerNames.begin(); i != mSamplerNames.end(); ++i)
		{
			if (mSamplerNamesArraySize[i->first] == 1)
			{
				sprintf(buffer, "SamplerState %s : register(s%d);\n", i->second.c_str(), i->first);
				/*
				sprintf(buffer, "SamplerState %s : register(s%d)\n"
				"{\n"
				"  AddressU = wrap;\n"
				"  AddressV = wrap;\n"
				"  AddressW = clamp;\n"
				"  BorderColor = 1;\n"
				"  MinLOD = 0;\n"
				"  MaxLOD = 0;\n"
				"  MipLODBias = -100;\n"
				"};\n", i->second.c_str(), i->first+5);
				*/
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
			else if (mSamplerNamesArraySize[i->first] > 1)
			{
				string baseName = i->second.substr(0, i->second.find('['));
				sprintf(buffer, "SamplerState %s[%d] : register(s%d);\n", baseName.c_str(), mSamplerNamesArraySize[i->first], i->first);
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
		}
		for (map<int, string>::iterator i = mSamplerComparisonNames.begin(); i != mSamplerComparisonNames.end(); ++i)
		{
			if (mSamplerComparisonNamesArraySize[i->first] == 1)
			{
				sprintf(buffer, "SamplerComparisonState %s : register(s%d);\n", i->second.c_str(), i->first);
				/*
				sprintf(buffer, "SamplerComparisonState %s : register(s%d)\n"
				"{\n"
				"  AddressU = wrap;\n"
				"  AddressV = wrap;\n"
				"  AddressW = clamp;\n"
				"  BorderColor = 1;\n"
				"  MinLOD = 0;\n"
				"  MaxLOD = 0;\n"
				"  MipLODBias = -100;\n"
				"};\n", i->second.c_str(), i->first+5);
				*/
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
			else if (mSamplerComparisonNamesArraySize[i->first] > 1)
			{
				string baseName = i->second.substr(0, i->second.find('['));
				sprintf(buffer, "SamplerComparisonState %s[%d] : register(s%d);\n", baseName.c_str(), mSamplerComparisonNamesArraySize[i->first], i->first);
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
		}
		for (map<int, string>::iterator i = mTextureNames.begin(); i != mTextureNames.end(); ++i)
		{
			if (mTextureNamesArraySize[i->first] == 1)
			{
				sprintf(buffer, "%s %s : register(t%d);\n", mTextureType[i->first].c_str(), i->second.c_str(), i->first);
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
			else if (mTextureNamesArraySize[i->first] > 1)
			{
				string baseName = i->second.substr(0, i->second.find('['));
				sprintf(buffer, "%s %s[%d] : register(t%d);\n", mTextureType[i->first].c_str(), baseName.c_str(), mTextureNamesArraySize[i->first], i->first);
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
		}
		for (map<int, string>::iterator i = mUAVNames.begin(); i != mUAVNames.end(); ++i)
		{
			if (mUAVNamesArraySize[i->first] == 1)
			{
				sprintf(buffer, "%s %s : register(u%d);\n", mUAVType[i->first].c_str(), i->second.c_str(), i->first);
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
			else if (mUAVNamesArraySize[i->first] > 1)
			{
				string baseName = i->second.substr(0, i->second.find('['));
				sprintf(buffer, "%s %s[%d] : register(u%d);\n", mUAVType[i->first].c_str(), baseName.c_str(), mUAVNamesArraySize[i->first], i->first);
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
		}
	}

	int getDataTypeSize(DataType d)
	{
		switch (d)
		{
			case DT_bool:
			case DT_uint:
			case DT_int:
			case DT_float:
				return 4;
			case DT_float2:
			case DT_uint2:
			case DT_int2:
				return 8;
			case DT_float3:
			case DT_uint3:
			case DT_int3:
				return 12;
			case DT_bool4:
			case DT_float4:
			case DT_uint4:
			case DT_int4:
				return 16;
			case DT_float2x4:
			case DT_float4x2:
				return 32;
			case DT_float3x3:
				return 36;
			case DT_float3x4:
				return 48;
			case DT_float4x3:
				return 48;
			case DT_float4x4:
				return 64;
		}
		logDecompileError("Unknown data type in getDataTypeSize");
		return 0;
	}

	void ParseBufferDefinitions(Shader *shader, const char *c, size_t size)
	{
		mUsesProjection = false;
		mCBufferData.clear();
		// Immediate buffer.
		BufferEntry immediateEntry;
		immediateEntry.Name = "icb[0]";
		immediateEntry.matrixRow = 0;
		immediateEntry.isRowMajor = false;
		immediateEntry.bt = DT_float4;
		mCBufferData[-1 << 16] = immediateEntry;
		vector<int> pendingStructAttributes[8];
		int structLevel = -1;
		// Search for buffer.
		const char *headerid = "// cbuffer ";
		size_t pos = 0;
		while (pos < size - strlen(headerid))
		{
			// Read next buffer.
			while (pos < size - strlen(headerid))
			{
				if (!strncmp(c + pos, headerid, strlen(headerid)))
					break;
				else
				{
					NextLine(c, pos, size);
				}
			}
			if (pos >= size - strlen(headerid)) return;
			char name[256];
			int numRead = sscanf_s(c + pos, "// cbuffer %s", name, UCOUNTOF(name));
			if (numRead != 1)
			{
				logDecompileError("Error parsing buffer name: " + string(c + pos, 80));
				return;
			}
			NextLine(c, pos, size);
			NextLine(c, pos, size);
			// Map buffer name to register.
			map<string, int>::iterator i = mCBufferNames.find(name);
			if (i == mCBufferNames.end())
			{
				logDecompileError("Buffer not found in resource declaration: " + string(name));
				return;
			}
			const int bufferRegister = i->second;
			// Write declaration.
			char buffer[256];
			if (name[0] == '$') name[0] = '_';
			_snprintf_s(buffer, 256, 256, "\ncbuffer %s : register(b%d)\n{\n", name, bufferRegister);
			mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			do
			{
				const char *eolPos = strchr(c + pos, '\n');
				memcpy(buffer, c + pos, eolPos - c - pos + 1);
				buffer[eolPos - c - pos + 1] = 0;
				// Skip opening bracket.
				if (strstr(buffer, " {\n"))
				{
					NextLine(c, pos, size);
					continue;
				}
				// Ignore empty line.
				if (buffer[0] == '/' && buffer[1] == '/')
				{
					int ePos = 2;
					while (buffer[ePos] != 0 && (buffer[ePos] == ' ' || buffer[ePos] == '\t' || buffer[ePos] == '\n' || buffer[ePos] == '\r')) ++ePos;
					if (!buffer[ePos])
					{
						NextLine(c, pos, size);
						continue;
					}
				}
				// Struct definition?
				//if (strstr(buffer, " struct\n") || strstr(buffer, " struct "))
				if (strstr(buffer, " struct\n") || strstr(buffer, " struct ") || strstr(buffer, "//   struct\r\n")) //dx9
				{
					++structLevel;
					mOutput.insert(mOutput.end(), '\n');
					for (int i = -1; i < structLevel; ++i)
					{
						mOutput.insert(mOutput.end(), ' '); mOutput.insert(mOutput.end(), ' ');
					}
					const char *structHeader = strstr(buffer, "struct");
					// Can't use structure declaration: If we use the structure name, it has to be copied on top.
					//if (structLevel)
					structHeader = "struct\n";
					mOutput.insert(mOutput.end(), structHeader, structHeader + strlen(structHeader));
					for (int i = -1; i < structLevel; ++i)
					{
						mOutput.push_back(' '); mOutput.push_back(' ');
					}
					const char *structHeader2 = "{\n";
					mOutput.insert(mOutput.end(), structHeader2, structHeader2 + strlen(structHeader2));
					//skip struct's next line"//   {\r\n" //dx9 (Was there a point to this commented out line? -DSS)
					NextLine(c, pos, size);
					continue;
				}
				if (strstr(buffer, " }"))
				{
					// Read struct name.
					while (c[pos] != '}') ++pos;
					int bpos = 0;
					while (c[pos] != ';')
						buffer[bpos++] = c[pos++];
					string structName = string(buffer + 2, buffer + bpos) + ".";
					// Read offset.
					while (c[pos] != '/' && pos < size) pos++;
					int offset = 0;
					numRead = sscanf_s(c + pos, "// Offset: %d", &offset);
					if (numRead != 1)
					{
						logDecompileError("6 Error parsing buffer offset: " + string(c + pos, 80));
						return;
					}
					if (!structLevel)
						sprintf_s(buffer + bpos, sizeof(buffer) - bpos, " : packoffset(c%d);\n\n", offset / 16);
					else
						sprintf_s(buffer + bpos, sizeof(buffer) - bpos, ";\n\n");
					for (int i = -1; i < structLevel; ++i)
					{
						mOutput.push_back(' '); mOutput.push_back(' ');
					}
					mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
					// Prefix struct attributes.
					if (structLevel < 0) {
						logDecompileError("structLevel is negative - malformed shader?\n");
						return;
					}
					size_t arrayPos = structName.find('[');
					if (arrayPos == string::npos)
					{
						for (vector<int>::iterator i = pendingStructAttributes[structLevel].begin(); i != pendingStructAttributes[structLevel].end(); ++i)
						{
							mCBufferData[*i].Name = structName + mCBufferData[*i].Name;
							if (structLevel)
								pendingStructAttributes[structLevel - 1].push_back(*i);
						}
					}
					else
					{
						int arraySize;
						if (sscanf_s(structName.c_str() + arrayPos + 1, "%d", &arraySize) != 1)
						{
							logDecompileError("Error parsing struct array size: " + structName);
							return;
						}
						structName = structName.substr(0, arrayPos);
						// Calculate struct size.
						int structSize = 0;
						for (vector<int>::iterator j = pendingStructAttributes[structLevel].begin(); j != pendingStructAttributes[structLevel].end(); ++j)
							structSize += getDataTypeSize(mCBufferData[*j].bt);
						for (int i = arraySize - 1; i >= 0; --i)
						{
							sprintf(buffer, "%s[%d].", structName.c_str(), i);
							for (vector<int>::iterator j = pendingStructAttributes[structLevel].begin(); j != pendingStructAttributes[structLevel].end(); ++j)
							{
								mCBufferData[*j + i * structSize].Name = buffer + mCBufferData[*j].Name;
								if (structLevel)
									pendingStructAttributes[structLevel - 1].push_back(*j + i * structSize);
							}
						}
					}
					pendingStructAttributes[structLevel].clear();
					NextLine(c, pos, size);
					--structLevel;
					continue;
				}
				// Read declaration.
				// With very long names, the disassembled code can have no space at the end, like:
				//   float2 __0RealtimeReflMul__1EnvCubeReflMul__2__3;// Offset:   96 Size:     8
				// This caused the %s %s to fail, so now looking specifically for required semicolon.
				char type[16]; type[0] = 0;
				numRead = sscanf_s(c + pos, "// %s %[^;]", type, UCOUNTOF(type), name, UCOUNTOF(name));
				if (numRead != 2)
				{
					logDecompileError("Error parsing buffer item: " + string(c + pos, 80));
					return;
				}
				string modifier;
				BufferEntry e;
				e.isRowMajor = false;
				if (!strcmp(type, "row_major") || !strcmp(type, "column_major"))
				{
					e.isRowMajor = !strcmp(type, "row_major");
					modifier = type;
					modifier.push_back(' ');
					numRead = sscanf_s(c + pos, "// %s %s %[^;]", buffer, UCOUNTOF(buffer), type, UCOUNTOF(type), name, UCOUNTOF(name));
					if (numRead != 3)
					{
						logDecompileError("Error parsing buffer item: " + string(c + pos, 80));
						return;
					}
				}
				pos += 2;
				while (c[pos] != '/' && pos < size) pos++;
				int offset = 0;
				numRead = sscanf_s(c + pos, "// Offset: %d", &offset);
				if (numRead != 1)
				{
					logDecompileError("7 Error parsing buffer offset: " + string(c + pos, 80));
					return;
				}
				e.Name = name;
				e.matrixRow = 0;
				e.bt = TranslateType(type);

				// Check binary type.
				/*
				for (int nBuf = 0; nBuf < shader->sInfo.ui32NumConstantBuffers; ++nBuf)
				{
				if (shader->sInfo.psConstantBuffers[nBuf].Name.compare(i->first)) continue;
				ConstantBuffer &cBuf = shader->sInfo.psConstantBuffers[nBuf];
				for (vector<ShaderVar>::iterator svar = cBuf.asVars.begin(); svar != cBuf.asVars.end(); ++svar)
				{
				if (svar->Name.compare(name)) continue;
				// Check type.
				DataType binaryType = TranslateTypeBin(*svar, type);
				}
				}
				*/

				// Uses projection matrix?
				if (e.bt == DT_float4x4 && e.Name.find_first_of("Projection") >= 0)
				{
					eolPos = strchr(c + pos, '\n');
					// Used?
					if (*(eolPos - 1) != ']')
						mUsesProjection = true;
				}

				// SR3 variant of projection matrix.
				if (e.bt == DT_float4x4 && e.Name.find_first_of("projTM") >= 0)
				{
					eolPos = strchr(c + pos, '\n');
					// Used?
					if (*(eolPos - 1) != ']')
						mUsesProjection = true;
				}

				// This section previously made a distinction for the floatYxZ formats, assuming that they were not able to
				// be arrays.  That's untrue, so they were added into the for loop to create the array elements possible.
				// This fixes a series of array out of bounds errors for anything using these floatYxZ matrices.

				// Look for array based elements.  We need an mCBufferData element for each possible array element, to match
				// possible uses of those offsets in the ASM code.
				size_t ep = e.Name.find('[');
				if (ep != string::npos &&
					(e.bt == DT_bool || e.bt == DT_bool4 ||
					e.bt == DT_float || e.bt == DT_float2 || e.bt == DT_float3 || e.bt == DT_float4 ||
					e.bt == DT_uint || e.bt == DT_uint2 || e.bt == DT_uint3 || e.bt == DT_uint4 ||
					e.bt == DT_int || e.bt == DT_int2 || e.bt == DT_int3 || e.bt == DT_int4 ||
					e.bt == DT_float4x4 || e.bt == DT_float3x4 || e.bt == DT_float4x3 || e.bt == DT_float3x3 || 
					e.bt == DT_float4x2 || e.bt == DT_float2x4))
				{
					// Register each array element.
					int numElements = 0;
					sscanf_s(e.Name.substr(ep + 1).c_str(), "%d", &numElements);
					string baseName = e.Name.substr(0, ep);

					int counter = 0;
					if (e.bt == DT_float || e.bt == DT_bool || e.bt == DT_uint || e.bt == DT_int) counter = 4;
					else if (e.bt == DT_float2 || e.bt == DT_uint2 || e.bt == DT_int2) counter = 8;
					else if (e.bt == DT_float3 || e.bt == DT_uint3 || e.bt == DT_int3) counter = 12;
					else if (e.bt == DT_bool4 || e.bt == DT_float4 || e.bt == DT_uint4 || e.bt == DT_int4) counter = 16;
					else if (e.bt == DT_float4x2 || e.bt == DT_float2x4) counter = 16 * 2;
					else if (e.bt == DT_float3x3 || e.bt == DT_float4x3) counter = 16 * 3;
					else if (e.bt == DT_float3x4 || e.bt == DT_float4x4) counter = 16 * 4;

					// Correct possible invalid array size. (16 byte boundaries)
					int byteSize;
					sscanf_s(strstr(c + pos, "Size:") + 5, "%d", &byteSize);
					if ((counter == 4 && numElements*counter < byteSize) ||
						(counter == 12 && numElements*counter < byteSize))
						counter = 16;

					for (int i = 0; i < numElements; ++i)
					{
						sprintf(buffer, "%s[%d]", baseName.c_str(), i);
						e.Name = buffer;
						int offsetPos;

						switch (e.bt)
						{
							case DT_float4x4:
							case DT_float3x4:
							case DT_float2x4:  // Not positive this is right
								e.matrixRow = 3;
								offsetPos = (bufferRegister << 16) + offset + i*counter + 3 * 16;
								mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);
							case DT_float4x3:
							case DT_float3x3:
								e.matrixRow = 2;
								offsetPos = (bufferRegister << 16) + offset + i*counter + 2 * 16;
								mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);
							case DT_float4x2:
								e.matrixRow = 1;
								offsetPos = (bufferRegister << 16) + offset + i*counter + 1 * 16;
								mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);
								e.matrixRow = 0;
								offsetPos = (bufferRegister << 16) + offset + i*counter;
								mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);

								break;
							default:
								offsetPos = (bufferRegister << 16) + offset + i*counter;
								mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);
						}
					}
				}
				// Non-array versions of floatYxZ
				else if (e.bt == DT_float4x2 || e.bt == DT_float3x3 || e.bt == DT_float4x3 ||
					e.bt == DT_float2x4 || e.bt == DT_float3x4 || e.bt == DT_float4x4)
				{
					e.matrixRow = 0; int offsetPos = (bufferRegister << 16) + offset; 
					mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);
					e.matrixRow = 1; offsetPos = (bufferRegister << 16) + offset + 1 * 16; 
					mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);
					if (e.bt != DT_float4x2)
					{
						e.matrixRow = 2; offsetPos = (bufferRegister << 16) + offset + 2 * 16; 
						mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);
						if (e.bt != DT_float4x3 && e.bt != DT_float3x3)
						{  // Nearly sure this missing nesting was a bug
							e.matrixRow = 3; offsetPos = (bufferRegister << 16) + offset + 3 * 16;  
							mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);
						}
					}
				}
				// Non-array versions of scalar entities like float4
				else
				{
					int offsetPos = (bufferRegister << 16) + offset;
					mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);
				}

				// Default value?
				NextLine(c, pos, size);
				const char *defaultid = "//      = ";
				int packoffset = offset / 16; int suboffset = (offset % 16) / 4;
				const char INDEX_MASK[] = "xyzw";
				string structSpacing;
				for (int i = -1; i < structLevel; ++i) structSpacing += "  ";
				if (!strncmp(c + pos, defaultid, strlen(defaultid)))
				{
					// No idea what an assignment to a bool4 would look like in ASM, so let's just log if we see
					// this, and fix it later.
					if (e.bt == DT_bool4)
						logDecompileError("*** assignment to bool4, unknown syntax\n");
					// For bool values, the usual conversion by %e creates QNAN, so handle them specifically. (e.g. = 0xffffffff)
					else if (e.bt == DT_bool)
					{
						unsigned int bHex = 0;
						numRead = sscanf_s(c + pos, "// = 0x%lx", &bHex);
						NextLine(c, pos, size);
						string bString = (bHex == 0) ? "false" : "true";
						if (structLevel < 0)
						{
							if (suboffset == 0)
								sprintf(buffer, "  %s%s %s : packoffset(c%d) = %s;\n", modifier.c_str(), type, name, packoffset, bString.c_str());
							else
								sprintf(buffer, "  %s%s %s : packoffset(c%d.%c) = %s;\n", modifier.c_str(), type, name, packoffset, INDEX_MASK[suboffset], bString.c_str());
						}
						else
							sprintf(buffer, "  %s%s%s %s = %s;\n", structSpacing.c_str(), modifier.c_str(), type, name, bString.c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
					}
					// Special int case, to avoid converting to float badly, creating #QNAN instead. 
					else if (e.bt == DT_int || e.bt == DT_int2 || e.bt == DT_int3 || e.bt == DT_int4)
					{
						int in[4] = { 0, 0, 0, 0 };

						numRead = sscanf_s(c + pos, "// = %i %i %i %i", in + 0, in + 1, in + 2, in + 3);
						NextLine(c, pos, size);

						if (structLevel < 0)
						{
							if (suboffset == 0)
								sprintf(buffer, "  %s%s %s : packoffset(c%d) = {", modifier.c_str(), type, name, packoffset);
							else
								sprintf(buffer, "  %s%s %s : packoffset(c%d.%c) = {", modifier.c_str(), type, name, packoffset, INDEX_MASK[suboffset]);
						}
						else
							sprintf(buffer, "  %s%s%s %s = {", structSpacing.c_str(), modifier.c_str(), type, name);
						
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));

						for (int i = 0; i < numRead - 1; ++i)
						{
							sprintf(buffer, "%i,", in[i]);
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						}
						sprintf(buffer, "%i};\n", in[numRead - 1]);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
					}
					else if (e.bt == DT_float || e.bt == DT_float2 || e.bt == DT_float3 || e.bt == DT_float4)
					{
						float v[4] = { 0, 0, 0, 0 };
						numRead = sscanf_s(c + pos, "// = 0x%lx 0x%lx 0x%lx 0x%lx", (unsigned long*)v + 0, (unsigned long*)v + 1, (unsigned long*)v + 2, (unsigned long*)v + 3);
						NextLine(c, pos, size);

						if (structLevel < 0)
						{
							if (suboffset == 0)
								sprintf(buffer, "  %s%s %s : packoffset(c%d) = {", modifier.c_str(), type, name, packoffset);
							else
								sprintf(buffer, "  %s%s %s : packoffset(c%d.%c) = {", modifier.c_str(), type, name, packoffset, INDEX_MASK[suboffset]);
						}
						else
							sprintf(buffer, "  %s%s%s %s = {", structSpacing.c_str(), modifier.c_str(), type, name);

						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));

						for (int i = 0; i < numRead - 1; ++i)
						{
							sprintf(buffer, "%.9g,", v[i]);
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						}
						sprintf(buffer, "%.9g};\n", v[numRead - 1]);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
					}
					// Only 4x4 for now, not sure this is all working, so going with known needed case.
					else if (e.bt == DT_float4x4)
					{
						float v[16];
						for (size_t i = 0; i < 4; i++)
						{
							numRead = sscanf_s(c + pos, "//%*[ =]0x%lx 0x%lx 0x%lx 0x%lx", (unsigned long*)&v[i * 4 + 0], (unsigned long*)&v[i * 4 + 1], (unsigned long*)&v[i * 4 + 2], (unsigned long*)&v[i * 4 + 3]);
							if (numRead != 4)
							{
								logDecompileError("Default values for float4x4 not read correctly, n:" + numRead);
								break;
							}
							NextLine(c, pos, size);
						}

						if (structLevel < 0)
						{
							if (suboffset == 0)
								sprintf(buffer, "  %s%s %s : packoffset(c%d) = {", modifier.c_str(), type, name, packoffset);
							else
								sprintf(buffer, "  %s%s %s : packoffset(c%d.%c) = {", modifier.c_str(), type, name, packoffset, INDEX_MASK[suboffset]);
						}
						else
							sprintf(buffer, "  %s%s%s %s = {", structSpacing.c_str(), modifier.c_str(), type, name);

						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));

						for (int i = 0; i < 16 - 1; ++i)
						{
							sprintf(buffer, "%.9g,", v[i]);
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						}
						sprintf(buffer, "%.9g};\n", v[15]);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
					}
					// If we don't know what the initializer is, let's not just keep reading through it.  Let's now scan 
					// them and output them, with a bad line in between for hand-fixing.  But, the shader will be generated.
					else
					{
						sprintf(buffer, "  %s%s %s : packoffset(c%d) = {", modifier.c_str(), type, name, packoffset);
						appendOutput(buffer);
						sprintf(buffer, "Unknown bad code for initializer (needs manual fix):\n");
						appendOutput(buffer);

						ASMLineOut(c, pos, size);
						NextLine(c, pos, size);

						// Loop through any remaining constant declarations to output
						const char *con = "//        0x";
						while (!strncmp(c + pos, con, strlen(con)))
						{
							ASMLineOut(c, pos, size);
							NextLine(c, pos, size);
						}
					}
				}
				// No default value.
				else
				{
					if (structLevel < 0)
					{
						if (suboffset == 0)
							sprintf(buffer, "  %s%s %s : packoffset(c%d);\n", modifier.c_str(), type, name, packoffset);
						else
							sprintf(buffer, "  %s%s %s : packoffset(c%d.%c);\n", modifier.c_str(), type, name, packoffset, INDEX_MASK[suboffset]);
					}
					else
						sprintf(buffer, "  %s%s%s %s;\n", structSpacing.c_str(), modifier.c_str(), type, name);
					mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
				}
			} while (strncmp(c + pos, "// }", 4));
			
			// Write closing declaration.
			const char *endBuffer = "}\n";
			mOutput.insert(mOutput.end(), endBuffer, endBuffer + strlen(endBuffer));
		}
	}

	// TODO: Convert other parsers to use this helper
	static size_t find_next_header(const char *headerid, const char *c, size_t pos, size_t size)
	{
		size_t header_len = strlen(headerid);

		while (pos < size - header_len) {
			if (!strncmp(c + pos, headerid, header_len))
				return pos;
			else
				NextLine(c, pos, size);
		}

		return 0;
	}

	bool warn_if_line_is_not(const char *expect, const char *c)
	{
		if (strncmp(c, expect, strlen(expect))) {
			logDecompileError("WARNING: Unexpected string in shader"
					"\n  Expected: " + string(expect) +
					"\n     Found: " + string(c, 80));
			return true;
		}
		return false;
	}

	void ParseStructureDefinitions(Shader *shader, const char *c, size_t size)
	{
		// Pulls out struct type declaration for structured buffers.
		// These will be referenced later when parsing the resource
		// bindings and any structured load/write instructions.
		//
		// - The struct definition in the assembly comment can be
		//   directly added to HLSL with only minimal changes:
		//   - We strip the $Element syntax from the end
		//   - We need to add a struct type name for shader model 4
		//     (SM5 already includes the type name we will use)
		//   - We need to strip type names from any embedded structs
		//
		// - We will need to note down which member is at each offset
		//   for use in later load instructions.
		//
		// TestShaders\resource_types* include test cases for these.

		size_t pos = 0;
		size_t spos, fpos;
		char bind_name[256];
		char type_name_buf[256];
		string type_name;
		int n;
		string hlsl;

		while (pos = find_next_header("// Resource bind info for ", c, pos, size)) {
			n = sscanf_s(c + pos, "// Resource bind info for %s", bind_name, UCOUNTOF(bind_name));
			if (n != 1) {
				logDecompileError("Error parsing structure bind name: " + string(c + pos, 80));
				continue;
			}
			NextLine(c, pos, size);

			warn_if_line_is_not("// {\n", c + pos);
			NextLine(c, pos, size);
			warn_if_line_is_not("//\n", c + pos);
			NextLine(c, pos, size);

			if (!strncmp(c + pos, "//   struct ", 12)) {
				// Shader model 5 has a type name after the
				// struct. We can't do this scanf without first
				// checking for this case since " " also
				// matches "\n" in scanf:
				n = sscanf_s(c + pos + 12, "%s", type_name_buf, UCOUNTOF(type_name_buf));
				if (n != 1) {
					logDecompileError("Error parsing structure type name: " + string(c + pos, 80));
					continue;
				}
				type_name = type_name_buf;
			} else if (!strncmp(c + pos, "//   struct\n", 12)) {
				// Shader model 4 lacks a type name after the
				// struct, so we have to invent one.
				type_name = bind_name + string("_type");
			} else {
				// Primitive type, not a structure
				n = 0;
				sscanf_s(c + pos, "//   %s $Element;%n", type_name_buf, UCOUNTOF(type_name_buf), &n);
				if (!n) {
					logDecompileError("Error parsing primitive structure type: " + string(c + pos, 80));
					continue;
				}
				mStructuredBufferTypes[bind_name] = type_name_buf;
				continue;
			}
			mStructuredBufferTypes[bind_name] = type_name;
			if (!mStructuredBufferUsedNames.insert(type_name).second) {
				// The same type name has been used previously.
				// Assuming the contents is going to be the same
				// and skipping redefining it.
				continue;
			}
			NextLine(c, pos, size);

			warn_if_line_is_not("//   {\n", c + pos);
			NextLine(c, pos, size);
			warn_if_line_is_not("//       \n", c + pos);
			NextLine(c, pos, size);

			hlsl = "\nstruct " + string(type_name) + "\n{\n";

			while (true) {
				// Strip comments:
				if (warn_if_line_is_not("//", c + pos))
					break;
				spos = pos + 2;

				// Strip first level of indentation if present:
				if (!strncmp(c + spos, "   ", 3))
					spos += 3;

				// Check if done signified by "} $Element;",
				// but for safety only checking "}"
				if (!strncmp(c + spos, "}", 1))
					break;

				// Find first non-blank character (without stripping):
				fpos = spos + strspn(&c[spos], " ");

				// Strip type names from inline embedded structs.
				// If these were declared separately in the
				// original HLSL they will have a valid type
				// name here, but if they were declared inline
				// they will have a placeholder name of
				// "parent_type::<unnamed>" instead. Either way
				// the HLSL we are generating will have these
				// inlined where specifying a type name is
				// illegal, so we have to strip it. We also
				// clean up the whitespace around these while
				// we're at it.
				if (!strncmp(&c[fpos], "struct ", 7) || !strncmp(&c[fpos], "struct\n", 7)) {
					hlsl += string(c, spos, fpos - spos) + "struct {\n";
					NextLine(c, pos, size); // struct typename
					warn_if_line_is_not("{\n", c + pos + strspn(c + pos, "/ "));
					NextLine(c, pos, size); // {
					warn_if_line_is_not("\n", c + pos + strspn(c + pos, "/ "));
					NextLine(c, pos, size); // blank
				} else {
					// Add the stripped line to the HLSL output, unless blank:
					NextLine(c, pos, size);
					if (c[fpos] != '\n') {
						string field(c, spos, pos - spos);
						const char *layout = strstr(field.c_str(), "Offset:");
						int field_offset = 0;
						int field_size = 0;
						if (layout && sscanf_s(layout, "Offset: %d Size: %d", &field_offset, &field_size) == 2)
							mStructuredBufferFieldRanges[type_name].push_back(make_pair(field_offset, field_size));
						hlsl += field;
					}
				}
			}
			hlsl += "};\n";
			mStructuredBufferDeclarations[type_name] = hlsl;
		}
	}

	void WriteStructureDefinitions()
	{
		for (map<string, string>::iterator declaration = mStructuredBufferDeclarations.begin();
			declaration != mStructuredBufferDeclarations.end(); ++declaration)
		{
			string hlsl = declaration->second;
			map<string, vector<pair<int, int> > >::iterator ranges = mStructuredBufferFieldRanges.find(declaration->first);
			set<int> *integer_offsets = NULL;
			map<string, set<int> >::iterator integer_type = mStructuredBufferIntegerOffsets.find(declaration->first);
			if (integer_type != mStructuredBufferIntegerOffsets.end())
				integer_offsets = &integer_type->second;

			if (ranges != mStructuredBufferFieldRanges.end() && integer_offsets && !integer_offsets->empty())
			{
				string rewritten;
				size_t line_start = 0;
				size_t range_index = 0;
				while (line_start < hlsl.size())
				{
					size_t line_end = hlsl.find('\n', line_start);
					if (line_end == string::npos)
						line_end = hlsl.size();
					string line = hlsl.substr(line_start, line_end - line_start);
					bool integer_field = false;
					if (line.find("Offset:") != string::npos && range_index < ranges->second.size())
					{
						int field_offset = ranges->second[range_index].first;
						int field_size = ranges->second[range_index].second;
						for (set<int>::iterator offset = integer_offsets->begin(); offset != integer_offsets->end(); ++offset)
						{
							if (*offset >= field_offset && *offset < field_offset + field_size)
							{
								integer_field = true;
								break;
							}
						}
						++range_index;
					}

					if (integer_field)
					{
						size_t type = line.find("float");
						if (type != string::npos)
							line.replace(type, 5, "uint");
					}
					rewritten += line;
					if (line_end < hlsl.size())
						rewritten += '\n';
					line_start = line_end < hlsl.size() ? line_end + 1 : hlsl.size();
				}
				hlsl.swap(rewritten);
			}
			mOutput.insert(mOutput.end(), hlsl.begin(), hlsl.end());
		}
	}

	void InferStructuredBufferIntegerTypes(Shader *shader)
	{
		struct StructuredOrigin
		{
			string type_name;
			int byte_offset;
		};

		map<pair<uint32_t, int>, StructuredOrigin> origins;

		for (uint32_t phase = 0; phase < NUM_PHASES; ++phase)
		{
			for (size_t instance = 0; instance < shader->asPhase[phase].ppsInst.size(); ++instance)
			{
				vector<Instruction> &instructions = shader->asPhase[phase].ppsInst[instance];
				for (size_t instruction_index = 0; instruction_index < instructions.size(); ++instruction_index)
				{
					Instruction &instruction = instructions[instruction_index];

					auto source_component = [](const Operand &operand, int lane) -> int {
						if (operand.eSelMode == OPERAND_4_COMPONENT_SELECT_1_MODE)
							return (int)operand.aui32Swizzle[0];
						if (operand.eSelMode == OPERAND_4_COMPONENT_SWIZZLE_MODE)
							return (int)operand.aui32Swizzle[lane];
						return lane;
					};

					auto source_origin = [&](const Operand &operand, int lane, StructuredOrigin &origin) -> bool {
						if (operand.eType != OPERAND_TYPE_TEMP)
							return false;
						int component = source_component(operand, lane);
						if (component < 0 || component > 3)
							return false;
						map<pair<uint32_t, int>, StructuredOrigin>::iterator found =
							origins.find(make_pair(operand.ui32RegisterNumber, component));
						if (found == origins.end())
							return false;
						origin = found->second;
						return true;
					};

					auto mark_integer_use = [&](const Operand &operand, int lane) {
						StructuredOrigin origin;
						if (source_origin(operand, lane, origin))
							mStructuredBufferIntegerOffsets[origin.type_name].insert(origin.byte_offset);
					};

					auto clear_destination = [&](const Operand &operand) {
						if (operand.eType != OPERAND_TYPE_TEMP)
							return;
						for (int lane = 0; lane < 4; ++lane)
						{
							if (operand.ui32CompMask & (1u << lane))
								origins.erase(make_pair(operand.ui32RegisterNumber, lane));
						}
					};

					// These instructions consume a raw integer value.  This is the
					// reverse edge that lets a later use refine the type of an earlier
					// structured load.
					switch (instruction.eOpcode)
					{
					case OPCODE_FIRSTBIT_HI:
					case OPCODE_FIRSTBIT_LO:
					case OPCODE_FIRSTBIT_SHI:
					case OPCODE_COUNTBITS:
					case OPCODE_NOT:
						mark_integer_use(instruction.asOperands[1], 0);
						break;
					case OPCODE_AND:
					case OPCODE_OR:
					case OPCODE_XOR:
			case OPCODE_ISHL:
			case OPCODE_ISHR:
			case OPCODE_USHR:
			case OPCODE_IADD:
			case OPCODE_IMAD:
			case OPCODE_IMUL:
			case OPCODE_INEG:
			case OPCODE_IEQ:
					case OPCODE_IGE:
					case OPCODE_ILT:
					case OPCODE_INE:
					case OPCODE_ULT:
					case OPCODE_UGE:
					case OPCODE_IMIN:
					case OPCODE_IMAX:
					case OPCODE_UMIN:
			case OPCODE_UMAX:
			case OPCODE_UMUL:
			case OPCODE_UMAD:
			case OPCODE_UDIV:
			case OPCODE_UBFE:
			case OPCODE_IBFE:
			case OPCODE_BFI:
			case OPCODE_BFREV:
						for (uint32_t operand_index = 1; operand_index < instruction.ui32NumOperands; ++operand_index)
							for (int lane = 0; lane < 4; ++lane)
								if (instruction.asOperands[operand_index].ui32CompMask & (1u << lane))
									mark_integer_use(instruction.asOperands[operand_index], lane);
						break;
					case OPCODE_LD_STRUCTURED:
						// The index operand is an integer sink as well (t6[t8[x]]).
						// Index operands are commonly select-one operands. Those do
						// not reliably expose a component mask, so checking the mask
						// here loses the t8[probeBit] -> probeIndex type edge.
						// source_component() resolves select-one/swizzled operands.
						for (int lane = 0; lane < 4; ++lane)
							mark_integer_use(instruction.asOperands[1], lane);
						break;
					default:
						break;
					}

					if (instruction.eOpcode == OPCODE_MOV && instruction.ui32NumOperands >= 2 &&
						instruction.asOperands[0].eType == OPERAND_TYPE_TEMP)
					{
						map<pair<uint32_t, int>, StructuredOrigin> moved;
						for (int lane = 0; lane < 4; ++lane)
						{
							if (!(instruction.asOperands[0].ui32CompMask & (1u << lane)))
								continue;
							StructuredOrigin origin;
							if (source_origin(instruction.asOperands[1], lane, origin))
								moved[make_pair(instruction.asOperands[0].ui32RegisterNumber, lane)] = origin;
						}
						clear_destination(instruction.asOperands[0]);
						origins.insert(moved.begin(), moved.end());
						continue;
					}

					if (instruction.eOpcode == OPCODE_LD_STRUCTURED && instruction.ui32NumOperands >= 4)
					{
						clear_destination(instruction.asOperands[0]);
						string type_name;
						Operand &resource = instruction.asOperands[3];
						ResourceGroup group = resource.eType == OPERAND_TYPE_UNORDERED_ACCESS_VIEW ? RGROUP_UAV : RGROUP_TEXTURE;
						ResourceBinding *bind_info = NULL;
						if (GetResourceFromBindingPoint(group, resource.ui32RegisterNumber, shader->sInfo, &bind_info))
						{
							map<string, string>::iterator type = mStructuredBufferTypes.find(bind_info->Name);
							if (type != mStructuredBufferTypes.end())
								type_name = type->second;
						}
						if (type_name.empty())
						{
							string resource_name = (group == RGROUP_UAV ? "u" : "t") + to_string(resource.ui32RegisterNumber);
							map<string, string>::iterator type = mStructuredBufferTypes.find(resource_name);
							if (type != mStructuredBufferTypes.end())
								type_name = type->second;
						}

						if (!type_name.empty() && instruction.asOperands[2].eType == OPERAND_TYPE_IMMEDIATE32)
						{
							int base_offset = (int)instruction.asOperands[2].afImmediates[0];
							for (int lane = 0; lane < 4; ++lane)
							{
								if (!(instruction.asOperands[0].ui32CompMask & (1u << lane)))
									continue;
								int source_lane = (int)resource.aui32Swizzle[lane];
								if (source_lane < 0 || source_lane > 3)
									continue;
								StructuredOrigin origin = { type_name, base_offset + source_lane * 4 };
								origins[make_pair(instruction.asOperands[0].ui32RegisterNumber, lane)] = origin;
							}
						}
						continue;
					}

					// Any other instruction writing a temporary kills the old load
					// origin.  Source-only/control-flow instructions have no temp
					// destination and are naturally ignored here.
					if (instruction.ui32NumOperands && instruction.asOperands[0].eType == OPERAND_TYPE_TEMP)
					{
						switch (instruction.eOpcode)
						{
						case OPCODE_IF:
						case OPCODE_LOOP:
						case OPCODE_SWITCH:
						case OPCODE_CASE:
						case OPCODE_BREAKC:
						case OPCODE_CONTINUEC:
						case OPCODE_RETC:
						case OPCODE_CALL:
						case OPCODE_CALLC:
							break;
						default:
							clear_destination(instruction.asOperands[0]);
							break;
						}
					}
				}
			}
		}
	}

	static void applySwizzleLiteral(char *right, char *right2, bool useInt, size_t pos, char idx[4])
	{
		// Single literal?
		if (!strchr(right, ','))
		{
			strcpy_s(right2, opcodeSize, right + 2);
			right2[strlen(right2) - 1] = 0;
			return;
		}

		char *beginPos = right + 2;
		float args[4];
		unsigned hex_args[4];
		bool is_hex[4];
		for (int i = 0; i < 4; ++i)
		{
			char *endPos = strchr(beginPos, ',');
			if (endPos) *endPos = 0;
			sscanf_s(beginPos, "%f", args + i);
			is_hex[i] = (sscanf_s(beginPos, " 0x%x", hex_args + i) == 1);
			beginPos = endPos + 1;
		}
		if (pos == 1)
		{
			sprintf_s(right2, opcodeSize, "%.9g", args[idx[0]]);
		}
		else
		{
			// Only integer values?
			bool isInt = true;
			for (int i = 0; idx[i] >= 0 && i < 4; ++i)
				isInt = isInt && (is_hex[idx[i]] || (floor(args[idx[i]]) == args[idx[i]]));
			if (isInt && useInt)
			{
				sprintf_s(right2, opcodeSize, "int%Id(", pos);
				for (int i = 0; idx[i] >= 0 && i < 4; ++i) {
					if (is_hex[idx[i]])
						sprintf_s(right2 + strlen(right2), opcodeSize - strlen(right2), "0x%x,", hex_args[idx[i]]);
					else
						sprintf_s(right2 + strlen(right2), opcodeSize - strlen(right2), "%d,", int(args[idx[i]]));
				}
				right2[strlen(right2) - 1] = 0;
				strcat_s(right2, opcodeSize, ")");
			}
			else
			{
				sprintf_s(right2, opcodeSize, "float%Id(", pos);
				for (int i = 0; idx[i] >= 0 && i < 4; ++i)
					sprintf_s(right2 + strlen(right2), opcodeSize - strlen(right2), "%.9g,", args[idx[i]]);
				right2[strlen(right2) - 1] = 0;
				strcat_s(right2, opcodeSize, ")");
			}
		}
	}

	void applySwizzle(const char *left, char *right, bool useInt = false)
	{
		char right2[opcodeSize];

		if (strlen(right) == 0) {
			// If we have been called without a second parameter
			// for any reason, bail out now before we corrupt the
			// heap later
			logDecompileError("applySwizzle called with no second parameter: " + string(left));
			return;
		}

		if (right[strlen(right) - 1] == ',') right[strlen(right) - 1] = 0;

		// Strip sign and absolute, so they can be re-added at the end.
		bool absolute = false, negative = false;
		if (right[0] == '-')
		{
			negative = true;
			strcpy_s(right2, sizeof(right2), &right[1]);
			strcpy_s(right, opcodeSize, right2);
		}
		if (right[0] == '|')
		{
			absolute = true;
			strncpy_s(right2, sizeof(right2), &right[1], strlen(right) - 2);
			strcpy_s(right, opcodeSize, right2);
		}

		//dx9
		string absTemp = right;

		size_t absPos = absTemp.find("_abs");
		if (absPos != -1)
		{
			absTemp.replace(absPos, 4, "");
			strcpy_s(right, opcodeSize, absTemp.c_str());
			absolute = true;
		}
		//dx9


		// Fairly bold change here- this fetches the source swizzle from 'left', and it previously would
		// find the first dot in the string.  That's not right for left side array indices, so I changed it
		// to look for the far right dot instead.  Should be correct, but this is used everywhere.
		const char *strPos = strrchr(left, '.') + 1;
		char idx[4] = { -1, -1, -1, -1 };
		char map[4] = { 3, 0, 1, 2 };
		size_t pos = 0;							// Used as index into string buffer
		if (strPos == (const char *)1)
			strPos = "x";
		while (*strPos && pos < 4)
			idx[pos++] = map[*strPos++ - 'w'];

		// literal?
		if (right[0] == 'l')
			applySwizzleLiteral(right, right2, useInt, pos, idx);
		else if (right[0] == 'c' && right[1] != 'b')
		{
			//dx9 const register, start with c
			// FIXME: Refactor this into a dedicated function

			char * result = strrchr(right, '.');
			if (result == NULL)		//if don't have swizzle info，add .xyzw
			{
				strcat_s(right, opcodeSize, ".xyzw");
			}

			strPos = strrchr(right, '.') + 1;
			strncpy(right2, right, strPos - right);
			right2[strPos - right] = 0;
			pos = strlen(right2);
			// Single value?
			if (strlen(right) - strlen(right2) == 1)
				strcpy(right2, right);
			else
			{
				for (int i = 0; idx[i] >= 0 && i < 4; ++i)
					right2[pos++] = strPos[idx[i]];
				right2[pos] = 0;
			}

			const char *strPos1 = strrchr(right2, '.') + 1;
			char idx1[4] = { -1, -1, -1, -1 };
			size_t pos1 = 0;
			while (*strPos1 && pos1 < 4)
				idx1[pos1++] = map[*strPos1++ - 'w'];

			char buff[opcodeSize];
			char suffix[opcodeSize];
			char * pos = strchr(right2, '.');
			if (pos != NULL)
			{
				strncpy(buff, right2, pos - right2);
				buff[pos - right2] = 0;
				size_t len = strlen(right2) - (right2 - pos) - 1;
				strncpy(suffix, pos + 1, len);
				suffix[len] = 0;
			}
			else
			{
				strcpy_s(buff, opcodeSize, right2);
				suffix[0] = 0;
			}

			int index = atoi(&buff[1]);

			std::map<int, string>::iterator it = mUniformNames.find(index);
			if (it != mUniformNames.end())
			{
				string temp = right;
				temp.replace(0, strlen(buff), it->second);
				strcpy_s(buff, opcodeSize, temp.c_str());
			}

			std::map<int, ConstantValue>::iterator cit = mConstantValues.find(index);
			if (cit != mConstantValues.end())
			{

				sprintf_s(buff, opcodeSize, "%s", right2);


				for (int i = 0; idx1[i] >= 0 && i < 4; ++i)
				{
					if (idx1[i] == 0)
					{
						sprintf_s(buff, opcodeSize, "%s %f", buff, cit->second.x);
					}
					else if (idx1[i] == 1)
					{
						sprintf_s(buff, opcodeSize, "%s %f", buff, cit->second.y);
					}
					else if (idx1[i] == 2)
					{
						sprintf_s(buff, opcodeSize, "%s %f", buff, cit->second.z);
					}
					else if (idx1[i] == 3)
					{
						sprintf_s(buff, opcodeSize, "%s %f", buff, cit->second.w);
					}
				}
			}

			strcpy_s(right2, opcodeSize, buff);
		/*else if (right[0] == 'v')
		+		{
		+			strcpy_s(right2, opcodeSize, right);
		+		}*/
		//dx9
		}
		else
		{
			//dx9
			char * result = strrchr(right, '.');
			if (result == NULL)		//if don't have swizzle info，add .xyzw
			{
				strcat_s(right, opcodeSize, ".xyzw");
			}
			//dx9

			strPos = strrchr(right, '.') + 1;
			if (strPos == (const char *)1) {
				// If there's no '.' in the string, strrchr
				// will have returned a NULL pointer. If we
				// were to continue we would write a 0 to a
				// random location since right2[1-right] will
				// run off the start of the buffer.
				//
				// XXX With the addition of the above default
				// swizzle, this should no longer be possible.
				// Leaving this error path in anyway just in
				// case since memory corruption is not fun.
				logDecompileError("applySwizzle 2nd parameter missing '.': " + string(right));
				return;
			}
			strncpy(right2, right, strPos - right);
			right2[strPos - right] = 0;
			pos = strlen(right2);
			// Single value?
			if (strlen(right) - strlen(right2) == 1)
				strcpy(right2, right);
			else
			{
				for (int i = 0; idx[i] >= 0 && i < 4; ++i)
					right2[pos++] = strPos[idx[i]];
				right2[pos] = 0;
			}

			// Buffer reference?  
			//  This section was doing some char-by-char indexing into the search string 'right2', and has been
			//  changed to just use sscanf_s as a more reliable way of parsing, although it's a risky change.
			strPos = strstr(right2, "cb");
			if (strPos)
			{
				int bufIndex = 0;
				int bufOffset;
				char regAndSwiz[opcodeSize];
				
				// By scanning these in this order, we are sure to cover every variant, without mismatches.
				// We use the unusual format of [^+] for the string lookup because ReadStatement has already 
				// crushed the spaces out of the input.

				// Like: -cb2[r12.w+63].xyzx  as : -cb(bufIndex)[(regAndSwiz)+(bufOffset)]
				if (sscanf_s(strPos, "cb%d[%[^+]+%d]", &bufIndex, regAndSwiz, UCOUNTOF(regAndSwiz), &bufOffset) == 3)
				{
					// Some constant buffers no longer have variable names, giving us generic names like cb0[23].
					// The syntax doesn't work to use those names, so in this scenario, we want to just use the strPos name, unchanged.
					CBufferData::iterator it = mCBufferData.find((bufIndex << 16) + bufOffset * 16);
					if (it == mCBufferData.end())
					{
						logDecompileError("Missing constant buffer name: " + string(right2));
						return;
					}

					if (strncmp(strPos, it->second.Name.c_str(), 4) == 0)
					{
						string full = strPos;
						size_t dotspot = full.rfind('.');
						it->second.Name = full.substr(0, dotspot);
						regAndSwiz[0] = 0;
					}
				}
				// Like: cb1[29].xyzx  Most common variant.  If a register, %d will fail.
				else if (sscanf_s(strPos, "cb%d[%d]", &bufIndex, &bufOffset) == 2)
				{
					regAndSwiz[0] = 0;
				}
				// Like: cb0[r0.w].xy
				else if (sscanf_s(strPos, "cb%d[%s]", &bufIndex, regAndSwiz, UCOUNTOF(regAndSwiz)) == 2)
				{
					bufOffset = 0;
				}
				// Like: icb[r0.w+0].xyzw
				else if (sscanf_s(strPos, "cb[%[^+]+%d]", regAndSwiz, UCOUNTOF(regAndSwiz), &bufOffset) == 2)
				{
					bufIndex = -1;		// -1 is used as 'index' for icb entries.
				}
				// Like: icb[22].w  doesn't seem to exist, but here for completeness.
				else if (sscanf_s(strPos, "cb[%d]", &bufOffset) == 1)
				{
					bufIndex = -1;		// -1 is used as 'index' for icb entries.
					regAndSwiz[0] = 0;
				}
				// Like: icb[r1.z].xy
				else if (sscanf_s(strPos, "cb[%s]", regAndSwiz, UCOUNTOF(regAndSwiz)) == 1)
				{
					bufIndex = -1;		// -1 is used as 'index' for icb entries.
					bufOffset = 0;
				}
				else
				{
					logDecompileError("Error parsing buffer register, unknown variant: " + string(right2));
					return;
				}

				// Missing mCBufferData name for this buffer entry.  In the icb case, that's
				// expected, because entries are not named. Just build the CBufferData to use.
				// Bit of a hack workaround, but icb doesn't really fit here.
				// output will be like: icb[r6.z + 10]
				if (bufIndex == -1)
				{
					BufferEntry immediateEntry;
					string full = right2;
					size_t dotspot = full.rfind('.');
					immediateEntry.Name = full.substr(0, dotspot);
					immediateEntry.matrixRow = 0;
					immediateEntry.isRowMajor = false;
					immediateEntry.bt = DT_float4;
					mCBufferData[(bufIndex << 16) + bufOffset * 16] = immediateEntry;
					regAndSwiz[0] = 0;
				}

				// mCBufferData includes the actual named variable, as the way to convert numeric offsets
				// back into proper names.  -cb2[r12.w + 63].xyzx -> _SpotLightDirection[r12.w].xyz
				CBufferData::iterator i = mCBufferData.find((bufIndex << 16) + bufOffset * 16);
				if (i == mCBufferData.end() && strrchr(right2, '.')[1] == 'y')
					i = mCBufferData.find((bufIndex << 16) + bufOffset * 16 + 4);
				if (i == mCBufferData.end() && strrchr(right2, '.')[1] == 'z')
					i = mCBufferData.find((bufIndex << 16) + bufOffset * 16 + 8);
				if (i == mCBufferData.end() && strrchr(right2, '.')[1] == 'w')
					i = mCBufferData.find((bufIndex << 16) + bufOffset * 16 + 12);
				if (i == mCBufferData.end())
				{
					logDecompileError("3 Error parsing buffer offset: " + string(right2));
					//error parsing shader> 3 Error parsing buffer offset : icb[r6.z + 10].w
					//bufIndex : -1  bufOffset : 10
					//strPos : +10].w
					return;
				}
				if ((i->second.bt == DT_float || i->second.bt == DT_uint || i->second.bt == DT_int || i->second.bt == DT_bool) ||
					((i->second.bt == DT_float2 || i->second.bt == DT_uint2 || i->second.bt == DT_int2 || i->second.bt == DT_bool2) && (strrchr(right2, '.')[1] == 'w' || strrchr(right2, '.')[1] == 'z')) ||
					((i->second.bt == DT_float3 || i->second.bt == DT_uint3 || i->second.bt == DT_int3 || i->second.bt == DT_bool3) && strrchr(right2, '.')[1] == 'w'))
				{
					int skip = 4;
					if (i->second.bt == DT_float || i->second.bt == DT_uint || i->second.bt == DT_int || i->second.bt == DT_bool) skip = 1;
					else if (i->second.bt == DT_float2 || i->second.bt == DT_uint2 || i->second.bt == DT_int2 || i->second.bt == DT_bool2) skip = 2;
					else if (i->second.bt == DT_float3 || i->second.bt == DT_uint3 || i->second.bt == DT_int3 || i->second.bt == DT_bool3) skip = 3;
					char *dotPos = strrchr(right2, '.');
					int lowOffset = dotPos[1] - 'x'; if (dotPos[1] == 'w') lowOffset = 3;
					if (lowOffset >= skip)
					{
						int skipEnd;
						for (skipEnd = lowOffset; skipEnd >= skip; --skipEnd)
						{
							i = mCBufferData.find((bufIndex << 16) + bufOffset * 16 + skipEnd * 4);
							if (i != mCBufferData.end())
								break;
						}
						if (i == mCBufferData.end())
						{
							logDecompileError("Error parsing buffer low offset: " + string(right2));
							return;
						}
						const char *INDEX_MASK = "xyzw";
						for (int cpos = 1; dotPos[cpos] >= 'w' && dotPos[cpos] <= 'z'; ++cpos)
						{
							lowOffset = dotPos[cpos] - 'x'; if (dotPos[cpos] == 'w') lowOffset = 3;
							dotPos[cpos] = INDEX_MASK[lowOffset - skipEnd];
						}
					}
				}
				char right3[opcodeSize]; right3[0] = 0;
				strcat(right3, i->second.Name.c_str());
				strPos = strchr(strPos, ']');
				if (!strPos)
				{
					logDecompileError("4 Error parsing buffer offset: " + string(right2));
					return;
				}
				if (regAndSwiz[0])
				{
					// Remove existing index.
					char *indexPos = strchr(right3, '[');
					if (indexPos) *indexPos = 0;
					string indexRegisterName(regAndSwiz, strchr(regAndSwiz, '.'));
					StringStringMap::iterator isCorrected = mCorrectedIndexRegisters.find(indexRegisterName);
					if (isCorrected != mCorrectedIndexRegisters.end())
					{
						char newOperand[opcodeSize]; strcpy(newOperand, isCorrected->second.c_str());
						applySwizzle(regAndSwiz, newOperand, true);
						sprintf_s(right3 + strlen(right3), sizeof(right3) - strlen(right3), "[%s]", newOperand);
					}
					else if (mLastStatement && mLastStatement->eOpcode == OPCODE_IMUL &&
						(i->second.bt == DT_float4x4 || i->second.bt == DT_float4x3 || i->second.bt == DT_float4x2 || i->second.bt == DT_float2x4 ||
						i->second.bt == DT_float3x4 || i->second.bt == DT_float3x3) &&
						mMulOperand.substr(0, mMulOperand.find(".")) != indexRegisterName) // We can't use the old non-multiplied index variable if it was the same one overwritten with the result of the multiplication. TestShaders/GameExamples/DOAXVV/ba2ad61fa36ff709-vs.bin
					{
						char newOperand[opcodeSize]; strcpy(newOperand, mMulOperand.c_str());
						applySwizzle(regAndSwiz, newOperand, true);
						sprintf_s(right3 + strlen(right3), sizeof(right3) - strlen(right3), "[%s]", newOperand);
						mCorrectedIndexRegisters[indexRegisterName] = mMulOperand;
					}
					else if (i->second.bt == DT_float4x2 || i->second.bt == DT_float2x4)
						sprintf_s(right3 + strlen(right3), sizeof(right3) - strlen(right3), "[%s/2]", regAndSwiz);
					else if (i->second.bt == DT_float4x3 || i->second.bt == DT_float3x4)
						sprintf_s(right3 + strlen(right3), sizeof(right3) - strlen(right3), "[%s/3]", regAndSwiz);
					else if (i->second.bt == DT_float4x4)
						sprintf_s(right3 + strlen(right3), sizeof(right3) - strlen(right3), "[%s/4]", regAndSwiz);
					else
					{
						// Most common case, like: g_AmbientCube[r3.w] and Globals[r19.w+63]

						// Bug was to not handle the struct case here, and truncate string.
						//  Like g_OmniLights[r5.w].m_PositionFar -> g_OmniLights[r5.w]
						//sprintf(right3 + strlen(right3), "[%s]", indexRegister);
						
						// Start fresh with original string and just replace, not char* manipulate.
						// base e.g: g_OmniLights[0].m_PositionFar
						string base = i->second.Name;
						size_t left = base.find('[') + 1;
						size_t length = base.find(']') - left;

						// Another bug was to miss array indexing into a variable name. So if i->second.Name
						// is not a zero index element, we need to preserve the offset.
						// Like a JC3 variant of cb0[r19.w + 63].xyzw becoming Globals[r19.w+63]
						// Normally an offset turns into an actual named variable.

						int offset;
						sscanf_s(i->second.Name.c_str(), "%*[^[][%d]", &offset);
						if (offset == 0)
						{
							// e.g. cb0[r21.y + 55] -> PointLight[r21.y]
							base.replace(left, length, regAndSwiz);
						}
						else
						{
							// leave it named with register and bufOffset, e.g. cb0[r19.w + 63] -> Globals[r19.w+63]
							char regAndOffset[opcodeSize];
							sprintf_s(regAndOffset, "%s+%d", regAndSwiz, bufOffset);
							base.replace(left, length, regAndOffset);
						}

						strcpy(right3, base.c_str());
					}
				}
				if (i->second.bt != DT_float && i->second.bt != DT_bool && i->second.bt != DT_uint && i->second.bt != DT_int)
				{
					strcat(right3, ".");
					if (i->second.bt == DT_float4x4 || i->second.bt == DT_float4x3 || i->second.bt == DT_float4x2 || i->second.bt == DT_float2x4 ||
						i->second.bt == DT_float3x4 || i->second.bt == DT_float3x3)
					{
						strPos = strrchr(right2, '.');
						while (*++strPos)
						{
							switch (*strPos)
							{
								case 'x':
									sprintf_s(right3 + strlen(right3), sizeof(right3) - strlen(right3), i->second.isRowMajor ? "_m%d0" : "_m0%d", i->second.matrixRow);
									break;
								case 'y':
									sprintf_s(right3 + strlen(right3), sizeof(right3) - strlen(right3), i->second.isRowMajor ? "_m%d1" : "_m1%d", i->second.matrixRow);
									break;
								case 'z':
									sprintf_s(right3 + strlen(right3), sizeof(right3) - strlen(right3), i->second.isRowMajor ? "_m%d2" : "_m2%d", i->second.matrixRow);
									break;
								case 'w':
									sprintf_s(right3 + strlen(right3), sizeof(right3) - strlen(right3), i->second.isRowMajor ? "_m%d3" : "_m3%d", i->second.matrixRow);
									break;
								default: logDecompileError("Error parsing matrix index: " + string(right2));
							}
						}
					}
					else
					{
						strPos = strrchr(right2, '.');
						strcat(right3, strPos + 1);
					}
				}
				strcpy(right2, right3);
			}
		}
		if (absolute && negative)
			sprintf_s(right, opcodeSize, "-abs(%s)", right2);
		else if (absolute)
			sprintf_s(right, opcodeSize, "abs(%s)", right2);
		else if (negative)
			sprintf_s(right, opcodeSize, "-%s", right2);
		else
			strcpy_s(right, opcodeSize, right2);		// All input params are 128 char arrays, like op1, op2, op3
	}


	char statement[128],
		op1[opcodeSize], op2[opcodeSize], op3[opcodeSize], op4[opcodeSize], op5[opcodeSize], op6[opcodeSize], op7[opcodeSize], op8[opcodeSize],
		op9[opcodeSize], op10[opcodeSize], op11[opcodeSize], op12[opcodeSize], op13[opcodeSize], op14[opcodeSize], op15[opcodeSize];
	int ReadStatement(const char *pos)
	{
		// Kill newline.
		char lineBuffer[256];
		strncpy(lineBuffer, pos, 255); lineBuffer[255] = 0;
		char *newlinePos = strchr(lineBuffer, '\n'); if (newlinePos) *newlinePos = 0;
		op1[0] = 0; op2[0] = 0; op3[0] = 0; op4[0] = 0; op5[0] = 0; op6[0] = 0; op7[0] = 0; op8[0] = 0;
		op9[0] = 0; op10[0] = 0; op11[0] = 0; op12[0] = 0; op13[0] = 0; op14[0] = 0; op15[0] = 0;

		int numRead = sscanf_s(lineBuffer, "%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s",
			statement, UCOUNTOF(statement),
			op1, opcodeSize, op2, opcodeSize, op3, opcodeSize, op4, opcodeSize, op5, opcodeSize, op6, opcodeSize, op7, opcodeSize, op8, opcodeSize,
			op9, opcodeSize, op10, opcodeSize, op11, opcodeSize, op12, opcodeSize, op13, opcodeSize, op14, opcodeSize, op15, opcodeSize);

		// Cull the [precise] from any instruction using it by moving down all opcodes to recreate
		// the instruction, minus the 'precise'.  ToDo: add 'precise' keyword to output variable.
		// e.g. mov [precise(xy)] r1.xy, v1.xyxx
		// to   mov r1.xy, v1.xyxx
		if (!strncmp(op1, "[precise", strlen("[precise")))
		{
			strcpy(op1, op2); strcpy(op2, op3); strcpy(op3, op4); strcpy(op4, op5); strcpy(op5, op6); strcpy(op6, op7); strcpy(op7, op8); strcpy(op8, op9); strcpy(op9, op10); strcpy(op10, op11); strcpy(op11, op12);
		}

		// Was previously a subroutine to CollectBrackets, but generated a lot of warnings, so putting it inline allows
		// the automatic CRT_SECURE macros to find the sizes.
		//  CollectBrackets(op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15);
		{
			if (!strncmp(op1, "l(", 2) && op1[strlen(op1) - 1] != ')' && op1[strlen(op1) - 2] != ')')
			{
				strcat(op1, " "); strcat(op1, op2);
				strcat(op1, " "); strcat(op1, op3);
				strcat(op1, " "); strcat(op1, op4);
				strcpy(op2, op5); strcpy(op3, op6); strcpy(op4, op7); strcpy(op5, op8); strcpy(op6, op9); strcpy(op7, op10); strcpy(op8, op11); strcpy(op9, op12); strcpy(op10, op13); strcpy(op11, op14); strcpy(op12, op15);
				op13[0] = 0; op14[0] = 0; op15[0] = 0;
			}
			if (!strncmp(op2, "l(", 2) && op2[strlen(op2) - 1] != ')' && op2[strlen(op2) - 2] != ')')
			{
				strcat(op2, " "); strcat(op2, op3);
				strcat(op2, " "); strcat(op2, op4);
				strcat(op2, " "); strcat(op2, op5);
				strcpy(op3, op6); strcpy(op4, op7); strcpy(op5, op8); strcpy(op6, op9); strcpy(op7, op10); strcpy(op8, op11); strcpy(op9, op12);  strcpy(op10, op13); strcpy(op11, op14); strcpy(op12, op15);
				op13[0] = 0; op14[0] = 0; op15[0] = 0;
			}
			if (!strncmp(op3, "l(", 2) && op3[strlen(op3) - 1] != ')' && op3[strlen(op3) - 2] != ')')
			{
				strcat(op3, " "); strcat(op3, op4);
				strcat(op3, " "); strcat(op3, op5);
				strcat(op3, " "); strcat(op3, op6);
				strcpy(op4, op7); strcpy(op5, op8); strcpy(op6, op9); strcpy(op7, op10); strcpy(op8, op11); strcpy(op9, op12);  strcpy(op10, op13); strcpy(op11, op14); strcpy(op12, op15);
				op13[0] = 0; op14[0] = 0; op15[0] = 0;
			}
			if (!strncmp(op4, "l(", 2) && op4[strlen(op4) - 1] != ')' && op4[strlen(op4) - 2] != ')')
			{
				strcat(op4, " "); strcat(op4, op5);
				strcat(op4, " "); strcat(op4, op6);
				strcat(op4, " "); strcat(op4, op7);
				strcpy(op5, op8); strcpy(op6, op9); strcpy(op7, op10); strcpy(op8, op11); strcpy(op9, op12);  strcpy(op10, op13); strcpy(op11, op14); strcpy(op12, op15);
				op13[0] = 0; op14[0] = 0; op15[0] = 0;
			}
			if (!strncmp(op5, "l(", 2) && op5[strlen(op5) - 1] != ')' && op5[strlen(op5) - 2] != ')')
			{
				strcat(op5, " "); strcat(op5, op6);
				strcat(op5, " "); strcat(op5, op7);
				strcat(op5, " "); strcat(op5, op8);
				strcpy(op6, op9); strcpy(op7, op10); strcpy(op8, op11); strcpy(op9, op12);  strcpy(op10, op13); strcpy(op11, op14); strcpy(op12, op15);
				op13[0] = 0; op14[0] = 0; op15[0] = 0;
			}
			if (!strncmp(op6, "l(", 2) && op6[strlen(op6) - 1] != ')' && op6[strlen(op6) - 2] != ')')
			{
				strcat(op6, " "); strcat(op6, op7);
				strcat(op6, " "); strcat(op6, op8);
				strcat(op6, " "); strcat(op6, op9);
				strcpy(op7, op10); strcpy(op8, op11); strcpy(op9, op12);  strcpy(op10, op13); strcpy(op11, op14); strcpy(op12, op15);
				op13[0] = 0; op14[0] = 0; op15[0] = 0;
			}
			if (!strncmp(op7, "l(", 2) && op7[strlen(op7) - 1] != ')' && op7[strlen(op7) - 2] != ')')
			{
				strcat(op7, " "); strcat(op7, op8);
				strcat(op7, " "); strcat(op7, op9);
				strcat(op7, " "); strcat(op7, op10);
				strcpy(op8, op11); strcpy(op9, op12);  strcpy(op10, op13); strcpy(op11, op14); strcpy(op12, op15);
				op13[0] = 0; op14[0] = 0; op15[0] = 0;
			}
			while (!strcmp(op2, "+"))
			{
				strcat(op1, op2); strcat(op1, op3);
				strcpy(op2, op4); strcpy(op3, op5); strcpy(op4, op6); strcpy(op5, op7); strcpy(op6, op8); strcpy(op7, op9); strcpy(op8, op10); strcpy(op9, op11); strcpy(op10, op12); strcpy(op11, op13); strcpy(op12, op14); strcpy(op13, op15);
				op14[0] = 0; op15[0] = 0;
			}
			while (!strcmp(op3, "+"))
			{
				strcat(op2, op3); strcat(op2, op4);
				strcpy(op3, op5); strcpy(op4, op6); strcpy(op5, op7); strcpy(op6, op8); strcpy(op7, op9); strcpy(op8, op10); strcpy(op9, op11); strcpy(op10, op12);  strcpy(op11, op13); strcpy(op12, op14); strcpy(op13, op15);
				op14[0] = 0; op15[0] = 0;
			}
			while (!strcmp(op4, "+"))
			{
				strcat(op3, op4); strcat(op3, op5);
				strcpy(op4, op6); strcpy(op5, op7); strcpy(op6, op8); strcpy(op7, op9); strcpy(op8, op10); strcpy(op9, op11); strcpy(op10, op12); strcpy(op11, op13); strcpy(op12, op14); strcpy(op13, op15);
				op14[0] = 0; op15[0] = 0;
			}
			while (!strcmp(op5, "+"))
			{
				strcat(op4, op5); strcat(op4, op6);
				strcpy(op5, op7); strcpy(op6, op8); strcpy(op7, op9); strcpy(op8, op10); strcpy(op9, op11); strcpy(op10, op12); strcpy(op11, op13); strcpy(op12, op14); strcpy(op13, op15);
				op14[0] = 0; op15[0] = 0;
			}
			while (!strcmp(op6, "+"))
			{
				strcat(op5, op6); strcat(op5, op7);
				strcpy(op6, op8); strcpy(op7, op9); strcpy(op8, op10); strcpy(op9, op11); strcpy(op10, op12); strcpy(op11, op13); strcpy(op12, op14); strcpy(op13, op15);
				op14[0] = 0; op15[0] = 0;
			}
		}
		return numRead;
	}

	string replaceInt(string input)
	{
		float number;
		if (sscanf_s(input.c_str(), "%f", &number) != 1)
			return input;
		if (floor(number) != number)
			return input;
		char buffer[64];
		sprintf(buffer, "%d", (int)number);
		return buffer;
	}

	string GetSuffix(char *s, int index)
	{
		if (s[strlen(s) - 1] == ',') s[strlen(s) - 1] = 0;
		char *dotPos = strrchr(s, '.');
		// Single byte?
		if (dotPos && dotPos[1] >= 'w' && dotPos[1] <= 'z')
		{
			string buffer = s;
			size_t targetPos = buffer.find_last_of('.');
			buffer[targetPos + 1] = dotPos[1 + index];
			buffer.resize(targetPos + 2);
			if (!buffer.substr(0, 4).compare("abs("))
				buffer.push_back(')');
			return buffer;
		}
		// Single literal?
		if (!strchr(s, ','))
		{
			return s;
		}
		// Vector literal.
		if (index == 0)
		{
			string buffer = strchr(s, '(') + 1;
			buffer.resize(buffer.find(','));
			return replaceInt(buffer);
		}
		if (index == 1)
		{
			string buffer = strchr(s, ',') + 1;
			buffer.resize(buffer.find_first_of(",)"));
			return replaceInt(buffer);
		}
		if (index == 2)
		{
			dotPos = strchr(s, ',') + 1;
			char *endPos = strchr(dotPos, ',');
			if (!endPos) endPos = strchr(dotPos, ')');
			string buffer = endPos + 1;
			buffer.resize(buffer.find_first_of(",)"));
			return replaceInt(buffer);
		}
		string buffer = strrchr(s, ',') + 1;
		buffer.resize(buffer.rfind(')'));
		return replaceInt(buffer);
	}

	// This was expected to take input of the form 
	//	r1.xyxx
	// and decide how to truncate it based on the input type of Texture2D, 3D or Cube.
	// If it's Texture2D for example, that would trim to only the first two elements
	// as a 2D texture, so r1.xy
	//
	// This failed for constants of the form 
	//	float4(0.5, 0.5, 0, 0)
	// which were found in AC3.
	// The fix is to look for that constant format too, and if we see those, truncate
	// the constant to match.  So, for example, float4(0.5, 0.5, 0, 0) with Texture2D
	// becomes float2(0.5, 0.5) as the two elements. 

	void truncateTexturePos(char *op, const char *textype)
	{
		char *cpos;

		if (!strncmp(op, "float", 5))
		{
			if (!strncmp(textype, "Texture2D", 9))
			{
				cpos = strchr(op, ',');
				cpos++;
				cpos = strchr(cpos, ',');
				cpos[0] = ')';
				cpos[1] = 0;
				op[5] = '2';	// now: float2(x,y)
			}
			else if (!strncmp(textype, "Texture3D", 9))
			{
				cpos = strchr(op, ',');
				cpos++;
				cpos = strchr(cpos, ',');
				cpos++;
				cpos = strchr(cpos, ',');
				cpos[0] = ')';
				cpos[1] = 0;
				op[5] = '3';	// now: float3(x,y,z)
			}
			else // if (!strncmp(textype, "TextureCube", 11) || !strncmp(textype, "TextureCubeArray", 11))
			{
				// left as: float4(x,y,z,w)
			}
		}
		else
		{
			// Normal variant like r1.xyxx
			int pos = 5;
			if (!strncmp(textype, "Texture1D<", strlen("Texture1D<"))) pos = 2;						// float .x
			else if (!strncmp(textype, "Texture2DMS<", strlen("Texture2DMS<"))) pos = 3;			// float2 .xy
			else if (!strncmp(textype, "Texture1DArray<", strlen("Texture1DArray<"))) pos = 3;
			else if (!strncmp(textype, "Texture2D<", strlen("Texture2D<"))) pos = 3;
			else if (!strncmp(textype, "Texture2DMSArray<", strlen("Texture2DMSArray<"))) pos = 3;
			else if (!strncmp(textype, "Texture2DArray<", strlen("Texture2DArray<"))) pos = 4;		// float3 .xyz
			else if (!strncmp(textype, "Texture3D<", strlen("Texture3D<"))) pos = 4;
			else if (!strncmp(textype, "TextureCube<", strlen("TextureCube<"))) pos = 4;
			else if (!strncmp(textype, "TextureCubeArray<", strlen("TextureCubeArray<"))) pos = 5;	// float4 .xyzw
			else logDecompileError("  unknown texture type for truncation: " + string(textype));
			cpos = strrchr(op, '.');
			cpos[pos] = 0;
		}
	}


	// This routine was expecting only r0.xyz type input parameters, but we can also get float4(0,0,0,0) type
	// inputs as constants to things like .Load().  If it's a constant of any form, leave it unchanged.
	// The reason there is a second version of this .xyzw truncator, is because this version is for the _LD
	// operands, and for that version it's one parameter larger.  Texture2D is 3 components, not 2 for this one.
	// The extra bracket for comparison is to avoid early mismatch like Texture1D instead of Texture1DArray.
	void truncateTextureLoadPos(char *op, const char *textype)
	{
		if (!strncmp(op, "float", 5))
			return;

		int pos = 5;																		
		if (!strncmp(textype, "Buffer<", strlen("Buffer<"))) pos = 2;						// int  .x
		else if (!strncmp(textype, "Texture1D<", strlen("Texture1D<"))) pos = 3;			// int2 .xy
		else if (!strncmp(textype, "Texture2DMS<", strlen("Texture2DMS<"))) pos = 3;
		else if (!strncmp(textype, "Texture1DArray<", strlen("Texture1DArray<"))) pos = 4;	// int3 .xyz
		else if (!strncmp(textype, "Texture2D<", strlen("Texture2D<"))) pos = 4;
		else if (!strncmp(textype, "Texture2DMSArray<", strlen("Texture2DMSArray<"))) pos = 4;
		else if (!strncmp(textype, "Texture2DArray<", strlen("Texture2DArray<"))) pos = 5;	// int4 .xyzw
		else if (!strncmp(textype, "Texture3D<", strlen("Texture3D<"))) pos = 5;
		else logDecompileError("  unknown texture type for truncation: " + string(textype));
		char *cpos = strrchr(op, '.');
		cpos[pos] = 0;
	}

	// Lops off excess mask/swizzle for textures of float1/2/3 or other vector
	// types to eliminate invalid subscript errors
	static void truncateTextureSwiz(char *op, const char *textype)
	{
		// Search for the end of the type, either a comma
		// (Texture2DMS<float,2>) or the closing >
		const char *tpos = strchr(textype, ',');
		if (!tpos)
			tpos = strchr(textype, '>');
		if (!tpos)
			return;

		int pos = 5;
		if (tpos[-1] == '4') // float4
			return;
		else if (tpos[-1] == '3') // float3
			pos = 4;
		else if (tpos[-1] == '2') // float2
			pos = 3;
		else // float1, float, etc
			pos = 2;
		char *cpos = strrchr(op, '.');
		if (strlen(cpos) >= (size_t)pos)
			cpos[pos] = 0;
	}

	void remapTarget(char *target)
	{
		char *pos = strchr(target, ',');
		if (pos) *pos = 0;
	}

	void stripMask(char *target)
	{
		char *pos = strchr(target, '.');
		if (pos) *pos = 0;
	}

	char *writeTarget(char *target)
	{
		StringStringMap::iterator i = mRemappedOutputRegisters.find(target);
		if (i != mRemappedOutputRegisters.end())
			strcpy_s(target, opcodeSize, i->second.c_str());		// only used for opcode strings.
		return target;
	}

	string ci(string input)
	{
		vector<pair<string, string> >::iterator i;
		for (i = mRemappedInputRegisters.begin(); i != mRemappedInputRegisters.end(); ++i)
		{
			const char *pos = strstr(input.c_str(), i->first.c_str());
			if (pos)
			{
				string ret(input.c_str(), pos);
				return ret + i->second + string(pos + strlen(i->first.c_str()));
			}
		}
		return input;
	}


	// Only necessary for opcode_AND when used as boolean tests.
	// Converts literals of the form: l(0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000) to l(1.0, 1.0, 1.0, 1.0)

	// In the microsoft world, 10 year old C specs are not interesting, so no stdclib function exists here for float,
	// and the std::hexfloat i/o stream fails to handle 32 bits properly. It's quite sad how weak this stuff is.
	// This took quite a long time to figure out how to make it work properly.  The easist answer seemed to be
	// to use reinterpret_cast from uint to float, but that appears to be unnecessary, as the %x assigned directly
	// to a float seems to work properly (but is not documented.)

	// 1/1/15: New variant of l(-1) as an int, but used after a boolean state, so it's a comparison, not a bitand.
	//  This would generate a QNAN as float. Made the hex comparison more strict with 0x%x, and thus that will fail
	//  if it's an int type, and allow for converting that variant.

	void convertHexToFloat(char *target)
	{
		char convert[opcodeSize];
		int count;
		float lit[4];
		int ilit[4];
		int printed;

		if (target[0] == 'l')
		{
			printed = sprintf_s(convert, sizeof(convert), "l(");

			count = sscanf_s(target, "l(%x,%x,%x,%x)", (unsigned int*)&lit[0], (unsigned int*)&lit[1], (unsigned int*)&lit[2], (unsigned int*)&lit[3]);
			if (count != 0)
			{
				for (int i = 0; i < count; i++)
					printed += sprintf_s(&convert[printed], sizeof(convert) - printed, "%f,", lit[i]);
			}
			else 
			{
				count = sscanf_s(target, "l(%i,%i,%i,%i)", &ilit[0], &ilit[1], &ilit[2], &ilit[3]);
				assert(count != 0);
				for (int i = 0; i < count; i++)
					printed += sprintf_s(&convert[printed], sizeof(convert) - printed, "%i,", ilit[i]);
			}


			// Overwrite trailing comma to be closing paren, no matter how many literals were converted.
			convert[printed - 1] = ')';

			strcpy_s(target, opcodeSize, convert);
		}
	}

	char *_convertToInt(char *target, bool fixupCBs)
	{
		char buffer[opcodeSize];

		int isMinus = target[0] == '-' ? 1 : 0;
		if (!strncmp(target + isMinus, "int", 3) || !strncmp(target + isMinus, "uint", 4)) return target;
		if (!strncmp(target + isMinus, "float", 5))
		{
			int size = 0; float f0, f1, f2, f3;
			sscanf_s(target + isMinus, "float%d(%f,%f,%f,%f)", &size, &f0, &f1, &f2, &f3);

			buffer[0] = 0;
			if (isMinus) strcpy(buffer, "-");

			if (size == 2) sprintf_s(buffer + strlen(buffer), opcodeSize - strlen(buffer), "int2(%d,%d)", (int)f0, (int)f1);
			else if (size == 3) sprintf_s(buffer + strlen(buffer), opcodeSize - strlen(buffer), "int3(%d,%d,%d)", (int)f0, (int)f1, (int)f2);
			else if (size == 4) sprintf_s(buffer + strlen(buffer), opcodeSize - strlen(buffer), "int4(%d,%d,%d,%d)", (int)f0, (int)f1, (int)f2, (int)f3);

			strcpy_s(target, opcodeSize, buffer);
			return target;
		}
		// Fixup for constant buffer reads where the type may not be known due to missing headers:
		if (fixupCBs && !strncmp(target + isMinus, "cb", 2))
		{
			_snprintf_s(buffer, opcodeSize, opcodeSize, "asint(%s)", target);
			strcpy_s(target, opcodeSize, buffer);
			return target;
		}
		char *pos = strrchr(target, '.');
		if (pos)
		{
			size_t size = strlen(pos + 1);
			if (size == 1)
				_snprintf_s(buffer, opcodeSize, opcodeSize, "(int)%s", target);
			else
				_snprintf_s(buffer, opcodeSize, opcodeSize, "(int%Id)%s", size, target);
			strcpy_s(target, opcodeSize, buffer);
		}
		return target;
	}

	char *_convertToUInt(char *target, bool fixupCBs)
	{
		char buffer[opcodeSize];

		int isMinus = target[0] == '-' ? 1 : 0;
		if (!strncmp(target + isMinus, "int", 3) || !strncmp(target + isMinus, "uint", 4)) return target;
		if (!strncmp(target + isMinus, "float", 5))
		{
			int size = 0;
			float f0, f1, f2, f3;

			sscanf_s(target + isMinus, "float%d(%f,%f,%f,%f)", &size, &f0, &f1, &f2, &f3);

			buffer[0] = 0;
			if (isMinus) strcpy(buffer, "-");

			if (size == 2) sprintf_s(buffer + strlen(buffer), opcodeSize - strlen(buffer), "uint2(%d,%d)", (int)f0, (int)f1);
			else if (size == 3) sprintf_s(buffer + strlen(buffer), opcodeSize - strlen(buffer), "uint3(%d,%d,%d)", (int)f0, (int)f1, (int)f2);
			else if (size == 4) sprintf_s(buffer + strlen(buffer), opcodeSize - strlen(buffer), "uint4(%d,%d,%d,%d)", (int)f0, (int)f1, (int)f2, (int)f3);

			strcpy_s(target, opcodeSize, buffer);
			return target;
		}
		// Fixup for constant buffer reads where the type may not be known due to missing headers:
		if (fixupCBs && !strncmp(target + isMinus, "cb", 2))
		{
			_snprintf_s(buffer, opcodeSize, opcodeSize, "asuint(%s)", target);
			strcpy_s(target, opcodeSize, buffer);
			return target;
		}
		char *pos = strrchr(target, '.');
		if (pos)
		{
			size_t size = strlen(pos + 1);
			if (size == 1)
				_snprintf_s(buffer, opcodeSize, opcodeSize, "(uint)%s", target);
			else
				_snprintf_s(buffer, opcodeSize, opcodeSize, "(uint%Id)%s", size, target);
			strcpy_s(target, opcodeSize, buffer);
		}
		return target;
	}

	char *convertToInt(char *target)
	{
		return _convertToInt(target, true);
	}

	char *convertToUInt(char *target)
	{
		return _convertToUInt(target, true);
	}

	char *castToInt(char *target)
	{
		return _convertToInt(target, false);
	}

	char *castToUInt(char *target)
	{
		return _convertToUInt(target, false);
	}

	// For a masked destination, DXBC source swizzles are indexed by the
	// physical destination lane.  A destination of .yzw therefore consumes
	// source/literal lanes y, z and w, not the compact x, y and z sequence.
	// Build each lane independently so aliases such as r0.yzw = imin(r0.yyzw,
	// l(0,31,15,23)) are read before the write and retain the literal mapping.
	string integerLaneOperand(const char *source, int lane, bool unsignedValue)
	{
		if (!strncmp(source, "l(", 2))
		{
			// integerImmediate supplied exact decimal DWORDs. Select their
			// text directly: applySwizzleLiteral parses through float and
			// would round integers larger than 2^24.
			string literal = source;
			size_t start = 2;
			if (literal.find(',') != string::npos)
				for (int component = 0; component < lane; ++component)
					start = literal.find(',', start) + 1;
			string value = literal.substr(start, literal.find_first_of(",)", start) - start);
			return unsignedValue ? "uint(" + value + ")" : value;
		}
		char left[8];
		char value[opcodeSize];
		sprintf_s(left, sizeof(left), ".%c", "xyzw"[lane]);
		strcpy_s(value, opcodeSize, source);
		applySwizzle(left, value, true);
		if (unsignedValue)
			bitcastToUInt(value);
		else
			bitcastToInt(value);
		return value;
	}

	bool isIntegerLiteral(const char *value)
	{
		if (!value || !*value)
			return false;
		char *end = NULL;
		strtol(value, &end, 0);
		return end && *end == 0;
	}

	string integerMinMaxExpression(const char *destination, const char *source0, const char *source1,
		bool unsignedValue, bool maximum)
	{
		const char *mask = strrchr(destination, '.');
		string lanes = mask ? mask + 1 : "xyzw";
		string lhs, rhs;

		for (size_t i = 0; i < lanes.size(); ++i)
		{
			const char *lane_position = strchr("xyzw", lanes[i]);
			if (!lane_position)
				continue;
			int lane = (int)(lane_position - "xyzw");
			if (!lhs.empty())
			{
				lhs += ", ";
				rhs += ", ";
			}
			lhs += integerLaneOperand(source0, lane, unsignedValue);
			rhs += integerLaneOperand(source1, lane, unsignedValue);
		}

		if (lanes.size() > 1)
		{
			string type = unsignedValue ? "uint" : "int";
			lhs = type + to_string(lanes.size()) + "(" + lhs + ")";
			rhs = type + to_string(lanes.size()) + "(" + rhs + ")";
		}

		return string(maximum ? "max(" : "min(") + lhs + ", " + rhs + ")";
	}

	bool singleTempComponent(const char *operand, string &key, int &register_number, char &component)
	{
		const char *dot = strchr(operand, '.');
		if (operand[0] != 'r' || !dot || !strchr("xyzw", dot[1]) || dot[2] != 0)
			return false;
		register_number = atoi(operand + 1);
		component = dot[1];
		key = operand;
		return true;
	}

	string integerAliasForOperand(const char *operand)
	{
		string value = operand;
		bool negative = false;
		if (!value.empty() && value[0] == '-')
		{
			negative = true;
			value.erase(0, 1);
		}

		map<string, string>::iterator exact = mIntegerAliases.find(value);
		if (exact != mIntegerAliases.end())
			return (negative ? "-" : "") + exact->second;
		exact = mIntegerReadAliases.find(value);
		if (exact != mIntegerReadAliases.end())
			return (negative ? "-" : "") + exact->second;

		size_t dot = value.find('.');
		if (dot == string::npos || value[0] != 'r')
			return "";
		string register_name = value.substr(0, dot);
		string components = value.substr(dot + 1);
		if (components.empty() || components.size() > 4)
			return "";

		string result;
		for (size_t i = 0; i < components.size(); ++i)
		{
			if (!strchr("xyzw", components[i]))
				return "";
			string key = register_name + "." + components.substr(i, 1);
			map<string, string>::iterator alias = mIntegerAliases.find(key);
			if (alias == mIntegerAliases.end())
			{
				alias = mIntegerReadAliases.find(key);
				if (alias == mIntegerReadAliases.end())
					return "";
			}
			if (!result.empty())
				result += ", ";
			result += alias->second;
		}
		if (components.size() > 1)
			result = "uint" + to_string(components.size()) + "(" + result + ")";
		if (negative)
			result = "-" + result;
		return result;
	}

	void preserveIntegerReadOperand(const char *operand)
	{
		if (!operand)
			return;

		const char *value = operand[0] == '-' ? operand + 1 : operand;
		if (value[0] != 'r')
			return;

		const char *dot = strchr(value, '.');
		string register_name = dot ? string(value, dot - value) : string(value);
		string components = dot ? dot + 1 : "xyzw";
		for (size_t i = 0; i < components.size(); ++i)
		{
			if (!strchr("xyzw", components[i]))
				continue;
			string key = register_name + "." + components.substr(i, 1);
			map<string, string>::iterator alias = mIntegerAliases.find(key);
			if (alias != mIntegerAliases.end())
				mIntegerReadAliases[key] = alias->second;
			if (mIntegerBitPatternComponents.find(key) != mIntegerBitPatternComponents.end())
				mIntegerReadBitPatternComponents.insert(key);
		}
	}

	// Texture Load coordinates are integer values.  DXBC temporary registers
	// still carry the integer bit pattern as floats, so an implicit HLSL float
	// to int conversion changes the coordinate instead of preserving it.
	void integerLoadCoordinate(char *target)
	{
		if (!target || (target[0] != 'r' && !(target[0] == '-' && target[1] == 'r')))
			return;

		const char *register_start = target[0] == '-' ? target + 1 : target;
		const char *dot = strchr(register_start, '.');
		if (!dot || !dot[1] || dot[1] == '0')
			return;

		string register_name(register_start, dot - register_start);
		string components = dot + 1;
		if (components.empty() || components.size() > 4)
			return;

		string values;
		for (size_t i = 0; i < components.size(); ++i)
		{
			char component = components[i];
			if (!strchr("xyzw", component))
				return;

			string key = register_name + "." + component;
			string value;
			map<string, string>::iterator alias = mIntegerAliases.find(key);
			if (alias != mIntegerAliases.end())
				value = alias->second;
			else if ((alias = mIntegerReadAliases.find(key)) != mIntegerReadAliases.end())
				value = alias->second;
			else if (mIntegerBitPatternComponents.find(key) != mIntegerBitPatternComponents.end() ||
				mIntegerReadBitPatternComponents.find(key) != mIntegerReadBitPatternComponents.end())
				value = "asint(" + key + ")";
			else
				// A temporary used as a DXBC Load coordinate is a typeless
				// register carrying the integer bit pattern, even when the
				// destination overlaps one of the coordinate components.
				value = "asint(" + key + ")";

			if (!values.empty())
				values += ", ";
			values += value;
		}

		string result = components.size() == 1
			? values
			: "int" + to_string(components.size()) + "(" + values + ")";
		strcpy_s(target, opcodeSize, result.c_str());
	}

	void invalidateIntegerAliases(const char *operand)
	{
		const char *dot = strchr(operand, '.');
		if (operand[0] != 'r')
			return;
		string register_name = dot ? string(operand, dot - operand) : string(operand);
		if (!dot)
		{
			for (map<string, string>::iterator i = mIntegerAliases.begin(); i != mIntegerAliases.end(); )
			{
				if (i->first.compare(0, register_name.size(), register_name) == 0)
					i = mIntegerAliases.erase(i);
				else
					++i;
			}
			return;
		}
		for (const char *component = dot + 1; *component; ++component)
		{
			if (strchr("xyzw", *component))
			{
				mIntegerAliases.erase(register_name + "." + string(1, *component));
				mIntegerBitPatternComponents.erase(register_name + "." + string(1, *component));
			}
		}
	}

	void markIntegerBitPatternComponents(const char *operand)
	{
		if (!operand || operand[0] != 'r')
			return;
		const char *dot = strchr(operand, '.');
		string register_name = dot ? string(operand, dot - operand) : string(operand);
		if (!dot)
		{
			for (const char *component = "xyzw"; *component; ++component)
				mIntegerBitPatternComponents.insert(register_name + "." + string(1, *component));
			return;
		}
		for (const char *component = dot + 1; *component; ++component)
			if (strchr("xyzw", *component))
				mIntegerBitPatternComponents.insert(register_name + "." + string(1, *component));
	}

	bool isIntegerBitPatternOperand(const char *operand) const
	{
		return mIntegerBitPatternComponents.find(operand) != mIntegerBitPatternComponents.end() ||
			mIntegerReadBitPatternComponents.find(operand) != mIntegerReadBitPatternComponents.end();
	}

	void markIntegerBitPatternLane(const char *target, int lane)
	{
		if (!target || target[0] != 'r' || lane < 0 || lane > 3)
			return;
		const char *dot = strchr(target, '.');
		string register_name = dot ? string(target, dot - target) : string(target);
		mIntegerBitPatternComponents.insert(register_name + "." + string(1, "xyzw"[lane]));
	}

	string declareIntegerAlias(const char *target, bool unsigned_value)
	{
		string key;
		int register_number;
		char component;
		if (!singleTempComponent(target, key, register_number, component))
			return "";
		string alias = "r" + to_string(register_number) + (unsigned_value ? "_uint_" : "_int_") + component;
		if (mDeclaredIntegerAliases.insert(alias).second)
		{
			string declaration = "  " + string(unsigned_value ? "uint " : "int ") + alias + ";\n";
			mOutput.insert(mOutput.begin() + mCodeStartPos, declaration.begin(), declaration.end());
			mCodeStartPos += declaration.size();
		}
		mIntegerAliases[key] = alias;
		return alias;
	}

	bool emitIntegerAssignment(const char *target, const char *expression, bool unsigned_value)
	{
		string alias = declareIntegerAlias(target, unsigned_value);
		if (alias.empty())
			return false;

		char buffer[opcodeSize * 2];
		char target_copy[opcodeSize];
		strcpy_s(target_copy, opcodeSize, target);
		sprintf(buffer, "  %s = %s;\n", alias.c_str(), expression);
		appendOutput(buffer);
		sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(target_copy), alias.c_str());
		appendOutput(buffer);
		return true;
	}

	void emitTextureLoad(char *target, int textureId, const char *expression)
	{
		const string &type = mTextureType[textureId];
		bool unsignedValue = type.find("<uint") != string::npos;
		bool integerValue = unsignedValue || type.find("<int") != string::npos;
		if (integerValue)
		{
			markIntegerBitPatternComponents(target);
			if (emitIntegerAssignment(target, expression, unsignedValue))
				return;
		}
		char assignment[opcodeSize * 2];
		if (integerValue)
			sprintf(assignment, "  %s = asfloat(%s);\n", writeTarget(target), expression);
		else
			sprintf(assignment, "  %s = %s;\n", writeTarget(target), expression);
		appendOutput(assignment);
	}

	// DXBC bitwise instructions operate on the raw 32-bit register contents.
	// The decompiler keeps temporary registers as float4 for compatibility with
	// the rest of the generated shader, so numeric casts are not sufficient here:
	// a comparison mask such as 0xffffffff must remain the NaN bit pattern when
	// it is stored in a float register.
	void bitcastToUInt(char *target)
	{
		char buffer[opcodeSize];
		if (isIntegerLiteral(target))
			return;
		string alias = integerAliasForOperand(target);
		if (!alias.empty())
		{
			_snprintf_s(target, opcodeSize, opcodeSize, "asuint(%s)", alias.c_str());
			return;
		}
		if (!strncmp(target, "uint", 4) ||
			!strncmp(target, "asint(", 6) || !strncmp(target, "asuint(", 7))
			return;
		_snprintf_s(buffer, opcodeSize, opcodeSize, "asuint(%s)", target);
		strcpy_s(target, opcodeSize, buffer);
	}

	// Integer DXBC results are kept in the float-shaped temporary register file
	// as their original 32-bit pattern.  Integer consumers must therefore use a
	// reinterpret cast, not a numeric float-to-int conversion.
	void bitcastToInt(char *target)
	{
		char buffer[opcodeSize];
		if (isIntegerLiteral(target))
			return;
		string alias = integerAliasForOperand(target);
		if (!alias.empty())
		{
			_snprintf_s(target, opcodeSize, opcodeSize, "asint(%s)", alias.c_str());
			return;
		}
		if (!strncmp(target, "int", 3) ||
			!strncmp(target, "asint(", 6) || !strncmp(target, "asuint(", 7))
			return;
		_snprintf_s(buffer, opcodeSize, opcodeSize, "asint(%s)", target);
		strcpy_s(target, opcodeSize, buffer);
	}

	// The boolean check routines had the problem that they only looked for the actual register
	// name, like r1, instead of including the component, like r1.x.  This fails in some cases
	// because another component can legally be used in between, which would not clear the 
	// boolean use case.
	// The fix here is to make the elements of the set include the swizzle components too.
	// It's not clear if the components can be broken into pieces by the compiler, like
	//  ge r0.zw
	//  and .. r0.z
	//  movc .. r0.w
	// But it makes no sense that an 'and .. r0.zw' would have one component boolean, and one not,
	// so rather than do them component by component, we'll use the whole operand.

	// Well, another example is a n "and r0.xyzw, r0.yyyy, r2.xyzw" where r0.y is set early. 
	// This requires at least single component entries in the mBooleanRegisters.

	// Comparison and bitwise instructions use this set to remember components
	// whose float-shaped storage contains a DXBC all-bits mask.  It is used by
	// control-flow and movc so those consumers test the mask as uint bits rather
	// than relying on floating-point NaN behavior.

	void addBoolean(char *arg)
	{
		string op = (arg[0] == '-') ? arg + 1 : arg;
		size_t dotspot = op.find('.');
		if (dotspot == string::npos)
		{
			mBooleanRegisters.insert(op);
			return;
		}

		string reg = op.substr(0, dotspot);
		for (size_t i = dotspot + 1; i < op.length(); i++)
		{
			mBooleanRegisters.insert(reg + '.' + op[i]);
		}
	}

	bool isBoolean(char *arg)
	{
		if (mBooleanRegisters.empty())
			return false;

		string op = (arg[0] == '-') ? arg + 1 : arg;
		size_t dotspot = op.find('.');
		string operand;

		if (dotspot == string::npos)
		{
			set<string>::iterator i = mBooleanRegisters.find(op);
			return i != mBooleanRegisters.end();
		}

		string reg = op.substr(0, dotspot);
		for (size_t i = dotspot + 1; i < op.length(); i++)
		{
			set<string>::iterator j = mBooleanRegisters.find(reg + '.' + op[i]);
			if (j != mBooleanRegisters.end())
				return true;									// Any single component found qualifies
		}

		return false;
	}

	void removeBoolean(char *arg)
	{
		if (mBooleanRegisters.empty())
			return;

		string op = arg[0] == '-' ? arg + 1 : arg;
		size_t dotspot = op.find('.');
		if (dotspot == string::npos)
		{
			mBooleanRegisters.erase(op);
			return;
		}

		string reg = op.substr(0, dotspot);
		for (size_t i = dotspot + 1; i < op.length(); i++)
		{
			mBooleanRegisters.erase(reg + '.' + op[i]);
		}
	}

	string conditionExpression(char *arg, bool testZero)
	{
		// DXBC tests the raw DWORD, including integers and comparison masks.
		bool isMask = isBoolean(arg) || arg[0] == 'r';
		string value = ci(arg);
		string alias = integerAliasForOperand(arg);
		if (!alias.empty())
		{
			value = "asuint(" + alias + ")";
			isMask = true;
		}
		else if (isMask)
			value = "asuint(" + value + ")";
		return "(" + value + (testZero ? " == " : " != ") + (isMask ? "0u" : "0") + ")";
	}


	char *fixImm(char *op, Operand &o)
	{
		// Check old value.
		if (o.eType == OPERAND_TYPE_IMMEDIATE32)
		{
			float oldValue;
			sscanf_s(op, "l(%e", &oldValue);
			if (!strncmp(op, "l(1.#INF00", strlen("l(1.#INF00")) || abs(oldValue - o.afImmediates[0]) < 0.1)
			{
				if (o.iNumComponents == 4)
					sprintf_s(op, opcodeSize, "l(%.9g,%.9g,%.9g,%.9g)", o.afImmediates[0], o.afImmediates[1], o.afImmediates[2], o.afImmediates[3]);
				else if (o.iNumComponents == 3)
					sprintf_s(op, opcodeSize, "l(%.9g,%.9g,%.9g)", o.afImmediates[0], o.afImmediates[1], o.afImmediates[2]);
				else if (o.iNumComponents == 2)
					sprintf_s(op, opcodeSize, "l(%.9g,%.9g)", o.afImmediates[0], o.afImmediates[1]);
				else if (o.iNumComponents == 1)
					sprintf_s(op, opcodeSize, "l(%.9g)", o.afImmediates[0]);
			}
		}
		return op;
	}

	char *integerImmediate(char *op, const Operand &operand)
	{
		if (operand.eType != OPERAND_TYPE_IMMEDIATE32)
			return op;
		string literal = "l(";
		for (int lane = 0; lane < operand.iNumComponents; ++lane)
		{
			int32_t bits;
			memcpy(&bits, &operand.afImmediates[lane], sizeof(bits));
			if (lane) literal += ",";
			literal += to_string(bits);
		}
		literal += ")";
		strcpy_s(op, opcodeSize, literal.c_str());
		return op;
	}

	void WritePatches()
	{
		bool stereoParamsWritten = false;
		const char *StereoDecl = "\n\n// Auto-fixed shader\nfloat4 stereo = StereoParams.Load(0);\nfloat separation = stereo.x;\nfloat convergence = stereo.y;";
		// float4 stereoScreenRes = StereoParams.Load(int3(2,0,0));\n  float4 stereoTune = StereoParams.Load(int3(1,0,0));";

		// Vertex shader patches
		if (!mShaderType.substr(0, 3).compare("vs_"))
		{
			bool isMono = false;
			bool screenToWorldMatrix1 = false, screenToWorldMatrix2 = false;
			string backProjectVector1, backProjectVector2;
			if (!G->BackProject_Vector1.empty())
				backProjectVector1 = G->BackProject_Vector1.substr(0, G->BackProject_Vector1.find_first_of(".,"));
			if (!G->BackProject_Vector2.empty())
				backProjectVector2 = G->BackProject_Vector2.substr(0, G->BackProject_Vector2.find_first_of(".,"));
			for (CBufferData::iterator i = mCBufferData.begin(); i != mCBufferData.end(); ++i)
			{
				if (!screenToWorldMatrix1 && i->second.Name == backProjectVector1)
					screenToWorldMatrix1 = true;
				if (!screenToWorldMatrix2 && i->second.Name == backProjectVector2)
					screenToWorldMatrix2 = true;
			}
			if (screenToWorldMatrix1 || screenToWorldMatrix2)
			{
				mOutput.push_back(0);

				char *screenToWorldPos;
				if (screenToWorldMatrix1)
				{
					backProjectVector1 += '.';
					screenToWorldPos = strstr(mOutput.data(), backProjectVector1.c_str());
				}
				else
				{
					backProjectVector2 += '.';
					screenToWorldPos = strstr(mOutput.data(), backProjectVector2.c_str());
				}
				char *viewProjectMatrix = strstr(mOutput.data(), "ViewProjectionMatrix.");
				if (screenToWorldPos)
				{
					// This is a deferred rendering vertex shader.
					isMono = true;
					// Add view direction out parameter.
					char *lastPos = 0;
					char *pos = mOutput.data();
					while (pos)
					{
						lastPos = pos;
						pos = strstr(pos + 12, " : TEXCOORD");
					}
					if (lastPos && lastPos != mOutput.data())
					{
						pos = strchr(lastPos, '\n');
						const char *viewDirectionDecl = "\nout float3 viewDirection : TEXCOORD31,";
						mOutput.insert(mOutput.begin() + (pos - mOutput.data()), viewDirectionDecl, viewDirectionDecl + strlen(viewDirectionDecl));
					}
					// Add view direction calculation.
					char buf[512];
					if (screenToWorldMatrix1)
						sprintf(buf, "  viewDirection = float3(%s);\n", G->BackProject_Vector1.c_str());
					else
						sprintf(buf, "  viewDirection = float3(%s);\n", G->BackProject_Vector2.c_str());
					mOutput.insert(mOutput.end() - 1, buf, buf + strlen(buf));
					mPatched = true;

					// If we have a projection, make mono.
					if (viewProjectMatrix)
					{
						vector<char>::iterator writePos = mOutput.end() - 1;
						if (*writePos != '\n') --writePos;
						mOutput.insert(writePos, StereoDecl, StereoDecl + strlen(StereoDecl));
						stereoParamsWritten = true;
						char buffer[256];
						sprintf(buffer, "%s.x -= separation * (%s.w - convergence);\n", mSV_Position.c_str(), mSV_Position.c_str());
						mOutput.insert(mOutput.end() - 1, buffer, buffer + strlen(buffer));
					}
				}
				mOutput.pop_back();
			}

			// Process copies of SV_Position.
			if (!isMono && G->fixSvPosition && mUsesProjection && !mSV_Position.empty())
			{
				map<string, string>::iterator positionValue = mOutputRegisterValues.find(mSV_Position);
				if (positionValue != mOutputRegisterValues.end())
				{
					size_t dotPos = positionValue->second.rfind('.');
					string rvalue = positionValue->second;
					if (dotPos > 0) rvalue = positionValue->second.substr(0, dotPos);
					// Search for same value on other outputs.
					for (map<string, string>::iterator i = mOutputRegisterValues.begin(); i != mOutputRegisterValues.end(); ++i)
					{
						// Ignore main output register
						if (!i->first.compare(mSV_Position)) continue;
						// Check for float 4 type.
						map<string, DataType>::iterator dataType = mOutputRegisterType.find(i->first);
						if (dataType == mOutputRegisterType.end() || dataType->second != DT_float4)
							continue;
						dotPos = i->second.rfind('.');
						string rvalue2 = i->second;
						if (dotPos > 0) rvalue2 = i->second.substr(0, dotPos);
						if (!rvalue2.compare(rvalue))
						{
							// Write params before return;.
							if (!stereoParamsWritten)
							{
								vector<char>::iterator writePos = mOutput.end() - 1;
								while (*writePos != '\n')
									--writePos;
								--writePos;
								while (*writePos != '\n')
									--writePos;
								mOutput.insert(writePos, StereoDecl, StereoDecl + strlen(StereoDecl));
								stereoParamsWritten = true;
							}

							// Back up to before the final return statement to output.
							vector<char>::iterator writePos = mOutput.end() - 1;
							while (*writePos != '\n')
								--writePos;
							--writePos;
							while (*writePos != '\n')
								--writePos;
							char buffer[256];
							string outputReg = i->first;
							if (outputReg.find('.') != string::npos) outputReg = outputReg.substr(0, outputReg.find('.'));
							sprintf(buffer, "\n%s.x += separation * (%s.w - convergence);", outputReg.c_str(), outputReg.c_str());
							mOutput.insert(writePos, buffer, buffer + strlen(buffer));
							mPatched = true;
						}
					}
				}
			}
			if (G->recompileVs) mPatched = true;
		}

		// Pixel shader patches.
		if (!mShaderType.substr(0, 3).compare("ps_"))
		{
			// Search for depth texture.
			bool wposAvailable = false;
			map<int, string>::iterator depthTexture;
			for (depthTexture = mTextureNames.begin(); depthTexture != mTextureNames.end(); ++depthTexture)
			{
				if (depthTexture->second == G->ZRepair_DepthTexture1)
					break;
			}
			if (depthTexture != mTextureNames.end())
			{
				long found = 0;
				for (CBufferData::iterator i = mCBufferData.begin(); i != mCBufferData.end(); ++i)
					for (unsigned int j = 0; j < G->ZRepair_Dependencies1.size(); ++j)
						if (i->second.Name == G->ZRepair_Dependencies1[j])
							found |= 1 << j;
				if (!G->ZRepair_Dependencies1.size() || found == (1 << G->ZRepair_Dependencies1.size()) - 1)
				{
					mOutput.push_back(0);
					// Search depth texture usage.
					sprintf(op1, " = %s.Sample", G->ZRepair_DepthTexture1.c_str());
					char *pos = strstr(mOutput.data(), op1);
					ptrdiff_t searchPos = 0;					// used as difference between pointers.
					while (pos)
					{
						char *bpos = pos;
						while (*--bpos != ' ');
						string regName(bpos + 1, pos);
						// constant expression?
						char *endPos = strchr(pos, ',') + 2;
						bool constantDeclaration = endPos[0] == 'v' && endPos[1] >= '0' && endPos[1] <= '9';
						endPos = strchr(endPos, '\n');
						while (*--endPos != ')'); ++endPos; pos += 3;
						string depthBufferStatement(pos, endPos);
						searchPos = endPos - mOutput.data();
						char buf[512];
						if (!wposAvailable)
						{
							sprintf(buf, "float4 zpos4 = %s;\n"
								"float zTex = zpos4.%c;\n"
								"float zpos = %s;\n"
								"float wpos = 1.0 / zpos;\n", depthBufferStatement.c_str(), G->ZRepair_DepthTextureReg1, G->ZRepair_ZPosCalc1.c_str());
						}
						else
						{
							sprintf(buf, "zpos4 = %s;\n"
								"zTex = zpos4.%c;\n"
								"zpos = %s;\n"
								"wpos = 1.0 / zpos;\n", depthBufferStatement.c_str(), G->ZRepair_DepthTextureReg1, G->ZRepair_ZPosCalc1.c_str());
						}
						if (constantDeclaration && !wposAvailable)
						{
							// Copy depth texture usage to top.
							//mCodeStartPos = mOutput.insert(mOutput.begin() + mCodeStartPos, buf, buf + strlen(buf)) - mOutput.begin();
							vector<char>::iterator iter = mOutput.insert(mOutput.begin() + mCodeStartPos, buf, buf + strlen(buf));
							mCodeStartPos = iter - mOutput.begin();
							mCodeStartPos += strlen(buf);
						}
						else if (!wposAvailable)
						{
							// Leave declaration where it is.
							while (*pos != '\n') --pos;
							//mCodeStartPos = mOutput.insert(mOutput.begin() + (pos + 1 - mOutput.data()), buf, buf + strlen(buf)) - mOutput.begin();
							vector<char>::iterator iter = mOutput.insert(mOutput.begin() + (pos + 1 - mOutput.data()), buf, buf + strlen(buf));
							mCodeStartPos = iter - mOutput.begin();
							mCodeStartPos += strlen(buf);
						}
						else
						{
							while (*pos != '\n') --pos;
							mOutput.insert(mOutput.begin() + (pos + 1 - mOutput.data()), buf, buf + strlen(buf));
						}
						searchPos += strlen(buf);
						wposAvailable = true;
						pos = strstr(mOutput.data() + searchPos, op1);
					}
					mOutput.pop_back();
				}
			}
			if (!wposAvailable)
			{
				for (depthTexture = mTextureNames.begin(); depthTexture != mTextureNames.end(); ++depthTexture)
				{
					if (depthTexture->second == G->ZRepair_DepthTexture2)
						break;
				}
				if (depthTexture != mTextureNames.end())
				{
					long found = 0;
					for (CBufferData::iterator i = mCBufferData.begin(); i != mCBufferData.end(); ++i)
						for (unsigned int j = 0; j < G->ZRepair_Dependencies2.size(); ++j)
							if (i->second.Name == G->ZRepair_Dependencies2[j])
								found |= 1 << j;
					if (!G->ZRepair_Dependencies2.size() || found == (1 << G->ZRepair_Dependencies2.size()) - 1)
					{
						mOutput.push_back(0);
						// Search depth texture usage.
						sprintf(op1, " = %s.Sample", G->ZRepair_DepthTexture2.c_str());
						char *pos = strstr(mOutput.data(), op1);
						if (pos)
						{
							char *bpos = pos;
							while (*--bpos != ' ');
							string regName(bpos + 1, pos);
							// constant expression?
							char *endPos = strchr(pos, ',') + 2;
							bool constantDeclaration = endPos[0] == 'v' && endPos[1] >= '0' && endPos[1] <= '9';
							endPos = strchr(endPos, '\n');
							while (*--endPos != ')'); ++endPos; pos += 3;
							string depthBufferStatement(pos, endPos);
							vector<char>::iterator wpos = mOutput.begin();
							wpos = mOutput.erase(wpos + (pos - mOutput.data()), wpos + (endPos - mOutput.data()));
							const char ZPOS_REG[] = "zpos4";
							wpos = mOutput.insert(wpos, ZPOS_REG, ZPOS_REG + strlen(ZPOS_REG));
							char buf[256];
							sprintf(buf, "float4 zpos4 = %s;\n"
								"float zTex = zpos4.%c;\n"
								"float zpos = %s;\n"
								"float wpos = 1.0 / zpos;\n", depthBufferStatement.c_str(), G->ZRepair_DepthTextureReg2, G->ZRepair_ZPosCalc2.c_str());

							// There are a whole series of fixes to the statements that are doing mOutput.insert() - mOutput.begin();
							// We would get a series of Asserts (vector mismatch), but only rarely, usually at starting a new game in AC3.  
							// The problem appers to be that mOutput.begin() was being calculated BEFORE the mOutput.insert(), which seems to
							// be the compiler optimizing.
							// The problem seems to be that the Insert could move the mOutput array altogether, and because the funny use of
							// vector<char> the vector winds up being closeset to a char* pointer.  With the pointer moved, the saved off
							// mOutput.begin() was invalid and would cause the debug Assert.
							// By calculating that AFTER the insert has finished, we can avoid this rare buffer movement.
							// I fixed every instance I could find of this insert-begin, not just the one that crashed.
							// Previous code left as example of expected behavior, and documentation for where changes happened.
							// It took two days to figure out, it's worth some comments.

							if (constantDeclaration)
							{
								// Copy depth texture usage to top.
								//mCodeStartPos = mOutput.insert(mOutput.begin() + mCodeStartPos, buf, buf + strlen(buf)) - mOutput.begin();
								vector<char>::iterator iter = mOutput.insert(mOutput.begin() + mCodeStartPos, buf, buf + strlen(buf));
								mCodeStartPos = iter - mOutput.begin();
								mCodeStartPos += strlen(buf);
							}
							else
							{
								// Leave declaration where it is.
								while (*wpos != '\n') --wpos;
								//mCodeStartPos = mOutput.insert(wpos + 1, buf, buf + strlen(buf)) - mOutput.begin();
								vector<char>::iterator iter = mOutput.insert(wpos + 1, buf, buf + strlen(buf));
								mCodeStartPos = iter - mOutput.begin();
								mCodeStartPos += strlen(buf);
							}
							wposAvailable = true;
						}
						mOutput.pop_back();
					}
				}
			}

			// Search for position texture.
			if (!wposAvailable)
			{
				map<int, string>::iterator positionTexture;
				for (positionTexture = mTextureNames.begin(); positionTexture != mTextureNames.end(); ++positionTexture)
				{
					if (positionTexture->second == G->ZRepair_PositionTexture)
						break;
				}
				if (positionTexture != mTextureNames.end())
				{
					mOutput.push_back(0);
					// Search position texture usage.
					sprintf(op1, " = %s.Sample", G->ZRepair_PositionTexture.c_str());
					char *pos = strstr(mOutput.data(), op1);
					if (pos)
					{
						char *bpos = pos;
						while (*--bpos != ' ');
						char buf[512];
						memcpy(buf, bpos + 1, pos - (bpos + 1));
						buf[pos - (bpos + 1)] = 0;
						applySwizzle(".xyz", buf);
						char calcStatement[256];
						sprintf(calcStatement, G->ZRepair_WorldPosCalc.c_str(), buf);
						sprintf(buf, "\nfloat3 worldPos = %s;"
							"\nfloat zpos = worldPos.z;"
							"\nfloat wpos = 1.0 / zpos;", calcStatement);
						pos = strchr(pos, '\n');

						//mCodeStartPos = mOutput.insert(mOutput.begin() + (pos - mOutput.data()), buf, buf + strlen(buf)) - mOutput.begin();
						vector<char>::iterator iter = mOutput.insert(mOutput.begin() + (pos - mOutput.data()), buf, buf + strlen(buf));
						mCodeStartPos = iter - mOutput.begin();
						mCodeStartPos += strlen(buf);
						wposAvailable = true;
					}
					mOutput.pop_back();
				}
			}

			// Add depth texture, as a last resort, but only if it's specified in d3dx.ini
			if (!wposAvailable && G->ZRepair_DepthBuffer)
			{
				const char *INJECT_HEADER = "float4 zpos4 = InjectedDepthTexture.Load((int3) injectedScreenPos.xyz);\n"
					"float zpos = zpos4.x - 1;\n"
					"float wpos = 1.0 / zpos;\n";
				// Copy depth texture usage to top.
				//mCodeStartPos = mOutput.insert(mOutput.begin() + mCodeStartPos, INJECT_HEADER, INJECT_HEADER + strlen(INJECT_HEADER)) - mOutput.begin();
				vector<char>::iterator iter = mOutput.insert(mOutput.begin() + mCodeStartPos, INJECT_HEADER, INJECT_HEADER + strlen(INJECT_HEADER));
				mCodeStartPos = iter - mOutput.begin();
				mCodeStartPos += strlen(INJECT_HEADER);

				// Add screen position parameter.
				char *pos = strstr(mOutput.data(), "void main(");

				// This section appeared to back up the pos pointer too far, and write the injection
				// text into the header instead of the var block.  It looks to me like this was intended
				// to be a while loop, and accidentally worked.

				// It seems to me that ALL output variables will start with SV_, so there is no point
				// in doing both 'out' and SV_.
				// There will always be at least one output parameter of SV_*
				// This is heavily modified from what it was.

				// This is now inserted as the first parameter, because any later can generate
				// an error in some shaders complaining that user SV vars must come before system SV vars.
				// It seems to be fine, if not exactly correct, to have it as first parameter.

				while (*++pos != '\n');
				assert(pos != NULL);

				const char *PARAM_HEADER = "\nfloat4 injectedScreenPos : SV_Position,";
				mOutput.insert(mOutput.begin() + (pos - mOutput.data()), PARAM_HEADER, PARAM_HEADER + strlen(PARAM_HEADER));
				mCodeStartPos += strlen(PARAM_HEADER);
				wposAvailable = true;
			}

			if (wposAvailable && G->InvTransforms.size())
			{
				CBufferData::iterator keyFind;
				for (keyFind = mCBufferData.begin(); keyFind != mCBufferData.end(); ++keyFind)
				{
					bool found = false;
					for (vector<string>::iterator j = G->InvTransforms.begin(); j != G->InvTransforms.end(); ++j)
						if (keyFind->second.Name == *j)
							found = true;
					if (found) break;
					if (!G->ObjectPos_ID1.empty() && keyFind->second.Name.find(G->ObjectPos_ID1) != string::npos)
						break;
					if (!G->ObjectPos_ID2.empty() && keyFind->second.Name.find(G->ObjectPos_ID2) != string::npos)
						break;
					if (!G->MatrixPos_ID1.empty() && keyFind->second.Name == G->MatrixPos_ID1)
						break;
				}
				if (keyFind != mCBufferData.end())
				{
					mOutput.push_back(0);
					if (!stereoParamsWritten)
					{
						if (mOutput[mCodeStartPos] != '\n') --mCodeStartPos;
						//mCodeStartPos = mOutput.insert(mOutput.begin() + mCodeStartPos, StereoDecl, StereoDecl + strlen(StereoDecl)) - mOutput.begin();
						vector<char>::iterator iter = mOutput.insert(mOutput.begin() + mCodeStartPos, StereoDecl, StereoDecl + strlen(StereoDecl));
						mCodeStartPos = iter - mOutput.begin();
						mCodeStartPos += strlen(StereoDecl);
						stereoParamsWritten = true;
					}

					for (vector<string>::iterator invT = G->InvTransforms.begin(); invT != G->InvTransforms.end(); ++invT)
					{
						char buf[128];
						sprintf(buf, " %s._m00", invT->c_str());
						char *pos = strstr(mOutput.data(), buf);
						if (pos)
						{
							pos += strlen(buf);
							char *mpos = pos;
							while (*mpos != '*' && *mpos != '\n') ++mpos;
							if (*mpos == '*')
							{
								char *bpos = mpos + 2;
								while (*bpos != ' ' && *bpos != ';') ++bpos;
								string regName(mpos + 2, bpos);
								size_t dotPos = regName.rfind('.');
								if (dotPos >= 0) regName = regName.substr(0, dotPos + 2);
								while (*mpos != '\n') --mpos;
								sprintf(buf, "\n%s -= separation * (wpos - convergence);", regName.c_str());
								mOutput.insert(mOutput.begin() + (mpos - mOutput.data()), buf, buf + strlen(buf));
								mPatched = true;
							}
							else
							{
								mpos = pos;
								while (*mpos != '(' && *mpos != '\n') --mpos;
								if (!strncmp(mpos - 3, "dot(", 4))
								{
									char *bpos = mpos + 1;
									while (*bpos != ' ' && *bpos != ',' && *bpos != ';') ++bpos;
									string regName(mpos + 1, bpos);
									size_t dotPos = regName.rfind('.');
									if (dotPos >= 0) regName = regName.substr(0, dotPos + 2);
									while (*mpos != '\n') --mpos;
									sprintf(buf, "\n%s -= separation * (wpos - convergence);", regName.c_str());
									mOutput.insert(mOutput.begin() + (mpos - mOutput.data()), buf, buf + strlen(buf));
									mPatched = true;
								}
							}
						}
					}
					bool parameterWritten = false;
					const char *ParamPos1 = "SV_Position0,";
					const char *ParamPos2 = "\n  out ";
					const char *NewParam = "\nfloat3 viewDirection : TEXCOORD31,";

					if (!G->ObjectPos_ID1.empty())
					{
						size_t offset = strstr(mOutput.data(), "void main(") - mOutput.data();	// pointer difference, but only used as offset.
						while (offset < mOutput.size())
						{
							char *pos = strstr(mOutput.data() + offset, G->ObjectPos_ID1.c_str());
							if (!pos) break;
							pos += G->ObjectPos_ID1.length();
							offset = pos - mOutput.data();
							if (*pos == '[') pos = strchr(pos, ']') + 1;
							if ((pos[1] == 'x' || pos[1] == 'y' || pos[1] == 'z') &&
								(pos[2] == 'x' || pos[2] == 'y' || pos[2] == 'z') &&
								(pos[3] == 'x' || pos[3] == 'y' || pos[3] == 'z'))
							{
								char *lightPosDecl = pos + 1;
								while (*--pos != '=');
								char *bpos = pos;
								while (*--bpos != '\n');
								size_t size = (pos - 1) - (bpos + 3);
								memcpy(op1, bpos + 3, size);
								op1[size] = 0;

								strcpy(op2, op1); applySwizzle(".x", op2);
								strcpy(op3, op1); applySwizzle(".y", op3);
								strcpy(op4, op1); applySwizzle(".z", op4);
								char buf[512];
								if (G->ObjectPos_MUL1.empty())
									G->ObjectPos_MUL1 = string("1,1,1");
								sprintf(buf, "\nfloat3 stereoPos%dMul = float3(%s);"
									"\n%s += viewDirection.%c * separation * (wpos - convergence) * stereoPos%dMul.x;"
									"\n%s += viewDirection.%c * separation * (wpos - convergence) * stereoPos%dMul.y;"
									"\n%s += viewDirection.%c * separation * (wpos - convergence) * stereoPos%dMul.z;",
									uuidVar, G->ObjectPos_MUL1.c_str(),
									op2, lightPosDecl[0], uuidVar,
									op3, lightPosDecl[1], uuidVar,
									op4, lightPosDecl[2], uuidVar);
								++uuidVar;
								pos = strchr(pos, '\n');
								mOutput.insert(mOutput.begin() + (pos - mOutput.data()), buf, buf + strlen(buf));
								offset += strlen(buf);
								mPatched = true;
								if (!parameterWritten)
								{
									char *posParam1 = strstr(mOutput.data(), ParamPos1);
									char *posParam2 = strstr(mOutput.data(), ParamPos2);
									char *posParam = posParam1 ? posParam1 : posParam2;
									while (*posParam != '\n') --posParam;
									mOutput.insert(mOutput.begin() + (posParam - mOutput.data()), NewParam, NewParam + strlen(NewParam));
									offset += strlen(NewParam);
									parameterWritten = true;
								}
							}
						}
					}

					if (!G->ObjectPos_ID2.empty())
					{
						size_t offset = strstr(mOutput.data(), "void main(") - mOutput.data();	// pointer difference, but only used as offset.
						while (offset < mOutput.size())
						{
							char *pos = strstr(mOutput.data() + offset, G->ObjectPos_ID2.c_str());
							if (!pos) break;
							pos += G->ObjectPos_ID2.length();
							offset = pos - mOutput.data();
							if (*pos == '[') pos = strchr(pos, ']') + 1;
							if ((pos[1] == 'x' || pos[1] == 'y' || pos[1] == 'z') &&
								(pos[2] == 'x' || pos[2] == 'y' || pos[2] == 'z') &&
								(pos[3] == 'x' || pos[3] == 'y' || pos[3] == 'z'))
							{
								char *spotPosDecl = pos + 1;
								while (*--pos != '=');
								char *bpos = pos;
								while (*--bpos != '\n');
								size_t size = (pos - 1) - (bpos + 3);
								memcpy(op1, bpos + 3, size);
								op1[size] = 0;

								strcpy(op2, op1); applySwizzle(".x", op2);
								strcpy(op3, op1); applySwizzle(".y", op3);
								strcpy(op4, op1); applySwizzle(".z", op4);
								char buf[512];
								if (G->ObjectPos_MUL2.empty())
									G->ObjectPos_MUL2 = string("1,1,1");
								sprintf(buf, "\nfloat3 stereoPos%dMul = float3(%s);"
									"\n%s += viewDirection.%c * separation * (wpos - convergence) * stereoPos%dMul.x;"
									"\n%s += viewDirection.%c * separation * (wpos - convergence) * stereoPos%dMul.y;"
									"\n%s += viewDirection.%c * separation * (wpos - convergence) * stereoPos%dMul.z;",
									uuidVar, G->ObjectPos_MUL2.c_str(),
									op2, spotPosDecl[0], uuidVar,
									op3, spotPosDecl[1], uuidVar,
									op4, spotPosDecl[2], uuidVar);
								++uuidVar;
								pos = strchr(pos, '\n');
								mOutput.insert(mOutput.begin() + (pos - mOutput.data()), buf, buf + strlen(buf));
								offset += strlen(buf);
								mPatched = true;
								if (!parameterWritten)
								{
									char *posParam1 = strstr(mOutput.data(), ParamPos1);
									char *posParam2 = strstr(mOutput.data(), ParamPos2);
									char *posParam = posParam1 ? posParam1 : posParam2;
									while (*posParam != '\n') --posParam;
									mOutput.insert(mOutput.begin() + (posParam - mOutput.data()), NewParam, NewParam + strlen(NewParam));
									offset += strlen(NewParam);
									parameterWritten = true;
								}
							}
						}
					}

					if (!G->MatrixPos_ID1.empty())
					{
						string ShadowPos1 = G->MatrixPos_ID1 + "._m00_m10_m20_m30 * ";
						string ShadowPos2 = G->MatrixPos_ID1 + "._m01_m11_m21_m31 * ";
						string ShadowPos3 = G->MatrixPos_ID1 + "._m02_m12_m22_m32 * ";
						char *pos1 = strstr(mOutput.data(), ShadowPos1.c_str());
						char *pos2 = strstr(mOutput.data(), ShadowPos2.c_str());
						char *pos3 = strstr(mOutput.data(), ShadowPos3.c_str());
						if (pos1 && pos2 && pos3)
						{
							string regName1(pos1 + ShadowPos1.length(), strchr(pos1 + ShadowPos1.length(), '.') + 2);
							string regName2(pos2 + ShadowPos2.length(), strchr(pos2 + ShadowPos2.length(), '.') + 2);
							string regName3(pos3 + ShadowPos3.length(), strchr(pos3 + ShadowPos3.length(), '.') + 2);
							char *pos = std::min(std::min(pos1, pos2), pos3);
							while (*--pos != '\n');
							char buf[512];
							if (G->MatrixPos_MUL1.empty())
								G->MatrixPos_MUL1 = string("1,1,1");
							_snprintf_s(buf, 512, 512, "\nfloat3 stereoMat%dMul = float3(%s);"
								"\n%s -= viewDirection.x * separation * (wpos - convergence) * stereoMat%dMul.x;"
								"\n%s -= viewDirection.y * separation * (wpos - convergence) * stereoMat%dMul.y;"
								"\n%s -= viewDirection.z * separation * (wpos - convergence) * stereoMat%dMul.z;",
								uuidVar, G->MatrixPos_MUL1.c_str(),
								regName1.c_str(), uuidVar,
								regName2.c_str(), uuidVar,
								regName3.c_str(), uuidVar);
							++uuidVar;
							mOutput.insert(mOutput.begin() + (pos - mOutput.data()), buf, buf + strlen(buf));
							mPatched = true;

							if (!parameterWritten)
							{
								char *posParam1 = strstr(mOutput.data(), ParamPos1);
								char *posParam2 = strstr(mOutput.data(), ParamPos2);
								char *posParam = posParam1 ? posParam1 : posParam2;
								if (posParam != NULL)
								{
									while (*posParam != '\n') --posParam;
									mOutput.insert(mOutput.begin() + (posParam - mOutput.data()), NewParam, NewParam + strlen(NewParam));
									parameterWritten = true;
								}
							}
						}
					}

					mOutput.pop_back();
				}
			}

		}
	}

	// Common routine for handling the no header info case for declarations.
	// This is grim, because we add the '4' to the format by default, as we
	// cannot determine the actual size of a declaration without reflection data.
	// It will mostly work, but generate hand-fix errors at times.
	void CreateRawFormat(string texType, int bufIndex)
	{
		char buffer[128];
		char format[16];

		sprintf(buffer, "t%d", bufIndex);
		mTextureNames[bufIndex] = buffer;

		sscanf_s(op1, "(%[^,]", format, 16);	// Match first xx of (xx,xx,xx,xx)
		if (!strcmp(format, "sint")) strcpy_s(format, sizeof(format), "int");
		string form4 = string(format) + "4";	// Grim. Known to fail sometimes.
		mTextureType[bufIndex] = texType + "<" + form4 + ">";

		sprintf(buffer, "%s t%d : register(t%d);\n\n", mTextureType[bufIndex].c_str(), bufIndex, bufIndex);
		mOutput.insert(mOutput.begin(), buffer, buffer + strlen(buffer));
		mCodeStartPos += strlen(buffer);
	}

	// General output routine while decoding shader, to automatically apply indenting to HLSL code.
	// When we see a '{' or '}' we'll increase or decrease the indent.
	void appendOutput(char* line)
	{
		bool open = (strchr(line, '{') != NULL);
		bool close = (strchr(line, '}') != NULL);

		if (close)
			nestCount--;
		for (int i = 0; i < nestCount; i++)
		{
			mOutput.insert(mOutput.end(), indent, indent + strlen(indent));
		}
		if (open)
			nestCount++;

		mOutput.insert(mOutput.end(), line, line + strlen(line));
	}

	static const char * offset2swiz(DataType type, int offset)
	{
		// For StructuredBuffers, where the swizzle is really an offset modifier
		switch (type) {
			case DT_float4x4: case DT_float4x3: case DT_float4x2: case DT_float4x1:
			case DT_half4x4: case DT_half4x3: case DT_half4x2: case DT_half4x1:
			case DT_uint4x4: case DT_uint4x3: case DT_uint4x2: case DT_uint4x1:
			case DT_int4x4: case DT_int4x3: case DT_int4x2: case DT_int4x1:
			case DT_bool4x4: case DT_bool4x3: case DT_bool4x2: case DT_bool4x1:
				switch (offset) {
					case  0: return "_m00"; case  4: return "_m10"; case  8: return "_m20"; case 12: return "_m30";
					case 16: return "_m01"; case 20: return "_m11"; case 24: return "_m21"; case 28: return "_m31";
					case 32: return "_m02"; case 36: return "_m12"; case 40: return "_m22"; case 44: return "_m32";
					case 48: return "_m03"; case 52: return "_m13"; case 56: return "_m23"; case 60: return "_m33";
					default: return "_m??";
				}
			case DT_float3x4: case DT_float3x3: case DT_float3x2: case DT_float3x1:
			case DT_half3x4: case DT_half3x3: case DT_half3x2: case DT_half3x1:
			case DT_uint3x4: case DT_uint3x3: case DT_uint3x2: case DT_uint3x1:
			case DT_int3x4: case DT_int3x3: case DT_int3x2: case DT_int3x1:
			case DT_bool3x4: case DT_bool3x3: case DT_bool3x2: case DT_bool3x1:
				switch (offset) {
					case  0: return "_m00"; case  4: return "_m10"; case  8: return "_m20";
					case 12: return "_m01"; case 16: return "_m11"; case 20: return "_m21";
					case 24: return "_m02"; case 28: return "_m12"; case 32: return "_m22";
					case 36: return "_m03"; case 40: return "_m13"; case 44: return "_m23";
					default: return "_m??";
				}
			case DT_float2x4: case DT_float2x3: case DT_float2x2: case DT_float2x1:
			case DT_half2x4: case DT_half2x3: case DT_half2x2: case DT_half2x1:
			case DT_uint2x4: case DT_uint2x3: case DT_uint2x2: case DT_uint2x1:
			case DT_int2x4: case DT_int2x3: case DT_int2x2: case DT_int2x1:
			case DT_bool2x4: case DT_bool2x3: case DT_bool2x2: case DT_bool2x1:
				switch (offset) {
					case  0: return "_m00"; case  4: return "_m10";
					case  8: return "_m01"; case 12: return "_m11";
					case 16: return "_m02"; case 20: return "_m12";
					case 24: return "_m03"; case 28: return "_m13";
					default: return "_m??";
				}
			case DT_float1x4: case DT_float1x3: case DT_float1x2: case DT_float1x1:
			case DT_half1x4: case DT_half1x3: case DT_half1x2: case DT_half1x1:
			case DT_uint1x4: case DT_uint1x3: case DT_uint1x2: case DT_uint1x1:
			case DT_int1x4: case DT_int1x3: case DT_int1x2: case DT_int1x1:
			case DT_bool1x4: case DT_bool1x3: case DT_bool1x2: case DT_bool1x1:
				switch (offset) {
					case  0: return "_m00";
					case  4: return "_m01";
					case  8: return "_m02";
					case 12: return "_m03";
					default: return "_m0?";
				}
			default:
				switch (offset) {
					case  0: return "x";
					case  4: return "y";
					case  8: return "z";
					case 12: return "w";
					default: return "?";
				}
		}
	}

	static const char * shadervar_offset2swiz(ShaderVarType *var, int offset)
	{
		if (!var)
			return "<NULL>";

		switch (var->Class) {
			case SVC_SCALAR:
				return "";
			case SVC_VECTOR:
				switch (offset) {
					case  0: return "x";
					case  4: return "y";
					case  8: return "z";
					case 12: return "w";
					default: return "?";
				}
				break;
			case SVC_MATRIX_ROWS:
			case SVC_MATRIX_COLUMNS:
				switch (var->Rows) {
					case 4:
						switch (offset) {
							case  0: return "_m00"; case  4: return "_m10"; case  8: return "_m20"; case 12: return "_m30";
							case 16: return "_m01"; case 20: return "_m11"; case 24: return "_m21"; case 28: return "_m31";
							case 32: return "_m02"; case 36: return "_m12"; case 40: return "_m22"; case 44: return "_m32";
							case 48: return "_m03"; case 52: return "_m13"; case 56: return "_m23"; case 60: return "_m33";
						}
						return "_m??";
					case 3:
						switch (offset) {
							case  0: return "_m00"; case  4: return "_m10"; case  8: return "_m20";
							case 12: return "_m01"; case 16: return "_m11"; case 20: return "_m21";
							case 24: return "_m02"; case 28: return "_m12"; case 32: return "_m22";
							case 36: return "_m03"; case 40: return "_m13"; case 44: return "_m23";
						}
						return "_m??";
					case 2:
						switch (offset) {
							case  0: return "_m00"; case  4: return "_m10";
							case  8: return "_m01"; case 12: return "_m11";
							case 16: return "_m02"; case 20: return "_m12";
							case 24: return "_m03"; case 28: return "_m13";
						}
						return "_m??";
					case 1:
						switch (offset) {
							case  0: return "_m00";
							case  4: return "_m01";
							case  8: return "_m02";
							case 12: return "_m03";
						}
						return "_m0?";
				}
			break;
		}

		return "";
	}

	static std::string shadervar_name(ShaderVarType *var, uint32_t offset)
	{
		std::string ret;
		uint32_t var_size, elem_size;
		uint32_t index;
		const char *swiz;

		if (!var || !var->Name.compare("$Element"))
			return "";

		if (var->ParentCount) {
			ret = shadervar_name(var->Parent, offset);
			if (ret.size())
				ret += ".";
		}

		ret += var->Name;

		var_size = ShaderVarSize(var, &elem_size);
		// Reflection reports a single-element field as Elements == 1, but
		// its HLSL declaration is a scalar ("float val"), not "float val[1]".
		// Indexing it as val[0] makes the regenerated shader invalid.
		if (var->Elements > 1) {
			// The index GetShaderVarFromOffset returns is crap, calculate it ourselves:
			index = (offset - var->Offset) / elem_size;
			ret += "[" + std::to_string(index) + "]";
		}

		if (offset - var->Offset < var_size) {
			swiz = shadervar_offset2swiz(var, (offset - var->Offset) % elem_size);
			if (swiz[0])
				ret += "." + std::string(swiz);
		}

		return ret;
	}

	bool IsStructuredBufferIntegerOffset(const string &type_name, int byte_offset)
	{
		map<string, set<int> >::iterator integer_type = mStructuredBufferIntegerOffsets.find(type_name);
		if (integer_type == mStructuredBufferIntegerOffsets.end())
			return false;

		map<string, vector<pair<int, int> > >::iterator ranges = mStructuredBufferFieldRanges.find(type_name);
		if (ranges == mStructuredBufferFieldRanges.end())
			return integer_type->second.find(byte_offset) != integer_type->second.end();

		for (size_t i = 0; i < ranges->second.size(); ++i)
		{
			int field_offset = ranges->second[i].first;
			int field_end = field_offset + ranges->second[i].second;
			if (byte_offset < field_offset || byte_offset >= field_end)
				continue;
			for (set<int>::iterator offset = integer_type->second.begin(); offset != integer_type->second.end(); ++offset)
			{
				if (*offset >= field_offset && *offset < field_end)
					return true;
			}
			return false;
		}
		return false;
	}

	bool translate_structured_var(Shader *shader, const char *c, size_t &pos, size_t &size, Instruction *instr,
			std::string ret[4], bool *combined, char *idx, char *off, char *reg, Operand *texture, int swiz_offsets[4])
	{
		Operand dst0 = instr->asOperands[0];
		ResourceGroup group = (ResourceGroup)-1;
		ResourceBinding *bindInfo;
		char buffer[512];

		applySwizzle(".x", idx);
		applySwizzle(".x", off);
		if (isIntegerBitPatternOperand(idx) || !integerAliasForOperand(idx).empty())
			bitcastToUInt(idx);

		// Resolve the old address before killing the actual destination.
		// The text parser's op1 may be "stride=..." for indexable loads.
		if (instr->eOpcode == OPCODE_LD_STRUCTURED && dst0.eType == OPERAND_TYPE_TEMP)
		{
			string destination = "r" + to_string(dst0.ui32RegisterNumber) + ".";
			for (int lane = 0; lane < 4; ++lane)
				if (dst0.ui32CompMask & (1u << lane)) destination += "xyzw"[lane];
			invalidateIntegerAliases(destination.c_str());
			// Even an unreflected float field is a raw DWORD when used by
			// a subsequent integer consumer or resource address.
			markIntegerBitPatternComponents(destination.c_str());
		}

		*combined = false;

		if (reg[0] == 't')
			group = RGROUP_TEXTURE;
		else if (reg[0] == 'u')
			group = RGROUP_UAV;
		// else 'g' = compute shader thread group shared memory, which will never have reflection info

		if (group != (ResourceGroup)-1 && GetResourceFromBindingPoint(group, texture->ui32RegisterNumber, shader->sInfo, &bindInfo))
		{
			map<string, string>::iterator struct_type_i;

			struct_type_i = mStructuredBufferTypes.find(bindInfo->Name);
			if (struct_type_i == mStructuredBufferTypes.end()) {
				sprintf(buffer, "// BUG: Cannot locate struct type:\n");
				appendOutput(buffer);
				ASMLineOut(c, pos, size);
				return false;
			}

			if (mStructuredBufferUsedNames.find(struct_type_i->second) != mStructuredBufferUsedNames.end())
			{
				int swiz_offset = 0;

				if (sscanf_s(off, "%d", &swiz_offset) == 1)
				{
					// Static offset:
					ConstantBuffer *bufInfo = NULL;
					GetConstantBufferFromBindingPoint(group, texture->ui32RegisterNumber, shader->sInfo, &bufInfo);
					if (!bufInfo) {
						sprintf(buffer, "// BUG: Cannot locate struct layout:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						return false;
					}

					for (uint32_t component = 0; component < 4; component++)
					{
						ShaderVarType *var = NULL;
						int32_t byte_offset = swiz_offsets[component] + swiz_offset;
						uint32_t swiz = byte_offset % 16 / 4;
						int32_t index = -1;
						int32_t rebase = -1;
						std::string var_txt;

						if (!(dst0.ui32CompMask & (1 << component)))
							continue;

						GetShaderVarFromOffset(byte_offset / 16, &swiz, bufInfo, &var, &index, &rebase);
						if (!var) {
							sprintf(buffer, "// BUG: Cannot locate variable in structure:\n");
							appendOutput(buffer);
							ASMLineOut(c, pos, size);
							return false;
						}

						var_txt = shadervar_name(var, byte_offset);

						sprintf(buffer, "%s[%s].%s",
								bindInfo->Name.c_str(),
								ci(idx).c_str(),
								var_txt.c_str());
						if (IsStructuredBufferIntegerOffset(struct_type_i->second, byte_offset))
						{
							string raw_value = buffer;
							ret[component] = "asfloat(" + raw_value + ")";
							if (dst0.eType == OPERAND_TYPE_TEMP)
							{
								string target = "r" + to_string(dst0.ui32RegisterNumber);
								target += ".";
								target += "xyzw"[component];
								string alias = declareIntegerAlias(target.c_str(), true);
								if (!alias.empty())
								{
									sprintf(buffer, "  %s = asuint(%s);\n", alias.c_str(), raw_value.c_str());
									appendOutput(buffer);
									ret[component] = "asfloat(" + alias + ")";
								}
								markIntegerBitPatternLane(target.c_str(), component);
							}
						}
						else
							ret[component] = buffer;
					}
					return true;
				} else {
					sprintf(buffer, "// Structured buffer using dynamic offset (needs manual fix):\n");
					appendOutput(buffer);
					ASMLineOut(c, pos, size);
					return false;
				}
			}
			else
			{
				// This StructuredBuffer is using a primitive type rather
				// than a structure (e.g. StructuredBuffer<float4> foo).
				DataType struct_type = TranslateType(struct_type_i->second.c_str());
				int swiz_offset = 0;

				if (sscanf_s(off, "%d", &swiz_offset) == 1) {
					// Static offset:
					for (int component = 0; component < 4; component++)
						swiz_offsets[component] += swiz_offset;
					sprintf(buffer, "%s[%s].%s%s%s%s",
							bindInfo->Name.c_str(), ci(idx).c_str(),
							(dst0.ui32CompMask & 0x1 ? offset2swiz(struct_type, swiz_offsets[0]) : ""),
							(dst0.ui32CompMask & 0x2 ? offset2swiz(struct_type, swiz_offsets[1]) : ""),
							(dst0.ui32CompMask & 0x4 ? offset2swiz(struct_type, swiz_offsets[2]) : ""),
							(dst0.ui32CompMask & 0x8 ? offset2swiz(struct_type, swiz_offsets[3]) : ""));
				} else {
					// Dynamic offset, use [] syntax:
					if (strcmp(strchr(reg, '.'), ".x")) {
						sprintf(buffer, "// Unexpected swizzle used with dynamic offset (needs manual fix):\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						return false;
					}
					sprintf(buffer, "%s[%s][%s/4]",
							bindInfo->Name.c_str(), ci(idx).c_str(), ci(off).c_str());
				}
				// Returning all components combined together:
				*combined = true;
				ret[0] = buffer;
				return true;
			}
		}
		else
		{
			// Missing reflection information - we have to use our fake
			// type information instead. Our fake type information is
			// an array of floats for the greatest compatibility with
			// any possible stride value that StructuredBuffers may posess,
			// but that means we have to calculate each component in the mask
			// separately, adjusting the offset based on the swizzle. The caller
			// recombines these expressions into a floatN(x,y,z,w) assignment.
			// We can't fix up types that
			// aren't floats here, because we won't know what types they
			// are until they are used - ideally we should switch to a
			// model that uses asfloat/asint where non-floats are used
			// to treat HLSL variables closer to typeless DX registers.
			stripMask(reg);
			for (int component = 0; component < 4; component++) {
				if (!(dst0.ui32CompMask & (1 << component)))
					continue;
				// The swizzle is a bit more complicated than the mask here,
				// because it represents extra 32bit offsets in the structure,
				// which is one whole index in the "val" array in our fake type.
				char *swiz_offset = "";
				switch (swiz_offsets[component]) {
					case  0: break;
					case  4: swiz_offset = "+1"; break;
					case  8: swiz_offset = "+2"; break;
					case 12: swiz_offset = "+3"; break;
					default: swiz_offset = "+?"; break;
				}
				// Writing it like this should work for both dynamic and static
				// offsets. We could pre-compute static offsets to clean up the
				// output, but since we've lost the swizzle by using fake types
				// it may actually be more informative to use this way:
				sprintf(buffer, "%s[%s].val[%s/4%s]",
						reg,
						ci(idx).c_str(),
						ci(off).c_str(),
						swiz_offset);
				ret[component] = buffer;
			}
			return true;
		}
	}

	void parse_ld_structured(Shader *shader, const char *c, size_t &pos, size_t &size, Instruction *instr)
	{
		std::string translated[4];
		char buffer[512];
		bool combined;

		// New variant found in Mordor.  Example:
		//   gInstanceBuffer                   texture  struct         r/o    0        1
		//   dcl_resource_structured t0, 16
		//   ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r1.xyzw, r0.x, l(0), t0.xyzw
		// becomes:
		//   StructuredBuffer<float4> gInstanceBuffer : register(t0);
		//   ...
		//   float4 c0 = gInstanceBuffer[worldMatrixOffset];

		// Example from Mordor, with bizarre struct offsets:
		// struct BufferSrc
		// {
		//  float3 vposition;              // offset:    0
		//  float3 vvelocity;              // offset:   12
		//  float ftime;                   // offset:   24
		//  float fuserdata;               // offset:   28
		// };                              // offset:    0 size:    32
		//
		// StructuredBuffer<BufferSrc> BufferSrc_SB : register(t0);
		//
		// Working fxc code (unrolled is necessary):
		// ld_structured_indexable(structured_buffer, stride=32)(mixed,mixed,mixed,mixed) r3.xyzw, v0.x, l(16), t0.xyzw
		//  r3.x = BufferSrc_SB[v0.x].vvelocity.y;
		//  r3.y = BufferSrc_SB[v0.x].vvelocity.z;
		//  r3.z = BufferSrc_SB[v0.x].ftime.x;
		//  r3.w = BufferSrc_SB[v0.x].fuserdata.x;

		// Since this has no prior code, and the text based parser fails on this complicated command, we are switching
		// to using the structure from the James-Jones decoder.
		// http://msdn.microsoft.com/en-us/library/windows/desktop/hh447157(v=vs.85).aspx

		// Shader model 4: ld_structured dst, index, offset, register
		// Shader model 5: ld_structured_indexable(structured_buffer, stride=N) dst, index, offset, register
		// That extra space throws out the opN variables, so we need
		// to check which it is.
		char *dst = op1, *idx = op2, *off = op3, *reg = op4;
		if (!strncmp(op1, "stride", 6))
			dst = op2, idx = op3, off = op4, reg = op5; // Note comma operator
		Operand dst0 = instr->asOperands[0];
		Operand texture = instr->asOperands[3];

		remapTarget(dst);
		applySwizzle(dst, reg);

		// The swizzle represents extra 32bit offsets within the structure:
		int swiz_offsets[4] = {0, 4, 8, 12};
		for (int component = 0; component < 4; component++) {
			switch (texture.aui32Swizzle[component]) {
				case OPERAND_4_COMPONENT_X: swiz_offsets[component] = 0; break;
				case OPERAND_4_COMPONENT_Y: swiz_offsets[component] = 4; break;
				case OPERAND_4_COMPONENT_Z: swiz_offsets[component] = 8; break;
				case OPERAND_4_COMPONENT_W: swiz_offsets[component] = 12; break;
			}
		}

		if (translate_structured_var(shader, c, pos, size, instr, translated, &combined, idx, off, reg, &texture, swiz_offsets)) {
			if (combined) {
				sprintf(buffer, "  %s = %s;\n", writeTarget(dst), translated[0].c_str());
				appendOutput(buffer);
			} else if (!writeStructuredComponents(dst, translated, dst0.ui32CompMask)) {
				DeferredWrite result = deferWrite(dst);
				stripMask(dst);
				for (int component = 0; component < 4; component++) {
					if (!(dst0.ui32CompMask & (1 << component)))
						continue;
					sprintf(buffer, "  %s.%c = %s;\n",
							result.temporary.c_str(),
							component == 3 ? 'w' : 'x' + component,
							translated[component].c_str());
					appendOutput(buffer);
				}
				commitWrite(result);
			}
		}

		removeBoolean(op1);
	}

	void parse_store_structured(Shader *shader, const char *c, size_t &pos, size_t &size, Instruction *instr)
	{
		std::string translated[4];
		char buffer[512];
		bool combined;

		// store_structured u1.x, v0.x, l(0), v1.x
		char *dst = op1, *idx = op2, *off = op3, *src = op4;
		Operand dst0 = instr->asOperands[0];
		Operand src0 = instr->asOperands[3];

		remapTarget(dst);
		int swiz_offsets[4] = {0, 4, 8, 12};

		if (translate_structured_var(shader, c, pos, size, instr, translated, &combined, idx, off, dst, &dst0, swiz_offsets)) {
			if (combined) {
				applySwizzle(dst, src);
				sprintf(buffer, "  %s = %s;\n", translated[0].c_str(), ci(src).c_str());
				appendOutput(buffer);
			} else {
				for (int component = 0; component < 4; component++) {
					if (!(dst0.ui32CompMask & (1 << component)))
						continue;

					strcpy(op5, src); fixImm(op5, src0);
					switch (component) {
						case 0: applySwizzle(".x", op5); break;
						case 1: applySwizzle(".y", op5); break;
						case 2: applySwizzle(".z", op5); break;
						case 3: applySwizzle(".w", op5); break;
					}

					sprintf(buffer, "  %s = %s;\n", translated[component].c_str(), ci(op5).c_str());
					appendOutput(buffer);
				}
			}
		}
	}

	//dx9
	//get component from Instruction
	string GetComponentStrFromInstruction(Instruction * instr, int opIndex)
	{
		assert(instr != NULL);
		char * componentX = "x";
		char * componentY = "y";
		char * componentZ = "z";
		char * componentW = "w";
		char * component[] = { componentX, componentY, componentZ, componentW };


		char buff[opcodeSize];
		buff[0] = 0;

		if (instr->asOperands[opIndex].eSelMode == OPERAND_4_COMPONENT_MASK_MODE)
		{
			if (instr->asOperands[opIndex].ui32CompMask & OPERAND_4_COMPONENT_MASK_X)
			{
				sprintf_s(buff, opcodeSize, "%s", componentX);
			}

			if (instr->asOperands[opIndex].ui32CompMask & OPERAND_4_COMPONENT_MASK_Y)
			{
				sprintf_s(buff, opcodeSize, "%s%s", buff, componentY);
			}

			if (instr->asOperands[opIndex].ui32CompMask & OPERAND_4_COMPONENT_MASK_Z)
			{
				sprintf_s(buff, opcodeSize, "%s%s", buff, componentZ);
			}

			if (instr->asOperands[opIndex].ui32CompMask & OPERAND_4_COMPONENT_MASK_W)
			{
				sprintf_s(buff, opcodeSize, "%s%s", buff, componentW);
			}

		}
		else if (instr->asOperands[opIndex].eSelMode == OPERAND_4_COMPONENT_SWIZZLE_MODE)
		{
			for (int i = 0; i < 4; i++)
			{
				sprintf_s(buff, opcodeSize, "%s%s", buff, component[instr->asOperands[opIndex].aui32Swizzle[i]]);
			}
		}
		else if ((instr->asOperands[opIndex].eSelMode == OPERAND_4_COMPONENT_SELECT_1_MODE))
		{
			sprintf_s(buff, opcodeSize, "%s", component[instr->asOperands[opIndex].aui32Swizzle[0]]);
		}

		return string(buff);
	}

	//0 different, 1 same, 2 same but sign different
	int IsInstructionOperandSame(Instruction * instr1, int opIndex1, Instruction * instr2, int opIndex2, const char * instr1Op1 = NULL, const char * instr2Op1 = NULL)
	{
		Operand & op1 = instr1->asOperands[opIndex1];
		Operand & op2 = instr2->asOperands[opIndex2];

		string component1 = GetComponentStrFromInstruction(instr1, opIndex1);
		string component2 = GetComponentStrFromInstruction(instr2, opIndex2);

		char buff1[opcodeSize];
		char buff2[opcodeSize];
		sprintf_s(buff1, opcodeSize, "r%d.%s", op1.ui32RegisterNumber, component1.c_str());
		sprintf_s(buff2, opcodeSize, "r%d.%s", op2.ui32RegisterNumber, component2.c_str());


		if (instr1Op1 != NULL)
		{
			char buff3[opcodeSize];
			sprintf_s(buff3, opcodeSize, ".%s", instr1Op1);
			applySwizzle(buff3, buff1);
		}

		if (instr2Op1 != NULL)
		{
			if (instr1Op1 == NULL)
			{
				applySwizzle(".xyz", buff1);
			}

			char buff4[opcodeSize];
			sprintf_s(buff4, opcodeSize, ".%s", instr2Op1);
			applySwizzle(buff4, buff2);
		}

		bool same = false;

		if (strcmp(buff1, buff2) == 0)
		{
			same = true;
		}

		if (same)
		{
			if (((op1.eModifier == OPERAND_MODIFIER_NEG || op1.eModifier == OPERAND_MODIFIER_ABSNEG) && (op2.eModifier == OPERAND_MODIFIER_NONE || op2.eModifier == OPERAND_MODIFIER_ABS)) ||
				((op2.eModifier == OPERAND_MODIFIER_NEG || op2.eModifier == OPERAND_MODIFIER_ABSNEG) && (op1.eModifier == OPERAND_MODIFIER_NONE || op1.eModifier == OPERAND_MODIFIER_ABS)))
			{
				return 2;
			}

			return 1;
		}

		return 0;
	}
	//dx9

	struct DeferredWrite
	{
		string target;
		string temporary;
		string mask;
	};

	// DXBC reads all sources before writing any destination. Instructions expanded
	// into several statements must finish their calculations before committing
	// results, including when another destination aliases a source.
	DeferredWrite deferWrite(const string &target, const char *type = "float4")
	{
		char destination[opcodeSize];
		strcpy_s(destination, opcodeSize, target.c_str());
		remapTarget(destination);
		const char *mask = strrchr(destination, '.');
		DeferredWrite result;
		result.mask = mask ? mask : ".xyzw";
		result.target = mask ? string(destination, mask - destination) : destination;
		result.temporary = "instructionResult" + to_string(uuidVar++);
		char buffer[512];
		sprintf(buffer, "  %s %s;\n", type, result.temporary.c_str());
		appendOutput(buffer);
		return result;
	}

	void commitWrite(const DeferredWrite &result)
	{
		// Preserve output registers split across multiple HLSL parameters.
		for (size_t i = 1; i < result.mask.size(); ++i)
		{
			char target[opcodeSize], buffer[512];
			sprintf(target, "%s.%c", result.target.c_str(), result.mask[i]);
			sprintf(buffer, "  %s = %s.%c;\n", writeTarget(target),
				result.temporary.c_str(), result.mask[i]);
			appendOutput(buffer);
		}
	}

	// Recombine a structured load that was split into per-component expressions.
	// This keeps the generated HLSL compact while preserving DXBC's read-before-
	// write behavior when the destination aliases the index register.
	bool writeStructuredComponents(const char *destination, const string translated[4], unsigned mask)
	{
		const char *maskText = strrchr(destination, '.');
		if (!maskText)
			return false;

		string target(destination, maskText - destination);
		string components(maskText + 1);
		string vectorTarget = target + "." + components;

		// A whole-vector remap is safe. If only individual components are
		// remapped, keep the DeferredWrite path because those components may
		// belong to different output registers.
		StringStringMap::iterator remapped = mRemappedOutputRegisters.find(vectorTarget);
		if (remapped != mRemappedOutputRegisters.end())
			vectorTarget = remapped->second;
		else
		{
			for (size_t i = 0; i < components.size(); ++i)
			{
				string componentTarget = target + "." + components[i];
				if (mRemappedOutputRegisters.find(componentTarget) != mRemappedOutputRegisters.end())
					return false;
			}
		}

		string values;
		int componentCount = 0;
		for (int component = 0; component < 4; ++component)
		{
			if (!(mask & (1 << component)))
				continue;
			if (!values.empty())
				values += ", ";
			values += translated[component];
			++componentCount;
		}

		if (!componentCount)
			return false;

		string expression = componentCount == 1
			? values
			: "float" + to_string(componentCount) + "(" + values + ")";
		char buffer[opcodeSize * 8];
		sprintf(buffer, "  %s = %s;\n", vectorTarget.c_str(), expression.c_str());
		appendOutput(buffer);
		return true;
	}

	void ParseCode(Shader *shader, const char *c, size_t size)
	{
		mOutputRegisterValues.clear();
		mBooleanRegisters.clear();
		mIntegerAliases.clear();
		mIntegerReadAliases.clear();
		mDeclaredIntegerAliases.clear();
		mIntegerBitPatternComponents.clear();
		mIntegerReadBitPatternComponents.clear();
		mCodeStartPos = mOutput.size();

		char buffer[512];
		size_t pos = 0;
		unsigned int iNr = 0;
		bool skip_shader = false;

		vector<Instruction> *instructions = &shader->asPhase[MAIN_PHASE].ppsInst[0];
		size_t inst_count = instructions->size();

		while (pos < size && iNr < inst_count)
		{
			Instruction *instr = &(*instructions)[iNr];

			// Now ignore '#line' or 'undecipherable' debug info (DefenseGrid2)
			if (!strncmp(c + pos, "#line", 5) ||
				!strncmp(c + pos, "undecipherable", 14))
			{
				NextLine(c, pos, size);
				continue;
			}

			// And the dissassembler apparently can add trailing \0 characters
			// and blank lines to the buffer in the debug case.
			if (c[pos] == 0x0a || c[pos] == 0x00)
			{
				pos++;
				continue;
			}

			// Ignore comments. 
			if (!strncmp(c + pos, "//", 2))
			{
				NextLine(c, pos, size);
				continue;
			}
			// Read statement.
			if (ReadStatement(c + pos) < 1)
			{
				logDecompileError("Error parsing statement: " + string(c + pos, 80));
				return;
			}
			//LogDebug("parsing statement %s with args %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s\n", statement,
			//	op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15);
			//

			// Some shaders seen in World of Diving contain multiple shader programs.
			// Ignore any instructions from old shader models that we do not handle to
			// avoid crashes.
			if (!shader->dx9Shader && (
			    !strncmp(statement, "vs_1", 4) || !strncmp(statement, "vs_2", 4) ||
			    !strncmp(statement, "ps_1", 4) || !strncmp(statement, "ps_2", 4))) {
				skip_shader = true;
				NextLine(c, pos, size);
				continue;
			}

			if (!strncmp(statement, "vs_", 3) ||
				!strncmp(statement, "hs_", 3) ||
				!strncmp(statement, "ds_", 3) ||
				!strncmp(statement, "gs_", 3) ||
				!strncmp(statement, "ps_", 3) ||
				!strncmp(statement, "cs_", 3))
			{
				skip_shader = false;
				mShaderType = statement;
			}
			else if (skip_shader)
			{
				NextLine(c, pos, size);
				continue;
			}
			else if (!strcmp(statement, "def"))		//dx9 const
			{
				int registerIndex = atoi(&op1[1]);


				ConstantValue value;
				value.name = op1;
				value.x = (float)atof(op2);
				value.y = (float)atof(op3);
				value.z = (float)atof(op4);
				value.w = (float)atof(op5);

				mConstantValues[registerIndex] = value;
			} //dx9
			else if (!strcmp(statement, "dcl_immediateConstantBuffer"))
			{
				sprintf(buffer, "  const float4 icb[] =");
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
				pos += strlen(statement);
				while (c[pos] != 0x0a && pos < size)
					mOutput.insert(mOutput.end(), c[pos++]);
				mOutput.insert(mOutput.end(), '\n');
			}
			else if (!strcmp(statement, "dcl_constantbuffer"))
			{
				char *strPos46 = strstr(op1, "cb"); // Match d3dcompiler_46 disassembly
				char *strPos47 = strstr(op1, "CB"); // Match d3dcompiler_47 disassembly
				char *strPos = strPos46 ? strPos46 : strPos47;
				if (strPos)
				{
					int bufIndex = 0;
					if (sscanf_s(strPos + 2, "%d", &bufIndex) != 1)
					{
						logDecompileError("Error parsing buffer register index: " + string(op1));
						return;
					}
					strPos = strchr(op1, '[');
					if (!strPos)
					{
						logDecompileError("5 Error parsing buffer offset: " + string(op1));
						return;
					}
					int bufSize = 0;
					if (sscanf_s(strPos + 1, "%d", &bufSize) != 1)
					{
						logDecompileError("Error parsing buffer size: " + string(op1));
						return;
					}
					// In the case where headers have been stripped, we will not have any CBufferNames,
					// but we will have these dcl_constantbuffer declarations.  We are now using
					// the absence of any mCBufferNames as indication that we have stripped headers,
					// and create the fake cb2 style names as the best we can do.  
					// Not sure this will work in all cases, because the offsets into the buffer are
					// not required to be zero for the first element, but we have no other info here.
					if (mCBufferNames.empty())
					{
						BufferEntry e;
						e.bt = DT_float4;
						e.matrixRow = 0;
						e.isRowMajor = false;
						sprintf(buffer, "cbuffer cb%d : register(b%d)\n"
							"{\n"
							"  float4 cb%d[%d];\n"
							"}\n\n", bufIndex, bufIndex, bufIndex, bufSize);
						mOutput.insert(mOutput.begin(), buffer, buffer + strlen(buffer));
						mCodeStartPos += strlen(buffer);
						for (int j = 0; j < bufSize; ++j)
						{
							sprintf(buffer, "cb%d[%d]", bufIndex, j);
							e.Name = buffer;
							mCBufferData[(bufIndex << 16) + j * 16] = e;
						}
					}
				}
			}
			else if (!strcmp(statement, "dcl_resource_structured") || !strcmp(statement, "dcl_uav_structured"))
			{
				bool uav = statement[4] == 'u';
				char prefix = uav ? 'u' : 't';
				int bufIndex = 0;
				int bufStride = 0;
				if (sscanf_s(&op1[1], "%d", &bufIndex) != 1)
				{
					logDecompileError("Error parsing structured buffer register: " + string(op1));
					return;
				}
				if (sscanf_s(op2, "%d", &bufStride) != 1)
				{
					logDecompileError("Error parsing structured buffer stride: " + string(op2));
					return;
				}
				// Similar concern to the constant buffers missing reflection info above - if we don't
				// have the structure definitions we have to manufacture our own. Without type information
				// we assume everything is a float because we can't do any better. This is worse news than
				// a constant buffer missing reflection info as structured buffers tend to contain much
				// more varied data types than constant buffers in practice. Ideally we should move to
				// a model where we do reinterpret casts whenever we need something other than a float.
				if (mStructuredBufferTypes.empty())
				{
					sprintf(buffer, "struct %c%d_t {\n"
						"  float val[%d];\n"
						"};\n"
						"%sStructuredBuffer<%c%d_t> %c%d : register(%c%d);\n\n",
						prefix, bufIndex, bufStride / 4, uav ? "RW" : "",
						prefix, bufIndex, prefix, bufIndex, prefix, bufIndex);
					mOutput.insert(mOutput.begin(), buffer, buffer + strlen(buffer));
					mCodeStartPos += strlen(buffer);

					if (bufStride % 4) {
						// I don't think this is ordinarily possible since almost all data types are 32bits
						// (or a pair of 2x32bit fields in the case of a double). Half types and minimum
						// precision types can theoretically be 16 bits on embedded implementations,
						// but in practice are 32bits on PC. If it does happen we need to know about it:
						sprintf(buffer, "FIXME: StructuredBuffer t%d stride %d is not a multiple of 4\n\n",
								bufIndex, bufStride);
						mOutput.insert(mOutput.begin(), buffer, buffer + strlen(buffer));
						mCodeStartPos += strlen(buffer);
					}
				}
			}
			else if (!strcmp(statement, "dcl_tgsm_structured"))
			{
				int bufIndex = 0;
				int bufStride = 0;
				int bufCount = 0;
				if (sscanf_s(op1, "g%d", &bufIndex) != 1)
				{
					logDecompileError("Error parsing tgsm structured buffer register: " + string(op1));
					return;
				}
				if (sscanf_s(op2, "%d", &bufStride) != 1)
				{
					logDecompileError("Error parsing tgsm structured buffer stride: " + string(op2));
					return;
				}
				if (sscanf_s(op3, "%d", &bufCount) != 1)
				{
					logDecompileError("Error parsing tgsm structured buffer count: " + string(op3));
					return;
				}
				// HLSL accepts the register(gN) syntax, but seems to disregard it, and
				// doesn't matter anyway since these don't correspond to any externally
				// bound resources. Use an inline type definition for conciseness:
				sprintf(buffer, "groupshared struct { float val[%d]; } g%d[%d];\n",
					bufStride / 4, bufIndex, bufCount);
				mOutput.insert(mOutput.begin(), buffer, buffer + strlen(buffer));
				mCodeStartPos += strlen(buffer);
			}
			// Create new map entries if there aren't any for dcl_sampler.  This can happen if
			// there is no Resource Binding section in the shader.  TODO: probably needs to handle arrays too.
			else if (!strcmp(statement, "dcl_sampler"))
			{
				if (op1[0] == 's')
				{
					int bufIndex = 0;
					if (sscanf_s(op1, "s%d", &bufIndex) != 1)
					{
						logDecompileError("Error parsing sampler register index: " + string(op1));
						return;
					}
					if (!strcmp(op2, "mode_default"))
					{
						map<int, string>::iterator i = mSamplerNames.find(bufIndex);
						if (i == mSamplerNames.end())
						{
							sprintf(buffer, "s%d_s", bufIndex);
							mSamplerNames[bufIndex] = buffer;
							sprintf(buffer, "SamplerState %s : register(s%d);\n\n", mSamplerNames[bufIndex].c_str(), bufIndex);
							mOutput.insert(mOutput.begin(), buffer, buffer + strlen(buffer));
							mCodeStartPos += strlen(buffer);
						}
					}
					else if (!strcmp(op2, "mode_comparison"))
					{
						map<int, string>::iterator i = mSamplerComparisonNames.find(bufIndex);
						if (i == mSamplerComparisonNames.end())
						{
							sprintf(buffer, "s%d_s", bufIndex);
							mSamplerComparisonNames[bufIndex] = buffer;
							sprintf(buffer, "SamplerComparisonState %s : register(s%d);\n\n", mSamplerComparisonNames[bufIndex].c_str(), bufIndex);
							mOutput.insert(mOutput.begin(), buffer, buffer + strlen(buffer));
							mCodeStartPos += strlen(buffer);
						}
					}
					else
					{
						logDecompileError("Error parsing dcl_sampler type: " + string(op2));
						return;
					}
				}
			}
			// for all of these dcl_resource_texture* variants, if we don't have any header information
			// available, we try to fall back to the raw text definitions, and use registers instead of
			// named variable.  A big problem though is that the count is unknown if we have no headers,
			// so it should be Texture2D<float2> for example, but we only see (float,float,float,float).
			// With no headers and no reflection this can't be done, so for now we'll set them to <*4> 
			// as the most common use case. But only for this case where the game is being hostile.
			// It will probably require hand tuning to correct usage of those texture registers.
			// Also unlikely to work for the (mixed,mixed,mixed,mixed) case.  <mixed4> will be wrong.
			else if (!strcmp(statement, "dcl_resource_texture2d"))
			{
				if (op2[0] == 't')
				{
					int bufIndex = 0;
					if (sscanf_s(op2 + 1, "%d", &bufIndex) != 1)
					{
						logDecompileError("Error parsing texture register index: " + string(op2));
						return;
					}
					// Create if not existing.  e.g. if no ResourceBinding section in ASM.
					map<int, string>::iterator i = mTextureNames.find(bufIndex);
					if (i == mTextureNames.end())
					{
						CreateRawFormat("Texture2D", bufIndex);
					}
				}
			}
			else if (!strcmp(statement, "dcl_resource_texture1darray"))	// dcl_resource_texture1darray (float,float,float,float) t6
			{
				if (op2[0] == 't')
				{
					int bufIndex = 0;
					if (sscanf_s(op2 + 1, "%d", &bufIndex) != 1)
					{
						logDecompileError("Error parsing texture1darray register index: " + string(op2));
						return;
					}
					map<int, string>::iterator i = mTextureNames.find(bufIndex);
					if (i == mTextureNames.end())
					{
						CreateRawFormat("Texture1DArray", bufIndex);
					}
				}
			}
			else if (!strcmp(statement, "dcl_resource_texture2darray"))	// dcl_resource_texture2darray (float,float,float,float) t0
			{
				if (op2[0] == 't')
				{
					int bufIndex = 0;
					if (sscanf_s(op2 + 1, "%d", &bufIndex) != 1)
					{
						logDecompileError("Error parsing texture2darray register index: " + string(op2));
						return;
					}
					// Create if not existing.   e.g. if no ResourceBinding section in ASM.
					map<int, string>::iterator i = mTextureNames.find(bufIndex);
					if (i == mTextureNames.end())
					{
						CreateRawFormat("Texture2DArray", bufIndex);
					}
				}
			}
			else if (!strncmp(statement, "dcl_resource_texture2dms", strlen("dcl_resource_texture2dms")))	// dcl_resource_texture2dms(8) (float,float,float,float) t4
			{
				if (op2[0] == 't')
				{
					int bufIndex = 0;
					if (sscanf_s(op2 + 1, "%d", &bufIndex) != 1)
					{
						logDecompileError("Error parsing dcl_resource_texture2dms register index: " + string(op2));
						return;
					}
					int dim = 0;
					if (sscanf_s(statement, "dcl_resource_texture2dms(%d)", &dim) != 1)
					{
						logDecompileError("Error parsing dcl_resource_texture2dms array dimension: " + string(statement));
						return;
					}
					// Create if not existing.   e.g. if no ResourceBinding section in ASM.  Might need <f,x> variant for texturetype.
					map<int, string>::iterator i = mTextureNames.find(bufIndex);
					if (i == mTextureNames.end())
					{
						sprintf(buffer, "t%d", bufIndex);
						mTextureNames[bufIndex] = buffer;

						char format[16];
						sscanf_s(op1, "(%[^,]", format, 16);	// Match first xx of (xx,xx,xx,xx)
						string form4 = string(format) + "4";
						mTextureType[bufIndex] = "Texture2DMS<" + form4 + ">";

						if (dim == 0)
							sprintf(buffer, "Texture2DMS<%s> t%d : register(t%d);\n\n", form4.c_str(), bufIndex, bufIndex);
						else
							sprintf(buffer, "Texture2DMS<%s,%d> t%d : register(t%d);\n\n", form4.c_str(), dim, bufIndex, bufIndex);
						mOutput.insert(mOutput.begin(), buffer, buffer + strlen(buffer));
						mCodeStartPos += strlen(buffer);
					}
				}
			}
			else if (!strcmp(statement, "dcl_resource_texture3d"))
			{
				if (op2[0] == 't')
				{
					int bufIndex = 0;
					if (sscanf_s(op2 + 1, "%d", &bufIndex) != 1)
					{
						logDecompileError("Error parsing texture register index: " + string(op2));
						return;
					}
					// Create if not existing.  e.g. if no ResourceBinding section in ASM.
					map<int, string>::iterator i = mTextureNames.find(bufIndex);
					if (i == mTextureNames.end())
					{
						CreateRawFormat("Texture3D", bufIndex);
					}
				}
			}
			else if (!strcmp(statement, "dcl_resource_texturecube"))
			{
				if (op2[0] == 't')
				{
					int bufIndex = 0;
					if (sscanf_s(op2 + 1, "%d", &bufIndex) != 1)
					{
						logDecompileError("Error parsing texture register index: " + string(op2));
						return;
					}
					// Create if not existing.  e.g. if no ResourceBinding section in ASM.
					map<int, string>::iterator i = mTextureNames.find(bufIndex);
					if (i == mTextureNames.end())
					{
						CreateRawFormat("TextureCube", bufIndex);
					}
				}
			}
			else if (!strcmp(statement, "dcl_resource_texturecubearray"))
			{
				if (op2[0] == 't')
				{
					int bufIndex = 0;
					if (sscanf_s(op2 + 1, "%d", &bufIndex) != 1)
					{
						logDecompileError("Error parsing texture register index: " + string(op2));
						return;
					}
					// Create if not existing.  e.g. if no ResourceBinding section in ASM.
					map<int, string>::iterator i = mTextureNames.find(bufIndex);
					if (i == mTextureNames.end())
					{
						CreateRawFormat("TextureCubeArray", bufIndex);
					}
				}
			}
			else if (!strcmp(statement, "dcl_resource_buffer"))		// dcl_resource_buffer (sint,sint,sint,sint) t2
			{
				if (op2[0] == 't')
				{
					int bufIndex = 0;
					if (sscanf_s(op2 + 1, "%d", &bufIndex) != 1)
					{
						logDecompileError("Error parsing texture register index: " + string(op2));
						return;
					}
					// Create if not existing.  e.g. if no ResourceBinding section in ASM.
					map<int, string>::iterator i = mTextureNames.find(bufIndex);
					if (i == mTextureNames.end())
					{
						CreateRawFormat("Buffer", bufIndex);
					}
				}
			}
			else if (!strcmp(statement, "dcl_resource_raw"))		// dcl_resource_raw t2
			{
				if (op2[0] == 't')
				{
					int bufIndex = 0;
					if (sscanf_s(op2 + 1, "%d", &bufIndex) != 1)
					{
						logDecompileError("Error parsing raw buffer register index: " + string(op2));
						return;
					}
					map<int, string>::iterator i = mTextureNames.find(bufIndex);
					if (i == mTextureNames.end())
					{
						// Raw buffer has no format parameter; create a ByteAddressBuffer directly.
						char buffer[128];
						sprintf(buffer, "t%d", bufIndex);
						mTextureNames[bufIndex] = buffer;
						mTextureType[bufIndex] = "ByteAddressBuffer";
						sprintf(buffer, "ByteAddressBuffer t%d : register(t%d);\n\n", bufIndex, bufIndex);
						mOutput.insert(mOutput.begin(), buffer, buffer + strlen(buffer));
						mCodeStartPos += strlen(buffer);
					}
				}
			}
			else if (!strcmp(statement, "{"))
			{
				// Declaration from 
				// dcl_immediateConstantBuffer { { 1.000000, 0, 0, 0},
				//                      { 0, 1.000000, 0, 0},
				//                      { 0, 0, 1.000000, 0},
				//                      { 0, 0, 0, 1.000000} }
				while (c[pos] != 0x0a && pos < size)
					mOutput.insert(mOutput.end(), c[pos++]);
				if (c[pos - 1] == '}')
					mOutput.insert(mOutput.end(), ';');
				mOutput.insert(mOutput.end(), c[pos++]);
				continue;
			}
			else if (!strcmp(statement, "dcl_indexrange"))
			{
				int numIndex = 0;
				sscanf_s(op2, "%d", &numIndex);
				sprintf(buffer, "  float4 v[%d] = { ", numIndex);
				for (int i = 0; i < numIndex; ++i)
					sprintf_s(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "v%d,", i);
				buffer[strlen(buffer) - 1] = 0;
				strcat(buffer, " };\n");
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
			else if (!strcmp(statement, "dcl_indexableTemp"))
			{
				// Always returned 4, not actual index.  ToDo: likely not always float4.
				// format as: dcl_indexableTemp x0[40], 4
				int numIndex = 0;
				char varName[opcodeSize];
				sscanf_s(op1, "%[^[][%d]", varName, opcodeSize, &numIndex);
				sprintf(buffer, "  float4 %s[%d];\n", varName, numIndex);
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
			else if (!strcmp(statement, "dcl_input"))
			{
				// Can have 'vCoverage' variable implicitly defined, 
				// not in input signature when reflection is stripped.
				if (!strcmp(op1, "vCoverage"))
				{
					char *pos = strstr(mOutput.data(), "void main(");
					while (*pos != 0x0a) pos++; pos++;
					sprintf(buffer, "  uint vCoverage : SV_Coverage,\n");
					mOutput.insert(mOutput.begin() + (pos - mOutput.data()), buffer, buffer + strlen(buffer));
				}
			}
			else if (!strcmp(statement, "dcl_temps"))
			{
				const char *varDecl = "  float4 ";
				mOutput.insert(mOutput.end(), varDecl, varDecl + strlen(varDecl));
				int numTemps;
				sscanf_s(c + pos, "%s %d", statement, UCOUNTOF(statement), &numTemps);
				for (int i = 0; i < numTemps; ++i)
				{
					sprintf(buffer, "r%d,", i);
					mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
				}
				mOutput.pop_back();
				mOutput.push_back(';');
				mOutput.push_back('\n');
				const char *helperDecl = "  uint4 bitmask, uiDest;\n  float4 fDest;\n\n";
				mOutput.insert(mOutput.end(), helperDecl, helperDecl + strlen(helperDecl));
			}
			// For Geometry Shaders, e.g. dcl_stream m0  TODO: make it StreamN, add to varlist
			else if (!strcmp(statement, "dcl_stream"))
			{
				// Write out original ASM, inline, for reference.
				sprintf(buffer, "// Needs manual fix for instruction:  \n//");
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
				ASMLineOut(c, pos, size);
				// Move back to input section and output something close to right
				char *main_ptr = strstr(mOutput.data(), "void main(");
				size_t offset = main_ptr - mOutput.data();
				NextLine(mOutput.data(), offset, mOutput.size());
				sprintf(buffer, "  inout TriangleStream<float> m0,\n");
				mOutput.insert(mOutput.begin() + offset , buffer, buffer + strlen(buffer));
			}
			// For Geometry Shaders, e.g. dcl_maxout n
			else if (!strcmp(statement, "dcl_maxout"))
			{
				char *main_ptr = strstr(mOutput.data(), "void main(");
				size_t offset = main_ptr - mOutput.data();
				sprintf(buffer, "[maxvertexcount(%s)]\n", op1);
				mOutput.insert(mOutput.begin() + offset, buffer, buffer + strlen(buffer));
			}
			else if (!strncmp(statement, "dcl_", 4))
			{
				// Hateful strcmp logic is upside down, only output for ones we aren't already handling.
				if (strcmp(statement, "dcl_output") && 
					strcmp(statement, "dcl_output_siv") &&
					strcmp(statement, "dcl_globalFlags") &&
					//strcmp(statement, "dcl_input_siv") && 
					strcmp(statement, "dcl_input_ps") && 
					strcmp(statement, "dcl_input_ps_sgv") &&
					strcmp(statement, "dcl_input_ps_siv"))
				{
					// Other declarations, unforeseen.
					sprintf(buffer, "// Needs manual fix for instruction:\n");
					mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
					sprintf(buffer, "// unknown dcl_: ");
					mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
					ASMLineOut(c, pos, size);
				}
			}
			else if (!strcmp(statement, "dcl"))		//dx9 dcl vFace
			{
				// Ummm... why is this empty block here? Is this here
				// intentionally to avoid the next else block, or was it
				// forgotten about? -DSS
			}//dx9
			else
			{
				// Capture the integer meaning of source components before the
				// destination invalidation below. DXBC permits read/write
				// overlap, e.g. ld_structured r9.z, r9.z, ..., t8.
				// The old value must remain available while translating the
				// source operand, but must not become a live alias afterwards.
				mIntegerReadAliases.clear();
				mIntegerReadBitPatternComponents.clear();
				char *read_operands[] = { op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15 };
				for (uint32_t read_index = 1; read_index < instr->ui32NumOperands && read_index <= 14; ++read_index)
					preserveIntegerReadOperand(read_operands[read_index - 1]);

				bool sourceOnly = instr->eOpcode == OPCODE_SWITCH || instr->eOpcode == OPCODE_IF ||
					instr->eOpcode == OPCODE_BREAKC || instr->eOpcode == OPCODE_CONTINUEC ||
					instr->eOpcode == OPCODE_RETC || instr->eOpcode == OPCODE_DISCARD;
				if (!sourceOnly && instr->eOpcode != OPCODE_LD_STRUCTURED && instr->ui32NumOperands && instr->asOperands[0].eType == OPERAND_TYPE_TEMP)
					invalidateIntegerAliases(op1);
				switch (instr->eOpcode)
				{
				case OPCODE_FTOI:
				case OPCODE_FTOU:
				case OPCODE_IMIN:
				case OPCODE_IMAX:
				case OPCODE_UMIN:
				case OPCODE_UMAX:
				case OPCODE_IADD:
				case OPCODE_IMAD:
				case OPCODE_UMAD:
				case OPCODE_INEG:
				case OPCODE_ISHL:
				case OPCODE_ISHR:
				case OPCODE_USHR:
				case OPCODE_AND:
				case OPCODE_OR:
				case OPCODE_XOR:
				case OPCODE_NOT:
				case OPCODE_FIRSTBIT_HI:
				case OPCODE_FIRSTBIT_LO:
				case OPCODE_FIRSTBIT_SHI:
				case OPCODE_COUNTBITS:
				case OPCODE_UBFE:
				case OPCODE_IBFE:
				case OPCODE_BFI:
				case OPCODE_BFREV:
				case OPCODE_UDIV:
					markIntegerBitPatternComponents(op1);
					break;
				case OPCODE_IMUL:
				case OPCODE_UMUL:
					invalidateIntegerAliases(op2);
					markIntegerBitPatternComponents(op2);
					break;
				default:
					break;
				}
				switch (instr->eOpcode)
				{

					case OPCODE_ITOF:
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						bitcastToInt(op2);
						sprintf(buffer, "  %s = %s;\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					case OPCODE_UTOF:
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						bitcastToUInt(op2);
						sprintf(buffer, "  %s = %s;\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

					case OPCODE_MOV:
					{
						remapTarget(op1);
						bool sourceIsBoolean = isBoolean(op2);
						bool movedIntegerBitPattern = isIntegerBitPatternOperand(op2);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						string movedIntegerAlias = integerAliasForOperand(op2);
						bool emittedIntegerMove = false;
						if (!instr->bSaturate && !movedIntegerAlias.empty())
						{
							// A DXBC mov copies a value, not a variable identity. Keep
							// tile index, probe masks, probe bit and probe index in
							// separate HLSL live ranges rather than mapping both
							// register components to one alias.
							bool unsigned_value = movedIntegerAlias.find("_uint_") != string::npos;
							emittedIntegerMove = emitIntegerAssignment(op1, movedIntegerAlias.c_str(), unsigned_value);
						}
						if (!emittedIntegerMove)
						{
							if (!instr->bSaturate)
								sprintf(buffer, "  %s = %s;\n", writeTarget(op1), ci(op2).c_str());
							else
								sprintf(buffer, "  %s = saturate(%s);\n", writeTarget(op1), ci(op2).c_str());
							appendOutput(buffer);
						}
						if (op1[0] == 'o')
						{
							char *dotPos = strchr(op1, '.'); if (dotPos) *dotPos = 0;
							if (!dotPos || dotPos[1] == 'x')
								mOutputRegisterValues[op1] = op2;
						}
						removeBoolean(op1);
						if (sourceIsBoolean)
							addBoolean(op1);
						if (!emittedIntegerMove && !movedIntegerAlias.empty())
						{
							string destination;
							int register_number;
							char component;
							if (singleTempComponent(op1, destination, register_number, component))
								mIntegerAliases[destination] = movedIntegerAlias;
						}
						if (movedIntegerBitPattern)
							markIntegerBitPatternComponents(op1);
						break;
					}

					case OPCODE_RCP:
						remapTarget(op1);
						applySwizzle(op1, op2);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = rcp(%s);\n", writeTarget(op1), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(rcp(%s));\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						break;

					case OPCODE_NOT:
						remapTarget(op1);
						applySwizzle(op1, op2);
						removeBoolean(op1);
						bitcastToUInt(op2);
						{
							string expression = "~" + ci(op2);
							if (!emitIntegerAssignment(op1, expression.c_str(), true))
							{
								sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
								appendOutput(buffer);
							}
						}
						addBoolean(op1);
						break;

					case OPCODE_INEG:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						bitcastToInt(op2);
						sprintf(buffer, "  %s = asfloat(-%s);\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

					case OPCODE_F32TOF16:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = f32tof16(%s);\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

					case OPCODE_F16TOF32:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = f16tof32(%s);\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

					case OPCODE_FRC:
						remapTarget(op1);
						applySwizzle(op1, op2);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = frac(%s);\n", writeTarget(op1), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(frac(%s));\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

					case OPCODE_MUL:
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						mMulOperand = op3; mMulOperand2 = op2; mMulTarget = op1;
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = %s * %s;\n", writeTarget(op1), ci(op3).c_str(), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(%s * %s);\n", writeTarget(op1), ci(op3).c_str(), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

					case OPCODE_IMUL:
						remapTarget(op2);
						applySwizzle(op2, op3, true);
						applySwizzle(op2, op4, true);
						bitcastToInt(op3);
						bitcastToInt(op4);
						mMulOperand = strncmp(op3, "int", 3) ? op3 : op4;
						{
							string expression = ci(op3) + " * " + ci(op4);
							if (!emitIntegerAssignment(op2, expression.c_str(), false))
							{
								sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op2), expression.c_str());
								appendOutput(buffer);
							}
						}
						removeBoolean(op2);
						break;

					case OPCODE_DIV:
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = %s / %s;\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						else
							sprintf(buffer, "  %s = saturate(%s / %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

						// UDIV instruction also could damage the original register before finishing, giving wrong results. 
						// e.g. udiv r0.x, r1.x, r0.x, r0.y
						// variant: udiv null, r1.xy, r3.zwzz, r2.zwzz
						// To fix this, we are using the temp variables declared at the top. 
						// expected output:
						//   uiDest.x = (uint)r0.x / (uint)r0.y;
						//   r1.x = (uint)r0.x % (uint)r0.y;
						//   r0.x = uiDest.x;
						//
						// This will swizzle based on either op1 or op2, as long as it's not null.  It's not clear whether
						// the swizzle is allowed to vary for each half of the instruction, like xy for /, zw for %.  
						// To allow for that, we'll set the temp registers with full swizzle, then only use the specific
						// parts required for each half, as the safest approach.  Might not generate udiv though.
						// Also removed saturate code, because udiv does not specify that.
						// Creates operand copies to applySwizzle to unchanged operands, as constant l values are otherwise damaged.
					case OPCODE_UDIV:
					{
						remapTarget(op1);
						remapTarget(op2);
						char divOut[opcodeSize] = "uiDest.xyzw";
						char *divSwiz = op1;
						char *remSwiz = op2;
						strcpy_s(op13, opcodeSize, op3);
						strcpy_s(op14, opcodeSize, op4);

						if (instr->asOperands[0].eType != OPERAND_TYPE_NULL)
						{
							applySwizzle(divSwiz, divOut, true);
							applySwizzle(divSwiz, fixImm(op13, instr->asOperands[2]), true);
							applySwizzle(divSwiz, fixImm(op14, instr->asOperands[3]), true);
							bitcastToUInt(op13);
							bitcastToUInt(op14);

							sprintf(buffer, "  %s = %s / %s;\n", divOut, ci(op13).c_str(), ci(op14).c_str());
							appendOutput(buffer);
						}
						if (instr->asOperands[1].eType != OPERAND_TYPE_NULL)
						{
							applySwizzle(remSwiz, fixImm(op3, instr->asOperands[2]), true);
							applySwizzle(remSwiz, fixImm(op4, instr->asOperands[3]), true);
							bitcastToUInt(op3);
							bitcastToUInt(op4);

							sprintf(buffer, "  %s = asfloat(%s %% %s);\n", writeTarget(op2), ci(op3).c_str(), ci(op4).c_str());
							appendOutput(buffer);
						}
						if (instr->asOperands[0].eType != OPERAND_TYPE_NULL)
						{
							sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), divOut);
							appendOutput(buffer);
						}
						removeBoolean(op1);
						removeBoolean(op2);
						break;
					}

					case OPCODE_ADD:
						// OPCODE_ADD is 0. Therefore, it is possible for us to
						// arrive here if the line has simply not been parsed.
						// Let's make sure we are actually parsing an 'add' before
						// we go any further. Only check first three characters
						// since we still want add_sat to parse.
						if (strncmp(statement, "add", 3)) {
							logDecompileError("No opcode: " + string(statement));
							return;
						}

						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						if (!instr->bSaturate) {
							// Reverting the DX9 port changes and going back to
							// the original opcode order here, since they
							// should be mathematically equivelent, but I seem
							// to recall Bo3b noticing that this order tends to
							// produce assembly closer to the original. -DSS
							sprintf(buffer, "  %s = %s + %s;\n", writeTarget(op1), ci(op3).c_str(), ci(op2).c_str());
							//sprintf(buffer, "  %s = %s + %s;\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str()); //dx9
						} else {
							sprintf(buffer, "  %s = saturate(%s + %s);\n", writeTarget(op1), ci(op3).c_str(), ci(op2).c_str());
							//sprintf(buffer, "  %s = saturate(%s + %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str()); //dx9
						}
						appendOutput(buffer);
						removeBoolean(op1);
						break;

						// IADD is apparently used by the fxc compiler in some cases like an AND operation.
						// After boolean operations, it will sum up two boolean operators. Since we are now
						// passing -1:0 instead of 1:0, it should work to just do the IADD operation, respecting
						// any negation the source applies. A common sequence, compiler trick is:
						//  lt r0.y, l(0.000000), r0.x
						//  lt r0.x, r0.x, l(0.000000)
						//  iadd r0.x, r0.x, -r0.y
						//  itof r0.x, r0.x
						//  mul r0.xyz, r0.xxxx, r1.xzwx
					case OPCODE_IADD:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						bitcastToInt(op2);
						bitcastToInt(op3);
						{
							string expression = ci(op2) + " + " + ci(op3);
							if (!emitIntegerAssignment(op1, expression.c_str(), false))
							{
								sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
								appendOutput(buffer);
							}
						}
						removeBoolean(op1);
						break;

					// Bitwise instructions operate on the register bit pattern, not on
					// the numeric float value.  In particular, comparison results are
					// 0xffffffff/0 and must not be converted to 1.0/-1.0.
					case OPCODE_AND:
						remapTarget(op1);
						removeBoolean(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						bitcastToUInt(op2);
						bitcastToUInt(op3);
						{
							string expression = ci(op2) + " & " + ci(op3);
							if (!emitIntegerAssignment(op1, expression.c_str(), true))
							{
								sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
								appendOutput(buffer);
							}
						}
						addBoolean(op1);
						break;

					case OPCODE_OR:
						remapTarget(op1);
						removeBoolean(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						bitcastToUInt(op2);
						bitcastToUInt(op3);
						{
							string expression = ci(op2) + " | " + ci(op3);
							if (!emitIntegerAssignment(op1, expression.c_str(), true))
							{
								sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
								appendOutput(buffer);
							}
						}
						addBoolean(op1);
						break;

					case OPCODE_XOR:
						remapTarget(op1);
						removeBoolean(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						bitcastToUInt(op2);
						bitcastToUInt(op3);
						{
							string expression = ci(op2) + " ^ " + ci(op3);
							if (!emitIntegerAssignment(op1, expression.c_str(), true))
							{
								sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
								appendOutput(buffer);
							}
						}
						addBoolean(op1);
						break;

						// Curiously enough, the documentation for ISHR and ISHL is wrong, and documents the parameters backwards.
						// http://msdn.microsoft.com/en-us/library/windows/desktop/hh447145(v=vs.85).aspx
						// This was proven by looking at actual game ASM, and trying to make the HLSL match the generated ASM.
						// It is the C standard of: shift-expression << additive-expression 
						// So, we need op2 as the Shift-Expression, op3 as the # of bits to shift.
					case OPCODE_ISHR:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						bitcastToInt(op2);
						bitcastToUInt(op3);
						{
							string expression = ci(op2) + " >> " + ci(op3);
							if (!emitIntegerAssignment(op1, expression.c_str(), false))
							{
								sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
								appendOutput(buffer);
							}
						}
						removeBoolean(op1);
						break;

					case OPCODE_ISHL:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						bitcastToInt(op2);
						bitcastToUInt(op3);
						{
							string expression = ci(op2) + " << " + ci(op3);
							if (!emitIntegerAssignment(op1, expression.c_str(), false))
							{
								sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
								appendOutput(buffer);
							}
						}
						removeBoolean(op1);
						break;

						// USHR appears to be documented correctly.
						// But this code was still backwards.
					case OPCODE_USHR:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						bitcastToUInt(op2);
						bitcastToUInt(op3);
						{
							string expression = ci(op2) + " >> " + ci(op3);
							if (!emitIntegerAssignment(op1, expression.c_str(), true))
							{
								sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
								appendOutput(buffer);
							}
						}
						removeBoolean(op1);
						break;

						// Newly found in CS for Prey
					case OPCODE_COUNTBITS:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						bitcastToUInt(op2);
						{
							string expression = "countbits(" + ci(op2) + ")";
							if (!emitIntegerAssignment(op1, expression.c_str(), true))
							{
								sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
								appendOutput(buffer);
							}
						}
						removeBoolean(op1);
						break;

						// Add the Firstbit ops, because now Just Cause 3 uses them.
						// firstbit{_hi|_lo|_shi} dest[.mask], src0[.swizzle]
					case OPCODE_FIRSTBIT_HI:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						bitcastToUInt(op2);
					{
						string expression = "firstbithigh(" + ci(op2) + ")";
						if (!emitIntegerAssignment(op1, expression.c_str(), true))
						{
							sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
							appendOutput(buffer);
						}
					}
						removeBoolean(op1);
						break;
					case OPCODE_FIRSTBIT_LO:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						bitcastToUInt(op2);
					{
						string expression = "firstbitlow(" + ci(op2) + ")";
						if (!emitIntegerAssignment(op1, expression.c_str(), true))
						{
							sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
							appendOutput(buffer);
						}
					}
						removeBoolean(op1);
						break;
					case OPCODE_FIRSTBIT_SHI:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						bitcastToInt(op2);
					{
						string expression = "firstbithigh(" + ci(op2) + ")";
						if (!emitIntegerAssignment(op1, expression.c_str(), false))
						{
							sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
							appendOutput(buffer);
						}
					}
						removeBoolean(op1);
						break;

						// Code generation for this weird instruction is tuned to indent the way we want, 
						// and still look like a single instruction.  Still has weird indent in middle of instruction,
						// but it seems more valuable to have it be a single line.
						//
						// Meh, you think this is weird? Looks like a less powerful version of rlwinm to me
					case OPCODE_UBFE:
					{
						remapTarget(op1);
						removeBoolean(op1);
						applySwizzle(op1, op2);	// width
						applySwizzle(op1, op3); // offset
						applySwizzle(op1, op4);
						DeferredWrite result = deferWrite(op1, "uint4");
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op5, "%s.%c", result.temporary.c_str(), *pop1);
							sprintf(buffer, "  if (%s == 0) %s = 0; else if (%s+%s < 32) { ",
								ci(GetSuffix(op2, idx)).c_str(), writeTarget(op5), ci(GetSuffix(op2, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str());
							appendOutput(buffer);
							// FIXME: May need fixup for read from constant buffer of unidentified type?
							sprintf(buffer, "%s = (uint)%s << (32-((uint)%s + (uint)%s)); %s = (uint)%s >> (32-(uint)%s); ", writeTarget(op5), ci(GetSuffix(op4, idx)).c_str(), ci(GetSuffix(op2, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str(), writeTarget(op5), writeTarget(op5), ci(GetSuffix(op2, idx)).c_str());
							appendOutput(buffer);
							sprintf(buffer, " } else %s = (uint)%s >> (uint)%s;\n",
								writeTarget(op5), ci(GetSuffix(op4, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str());
							appendOutput(buffer);
							++idx;
						}
						commitWrite(result);
						break;
					}

					case OPCODE_IBFE:
						{
							// Instruction for sign extending field extraction. Used in new versions of Dolphin
							// ibfe r0.xyzw, l(24, 24, 24, 24), l(0, 0, 0, 0), cb0[12].xyzw
							remapTarget(op1);
							removeBoolean(op1);
							applySwizzle(op1, op2, true);
							applySwizzle(op1, op3, true);
							applySwizzle(op1, op4);
							sprintf(buffer, "  %s = asfloat((%s == 0 ? 0 : ("
										"%s + %s < 32 ? ("
											"((int%s)%s << (32 - %s - %s)) >> (32 - %s)"
										") : ("
											"(int%s)%s >> %s"
									"))));\n",
									writeTarget(op1), ci(op2).c_str(),
										ci(op2).c_str(), ci(op3).c_str(),
											swizCount(op4).c_str(), ci(op4).c_str(), ci(op2).c_str(), ci(op3).c_str(), ci(op2).c_str(),
											swizCount(op4).c_str(), ci(op4).c_str(), ci(op3).c_str()
								);

							appendOutput(buffer);
						}
						break;

					case OPCODE_BFREV:
						remapTarget(op1);
						applySwizzle(op1, op2);
						bitcastToUInt(op2);
						sprintf(buffer, "  %s = asfloat(reversebits(%s));\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

					case OPCODE_EXP:
						remapTarget(op1);
						applySwizzle(op1, op2);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = exp2(%s);\n", writeTarget(op1), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(exp2(%s));\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

					case OPCODE_LOG:
					{
						const int lookahead = 6;
						if (shader->dx9Shader && (iNr + lookahead < inst_count)) // FIXME: This is actually a lookahead code path and may not be DX9 specific
						{
							Instruction * nextIns[lookahead];
							for (int i = 0; i < lookahead; i++)
							{
								nextIns[i] = &(*instructions)[iNr + i + 1];
							}

							if (nextIns[0]->eOpcode == OPCODE_LOG && nextIns[1]->eOpcode == OPCODE_LOG && nextIns[2]->eOpcode == OPCODE_MUL &&
								nextIns[3]->eOpcode == OPCODE_EXP && nextIns[4]->eOpcode == OPCODE_EXP && nextIns[5]->eOpcode == OPCODE_EXP &&
								instr->asOperands[1].ui32RegisterNumber == nextIns[0]->asOperands[1].ui32RegisterNumber &&
								nextIns[0]->asOperands[1].ui32RegisterNumber == nextIns[1]->asOperands[1].ui32RegisterNumber)
							{
								string op1Str;
								string op3Str;

								//read next instruction
								for (int i = 0; i < lookahead; i++)
								{
									while (c[pos] != 0x0a && pos < size) pos++; pos++;

									if (ReadStatement(c + pos) < 1)
									{
										logDecompileError("Error parsing statement: " + string(c + pos, 80));
										return;
									}

									if (i == 2)
									{
										op1Str = op1;
										applySwizzle(op1, op3);
										op3Str = op3;
									}
								}

								sprintf(buffer, "  r%d.%s%s%s = pow(r%d.%s%s%s, %s);\n", nextIns[3]->asOperands[0].ui32RegisterNumber, GetComponentStrFromInstruction(nextIns[3], 0).c_str(),
									GetComponentStrFromInstruction(nextIns[4], 0).c_str(), GetComponentStrFromInstruction(nextIns[5], 0).c_str(),
									instr->asOperands[1].ui32RegisterNumber, GetComponentStrFromInstruction(instr, 1).c_str(), GetComponentStrFromInstruction(nextIns[0], 1).c_str(),
									GetComponentStrFromInstruction(nextIns[1], 1).c_str(), op3Str.c_str());

								appendOutput(buffer);

								while (c[pos] != 0x0a && pos < size) pos++; pos++;
								mLastStatement = nextIns[5];
								iNr += lookahead + 1;
								continue;
							}
						}

						remapTarget(op1);
						applySwizzle(op1, op2);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = log2(%s);\n", writeTarget(op1), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(log2(%s));\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

						// Opcodes for Sqrt, Min, Max, IMin, IMax all were using a 'statement' that is parsed
						// from the text ASM.  This did not match the Mov, or Add or other opcodes, and was
						// generating errors when we'd see 'max_sat'.  Anything with saturation added of these
						// 5 could generate an error.
						// This fix removes the dependency on 'statement', and codes the generated HLSL line directly.
						// We are guessing, but it appears that this was a left-over from a conversion to using the
						// James-Jones opcode parser as the primary parser.
					case OPCODE_SQRT:
						remapTarget(op1);
						applySwizzle(op1, op2);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = sqrt(%s);\n", writeTarget(op1), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(sqrt(%s));\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

						// Minor tweak, but if we reverse the order of Min/Max parameters here, the
						// recompile comes out identical.
					case OPCODE_MIN:
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = min(%s, %s);\n", writeTarget(op1), ci(op3).c_str(), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(min(%s, %s));\n", writeTarget(op1), ci(op3).c_str(), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

					// Missing opcode for UMin, used in Dragon Age
					case OPCODE_UMIN:
					{
						remapTarget(op1);
						char *source0 = integerImmediate(op2, instr->asOperands[1]);
						char *source1 = integerImmediate(op3, instr->asOperands[2]);
						string expression = integerMinMaxExpression(op1, source0, source1, true, false);
						sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

					// Missing opcode for UMax, used in Witcher3
					case OPCODE_UMAX:
					{
						remapTarget(op1);
						char *source0 = integerImmediate(op2, instr->asOperands[1]);
						char *source1 = integerImmediate(op3, instr->asOperands[2]);
						string expression = integerMinMaxExpression(op1, source0, source1, true, true);
						sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

						// Add remaining atomic ops, we see atomic_or in Song of the Deep.
						// Needs an unclear manual fix, but better than not generating any HLSL at all.
						// Opcodes found in Witcher3 Compute Shader, manual fix needed.
					case OPCODE_ATOMIC_AND:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedAnd(dest, value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_ATOMIC_OR:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedOr(dest, value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_ATOMIC_XOR:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedXor(dest, value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_ATOMIC_CMP_STORE:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedCompareStore(dest, value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_ATOMIC_IADD:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedAdd(dest, value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_ATOMIC_IMAX:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedMax(dest, value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_ATOMIC_IMIN:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedMin(dest, value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_ATOMIC_UMAX:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedMax(dest, value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_ATOMIC_UMIN:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedMin(dest, value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_IMM_ATOMIC_ALLOC:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedExchange ?(dest, value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_IMM_ATOMIC_CONSUME:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  Interlocked... ?(dest, value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_IMM_ATOMIC_IADD:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedAdd(dest, imm_value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_IMM_ATOMIC_AND:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedAnd(dest, imm_value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_IMM_ATOMIC_OR:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedOr(dest, imm_value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_IMM_ATOMIC_XOR:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedXor(dest, imm_value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_IMM_ATOMIC_EXCH:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedExchange(dest, imm_value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_IMM_ATOMIC_CMP_EXCH:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedCompareExchange(dest, compare_value, imm_value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_IMM_ATOMIC_IMAX:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedMax(dest, imm_value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_IMM_ATOMIC_IMIN:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedMin(dest, imm_value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_IMM_ATOMIC_UMAX:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedMax(dest, imm_value, orig_value);\n");
						appendOutput(buffer);
						break;
					}
					case OPCODE_IMM_ATOMIC_UMIN:
					{
						sprintf(buffer, "  // Needs manual fix for instruction:\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "  InterlockedMin(dest, imm_value, orig_value);\n");
						appendOutput(buffer);
						break;
					}


					case OPCODE_MAX:
					{
						const int lookahead = 1;
						if (shader->dx9Shader && (iNr + lookahead < inst_count)) // FIXME: This is actually a lookahead code path and may not be DX9 specific
						{
							Instruction * nextIns = &(*instructions)[iNr + 1];
							if (nextIns->eOpcode == OPCODE_MAD &&
								IsInstructionOperandSame(instr, 3, nextIns, 3, GetComponentStrFromInstruction(instr, 0).c_str(), GetComponentStrFromInstruction(nextIns, 0).c_str()) == 2 &&
								IsInstructionOperandSame(instr, 0, nextIns, 2, NULL, GetComponentStrFromInstruction(nextIns, 0).c_str()) == 1)
							{
								applySwizzle(op1, op2);
								applySwizzle(op1, op3);

								char y[opcodeSize];
								sprintf_s(y, opcodeSize, "%s * %s", op2, op3);

								//read next instruction
								for (int i = 0; i < lookahead; i++)
								{
									while (c[pos] != 0x0a && pos < size) pos++; pos++;

									if (ReadStatement(c + pos) < 1)
									{
										logDecompileError("Error parsing statement: " + string(c + pos, 80));
										return;
									}
								}

								remapTarget(op1);
								applySwizzle(op1, op2);
								applySwizzle(op1, op4);
								sprintf_s(buffer, opcodeSize, "  %s = lerp(%s, %s, %s);\n", op1, op4, y, op2);
								appendOutput(buffer);

								while (c[pos] != 0x0a && pos < size) pos++; pos++;
								mLastStatement = nextIns;
								iNr += lookahead + 1;
								continue;
							}
						}

						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = max(%s, %s);\n", writeTarget(op1), ci(op3).c_str(), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(max(%s, %s));\n", writeTarget(op1), ci(op3).c_str(), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}
					case OPCODE_IMIN:
					{
						remapTarget(op1);
						char *source0 = integerImmediate(op2, instr->asOperands[1]);
						char *source1 = integerImmediate(op3, instr->asOperands[2]);
						string expression = integerMinMaxExpression(op1, source0, source1, false, false);
						if (instr->bSaturate)
							expression = "saturate(" + expression + ")";
						sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}
					case OPCODE_IMAX:
					{
						remapTarget(op1);
						char *source0 = integerImmediate(op2, instr->asOperands[1]);
						char *source1 = integerImmediate(op3, instr->asOperands[2]);
						string expression = integerMinMaxExpression(op1, source0, source1, false, true);
						if (instr->bSaturate)
							expression = "saturate(" + expression + ")";
						sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), expression.c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

					case OPCODE_MAD:
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						applySwizzle(op1, fixImm(op4, instr->asOperands[3]));
						// Check for operation reorder.
						/*
						if (mLastStatement && mLastStatement->eOpcode == OPCODE_MUL && strstr(op4, mMulTarget.c_str()) &&
						mMulTarget.compare(0, 3, mMulOperand, mMulOperand[0] == '-' ? 1 : 0, 3) &&
						mMulTarget.compare(0, 3, mMulOperand2, mMulOperand2[0] == '-' ? 1 : 0, 3))
						sprintf(op4 + ((op4[0] == '-') ? 1 : 0), "(%s * %s)", mMulOperand.c_str(), mMulOperand2.c_str());
						*/
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = %s * %s + %s;\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str(), ci(op4).c_str());
						else
							sprintf(buffer, "  %s = saturate(%s * %s + %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str(), ci(op4).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

					case OPCODE_IMAD:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						applySwizzle(op1, op4, true);
						bitcastToInt(op2);
						bitcastToInt(op3);
						bitcastToInt(op4);
						sprintf(buffer, "  %s = asfloat(mad(%s, %s, %s));\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str(), ci(op4).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					case OPCODE_UMAD:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						applySwizzle(op1, op4, true);
						bitcastToUInt(op2);
						bitcastToUInt(op3);
						bitcastToUInt(op4);
						sprintf(buffer, "  %s = asfloat(mad(%s, %s, %s));\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str(), ci(op4).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

					case OPCODE_DP2:
						remapTarget(op1);
						applySwizzle(".xy", fixImm(op2, instr->asOperands[1]));
						applySwizzle(".xy", fixImm(op3, instr->asOperands[2]));
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = dot(%s, %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						else
							sprintf(buffer, "  %s = saturate(dot(%s, %s));\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

					case OPCODE_DP3:
					{
						const int lookahead = 2;
						if (shader->dx9Shader && (iNr + lookahead < inst_count)) // FIXME: This is actually a lookahead code path and may not be DX9 specific
						{
							Instruction * nextIns[lookahead];
							for (int i = 0; i < lookahead; i++)
							{
								nextIns[i] = &(*instructions)[iNr + i + 1];
							}

							string outputOp1 = GetComponentStrFromInstruction(nextIns[1], 0);

							if (nextIns[0]->eOpcode == OPCODE_ADD && nextIns[1]->eOpcode == OPCODE_MAD &&
								IsInstructionOperandSame(instr, 0, nextIns[0], 1) == 1 && IsInstructionOperandSame(instr, 0, nextIns[0], 2) == 1 &&
								IsInstructionOperandSame(nextIns[0], 0, nextIns[1], 2) == 2 &&
								IsInstructionOperandSame(instr, 1, nextIns[1], 3, NULL, outputOp1.c_str()) == 1 && IsInstructionOperandSame(instr, 2, nextIns[1], 1, NULL, outputOp1.c_str()) == 1)
							{
								//read next instruction
								for (int i = 0; i < lookahead; i++)
								{
									while (c[pos] != 0x0a && pos < size) pos++; pos++;

									if (ReadStatement(c + pos) < 1)
									{
										logDecompileError("Error parsing statement: " + string(c + pos, 80));
										return;
									}
								}

								remapTarget(op1);
								sprintf_s(op2, opcodeSize, "r%d.%s", nextIns[1]->asOperands[3].ui32RegisterNumber, GetComponentStrFromInstruction(nextIns[1], 3).c_str());
								sprintf_s(op3, opcodeSize, "r%d.%s", nextIns[1]->asOperands[1].ui32RegisterNumber, GetComponentStrFromInstruction(nextIns[1], 1).c_str());
								applySwizzle(op1, op2);
								applySwizzle(op1, op3);

								sprintf(buffer, "  %s = reflect(%s, %s);\n", writeTarget(op1), op2, op3);

								appendOutput(buffer);

								while (c[pos] != 0x0a && pos < size) pos++; pos++;
								mLastStatement = nextIns[1];
								iNr += lookahead + 1;
								continue;
							}
						}

						remapTarget(op1);
						applySwizzle(".xyz", fixImm(op2, instr->asOperands[1]));
						applySwizzle(".xyz", fixImm(op3, instr->asOperands[2]));
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = dot(%s, %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						else
							sprintf(buffer, "  %s = saturate(dot(%s, %s));\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

					case OPCODE_DP4:
					{
						const int lookahead = 1;
						if (shader->dx9Shader && (iNr + lookahead < inst_count)) // FIXME: This is actually a lookahead code path and may not be DX9 specific
						{
							remapTarget(op1);
							Instruction * nextInstr = &(*instructions)[iNr + 1];
							string outputOp0 = GetComponentStrFromInstruction(instr, 0);

							//nrm generate two instructions，dp4 and rsq
							if (nextInstr->eOpcode == OPCODE_RSQ && outputOp0.size() == 3)
							{
								applySwizzle(op1, op2);
								sprintf(buffer, "  %s = normalize(%s);\n", writeTarget(op1), ci(op2).c_str());
								appendOutput(buffer);

								//asm just one line，don't need call ReadStatement
								//have two instructions
								iNr++;

								// NOTE: NO CONTINUE HERE - NEED ONE BEFORE ELIMINATING DUPLICATE CODE BELOW
								// AND NEED A REGRESSION TEST BEFORE DOING THAT.
							}
							else
							{
								// XXX NOTE Duplicated code below!!!
								applySwizzle(".xyzw", fixImm(op2, instr->asOperands[1]));
								applySwizzle(".xyzw", fixImm(op3, instr->asOperands[2]));
								if (!instr->bSaturate)
									sprintf(buffer, "  %s = dot(%s, %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
								else
									sprintf(buffer, "  %s = saturate(dot(%s, %s));\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
								appendOutput(buffer);
								// XXX NOTE Duplicated code below!!!
							}
						}
						else
						{
							// XXX NOTE Duplicated code above!!!
							remapTarget(op1);
							applySwizzle(".xyzw", fixImm(op2, instr->asOperands[1]));
							applySwizzle(".xyzw", fixImm(op3, instr->asOperands[2]));
							if (!instr->bSaturate)
								sprintf(buffer, "  %s = dot(%s, %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
							else
								sprintf(buffer, "  %s = saturate(dot(%s, %s));\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
							appendOutput(buffer);
							removeBoolean(op1);
							// XXX NOTE Duplicated code above!!!
						}
						break;
					}
					case OPCODE_DP2ADD:
						remapTarget(op1);
						applySwizzle(".xy", op2);
						applySwizzle(".xy", op3);
						applySwizzle(".xy", op4);
						sprintf(buffer, "  %s = dot2(%s, %s) + %s;\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str(), ci(op4).c_str());
						appendOutput(buffer);

						//dx9
						break;

						//dx9
					case OPCODE_LRP:
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						applySwizzle(op1, op4);
						sprintf(buffer, "  %s = lerp(%s, %s, %s);\n", writeTarget(op1), ci(op4).c_str(), ci(op3).c_str(), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;

					case OPCODE_POW:
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						sprintf(buffer, "  %s = pow(%s, %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						appendOutput(buffer);
						break;
						//dx9
					case OPCODE_RSQ:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						if (!instr->bSaturate) {
							// The DX9 port switched this to 1/sqrt(), however
							// it is unclear why that was necessary - rsqrt
							// should work in everything since vs_1_1 and
							// ps_2_0 (and in fact the regular sqrt didn't
							// exist until shader model 4). Look up "fast
							// inverse square root" to have your mind blown and
							// get an idea of why this matters.
							//
							// Reverting this to rsqrt since the DX9 decompiler
							// support is clearly unfinished and no explanation
							// for this change was provided.
							//
							sprintf(buffer, "  %s = rsqrt(%s);\n", writeTarget(op1), ci(op2).c_str());
						} else {
							sprintf(buffer, "  %s = saturate(rsqrt(%s));\n", writeTarget(op1), ci(op2).c_str());
						}
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

					// Double checked this while looking at other round bugs.  Looks correct to use 'floor'.
					case OPCODE_ROUND_NI:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = floor(%s);\n", writeTarget(op1), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(floor(%s));\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}
					case OPCODE_ROUND_PI:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = ceil(%s);\n", writeTarget(op1), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(ceil(%s));\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

					// Was previously doing a Round operation, but that is not correct because this needs to
					// round toward zero, and Round can go larger.  Trunc(1.6)->1.0 Round(1.6)->2.0
					// Also removed the unrolling, the instruction works with swizzle. e.g. r0.xy = trunc(r2.yz)
					case OPCODE_ROUND_Z:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = trunc(%s);\n", writeTarget(op1), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(trunc(%s));\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

					// Round_NE is Round Nearest Even, and using HLSL Round here is correct.
					// But it previously used a *0.5*2 rounding which is unnecessary.
					// The HLSL intrinsics of Round will already do that.
					case OPCODE_ROUND_NE:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = round(%s);\n", writeTarget(op1), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(round(%s));\n", writeTarget(op1), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}
					case OPCODE_FTOI:
						remapTarget(op1);
						applySwizzle(op1, op2);
						{
							string alias = declareIntegerAlias(op1, false);
							if (!alias.empty())
							{
								sprintf(buffer, "  %s = %s;\n", alias.c_str(), ci(castToInt(op2)).c_str());
								appendOutput(buffer);
								sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), alias.c_str());
								appendOutput(buffer);
							}
							else
							{
								sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), ci(castToInt(op2)).c_str());
								appendOutput(buffer);
							}
						}
						removeBoolean(op1);
						break;

					case OPCODE_FTOU:
						remapTarget(op1);
						applySwizzle(op1, op2);
						{
							string alias = declareIntegerAlias(op1, true);
							if (!alias.empty())
							{
								sprintf(buffer, "  %s = %s;\n", alias.c_str(), ci(castToUInt(op2)).c_str());
								appendOutput(buffer);
								sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), alias.c_str());
								appendOutput(buffer);
							}
							else
							{
								sprintf(buffer, "  %s = asfloat(%s);\n", writeTarget(op1), ci(castToUInt(op2)).c_str());
								appendOutput(buffer);
							}
						}
						removeBoolean(op1);
						break;

					case OPCODE_SINCOS:
						remapTarget(op1);
						remapTarget(op2);
						if (!strncmp(op1, "null", 4))
							applySwizzle(op2, op3);
						else
							applySwizzle(op1, op3);
						if (!strncmp(op1, "null", 4))
							sprintf(buffer, "  %s = cos(%s);\n", writeTarget(op2), ci(op3).c_str());
						else if (!strncmp(op2, "null", 4))
							sprintf(buffer, "  %s = sin(%s);\n", writeTarget(op1), ci(op3).c_str());
						else
							sprintf(buffer, "  sincos(%s, %s, %s);\n", ci(op3).c_str(), writeTarget(op1), writeTarget(op2));
						appendOutput(buffer);
						removeBoolean(op1);
						removeBoolean(op2);
						break;

						// Failing case of: "movc_sat r2.xyzw, r2.xxxx, r7.xyzw, r4.xyzw"
						// Turned into: 
						// r2.x = saturate(r2.x ? r7.x : r4.x);
						// r2.y = saturate(r2.x ? r7.y : r4.y);
						// r2.z = saturate(r2.x ? r7.z : r4.z);
						// r2.w = saturate(r2.x ? r7.w : r4.w);
						// which damages r2.x at the first line, and uses it in each. 
						// Changed it to just be:   "r2.xyzw = saturate(r2.xxxx ? r7.xyzw : r4.xyzw);"
						// But I'm not sure why this was unrolled to begin with.
					case OPCODE_MOVC:
					{
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						applySwizzle(op1, fixImm(op4, instr->asOperands[3]));
						string condition = isBoolean(op2) ? conditionExpression(op2, false) : ci(op2);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = %s ? %s : %s;\n", writeTarget(op1), condition.c_str(), ci(op3).c_str(), ci(op4).c_str());
						else
							sprintf(buffer, "  %s = saturate(%s ? %s : %s);\n", writeTarget(op1), condition.c_str(), ci(op3).c_str(), ci(op4).c_str());
						appendOutput(buffer);

						//int idx = 0;
						//char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						//char *pop2 = strrchr(op2, '.'); if (pop2) *pop2 = 0;
						//while (*++pop1)
						//{
						//if (pop1) sprintf(op5, "%s.%c", op1, *pop1); else sprintf(op5, "%s", op1);
						//if (pop2) sprintf(op6, "%s.%c", op2, *++pop2); else sprintf(op6, "%s", op2);
						//if (!instr->bSaturate)
						//	sprintf(buffer, "  %s = %s ? %s : %s;\n", writeTarget(op5), ci(op6).c_str(), ci(GetSuffix(op3, idx)).c_str(), ci(GetSuffix(op4, idx)).c_str());
						//else
						//	sprintf(buffer, "  %s = saturate(%s ? %s : %s);\n", writeTarget(op5), ci(op6).c_str(), ci(GetSuffix(op3, idx)).c_str(), ci(GetSuffix(op4, idx)).c_str());
						//appendOutput(buffer);
						//	++idx;
						//}
						removeBoolean(op1);
						break;
					}

					// Big change to all these boolean test opcodes.  All were unrolled and generated a code line per component.
					// To make the boolean tests work correctly for AND, these were rolled back into one, and added to the boolean
					// set list as a complete operand, like 'r0.xyw'.
					case OPCODE_NE:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						sprintf(buffer, "  %s = cmp(%s != %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						appendOutput(buffer);
						addBoolean(op1);
						break;
					}
					case OPCODE_INE:
					{
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						bitcastToInt(op2);
						bitcastToInt(op3);
						sprintf(buffer, "  %s = cmp(%s != %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						appendOutput(buffer);
						addBoolean(op1);
						break;
					}
					case OPCODE_EQ:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						sprintf(buffer, "  %s = cmp(%s == %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						appendOutput(buffer);
						addBoolean(op1);
						break;
					}
					case OPCODE_IEQ: 
					{
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						bitcastToInt(op2);
						bitcastToInt(op3);
						sprintf(buffer, "  %s = cmp(%s == %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						appendOutput(buffer);
						addBoolean(op1);
						break;
					}
					case OPCODE_LT:
					{
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						sprintf(buffer, "  %s = cmp(%s < %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						appendOutput(buffer);
						addBoolean(op1);
						break;
					}
					case OPCODE_ILT:
					{
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						bitcastToInt(op2);
						bitcastToInt(op3);
						sprintf(buffer, "  %s = cmp(%s < %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						appendOutput(buffer);
						addBoolean(op1);
						break;
					}
					case OPCODE_ULT:
					{
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						bitcastToUInt(op2);
						bitcastToUInt(op3);
						sprintf(buffer, "  %s = cmp(%s < %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						appendOutput(buffer);
						addBoolean(op1);
						break;
					}
					case OPCODE_GE:
					{
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						sprintf(buffer, "  %s = cmp(%s >= %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						appendOutput(buffer);
						addBoolean(op1);
						break;
					}
					case OPCODE_IGE:
					{
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						bitcastToInt(op2);
						bitcastToInt(op3);
						sprintf(buffer, "  %s = cmp(%s >= %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						appendOutput(buffer);
						addBoolean(op1);
						break;
					}
					case OPCODE_UGE:
					{
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						bitcastToUInt(op2);
						bitcastToUInt(op3);
						sprintf(buffer, "  %s = cmp(%s >= %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						appendOutput(buffer);
						addBoolean(op1);
						break;
					}

					// Switch statement in HLSL was missing. Added because AC4 uses it.
					case OPCODE_SWITCH:
						applySwizzle(".x", op1);
						bitcastToUInt(op1);
						sprintf(buffer, "  switch (%s) {\n", ci(op1).c_str());
						appendOutput(buffer);
						mIntegerAliases.clear();
						break;
					case OPCODE_CASE:
						mIntegerAliases.clear();
						applySwizzle(".x", integerImmediate(op1, instr->asOperands[0]), true);
						sprintf(buffer, "  case %s :", op1);
						appendOutput(buffer);
						break;
					case OPCODE_ENDSWITCH:
						mIntegerAliases.clear();
						sprintf(buffer, "  }\n");
						appendOutput(buffer);
						break;
					case OPCODE_DEFAULT:
						mIntegerAliases.clear();
						sprintf(buffer, "  default :\n");
						appendOutput(buffer);
						break;

					case OPCODE_IF:
						applySwizzle(".x", op1);
						{
							string condition = conditionExpression(op1, instr->eBooleanTestType == INSTRUCTION_TEST_ZERO);
							sprintf(buffer, "  if %s {\n", condition.c_str());
						}
						appendOutput(buffer);
						break;
					case OPCODE_ELSE:
						mIntegerAliases.clear();
						sprintf(buffer, "  } else {\n");
						appendOutput(buffer);
						break;
					case OPCODE_ENDIF:
						mIntegerAliases.clear();
						sprintf(buffer, "  }\n");
						appendOutput(buffer);
						break;

					case OPCODE_LOOP:
						// A preheader alias does not describe later iterations.
						// Use the register copy at joins until loop-carried values
						// have a proper reaching-definition analysis.
						mIntegerAliases.clear();
						sprintf(buffer, "  while (true) {\n");
						appendOutput(buffer);
						break;
					case OPCODE_BREAK:
						sprintf(buffer, "  break;\n");
						appendOutput(buffer);
						break;
					case OPCODE_BREAKC:
						applySwizzle(".x", op1);
						{
							string condition = conditionExpression(op1, instr->eBooleanTestType == INSTRUCTION_TEST_ZERO);
							sprintf(buffer, "  if %s break;\n", condition.c_str());
						}
						appendOutput(buffer);
						break;
					case OPCODE_CONTINUE:
						sprintf(buffer, "  continue;\n");
						appendOutput(buffer);
						break;
					case OPCODE_CONTINUEC:
						applySwizzle(".x", op1);
						{
							string condition = conditionExpression(op1, instr->eBooleanTestType == INSTRUCTION_TEST_ZERO);
							sprintf(buffer, "  if %s continue;\n", condition.c_str());
						}
						appendOutput(buffer);
						break;
					case OPCODE_ENDLOOP:
						mIntegerAliases.clear();
						sprintf(buffer, "  }\n");
						appendOutput(buffer);
						break;
					
						// Found in Witcher3 Compute Shaders 
					case OPCODE_SYNC:
						if (!strcmp(statement, "sync_g_t"))
							sprintf(buffer, "  GroupMemoryBarrierWithGroupSync();\n");
						else
							sprintf(buffer, "  Unknown sync instruction;\n");
						appendOutput(buffer);
						break;

					case OPCODE_SWAPC:
					{
						remapTarget(op1);
						remapTarget(op2);
						removeBoolean(op1);		// The code damages the op1, op2 below.
						removeBoolean(op2);
						applySwizzle(op1, op3);
						applySwizzle(op1, op4);
						applySwizzle(op1, op5);
						DeferredWrite first = deferWrite(op1);
						DeferredWrite second = deferWrite(op2);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						char *pop2 = strrchr(op2, '.'); if (pop2) *pop2 = 0;
						char *pop3 = strrchr(op3, '.'); if (pop3) *pop3 = 0;
						while (*++pop1)
						{
							sprintf(op6, "%s.%c", first.temporary.c_str(), *pop1);
							if (pop2) sprintf(op7, "%s.%c", second.temporary.c_str(), *++pop2); else sprintf(op7, "%s", op2);
							if (pop3) sprintf(op8, "%s.%c", op3, *++pop3); else sprintf(op8, "%s", op3);
							// FIXME: May need fixup for read from constant buffer of unidentified type
							sprintf(buffer, "  %s = (int)%s ? %s : %s; %s = (int)%s ? %s : %s;\n",
								writeTarget(op6), ci(op8).c_str(), ci(GetSuffix(op5, idx)).c_str(), ci(GetSuffix(op4, idx)).c_str(),
								writeTarget(op7), ci(op8).c_str(), ci(GetSuffix(op4, idx)).c_str(), ci(GetSuffix(op5, idx)).c_str());
							appendOutput(buffer);
							++idx;
						}
						commitWrite(first);
						commitWrite(second);
						break;
					}

					// This generated code needed to change because the fxc compiler generates bad code when
					// using the sample that they specify in the documentation at:
					// http://msdn.microsoft.com/en-us/library/windows/desktop/hh446837(v=vs.85).aspx
					// I worked out the alternate technique that works for all, and does not tickle
					// the bug that treats 0x80000000 as "-0" as a uint, where it should not exist.
					case OPCODE_BFI:
					{
						remapTarget(op1);
						removeBoolean(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						applySwizzle(op1, op4);
						applySwizzle(op1, op5);
						DeferredWrite result = deferWrite(op1, "uint4");
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op6, "%s.%c", result.temporary.c_str(), *pop1);

							// Fails: bitmask.%c = (((1 << %s) - 1) << %s) & 0xffffffff;

							// FIXME: May need fixup for read from constant buffer of unidentified type
							sprintf(buffer, "  bitmask.%c = ((~(-1 << (uint)%s)) << (uint)%s) & 0xffffffff;"
								"  %s = (((uint)%s << (uint)%s) & bitmask.%c) | ((uint)%s & ~bitmask.%c);\n",
								*pop1, ci(GetSuffix(op2, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str(),
								writeTarget(op6), ci(GetSuffix(op4, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str(), *pop1, ci(GetSuffix(op5, idx)).c_str(), *pop1);
							appendOutput(buffer);
							++idx;
						}
						commitWrite(result);
						break;
					}

					// Was missing the sample_aoffimmi variant. Added as matching sample_b type. Used in FC4.
					case OPCODE_SAMPLE:
					{
						if (shader->dx9Shader)
						{
							remapTarget(op1);
							applySwizzle(".xyzw", op2);

							int textureId = atoi(&op3[1]);
							sprintf(buffer, "  %s = %s.Sample(%s);\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), ci(op2).c_str());

							appendOutput(buffer);
						}
						else
						{
							//	else if (!strncmp(statement, "sample_indexable", strlen("sample_indexable")))
							remapTarget(op1);
							applySwizzle(".xyzw", op2);
							applySwizzle(op1, op3);
							int textureId, samplerId;
							sscanf_s(op3, "t%d.", &textureId);
							sscanf_s(op4, "s%d", &samplerId);
							truncateTexturePos(op2, mTextureType[textureId].c_str());
							truncateTextureSwiz(op1, mTextureType[textureId].c_str());
							truncateTextureSwiz(op3, mTextureType[textureId].c_str());
							if (!instr->bAddressOffset)
								sprintf(buffer, "  %s = %s.Sample(%s, %s)%s;\n", writeTarget(op1),
									mTextureNames[textureId].c_str(), mSamplerNames[samplerId].c_str(), ci(op2).c_str(), strrchr(op3, '.'));
							else
							{
								int offsetx = 0, offsety = 0, offsetz = 0;
								sscanf_s(statement, "sample_aoffimmi(%d,%d,%d", &offsetx, &offsety, &offsetz);
								sprintf(buffer, "  %s = %s.Sample(%s, %s, int2(%d, %d))%s;\n", writeTarget(op1),
									mTextureNames[textureId].c_str(), mSamplerNames[samplerId].c_str(), ci(op2).c_str(),
									offsetx, offsety, strrchr(op3, '.'));
							}
							appendOutput(buffer);
							removeBoolean(op1);
						}

						break;
					}

					// Missing opcode for WatchDogs.  Very similar to SAMPLE_L, so copied from there.
					case OPCODE_SAMPLE_B:
					{
						remapTarget(op1);
						applySwizzle(".xyzw", op2);
						applySwizzle(op1, op3);
						applySwizzle(".x", fixImm(op5, instr->asOperands[4]));
						int textureId, samplerId;
						sscanf_s(op3, "t%d.", &textureId);
						sscanf_s(op4, "s%d", &samplerId);
						truncateTexturePos(op2, mTextureType[textureId].c_str());
						truncateTextureSwiz(op1, mTextureType[textureId].c_str());
						truncateTextureSwiz(op3, mTextureType[textureId].c_str());
						if (!instr->bAddressOffset)
							sprintf(buffer, "  %s = %s.SampleBias(%s, %s, %s)%s;\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerNames[samplerId].c_str(), ci(op2).c_str(), ci(op5).c_str(), strrchr(op3, '.'));
						else
						{
							int offsetx = 0, offsety = 0, offsetz = 0;
							sscanf_s(statement, "sample_b_aoffimmi_indexable(%d,%d,%d", &offsetx, &offsety, &offsetz);
							sprintf(buffer, "  %s = %s.SampleBias(%s, %s, %s, int2(%d, %d))%s;\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerNames[samplerId].c_str(), ci(op2).c_str(), ci(op5).c_str(),
								offsetx, offsety, strrchr(op3, '.'));
						}
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}
					case OPCODE_SAMPLE_L:
					{
						remapTarget(op1);
						applySwizzle(".xyzw", op2);
						applySwizzle(op1, op3);
						applySwizzle(".x", fixImm(op5, instr->asOperands[4]));
						int textureId, samplerId;
						sscanf_s(op3, "t%d.", &textureId);
						sscanf_s(op4, "s%d", &samplerId);
						truncateTexturePos(op2, mTextureType[textureId].c_str());
						truncateTextureSwiz(op1, mTextureType[textureId].c_str());
						truncateTextureSwiz(op3, mTextureType[textureId].c_str());
						if (!instr->bAddressOffset)
							sprintf(buffer, "  %s = %s.SampleLevel(%s, %s, %s)%s;\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerNames[samplerId].c_str(), ci(op2).c_str(), ci(op5).c_str(), strrchr(op3, '.'));
						else
						{
							int offsetx = 0, offsety = 0, offsetz = 0;
							sscanf_s(statement, "sample_l_aoffimmi_indexable(%d,%d,%d", &offsetx, &offsety, &offsetz);
							sprintf(buffer, "  %s = %s.SampleLevel(%s, %s, %s, int2(%d, %d))%s;\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerNames[samplerId].c_str(), ci(op2).c_str(), ci(op5).c_str(),
								offsetx, offsety, strrchr(op3, '.'));
						}
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}
					case OPCODE_SAMPLE_D:
					{
						remapTarget(op1);
						applySwizzle(".xyzw", op2);
						applySwizzle(op1, op3);
						applySwizzle(op1, fixImm(op5, instr->asOperands[4]));
						applySwizzle(op1, fixImm(op6, instr->asOperands[5]));
						int textureId, samplerId;
						sscanf_s(op3, "t%d.", &textureId);
						sscanf_s(op4, "s%d", &samplerId);
						truncateTexturePos(op2, mTextureType[textureId].c_str());
						truncateTextureSwiz(op1, mTextureType[textureId].c_str());
						truncateTextureSwiz(op3, mTextureType[textureId].c_str());
						if (!instr->bAddressOffset)
							sprintf(buffer, "  %s = %s.SampleGrad(%s, %s, %s, %s)%s;\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerNames[samplerId].c_str(), ci(op2).c_str(), ci(op5).c_str(), ci(op6).c_str(), strrchr(op3, '.'));
						else
						{
							int offsetx = 0, offsety = 0, offsetz = 0;
							sscanf_s(statement, "sample_d_aoffimmi_indexable(%d,%d,%d", &offsetx, &offsety, &offsetz);
							sprintf(buffer, "  %s = %s.SampleGrad(%s, %s, %s, %s, int2(%d, %d))%s;\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerNames[samplerId].c_str(), ci(op2).c_str(), ci(op5).c_str(), ci(op6).c_str(),
								offsetx, offsety, strrchr(op3, '.'));
						}
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}
					case OPCODE_SAMPLE_C:
					{
						remapTarget(op1);
						applySwizzle(".xyzw", op2);
						applySwizzle(op1, op3);
						applySwizzle(".x", fixImm(op5, instr->asOperands[4]));
						int textureId, samplerId;
						sscanf_s(op3, "t%d.", &textureId);
						sscanf_s(op4, "s%d", &samplerId);
						truncateTexturePos(op2, mTextureType[textureId].c_str());
						truncateTextureSwiz(op1, mTextureType[textureId].c_str());
						truncateTextureSwiz(op3, mTextureType[textureId].c_str());
						if (!instr->bAddressOffset)
							sprintf(buffer, "  %s = %s.SampleCmp(%s, %s, %s)%s;\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerComparisonNames[samplerId].c_str(), ci(op2).c_str(), ci(op5).c_str(), strrchr(op3, '.'));
						else
						{
							int offsetx = 0, offsety = 0, offsetz = 0;
							sscanf_s(statement, "sample_c_aoffimmi_indexable(%d,%d,%d", &offsetx, &offsety, &offsetz);
							sprintf(buffer, "  %s = %s.SampleCmp(%s, %s, %s, int2(%d, %d))%s;\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerComparisonNames[samplerId].c_str(), ci(op2).c_str(), ci(op5).c_str(),
								offsetx, offsety, strrchr(op3, '.'));
						}
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}
					//   sample_c_lz_indexable(texture2d)(float,float,float,float) r1.y, r3.zwzz, t0.xxxx, s1, r1.z
					case OPCODE_SAMPLE_C_LZ:
					{
						remapTarget(op1);
						applySwizzle(".xyzw", op2);
						applySwizzle(op1, op3);
						applySwizzle(".x", fixImm(op5, instr->asOperands[4]));
						int textureId, samplerId;
						sscanf_s(op3, "t%d.", &textureId);
						sscanf_s(op4, "s%d", &samplerId);
						truncateTexturePos(op2, mTextureType[textureId].c_str());
						truncateTextureSwiz(op1, mTextureType[textureId].c_str());
						truncateTextureSwiz(op3, mTextureType[textureId].c_str());
						if (!instr->bAddressOffset)
							sprintf(buffer, "  %s = %s.SampleCmpLevelZero(%s, %s, %s)%s;\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerComparisonNames[samplerId].c_str(), ci(op2).c_str(), ci(op5).c_str(), strrchr(op3, '.'));
						else
						{
							int offsetx = 0, offsety = 0, offsetz = 0;
							sscanf_s(statement, "sample_c_lz_aoffimmi_indexable(%d,%d,%d", &offsetx, &offsety, &offsetz);
							sprintf(buffer, "  %s = %s.SampleCmpLevelZero(%s, %s, %s, int2(%d, %d))%s;\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerComparisonNames[samplerId].c_str(), ci(op2).c_str(), ci(op5).c_str(),
								offsetx, offsety, strrchr(op3, '.'));
						}
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}
					// This opcode was missing, and used in WatchDogs. 
					// expected code "samplepos r0.xy, t1.xyxx, v1.x" -> "r0.xy = t1.GetSamplePosition(v1.x);"
					case OPCODE_SAMPLE_POS:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						int textureId;
						sscanf_s(op2, "t%d.", &textureId);
						sprintf(buffer, "  %s = %s.GetSamplePosition(%s);\n", writeTarget(op1),
							mTextureNames[textureId].c_str(), ci(op3).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

						// Missing opcode, used in FC4.  Similar to 'sample'
						// lod dest[.mask], srcAddress[.swizzle], srcResource[.swizzle], srcSampler
						// ret Object.CalculateLevelOfDetail(sampler_state S, float x);
						// "lod r0.x, r0.xyzx, t2.y, s2" -> "r0.x = t2.CalculateLevelOfDetailUnclamped(s2, r0.xyz);"
						// CalculateLevelOfDetailUnclamped compiles to t2.y, 
						// CalculateLevelOfDetail compiles to t2.x
					case OPCODE_LOD:
					{
						remapTarget(op1);
						applySwizzle(".xyzw", op2);
						applySwizzle(op1, op3);
						int textureId, samplerId;
						sscanf_s(op3, "t%d.", &textureId);
						sscanf_s(op4, "s%d", &samplerId);
						truncateTexturePos(op2, mTextureType[textureId].c_str());
						char *clamped = strrchr(op3, '.') + 1;
						if (*clamped == 'x')
							sprintf(buffer, "  %s = %s.CalculateLevelOfDetail(%s, %s);\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerNames[samplerId].c_str(), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = %s.CalculateLevelOfDetailUnclamped(%s, %s);\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerNames[samplerId].c_str(), ci(op2).c_str());
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

					case OPCODE_GATHER4:
					{
						remapTarget(op1);
						applySwizzle(".xyzw", op2);
						applySwizzle(op1, op3);
						int textureId, samplerId;
						sscanf_s(op3, "t%d.", &textureId);
						sscanf_s(op4, "s%d", &samplerId);
						truncateTexturePos(op2, mTextureType[textureId].c_str());
						if (!instr->bAddressOffset)
							sprintf(buffer, "  %s = %s.Gather(%s, %s)%s;\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerNames[samplerId].c_str(), ci(op2).c_str(), strrchr(op3, '.'));
						else
						{
							int offsetx = 0, offsety = 0, offsetz = 0;
							sscanf_s(statement, "gather4_aoffimmi_indexable(%d,%d,%d", &offsetx, &offsety, &offsetz);
							sprintf(buffer, "  %s = %s.Gather(%s, %s, int2(%d, %d))%s;\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerNames[samplerId].c_str(), ci(op2).c_str(),
								offsetx, offsety, strrchr(op3, '.'));
						}
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

					case OPCODE_GATHER4_C:
					{
						remapTarget(op1);
						applySwizzle(".xyzw", op2);
						applySwizzle(op1, op3);
						int textureId, samplerId;
						sscanf_s(op3, "t%d.", &textureId);
						sscanf_s(op4, "s%d", &samplerId);
						truncateTexturePos(op2, mTextureType[textureId].c_str());
						if (!instr->bAddressOffset)
							sprintf(buffer, "  %s = %s.GatherCmp(%s, %s, %s)%s;\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerComparisonNames[samplerId].c_str(), ci(op2).c_str(), ci(op5).c_str(), strrchr(op3, '.'));
						else
						{
							int offsetx = 0, offsety = 0, offsetz = 0;
							sscanf_s(statement, "gather4_c_aoffimmi_indexable(%d,%d,%d", &offsetx, &offsety, &offsetz);
							sprintf(buffer, "  %s = %s.GatherCmp(%s, %s, %s, int2(%d,%d))%s;\n", writeTarget(op1),
								mTextureNames[textureId].c_str(), mSamplerComparisonNames[samplerId].c_str(), ci(op2).c_str(), ci(op5).c_str(),
								offsetx, offsety, strrchr(op3, '.'));
						}
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

					// Add the Gather4_PO opcodes for Dragon Age.  Copied from Gather4.
					//   gather4_po dest[.mask], srcAddress[.swizzle], srcOffset[.swizzle], srcResource[.swizzle], srcSampler[.select_component]
					//   output.color	= texture2d.Gather(samplerState, input.texcoord, int2(0,0));
					case OPCODE_GATHER4_PO:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						applySwizzle(op1, op4);
						int textureId, samplerId;
						sscanf_s(op4, "t%d.", &textureId);
						sscanf_s(op5, "s%d", &samplerId);
						truncateTexturePos(op2, mTextureType[textureId].c_str());
						sprintf(buffer, "  %s = %s.Gather(%s, %s, %s)%s;\n", writeTarget(op1), mTextureNames[textureId].c_str(), 
							mSamplerNames[samplerId].c_str(), ci(op2).c_str(), ci(op3).c_str(), strrchr(op4, '.'));
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

					// gather4_po_c dest[.mask], srcAddress[.swizzle], srcOffset[.swizzle], srcResource[.swizzle], srcSampler[.R], srcReferenceValue
					case OPCODE_GATHER4_PO_C:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						applySwizzle(op1, op4);
						int textureId, samplerId;
						sscanf_s(op4, "t%d.", &textureId);
						sscanf_s(op5, "s%d", &samplerId);
						truncateTexturePos(op2, mTextureType[textureId].c_str());
						sprintf(buffer, "  %s = %s.GatherCmp(%s, %s, %s, %s)%s;\n", writeTarget(op1), mTextureNames[textureId].c_str(), 
							mSamplerComparisonNames[samplerId].c_str(), ci(op2).c_str(), ci(op6).c_str(), ci(op3).c_str(), strrchr(op4, '.'));
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

						// For the _aoffimmi format, this was picking up 0..15, instead of the necessary -8..7
						// It's a 4 bit number being used in an int, hence missing negatives.
						// This fix follows the form of the _Gather opcode above, but should maybe use the
						// instr-> parameters after they are fixed.
						// Fixed both _LD and LD_MS
					case OPCODE_LD:
					{
						remapTarget(op1);
						applySwizzle(".xyzw", op2);
						applySwizzle(op1, op3);
						int textureId;
						sscanf_s(op3, "t%d.", &textureId);
						truncateTextureLoadPos(op2, mTextureType[textureId].c_str());
						integerLoadCoordinate(op2);
						truncateTextureSwiz(op1, mTextureType[textureId].c_str());
						truncateTextureSwiz(op3, mTextureType[textureId].c_str());
						if (!instr->bAddressOffset)
							sprintf(buffer, "%s.Load(%s)%s", mTextureNames[textureId].c_str(), ci(op2).c_str(), strrchr(op3, '.'));
						else {
							int offsetU = 0, offsetV = 0, offsetW = 0;
							sscanf_s(statement, "ld_aoffimmi(%d,%d,%d", &offsetU, &offsetV, &offsetW);
							sprintf(buffer, "%s.Load(%s, int3(%d, %d, %d))%s", mTextureNames[textureId].c_str(), ci(op2).c_str(),
								offsetU, offsetV, offsetW, strrchr(op3, '.'));
						}
						emitTextureLoad(op1, textureId, buffer);
						removeBoolean(op1);
						break;
					}
					case OPCODE_LD_MS:
					{
						remapTarget(op1);
						applySwizzle(".xyzw", op2);
						applySwizzle(op1, op3);
						applySwizzle(".x", fixImm(op4, instr->asOperands[3]), true);
						int textureId;
						sscanf_s(op3, "t%d.", &textureId);
						truncateTextureLoadPos(op2, mTextureType[textureId].c_str());
						integerLoadCoordinate(op2);
						truncateTextureSwiz(op1, mTextureType[textureId].c_str());
						truncateTextureSwiz(op3, mTextureType[textureId].c_str());
						if (!instr->bAddressOffset)
							sprintf(buffer, "%s.Load(%s, %s)%s", mTextureNames[textureId].c_str(), ci(op2).c_str(), ci(op4).c_str(), strrchr(op3, '.'));
						else{
							int offsetU = 0, offsetV = 0, offsetW = 0;
							sscanf_s(statement, "ld_aoffimmi(%d,%d,%d", &offsetU, &offsetV, &offsetW);
							sprintf(buffer, "%s.Load(%s, %s, int3(%d, %d, %d))%s", mTextureNames[textureId].c_str(), ci(op2).c_str(), ci(op4).c_str(),
								offsetU, offsetV, offsetW, strrchr(op3, '.'));
						}
						emitTextureLoad(op1, textureId, buffer);
						removeBoolean(op1);
						break;
					}

					case OPCODE_LD_STRUCTURED:
					{
						parse_ld_structured(shader, c, pos, size, instr);
						break;
					}
						//	  gInstanceBuffer[worldMatrixOffset] = x.y;
					case OPCODE_STORE_STRUCTURED:
					{
						parse_store_structured(shader, c, pos, size, instr);
						break;
					}

					case OPCODE_LD_RAW:
					{
						remapTarget(op1);
						applySwizzle(".xyzw", op2);
						applySwizzle(op1, op3);
						int textureId;
						sscanf_s(op3, "t%d.", &textureId);
						// ByteAddressBuffer.Load(byteOffset) — offset is a raw byte offset, no texture-pos truncation
						sprintf(buffer, "  %s = %s.Load(%s)%s;\n", writeTarget(op1), mTextureNames[textureId].c_str(), ci(op2).c_str(), strrchr(op3, '.'));
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					}

						// Missing opcodes for SM5.  Not implemented yet, but we want to generate some sort of code, in case
						// these are used in needed shaders.  That way we can hand edit the shader to make it usable, until 
						// this is completed.
					case OPCODE_STORE_UAV_TYPED:
					case OPCODE_LD_UAV_TYPED:
					case OPCODE_STORE_RAW:
					{
						sprintf(buffer, "// No code for instruction (needs manual fix):\n");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						break;
					}

					case OPCODE_DISCARD:
						applySwizzle(".x", op1);
						{
							string condition = conditionExpression(op1, instr->eBooleanTestType == INSTRUCTION_TEST_ZERO);
							sprintf(buffer, "  if %s discard;\n", condition.c_str());
						}
						appendOutput(buffer);
						break;

						// The GetDimensions can also see a 4 parameter version in the immediate case. 
						// https://msdn.microsoft.com/en-us/library/windows/desktop/hh447214(v=vs.85).aspx
						//
						// resinfo[_uint|_rcpFloat] dest[.mask], srcMipLevel.select_component, srcResource[.swizzle]
						// In different variants based on input texture, becomes:
						// void Object.GetDimensions(UINT MipLevel, typeX Width, typeX Height, typeX Elements, typeX Depth, typeX NumberOfLevels, typeX NumberOfSamples);
						// 
						// We only see the immediate version l(0) in use, like:
						//
						//  resinfo_indexable(texture2d)(uint, uint, uint, uint)_uint r2.zw, l(0), t5.zwxy  
						//   Becomes 2 param version:   SectorAtlasTexture_UINT_TextureObject.GetDimensions(r2.z, r2.w);
						//
						//  resinfo_indexable(texture2dms)(float, float, float, float)_uint r0.xy, l(0), t0.xyzw 
						//   Becomes 3 param version:   DepthVPSampler_TextureObject.GetDimensions(r0.x, r0.y, bitmask.x);
						// 
						//  resinfo_indexable(texture2darray)(float, float, float, float) r0.xy, l(0), t0.xyzw
						//
						// So, we'll only handle that immediate for now, and generate syntax errors if we see any other variant.
						// We don't want to knowingly generate code that compiles, but has errors.  Includes _rcpFloat as unknown.
						//
						// This also added new ResInfo parsing that was not in our older BinaryCompiler.
						//
						// bindInfo is zeroed out, and GetResourceFromBindingPoint fails when the headers have been stripped.
						// With no reflection information, we are left with only the text.
					case OPCODE_RESINFO:
					{
						remapTarget(op1);

						bool unknownVariant = true;
						Operand output = instr->asOperands[0];
						Operand constZero = instr->asOperands[1];
						Operand texture = instr->asOperands[2];
						RESINFO_RETURN_TYPE returnType = instr->eResInfoReturnType;
						int texReg = texture.ui32RegisterNumber;
						ResourceBinding bindInfo;
						ResourceBinding *bindInfoPtr = &bindInfo;

						memset(&bindInfo, 0, sizeof(bindInfo));
						int bindstate = GetResourceFromBindingPoint(RGROUP_TEXTURE, texReg, shader->sInfo, &bindInfoPtr);
						bool bindStripped = (bindstate == 0);

						if (bindStripped)
						{
							// In the case where the reflection information has been stripped from the headers,
							// we are left with only the text line itself.  Try to parse the text for variants 
							// we know, and add them to the bindInfo.
							//
							// e.g. from Batman and Witcher3:
							//  resinfo_indexable(texture2d)(float,float,float,float)_uint r1.yw, l(0), t3.zxwy 

							char texType[opcodeSize];
							char retType[opcodeSize];
							int numInfo = sscanf_s(statement, "resinfo_indexable(%[^)])%s", texType, opcodeSize, retType, opcodeSize) ;

							bool isConstant = (!strcmp(op2, "l(0),"));

							if ((numInfo == 2) && isConstant)
							{
								constZero.eType = OPERAND_TYPE_IMMEDIATE32;
								constZero.afImmediates[0] = 0.0;

								if (!strcmp(texType, "texture2d"))
									bindInfoPtr->eDimension = REFLECT_RESOURCE_DIMENSION_TEXTURE2D;
								else if (!strcmp(texType, "texture2dms"))
									bindInfoPtr->eDimension = REFLECT_RESOURCE_DIMENSION_TEXTURE2DMS;
								else if (!strcmp(texType, "texture2darray"))
									bindInfoPtr->eDimension = REFLECT_RESOURCE_DIMENSION_TEXTURE2DARRAY;

								bindInfoPtr->Name = string(op3, strrchr(op3, '.'));

								if (strstr(retType, "_uint"))
									returnType = RESINFO_INSTRUCTION_RETURN_UINT;
								else
									returnType = RESINFO_INSTRUCTION_RETURN_FLOAT;

								texture.eType = OPERAND_TYPE_RESOURCE;

								bindStripped = false;
							}
						}

						// We only presently handle the float and _uint return types, and the const 0 mode. 
						// And the texture2d and textures2dms types. That's all we've seen so far.
						// This same output sequence is used for both a normal parse case, and the stripped header case.

						if ((constZero.eType == OPERAND_TYPE_IMMEDIATE32) && (constZero.afImmediates[0] == 0.0)
							&& (returnType == RESINFO_INSTRUCTION_RETURN_UINT || returnType == RESINFO_INSTRUCTION_RETURN_FLOAT)
							&& texture.eType == OPERAND_TYPE_RESOURCE
							&& !bindStripped)
						{
							//string out1, out2, out3;

							// return results into uint variables forces compiler to generate _uint variant.
							//if (returnType == RESINFO_INSTRUCTION_RETURN_UINT)
							//{
							//	out1 = "dst0.x";
							//	out2 = "dst0.y";
							//	out3 = "dst0.w";
							//}
							//else
							//{
							//	out1 = ci(GetSuffix(op1, 0));
							//	out2 = ci(GetSuffix(op1, 1));
							//	out3 = "fDst0.x";
							//}

							if (bindInfoPtr->eDimension == REFLECT_RESOURCE_DIMENSION_TEXTURE2D)
							{
								if (returnType == RESINFO_INSTRUCTION_RETURN_UINT)
								{
									sprintf(buffer, "  uiDest = 0; %s.GetDimensions(0, uiDest.x, uiDest.y, uiDest.w);\n", bindInfoPtr->Name.c_str());
								}
								else
								{
									sprintf(buffer, "  fDest = 0; %s.GetDimensions(0, fDest.x, fDest.y, fDest.w);\n", bindInfoPtr->Name.c_str());
								}
								appendOutput(buffer);
								unknownVariant = false;
							}
							else if (bindInfoPtr->eDimension == REFLECT_RESOURCE_DIMENSION_TEXTURE2DMS)
							{
								if (returnType == RESINFO_INSTRUCTION_RETURN_UINT)
								{
									sprintf(buffer, "  uiDest = 0; %s.GetDimensions(uiDest.x, uiDest.y, uiDest.z);\n", bindInfoPtr->Name.c_str());
								}
								else
								{
									sprintf(buffer, "  fDest = 0; %s.GetDimensions(fDest.x, fDest.y, fDest.z);\n", bindInfoPtr->Name.c_str());
								}
								appendOutput(buffer);
								unknownVariant = false;
							}
							else if (bindInfoPtr->eDimension == REFLECT_RESOURCE_DIMENSION_TEXTURE2DARRAY)
							{
								if (returnType == RESINFO_INSTRUCTION_RETURN_UINT)
									sprintf(buffer, "  %s.GetDimensions(0, uiDest.x, uiDest.y, uiDest.z, uiDest.w);\n", bindInfoPtr->Name.c_str());
								else
									sprintf(buffer, "  %s.GetDimensions(0, fDest.x, fDest.y, fDest.z, fDest.w);\n", bindInfoPtr->Name.c_str());
								appendOutput(buffer);
								unknownVariant = false;
							}

							// For the output, we saw a r3.xyzw which makes no sense for this instruction. 
							// Not sure this is fully correct, but the goal here is to apply the swizzle from the op3, which is the texture
							// register, as the pieces that are valid to copy to the op1 output.  
							// The op3 texture swizzle can determine which components to use, and what order.  
							// The op1 output determines which ones are valid for output.
							if (returnType == RESINFO_INSTRUCTION_RETURN_UINT)
							{
								char dest[opcodeSize] = "uiDest.xyzw";
								applySwizzle(op3, dest);
								applySwizzle(op1, dest);
								sprintf(buffer, "  %s = asfloat(%s);\n", op1, dest);
								appendOutput(buffer);
								markIntegerBitPatternComponents(op1);
							}
							else
							{
								char dest[opcodeSize] = "fDest.xyzw";
								applySwizzle(op3, dest);
								applySwizzle(op1, dest);
								sprintf(buffer, "  %s = %s;\n", op1, dest);
								appendOutput(buffer);
							}
						}
						if (unknownVariant)
						{
							// Completely new variant, write out the reminder.
							string line = string(c + pos);
							line = line.substr(0, line.find('\n'));
							sprintf(buffer, "// Unknown use of GetDimensions for resinfo_ from missing reflection info, need manual fix.\n");
							appendOutput(buffer);
							sprintf(buffer, "// %s\n", line.c_str());
							appendOutput(buffer);
							sprintf(buffer, "// Example for texture2d type, uint return:\n");
							appendOutput(buffer);
							sprintf(buffer, "tx.GetDimensions(0, uiDest.x, uiDest.y, uiDest.z);\n");
							appendOutput(buffer);
							sprintf(buffer, "rx = uiDest;\n");
							appendOutput(buffer);

							sprintf(buffer, " state=%d, constZero.eType=%d, returnType=%d, texture.eType=%d, afImmediates[0]=%f\n", bindstate, constZero.eType, returnType, texture.eType, constZero.afImmediates[0]);
							appendOutput(buffer);

							//logDecompileError("Unknown _resinfo variant: " + line);
						}
						break;
					}

					case OPCODE_EVAL_SAMPLE_INDEX:
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(".x", fixImm(op3, instr->asOperands[2]), true);
						sprintf(buffer, "  %s = EvaluateAttributeAtSample(%s, %s);\n", writeTarget(op1), op2, op3);
						appendOutput(buffer);
						break;
					case OPCODE_EVAL_CENTROID:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = EvaluateAttributeCentroid(%s);\n", writeTarget(op1), op2);
						appendOutput(buffer);
						break;
					case OPCODE_EVAL_SNAPPED:
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(".xy", fixImm(op3, instr->asOperands[2]), true);
						sprintf(buffer, "  %s = EvaluateAttributeSnapped(%s, %s);\n", writeTarget(op1), op2, op3);
						appendOutput(buffer);
						break;

					case OPCODE_DERIV_RTX_COARSE:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = ddx_coarse(%s);\n", writeTarget(op1), op2);
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					case OPCODE_DERIV_RTX_FINE:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = ddx_fine(%s);\n", writeTarget(op1), op2);
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					case OPCODE_DERIV_RTY_COARSE:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = ddy_coarse(%s);\n", writeTarget(op1), op2);
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					case OPCODE_DERIV_RTY_FINE:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = ddy_fine(%s);\n", writeTarget(op1), op2);
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					case OPCODE_DERIV_RTX:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = ddx(%s);\n", writeTarget(op1), op2);
						appendOutput(buffer);
						removeBoolean(op1);
						break;
					case OPCODE_DERIV_RTY:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = ddy(%s);\n", writeTarget(op1), op2);
						appendOutput(buffer);
						removeBoolean(op1);
						break;

					case OPCODE_RET:
						sprintf(buffer, "  return;\n");
						appendOutput(buffer);
						break;

						// Missing opcode needed for WatchDogs. Used as "retc_nz r0.x"
					case OPCODE_RETC:
						applySwizzle(".x", op1);
						{
							string condition = conditionExpression(op1, instr->eBooleanTestType == INSTRUCTION_TEST_ZERO);
							sprintf(buffer, "  if %s return;\n", condition.c_str());
						}
						appendOutput(buffer);
						break;

						// FarCry4 GeometryShader
					case OPCODE_EMIT_STREAM:
						sprintf(buffer, "// Needs manual fix for instruction, maybe. \n//");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "m0.Append(0);\n");
						appendOutput(buffer);
						break;
					case OPCODE_CUT_STREAM:
						sprintf(buffer, "// Needs manual fix for instruction, maybe. \n//");
						appendOutput(buffer);
						ASMLineOut(c, pos, size);
						sprintf(buffer, "m0.RestartStrip();\n");
						appendOutput(buffer);
						break;

					case OPCODE_NOP:
						// Used in MGSV:TPP, perhaps an artefact of having debug
						// info enabled in the shaders?
						break;

					default:
						logDecompileError("Unknown statement: " + string(statement));
						return;
				}
				iNr++;
			}

			NextLine(c, pos, size);
			mLastStatement = instr;
		}

		// Moved this out of Opcode_ret, because it's possible to have more than one ret
		// in a shader.  This is the last of a given shader, which seems more correct.
		// This fixes the double injection of "injectedScreenPos : SV_Position"
		if (!shader->dx9Shader)
			WritePatches();
	}

	void ParseCodeOnlyShaderType(Shader *shader, const char *c, size_t size)
	{
		mOutputRegisterValues.clear();
		mBooleanRegisters.clear();
		mCodeStartPos = mOutput.size();

		size_t pos = 0;
		while (pos < size)
		{
			// Ignore comments.
			if (!strncmp(c + pos, "//", 2))
			{
				NextLine(c, pos, size);
				continue;
			}
			// Read statement.
			if (ReadStatement(c + pos) < 1)
			{
				logDecompileError("Error parsing statement: " + string(c + pos, 80));
				return;
			}
			if (!strncmp(statement, "vs_", 3) ||
				!strncmp(statement, "hs_", 3) ||
				!strncmp(statement, "ds_", 3) ||
				!strncmp(statement, "gs_", 3) ||
				!strncmp(statement, "ps_", 3) ||
				!strncmp(statement, "cs_", 3))
			{
				mShaderType = statement;
				return;
			}

			// Next line.
			NextLine(c, pos, size);
		}
	}


	// The StereoParams are nearly always useful, but the depth buffer texture is rarely used.
	// Adding .ini declaration, since declaring it doesn't cost anything and saves typing them in later.

	void WriteAddOnDeclarations()
	{
		string declaration = 
			"\n\n"
			"// 3Dmigoto declarations\n";

		// DXBC comparisons produce an all-bits mask (0xffffffff or 0), not the
		// HLSL numeric values 1 and 0.  Keep that mask when it is stored in the
		// float-shaped temporary registers.  The overloads avoid relying on an
		// implicit bool-to-float conversion, which would lose the mask bits.
		declaration +=
			"uint cmp_mask(bool v) { return v ? 0xffffffffu : 0u; }\n"
			"uint2 cmp_mask(bool2 v) { return uint2(v.x ? 0xffffffffu : 0u, v.y ? 0xffffffffu : 0u); }\n"
			"uint3 cmp_mask(bool3 v) { return uint3(v.x ? 0xffffffffu : 0u, v.y ? 0xffffffffu : 0u, v.z ? 0xffffffffu : 0u); }\n"
			"uint4 cmp_mask(bool4 v) { return uint4(v.x ? 0xffffffffu : 0u, v.y ? 0xffffffffu : 0u, v.z ? 0xffffffffu : 0u, v.w ? 0xffffffffu : 0u); }\n"
			"#define cmp(x) asfloat(cmp_mask(x))\n";

		if (G->IniParamsReg >= 0) {
			declaration +=
				"Texture1D<float4> IniParams : register(t" + std::to_string(G->IniParamsReg) + ");\n";
		}

		if (G->StereoParamsReg >= 0) {
			declaration +=
				"Texture2D<float4> StereoParams : register(t" + std::to_string(G->StereoParamsReg) + ");\n";
		}

		if (G->ZRepair_DepthBuffer)
		{
			declaration +=
				"Texture2D<float4> InjectedDepthTexture : register(t126);\n";
		}

		declaration +=
			"\n";

		mOutput.insert(mOutput.end(), declaration.c_str(), declaration.c_str() + declaration.length());
	}


	// Header for the file, version and time stamp.  Skipping first line of file, to allow
	// it to be used for on-screen comments.

	void WriteHeaderDeclarations()
	{
		string header =
			"// ---- Created with 3Dmigoto v" + string(VER_FILE_VERSION_STR) + " on " + LogTime();

		// using .begin() to ensure first lines in files.
		mOutput.insert(mOutput.begin(), header.c_str(), header.c_str() + header.length());
	}
};

const string DecompileBinaryHLSL(ParseParameters &params, bool &patched, std::string &shaderModel, bool &errorOccurred)
{
	Decompiler d;

	d.mCodeStartPos = 0;
	d.mCorrectedIndexRegisters.clear();
	d.mOutput.reserve(16 * 1024);
	d.mErrorOccurred = false;
	d.mShaderType = "unknown";
	d.mPatched = false;
	d.G = params.G;

	// Decompile binary.

	// This can crash, because of unknown or unseen syntax, so we wrap it in try/catch
	// block to handle any exceptions and mark the shader as presently bad.
	// In order for this to work, the /EHa option must be enabled for code-generation
	// so that system level exceptions are caught too.
	
	// It's worth noting that some fatal exceptions will still bypass this catch,
	// like a stack corruption, stack overflow, or out of memory, and crash the game.
	// The termination handler approach does not catch those errors either.
	try
	{
		Shader *shader = DecodeDXBC((uint32_t*)params.bytecode);
		if (!shader) return string();

		if (shader->dx9Shader)
		{
			d.ReadResourceBindingsDX9(params.decompiled, params.decompiledSize);
		}
		else
		{
			d.ParseStructureDefinitions(shader, params.decompiled, params.decompiledSize);
			d.ReadResourceBindings(params.decompiled, params.decompiledSize);
			d.InferStructuredBufferIntegerTypes(shader);
			d.WriteStructureDefinitions();
		}

		d.ParseBufferDefinitions(shader, params.decompiled, params.decompiledSize);
		d.WriteResourceDefinitions();
		d.WriteAddOnDeclarations();
		d.ParseInputSignature(shader, params.decompiled, params.decompiledSize);
		d.ParseOutputSignature(params.decompiled, params.decompiledSize);
		if (!params.ZeroOutput)
		{
			d.ParseCode(shader, params.decompiled, params.decompiledSize);
		}
		else
		{
			d.ParseCodeOnlyShaderType(shader, params.decompiled, params.decompiledSize);
			d.WriteZeroOutputSignature(params.decompiled, params.decompiledSize);
		}
		d.mOutput.push_back('}');
		d.WriteHeaderDeclarations();

		shaderModel = d.mShaderType;
		errorOccurred = d.mErrorOccurred;
		FreeShaderInfo(shader->sInfo);
		delete shader;
		patched = d.mPatched;
		return string(d.mOutput.begin(), d.mOutput.end());
	}
	catch (...)
	{
		// Fatal error, but catch it and mark it as bad.
		LogInfo("   ******* Exception caught while decompiling shader ******\n");

		errorOccurred = true;
		return string();
	}
}
