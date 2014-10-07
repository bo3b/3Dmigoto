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
#include <vector>
#include <set>
#include <algorithm>

#include "DecompileHLSL.h"

#include "..\BinaryDecompiler\include\pstdint.h"
#include "..\BinaryDecompiler\internal_includes\structs.h"
#include "..\BinaryDecompiler\internal_includes\decode.h"

#include <excpt.h>

#include "assert.h"

using namespace std;

enum DataType
{
	DT_bool,
	DT_float,
	DT_float2,
	DT_float3,
	DT_float4,
	DT_float4x2,
	DT_float3x3,
	DT_float4x3,
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

extern FILE *LogFile;
extern bool LogDebug;

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

	vector<char> mOutput;
	size_t mCodeStartPos;		// Used as index into buffer, name misleadingly suggests pointer usage.
	bool mErrorOccurred;
	bool mFixSvPosition;
	bool mRecompileVs;
	bool mPatched;
	string ZRepair_DepthTexture1, ZRepair_DepthTexture2;
	string BackProject_Vector1, BackProject_Vector2;
	char ZRepair_DepthTextureReg1, ZRepair_DepthTextureReg2;
	vector<string> ZRepair_Dependencies1, ZRepair_Dependencies2;
	bool mZRepair_DepthBuffer;
	vector<string> InvTransforms;
	string ZRepair_ZPosCalc1, ZRepair_ZPosCalc2;
	string ZRepair_PositionTexture;
	string ZRepair_WorldPosCalc;
	string ObjectPos_ID1, ObjectPos_ID2, ObjectPos_MUL1, ObjectPos_MUL2;
	string MatrixPos_ID1, MatrixPos_MUL1;
	int uuidVar;

	Decompiler()
		: mLastStatement(0),
		uuidVar(0)
	{}

	void logDecompileError(const string &err)
	{
		mErrorOccurred = true;
		if (LogFile) fprintf(LogFile, "    error parsing shader> %s\n", err.c_str());
	}

	DataType TranslateType(const char *name)
	{
		if (!strcmp(name, "float4x4")) return DT_float4x4;
		if (!strcmp(name, "float4x3")) return DT_float4x3;
		if (!strcmp(name, "float4x2")) return DT_float4x2;
		if (!strcmp(name, "float3x4")) return DT_float3x4;
		if (!strcmp(name, "float4")) return DT_float4;
		if (!strcmp(name, "float3x3")) return DT_float3x3;
		if (!strcmp(name, "float3")) return DT_float3;
		if (!strcmp(name, "float2")) return DT_float2;
		if (!strcmp(name, "float")) return DT_float;
		if (!strcmp(name, "bool")) return DT_bool;
		if (!strcmp(name, "uint4")) return DT_uint4;
		if (!strcmp(name, "uint3")) return DT_uint3;
		if (!strcmp(name, "uint2")) return DT_uint2;
		if (!strcmp(name, "uint")) return DT_uint;
		if (!strcmp(name, "int4")) return DT_int4;
		if (!strcmp(name, "int3")) return DT_int3;
		if (!strcmp(name, "int2")) return DT_int2;
		if (!strcmp(name, "int")) return DT_int;
		logDecompileError("Unknown data type: " + string(name));
		return DT_Unknown;
	}

	void ParseInputSignature(const char *c, size_t size)
	{
		mRemappedInputRegisters.clear();
		// Write header.
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
				while (c[pos] != 0x0a && pos < size) pos++; pos++;
			}
		}
		if (pos >= size - strlen(headerid)) return;
		// Skip header.
		for (int i = 0; i < 4; ++i)
		{
			while (c[pos] != 0x0a && pos < size) pos++; pos++;
		}
		// Read list.
		set<string> usedInputRegisters;
		while (pos < size)
		{
			char name[256], mask[16], format[16], format2[16];
			int index, slot; format[0] = 0; mask[0] = 0;
			if (!strncmp(c + pos, "// no Input", strlen("// no Input")))
				break;
			int numRead = sscanf_s(c + pos, "// %s %d %s %d %s %s",
				name, sizeof(name), &index, mask, sizeof(mask), &slot, format2, sizeof(format2), format, sizeof(format));
			if (numRead != 6)
			{
				logDecompileError("Error parsing input signature: " + string(c + pos, 80));
				return;
			}
			// finish type.
			if (strlen(mask) > 1)
				sprintf(format2, "%s%d", format, strlen(mask));
			else
				strcpy(format2, format);
			// Already used?
			char registerName[32];
			sprintf(registerName, "v%d", slot);
			string regNameStr = registerName;
			set<string>::iterator i = usedInputRegisters.find(regNameStr);
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
				usedInputRegisters.insert(regNameStr);
			}
			// Write.
			char buffer[256];
			sprintf(buffer, "  %s %s : %s%d,\n", format2, regNameStr.c_str(), name, index);
			mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			while (c[pos] != 0x0a && pos < size) pos++; pos++;
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
				while (c[pos] != 0x0a && pos < size) pos++; pos++;
			}
		}
		if (pos >= size - strlen(headerid)) return;
		// Skip header.
		for (int i = 0; i < 4; ++i)
		{
			while (c[pos] != 0x0a && pos < size) pos++; pos++;
		}
		// Read list.
		while (pos < size)
		{
			char name[256], mask[16], format[16], format2[16];
			int index, slot; format[0] = 0; mask[0] = 0;
			if (!strncmp(c + pos, "// no Output", strlen("// no Output")))
				break;
			int numRead = sscanf_s(c + pos, "// %s %d %s %d %s %s",
				name, sizeof(name), &index, mask, sizeof(mask), &slot, format2, sizeof(format2), format, sizeof(format));
			if (numRead == 6)
			{
				// finish type.
				if (strlen(mask) > 1)
					sprintf(format2, "%s%d", format, strlen(mask));
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
				char sysValue[64];
				int numRead = sscanf_s(c + pos, "// %s %d %s %d %s %s",
					name, sizeof(name), &index, mask, sizeof(mask), sysValue, sizeof(sysValue), format2, sizeof(format2), format, sizeof(format));
				// Write.
				char buffer[256];
				sprintf(buffer, "  out %s %s : %s,\n", format, sysValue, name);
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
			if (numRead != 6)
			{
				logDecompileError("Error parsing output signature: " + string(c + pos, 80));
				break;
			}
			while (c[pos] != 0x0a && pos < size) pos++; pos++;
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
				while (c[pos] != 0x0a && pos < size) pos++; pos++;
			}
		}
		if (pos >= size - strlen(headerid)) return;
		// Skip header.
		for (int i = 0; i < 4; ++i)
		{
			while (c[pos] != 0x0a && pos < size) pos++; pos++;
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
				name, sizeof(name), &index, mask, sizeof(mask), &slot, format2, sizeof(format2), format, sizeof(format));
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
				int numRead = sscanf_s(c + pos, "// %s %d %s %d %s %s",
					name, sizeof(name), &index, mask, sizeof(mask), sysValue, sizeof(sysValue), format2, sizeof(format2), format, sizeof(format));
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
			while (c[pos] != 0x0a && pos < size) pos++; pos++;
			// End?
			if (!strncmp(c + pos, "//\n", 3) || !strncmp(c + pos, "//\r", 3))
				break;
		}
	}

	void ReadResourceBindings(const char *c, size_t size)
	{
		mCBufferNames.clear();
		mSamplerNames.clear();
		mSamplerNamesArraySize.clear();
		mSamplerComparisonNames.clear();
		mSamplerComparisonNamesArraySize.clear();
		mTextureNames.clear();
		mTextureNamesArraySize.clear();
		// Read until header.
		const char *headerid = "// Resource Bindings:";
		size_t pos = 0;
		while (pos < size - strlen(headerid))
		{
			if (!strncmp(c + pos, headerid, strlen(headerid)))
				break;
			else
			{
				while (c[pos] != 0x0a && pos < size) pos++; pos++;
			}
		}
		if (pos >= size - strlen(headerid)) return;
		// Skip header.
		for (int i = 0; i < 4; ++i)
		{
			while (c[pos] != 0x0a && pos < size) pos++; pos++;
		}
		// Read list.
		while (pos < size)
		{
			char name[256], type[16], format[16], dim[16];
			int slot, arraySize;
			type[0] = 0;
			int numRead = sscanf_s(c + pos, "// %s %s %s %s %d %d",
				name, sizeof(name), type, sizeof(type), format, sizeof(format), dim, sizeof(dim), &slot, &arraySize);
			if (numRead != 6)
				logDecompileError("Error parsing resource declaration: " + string(c + pos, 80));
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
			else if (!strcmp(type, "texture"))
			{
				char *escapePos = strchr(name, '['); if (escapePos) *escapePos = '_';
				escapePos = strchr(name, ']'); if (escapePos) *escapePos = '_';
				string baseName = string(name);
				mTextureNames[slot] = baseName;
				mTextureNamesArraySize[slot] = arraySize;
				if (arraySize > 1)
					for (int i = 0; i < arraySize; ++i)
					{
					sprintf(name, "%s[%d]", baseName.c_str(), i);
					mTextureNames[slot + i] = name;
					}
				if (!strcmp(dim, "2d"))
					mTextureType[slot] = "Texture2D<" + string(format) + ">";
				else if (!strcmp(dim, "2darray"))
					mTextureType[slot] = "Texture2DArray<" + string(format) + ">";
				else if (!strcmp(dim, "3d"))
					mTextureType[slot] = "Texture3D<" + string(format) + ">";
				else if (!strcmp(dim, "cube"))
					mTextureType[slot] = "TextureCube<" + string(format) + ">";
				else if (!strncmp(dim, "2dMS", 4))
				{
					// The documentation says it's not legal, but we see Texture 2DMS with no ending size in WatchDogs. 
					// Instead of "2dMS4", it's just "2dMS". If we set that to Texture2DMS<float4>, with no size, 
					// it generates the same code as the ASM.	 sscanf returns -1 as an error.
					//
					// if (LogFile) fprintf(LogFile, "--> Texture %s: sprintf=%d as %s\n", dim, error, buffer);
					int msnumber;
					int scanned;
					char buffer[256];
					scanned = sscanf_s(dim + 4, "%d", &msnumber);
					if (scanned == 1)
						sprintf(buffer, "Texture2DMS<%s,%d>", format, msnumber);
					else
						sprintf(buffer, "Texture2DMS<%s>", format);
					mTextureType[slot] = buffer;
				}
				// Two new ones for Mordor.
				else if (!strcmp(dim, "buf"))
					mTextureType[slot] = "Buffer<" + string(format) + ">";	
				else if (!strcmp(dim, "r/o"))
					mTextureType[slot] = "StructuredBuffer<" + string(name) + ">";
				//else if (!strcmp(dim, "r/w"))
				//	mTextureType[slot] = "RWStructuredBuffer<" + string(name) + ">";  // probable, not seen yet.
				else
					logDecompileError("Unknown texture dimension: " + string(dim));
			}
			else if (!strcmp(type, "cbuffer"))
				mCBufferNames[name] = slot;
			while (c[pos] != 0x0a && pos < size) pos++; pos++;
			// End?
			if (!strncmp(c + pos, "//\n", 3) || !strncmp(c + pos, "//\r", 3))
				break;
		}
	}

	void WriteResourceDefinitions()
	{
		char buffer[256];
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
			case DT_float4:
			case DT_uint4:
			case DT_int4:
				return 16;
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
					while (c[pos] != 0x0a && pos < size) pos++; pos++;
				}
			}
			if (pos >= size - strlen(headerid)) return;
			char name[256];
			int numRead = sscanf_s(c + pos, "// cbuffer %s", name, sizeof(name));
			if (numRead != 1)
			{
				logDecompileError("Error parsing buffer name: " + string(c + pos, 80));
				return;
			}
			while (c[pos] != 0x0a && pos < size) pos++; pos++;
			while (c[pos] != 0x0a && pos < size) pos++; pos++;
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
			sprintf(buffer, "\ncbuffer %s : register(b%d)\n{\n", name, bufferRegister);
			mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			do
			{
				const char *eolPos = strchr(c + pos, '\n');
				memcpy(buffer, c + pos, eolPos - c - pos + 1);
				buffer[eolPos - c - pos + 1] = 0;
				// Skip opening bracket.
				if (strstr(buffer, " {\n"))
				{
					while (c[pos] != 0x0a && pos < size) pos++; pos++;
					continue;
				}
				// Ignore empty line.
				if (buffer[0] == '/' && buffer[1] == '/')
				{
					int ePos = 2;
					while (buffer[ePos] != 0 && (buffer[ePos] == ' ' || buffer[ePos] == '\t' || buffer[ePos] == '\n' || buffer[ePos] == '\r')) ++ePos;
					if (!buffer[ePos])
					{
						while (c[pos] != 0x0a && pos < size) pos++; pos++;
						continue;
					}
				}
				// Struct definition?
				if (strstr(buffer, " struct\n") || strstr(buffer, " struct "))
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
					while (c[pos] != 0x0a && pos < size) pos++; pos++;
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
						logDecompileError("Error parsing buffer offset: " + string(c + pos, 80));
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
					while (c[pos] != 0x0a && pos < size) pos++; pos++;
					--structLevel;
					continue;
				}
				// Read declaration.
				char type[16]; type[0] = 0;
				numRead = sscanf_s(c + pos, "// %s %s", type, sizeof(type), name, sizeof(name));
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
					numRead = sscanf_s(c + pos, "// %s %s %s", buffer, sizeof(buffer), type, sizeof(type), name, sizeof(name));
					if (numRead != 3)
					{
						logDecompileError("Error parsing buffer item: " + string(c + pos, 80));
						return;
					}
				}
				char *endStatement = strchr(name, ';');
				if (!endStatement)
				{
					logDecompileError("Error parsing buffer item: " + string(c + pos, 80));
					return;
				}
				*endStatement = 0;
				pos += 2;
				while (c[pos] != '/' && pos < size) pos++;
				int offset = 0;
				numRead = sscanf_s(c + pos, "// Offset: %d", &offset);
				if (numRead != 1)
				{
					logDecompileError("Error parsing buffer offset: " + string(c + pos, 80));
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
					(e.bt == DT_bool || e.bt == DT_float || e.bt == DT_float2 || e.bt == DT_float3 || e.bt == DT_float4 ||
					e.bt == DT_uint || e.bt == DT_uint2 || e.bt == DT_uint3 || e.bt == DT_uint4 ||
					e.bt == DT_int || e.bt == DT_int2 || e.bt == DT_int3 || e.bt == DT_int4 ||
					e.bt == DT_float4x4 || e.bt == DT_float3x4 || e.bt == DT_float4x3 || e.bt == DT_float3x3 || e.bt == DT_float4x2))
				{
					// Register each array element.
					int numElements = 0;
					sscanf_s(e.Name.substr(ep + 1).c_str(), "%d", &numElements);
					string baseName = e.Name.substr(0, ep);

					int counter = 0;
					if (e.bt == DT_float || e.bt == DT_bool || e.bt == DT_uint || e.bt == DT_int) counter = 4;
					else if (e.bt == DT_float2 || e.bt == DT_uint2 || e.bt == DT_int2) counter = 8;
					else if (e.bt == DT_float3 || e.bt == DT_uint3 || e.bt == DT_int3) counter = 12;
					else if (e.bt == DT_float4 || e.bt == DT_uint4 || e.bt == DT_int4) counter = 16;
					else if (e.bt == DT_float4x2) counter = 16 * 2;
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
				else if (e.bt == DT_float4x4 || e.bt == DT_float4x3 || e.bt == DT_float4x2 || e.bt == DT_float3x4 || e.bt == DT_float3x3)
				{
					e.matrixRow = 0; int offsetPos = (bufferRegister << 16) + offset; mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);
					e.matrixRow = 1; offsetPos = (bufferRegister << 16) + offset + 1 * 16; mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);
					if (e.bt != DT_float4x2)
					{
						e.matrixRow = 2; offsetPos = (bufferRegister << 16) + offset + 2 * 16; mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);
						if (e.bt != DT_float4x3 || e.bt != DT_float3x3)
							e.matrixRow = 3; offsetPos = (bufferRegister << 16) + offset + 3 * 16; mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);
					}
				}
				// Non-array versions of scalar entities like float4
				else
				{
					int offsetPos = (bufferRegister << 16) + offset;
					mCBufferData[offsetPos] = e; if (structLevel >= 0) pendingStructAttributes[structLevel].push_back(offsetPos);
				}

				// Default value?
				while (c[pos] != 0x0a && pos < size) pos++; pos++;
				const char *defaultid = "//      = ";
				int packoffset = offset / 16; int suboffset = (offset % 16) / 4;
				const char INDEX_MASK[] = "xyzw";
				string structSpacing;
				for (int i = -1; i < structLevel; ++i) structSpacing += "  ";
				if (!strncmp(c + pos, defaultid, strlen(defaultid)))
				{
					// For bool values, the usual conversion by %e creates QNAN, so handle them specifically. (e.g. = 0xffffffff)
					if (e.bt == DT_bool)
					{
						unsigned int bHex = 0;
						numRead = sscanf_s(c + pos, "// = 0x%lx", &bHex);
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
						while (c[pos] != 0x0a && pos < size) pos++; pos++;
					}
					else
					{
						float v[4] = { 0, 0, 0, 0 };
						numRead = sscanf_s(c + pos, "// = 0x%lx 0x%lx 0x%lx 0x%lx;", v + 0, v + 1, v + 2, v + 3);
						if (structLevel < 0)
						{
							if (suboffset == 0)
								sprintf(buffer, "  %s%s %s : packoffset(c%d) = %s(", modifier.c_str(), type, name, packoffset, type);
							else
								sprintf(buffer, "  %s%s %s : packoffset(c%d.%c) = %s(", modifier.c_str(), type, name, packoffset, INDEX_MASK[suboffset], type);
						}
						else
							sprintf(buffer, "  %s%s%s %s = %s(", structSpacing.c_str(), modifier.c_str(), type, name, type);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						for (int i = 0; i < numRead - 1; ++i)
						{
							sprintf(buffer, "%e,", v[i]);
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						}
						sprintf(buffer, "%e);\n", v[numRead - 1]);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						while (c[pos] != 0x0a && pos < size) pos++; pos++;
					}
				}
				else
				{
					if (structLevel < 0)
					{
						if (suboffset == 0)
							sprintf(buffer, "  %s%s %s : packoffset(c%d);\n", modifier.c_str(), type, name, packoffset);
						else
							sprintf(buffer, "  %s%s %s : packoffset(c%d.%c);\n", modifier.c_str(), type, name, packoffset, INDEX_MASK[suboffset], type);
					}
					else
						sprintf(buffer, "  %s%s%s %s;\n", structSpacing.c_str(), modifier.c_str(), type, name);
					mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
				}
			} while (strncmp(c + pos, "// }", 4));
			// Write declaration.
			const char *endBuffer = "}\n";
			mOutput.insert(mOutput.end(), endBuffer, endBuffer + strlen(endBuffer));
		}
	}

	void applySwizzle(const char *left, char *right, bool useInt = false)
	{
		char right2[128];
		if (right[strlen(right) - 1] == ',') right[strlen(right) - 1] = 0;
		bool absolute = false, negative = false;
		if (right[0] == '-')
			negative = true;
		if (right[0] == '|' || right[1] == '|')
		{
			absolute = true;
			strcpy_s(strchr(right, '|'), strlen(right), strchr(right, '|') + 1);	// Shrinking string, can never overflow.
			*strchr(right, '|') = 0;
		}
		const char *strPos = strchr(left, '.') + 1;
		char idx[4] = { -1, -1, -1, -1 };
		char map[4] = { 3, 0, 1, 2 };
		size_t pos = 0;							// Used as index into string buffer
		if (strPos == (const char *)1)
			strPos = "x";
		while (*strPos && pos < 4)
			idx[pos++] = map[*strPos++ - 'w'];
		// literal?
		if (right[0] == 'l')
		{
			strPos = strchr(right, ',');
			// Single literal?
			if (!strPos)
			{
				strcpy(right2, right + 2);
				right2[strlen(right2) - 1] = 0;
			}
			else
			{
				char *beginPos = right + 2;
				float args[4];
				for (int i = 0; i < 4; ++i)
				{
					char *endPos = strchr(beginPos, ',');
					if (endPos) *endPos = 0;
					sscanf_s(beginPos, "%f", args + i);
					beginPos = endPos + 1;
				}
				if (pos == 1)
				{
					sprintf(right2, "%e", args[idx[0]]);
				}
				else
				{
					// Only integer values?
					bool isInt = true;
					for (int i = 0; idx[i] >= 0 && i < 4; ++i)
						isInt = isInt && (floor(args[idx[i]]) == args[idx[i]]);
					if (isInt && useInt)
					{
						sprintf(right2, "int%d(", pos);
						for (int i = 0; idx[i] >= 0 && i < 4; ++i)
							sprintf_s(right2 + strlen(right2), sizeof(right2) - strlen(right2), "%d,", int(args[idx[i]]));
						right2[strlen(right2) - 1] = 0;
						strcat(right2, ")");
					}
					else
					{
						sprintf(right2, "float%d(", pos);
						for (int i = 0; idx[i] >= 0 && i < 4; ++i)
							sprintf_s(right2 + strlen(right2), sizeof(right2) - strlen(right2), "%e,", args[idx[i]]);
						right2[strlen(right2) - 1] = 0;
						strcat(right2, ")");
					}
				}
			}
		}
		else
		{
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
			// Buffer reference?
			strPos = strstr(right2, "cb");
			if (strPos)
			{
				int bufIndex = 0;
				if (strstr(right2, "icb"))
				{
					bufIndex = -1;
				}
				else
				{
					if (sscanf_s(strPos + 2, "%d", &bufIndex) != 1)
					{
						logDecompileError("Error parsing buffer register index: " + string(right2));
						return;
					}
				}
				strPos = strchr(right2, '[');
				if (!strPos)
				{
					logDecompileError("Error parsing buffer offset: " + string(right2));
					return;
				}
				int bufOffset = 0;
				char indexRegister[5]; indexRegister[0] = 0;
				// Indexed by register? "r1.y+"
				if (strPos[1] == 'r')
				{
					strncpy(indexRegister, strPos + 1, 4);
					indexRegister[4] = 0;
					strPos += 5;
				}
				if (sscanf_s(strPos + 1, "%d", &bufOffset) != 1)
				{
					logDecompileError("Error parsing buffer offset: " + string(right2));
					return;
				}
				CBufferData::iterator i = mCBufferData.find((bufIndex << 16) + bufOffset * 16);
				if (i == mCBufferData.end() && strrchr(right2, '.')[1] == 'y')
					i = mCBufferData.find((bufIndex << 16) + bufOffset * 16 + 4);
				if (i == mCBufferData.end() && strrchr(right2, '.')[1] == 'z')
					i = mCBufferData.find((bufIndex << 16) + bufOffset * 16 + 8);
				if (i == mCBufferData.end() && strrchr(right2, '.')[1] == 'w')
					i = mCBufferData.find((bufIndex << 16) + bufOffset * 16 + 12);
				if (i == mCBufferData.end())
				{
					logDecompileError("Error parsing buffer offset: " + string(right2));
					return;
				}
				if ((i->second.bt == DT_float || i->second.bt == DT_uint || i->second.bt == DT_int) ||
					((i->second.bt == DT_float2 || i->second.bt == DT_uint2 || i->second.bt == DT_int2) && (strrchr(right2, '.')[1] == 'w' || strrchr(right2, '.')[1] == 'z')) ||
					((i->second.bt == DT_float3 || i->second.bt == DT_uint3 || i->second.bt == DT_int3) && strrchr(right2, '.')[1] == 'w'))
				{
					int skip;
					if (i->second.bt == DT_float || i->second.bt == DT_uint || i->second.bt == DT_int) skip = 1;
					else if (i->second.bt == DT_float2 || i->second.bt == DT_uint2 || i->second.bt == DT_int2) skip = 2;
					else if (i->second.bt == DT_float3 || i->second.bt == DT_uint3 || i->second.bt == DT_int3) skip = 3;
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
				char right3[128]; right3[0] = 0;
				if (right2[0] == '-')
					strcpy(right3, "-");
				strcat(right3, i->second.Name.c_str());
				strPos = strchr(strPos, ']');
				if (!strPos)
				{
					logDecompileError("Error parsing buffer offset: " + string(right2));
					return;
				}
				if (indexRegister[0])
				{
					// Remove existing index.
					char *indexPos = strchr(right3, '[');
					if (indexPos) *indexPos = 0;
					string indexRegisterName(indexRegister, strchr(indexRegister, '.'));
					StringStringMap::iterator isCorrected = mCorrectedIndexRegisters.find(indexRegisterName);
					if (isCorrected != mCorrectedIndexRegisters.end())
					{
						char newOperand[32]; strcpy(newOperand, isCorrected->second.c_str());
						applySwizzle(indexRegister, newOperand, true);
						sprintf_s(right3 + strlen(right3), sizeof(right3) - strlen(right3), "[%s]", newOperand);
					}
					else if (mLastStatement && mLastStatement->eOpcode == OPCODE_IMUL &&
						(i->second.bt == DT_float4x4 || i->second.bt == DT_float4x3 || i->second.bt == DT_float4x2 || i->second.bt == DT_float3x4 || i->second.bt == DT_float3x3))
					{
						char newOperand[32]; strcpy(newOperand, mMulOperand.c_str());
						applySwizzle(indexRegister, newOperand, true);
						sprintf_s(right3 + strlen(right3), sizeof(right3) - strlen(right3), "[%s]", newOperand);
						mCorrectedIndexRegisters[indexRegisterName] = mMulOperand;
					}
					else if (i->second.bt == DT_float4x2)
						sprintf_s(right3 + strlen(right3), sizeof(right3) - strlen(right3), "[%s/2]", indexRegister);
					else if (i->second.bt == DT_float4x3 || i->second.bt == DT_float3x4)
						sprintf_s(right3 + strlen(right3), sizeof(right3) - strlen(right3), "[%s/3]", indexRegister);
					else if (i->second.bt == DT_float4x4)
						sprintf_s(right3 + strlen(right3), sizeof(right3) - strlen(right3), "[%s/4]", indexRegister);
					else
					{
						// Most common case, like: g_AmbientCube[r3.w]

						// Bug was to not handle the struct case here, and truncate string.
						//  Like g_OmniLights[r5.w].m_PositionFar -> g_OmniLights[r5.w]
						//sprintf(right3 + strlen(right3), "[%s]", indexRegister);

						// Start fresh with original string and just replace, not char* manipulate.
						// base: g_OmniLights[0].m_PositionFar
						string base = i->second.Name;
						size_t left = base.find('[') + 1;
						size_t length = base.find(']') - left;
						base.replace(left, length, indexRegister);
						strcpy(right3, base.c_str());
					}
				}
				if (i->second.bt != DT_float && i->second.bt != DT_bool && i->second.bt != DT_uint && i->second.bt != DT_int)
				{
					strcat(right3, ".");
					if (i->second.bt == DT_float4x4 || i->second.bt == DT_float4x3 || i->second.bt == DT_float4x2 || i->second.bt == DT_float3x4 || i->second.bt == DT_float3x3)
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
		else
			strcpy_s(right, opcodeSize, right2);		// All input params are 128 char arrays, like op1, op2, op3
	}


	char statement[128],
		op1[opcodeSize], op2[opcodeSize], op3[opcodeSize], op4[opcodeSize], op5[opcodeSize], op6[opcodeSize], op7[opcodeSize], op8[opcodeSize],
		op9[opcodeSize], op10[opcodeSize], op11[opcodeSize], op12[opcodeSize], op13[opcodeSize], op14[opcodeSize], op15[opcodeSize];
	int ReadStatement(const char *pos)
	{
		// Kill newline.
		static char lineBuffer[256];
		strncpy(lineBuffer, pos, 255); lineBuffer[255] = 0;
		char *newlinePos = strchr(lineBuffer, '\n'); if (newlinePos) *newlinePos = 0;
		op1[0] = 0; op2[0] = 0; op3[0] = 0; op4[0] = 0; op5[0] = 0; op6[0] = 0; op7[0] = 0; op8[0] = 0;
		op9[0] = 0; op10[0] = 0; op11[0] = 0; op12[0] = 0; op13[0] = 0; op14[0] = 0; op15[0] = 0;

		int numRead = sscanf_s(lineBuffer, "%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s",
			statement, sizeof(statement),
			op1, opcodeSize, op2, opcodeSize, op3, opcodeSize, op4, opcodeSize, op5, opcodeSize, op6, opcodeSize, op7, opcodeSize, op8, opcodeSize,
			op9, opcodeSize, op10, opcodeSize, op11, opcodeSize, op12, opcodeSize, op13, opcodeSize, op14, opcodeSize, op15, opcodeSize);

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
			else if (!strncmp(textype, "TextureCube", 11))
			{
				// left as: float4(x,y,z,w)
			}
		}
		else
		{
			// Normal variant like r1.xyxx
			int pos = 5;
			if (!strncmp(textype, "Texture2DArray", 14)) pos = 4;
			else if (!strncmp(textype, "Texture2D", 9)) pos = 3;
			else if (!strncmp(textype, "Texture3D", 9)) pos = 4;
			else if (!strncmp(textype, "TextureCube", 11)) pos = 4;
			cpos = strrchr(op, '.');
			cpos[pos] = 0;
		}
	}

	// TODO: why are there two of these routines?

	// This routine was expecting only r0.xyz type input parameters, but we can also get float4(0,0,0,0) type
	// inputs as constants to things like .Load().  If it's a constant of any form, leave it unchanged.
	void truncateTextureLoadPos(char *op, const char *textype)
	{
		if (!strncmp(op, "float", 5)) 
			return;

		int pos = 5;
		if (!strncmp(textype, "Texture2D", 9)) pos = 4;
		else if (!strncmp(textype, "Texture3D", 9)) pos = 5;
		else if (!strncmp(textype, "TextureCube", 11)) pos = 5;
		char *cpos = strrchr(op, '.');
		cpos[pos] = 0;
	}

	void remapTarget(char *target)
	{
		char *pos = strchr(target, ',');
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

	void convertHexToFloat(char *target)
	{
		char convert[opcodeSize];
		int count;
		float lit[4];
		int printed;

		if (target[0] == 'l')
		{
			count = sscanf_s(target, "l(%x,%x,%x,%x)", &lit[0], &lit[1], &lit[2], &lit[3]);
			assert(count != 0);

			printed = sprintf_s(convert, sizeof(convert), "l(");
			for (int i = 0; i < count; i++)
			{
				printed += sprintf_s(&convert[printed], sizeof(convert) - printed, "%f,", lit[i]);
			}

			// Overwrite trailing comma to be closing paren, no matter how many literals were converted.
			convert[printed - 1] = ')';

			strcpy_s(target, opcodeSize, convert);
		}
	}

	char *convertToInt(char *target)
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
		char *pos = strrchr(target, '.');
		if (pos)
		{
			size_t size = strlen(pos + 1);
			if (size == 1)
				sprintf(buffer, "(int)%s", target);
			else
				sprintf(buffer, "(int%d)%s", size, target);
			strcpy_s(target, opcodeSize, buffer);
		}
		return target;
	}

	char *convertToUInt(char *target)
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
		char *pos = strrchr(target, '.');
		if (pos)
		{
			size_t size = strlen(pos + 1);
			if (size == 1)
				sprintf(buffer, "(uint)%s", target);
			else
				sprintf(buffer, "(uint%d)%s", size, target);
			strcpy_s(target, opcodeSize, buffer);
		}
		return target;
	}

	bool isBoolean(char *arg)
	{
		string regName = arg[0] == '-' ? arg + 1 : arg;
		size_t dotPos = regName.rfind('.');
		if (dotPos >= 0) regName = regName.substr(0, dotPos);
		set<string>::iterator i = mBooleanRegisters.find(regName);
		return i != mBooleanRegisters.end();
	}

	void removeBoolean(char *arg)
	{
		string regName = arg[0] == '-' ? arg + 1 : arg;
		size_t dotPos = regName.rfind('.');
		if (dotPos >= 0) regName = regName.substr(0, dotPos);
		mBooleanRegisters.erase(regName);
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
					sprintf_s(op, opcodeSize, "l(%.9e,%.9e,%.9e,%.9e)", o.afImmediates[0], o.afImmediates[1], o.afImmediates[2], o.afImmediates[3]);
				else if (o.iNumComponents == 3)
					sprintf_s(op, opcodeSize, "l(%.9e,%.9e,%.9e)", o.afImmediates[0], o.afImmediates[1], o.afImmediates[2]);
				else if (o.iNumComponents == 2)
					sprintf_s(op, opcodeSize, "l(%.9e,%.9e)", o.afImmediates[0], o.afImmediates[1]);
				else if (o.iNumComponents == 1)
					sprintf_s(op, opcodeSize, "l(%.9e)", o.afImmediates[0]);
			}
		}
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
			if (!BackProject_Vector1.empty())
				backProjectVector1 = BackProject_Vector1.substr(0, BackProject_Vector1.find_first_of(".,"));
			if (!BackProject_Vector2.empty())
				backProjectVector2 = BackProject_Vector2.substr(0, BackProject_Vector2.find_first_of(".,"));
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
						sprintf(buf, "  viewDirection = float3(%s);\n", BackProject_Vector1.c_str());
					else
						sprintf(buf, "  viewDirection = float3(%s);\n", BackProject_Vector2.c_str());
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
						sprintf(buffer, "%s.x -= separation * (%s.w - convergence);\n", mSV_Position.c_str(), mSV_Position.c_str(), mSV_Position.c_str());
						mOutput.insert(mOutput.end() - 1, buffer, buffer + strlen(buffer));
					}
				}
				mOutput.pop_back();
			}

			// Process copies of SV_Position.
			if (!isMono && mFixSvPosition && mUsesProjection && !mSV_Position.empty())
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
							sprintf(buffer, "\n%s.x += separation * (%s.w - convergence);", outputReg.c_str(), outputReg.c_str(), outputReg.c_str());
							mOutput.insert(writePos, buffer, buffer + strlen(buffer));
							mPatched = true;
						}
					}
				}
			}
			if (mRecompileVs) mPatched = true;
		}

		// Pixel shader patches.
		if (!mShaderType.substr(0, 3).compare("ps_"))
		{
			// Search for depth texture.
			bool wposAvailable = false;
			map<int, string>::iterator depthTexture;
			for (depthTexture = mTextureNames.begin(); depthTexture != mTextureNames.end(); ++depthTexture)
			{
				if (depthTexture->second == ZRepair_DepthTexture1)
					break;
			}
			if (depthTexture != mTextureNames.end())
			{
				long found = 0;
				for (CBufferData::iterator i = mCBufferData.begin(); i != mCBufferData.end(); ++i)
					for (unsigned int j = 0; j < ZRepair_Dependencies1.size(); ++j)
						if (i->second.Name == ZRepair_Dependencies1[j])
							found |= 1 << j;
				if (!ZRepair_Dependencies1.size() || found == (1 << ZRepair_Dependencies1.size()) - 1)
				{
					mOutput.push_back(0);
					// Search depth texture usage.
					sprintf(op1, " = %s.Sample", ZRepair_DepthTexture1.c_str());
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
								"float wpos = 1.0 / zpos;\n", depthBufferStatement.c_str(), ZRepair_DepthTextureReg1, ZRepair_ZPosCalc1.c_str());
						}
						else
						{
							sprintf(buf, "zpos4 = %s;\n"
								"zTex = zpos4.%c;\n"
								"zpos = %s;\n"
								"wpos = 1.0 / zpos;\n", depthBufferStatement.c_str(), ZRepair_DepthTextureReg1, ZRepair_ZPosCalc1.c_str());
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
					if (depthTexture->second == ZRepair_DepthTexture2)
						break;
				}
				if (depthTexture != mTextureNames.end())
				{
					long found = 0;
					for (CBufferData::iterator i = mCBufferData.begin(); i != mCBufferData.end(); ++i)
						for (unsigned int j = 0; j < ZRepair_Dependencies2.size(); ++j)
							if (i->second.Name == ZRepair_Dependencies2[j])
								found |= 1 << j;
					if (!ZRepair_Dependencies2.size() || found == (1 << ZRepair_Dependencies2.size()) - 1)
					{
						mOutput.push_back(0);
						// Search depth texture usage.
						sprintf(op1, " = %s.Sample", ZRepair_DepthTexture2.c_str());
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
								"float wpos = 1.0 / zpos;\n", depthBufferStatement.c_str(), ZRepair_DepthTextureReg2, ZRepair_ZPosCalc2.c_str());

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
					if (positionTexture->second == ZRepair_PositionTexture)
						break;
				}
				if (positionTexture != mTextureNames.end())
				{
					mOutput.push_back(0);
					// Search position texture usage.
					sprintf(op1, " = %s.Sample", ZRepair_PositionTexture.c_str());
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
						sprintf(calcStatement, ZRepair_WorldPosCalc.c_str(), buf);
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
			if (!wposAvailable && mZRepair_DepthBuffer)
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

			if (wposAvailable && InvTransforms.size())
			{
				CBufferData::iterator keyFind;
				for (keyFind = mCBufferData.begin(); keyFind != mCBufferData.end(); ++keyFind)
				{
					bool found = false;
					for (vector<string>::iterator j = InvTransforms.begin(); j != InvTransforms.end(); ++j)
						if (keyFind->second.Name == *j)
							found = true;
					if (found) break;
					if (!ObjectPos_ID1.empty() && keyFind->second.Name.find(ObjectPos_ID1) != string::npos)
						break;
					if (!ObjectPos_ID2.empty() && keyFind->second.Name.find(ObjectPos_ID2) != string::npos)
						break;
					if (!MatrixPos_ID1.empty() && keyFind->second.Name == MatrixPos_ID1)
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

					for (vector<string>::iterator invT = InvTransforms.begin(); invT != InvTransforms.end(); ++invT)
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

					if (!ObjectPos_ID1.empty())
					{
						size_t offset = strstr(mOutput.data(), "void main(") - mOutput.data();	// pointer difference, but only used as offset.
						while (offset < mOutput.size())
						{
							char *pos = strstr(mOutput.data() + offset, ObjectPos_ID1.c_str());
							if (!pos) break;
							pos += ObjectPos_ID1.length();
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
								if (ObjectPos_MUL1.empty())
									ObjectPos_MUL1 = string("1,1,1");
								sprintf(buf, "\nfloat3 stereoPos%dMul = float3(%s);"
									"\n%s += viewDirection.%c * separation * (wpos - convergence) * stereoPos%dMul.x;"
									"\n%s += viewDirection.%c * separation * (wpos - convergence) * stereoPos%dMul.y;"
									"\n%s += viewDirection.%c * separation * (wpos - convergence) * stereoPos%dMul.z;",
									uuidVar, ObjectPos_MUL1.c_str(),
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

					if (!ObjectPos_ID2.empty())
					{
						size_t offset = strstr(mOutput.data(), "void main(") - mOutput.data();	// pointer difference, but only used as offset.
						while (offset < mOutput.size())
						{
							char *pos = strstr(mOutput.data() + offset, ObjectPos_ID2.c_str());
							if (!pos) break;
							pos += ObjectPos_ID2.length();
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
								if (ObjectPos_MUL2.empty())
									ObjectPos_MUL2 = string("1,1,1");
								sprintf(buf, "\nfloat3 stereoPos%dMul = float3(%s);"
									"\n%s += viewDirection.%c * separation * (wpos - convergence) * stereoPos%dMul.x;"
									"\n%s += viewDirection.%c * separation * (wpos - convergence) * stereoPos%dMul.y;"
									"\n%s += viewDirection.%c * separation * (wpos - convergence) * stereoPos%dMul.z;",
									uuidVar, ObjectPos_MUL2.c_str(),
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

					if (!MatrixPos_ID1.empty())
					{
						string ShadowPos1 = MatrixPos_ID1 + "._m00_m10_m20_m30 * ";
						string ShadowPos2 = MatrixPos_ID1 + "._m01_m11_m21_m31 * ";
						string ShadowPos3 = MatrixPos_ID1 + "._m02_m12_m22_m32 * ";
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
							if (MatrixPos_MUL1.empty())
								MatrixPos_MUL1 = string("1,1,1");
							sprintf(buf, "\nfloat3 stereoMat%dMul = float3(%s);"
								"\n%s -= viewDirection.x * separation * (wpos - convergence) * stereoMat%dMul.x;"
								"\n%s -= viewDirection.y * separation * (wpos - convergence) * stereoMat%dMul.y;"
								"\n%s -= viewDirection.z * separation * (wpos - convergence) * stereoMat%dMul.z;",
								uuidVar, MatrixPos_MUL1.c_str(),
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
								while (*posParam != '\n') --posParam;
								mOutput.insert(mOutput.begin() + (posParam - mOutput.data()), NewParam, NewParam + strlen(NewParam));
								parameterWritten = true;
							}
						}
					}

					mOutput.pop_back();
				}
			}

		}
	}

	void ParseCode(Shader *shader, const char *c, size_t size)
	{
		mOutputRegisterValues.clear();
		mBooleanRegisters.clear();
		mCodeStartPos = mOutput.size();

		char buffer[512];
		size_t pos = 0;
		unsigned int iNr = 0;
		while (pos < size && iNr < shader->psInst.size())
		{
			Instruction *instr = &shader->psInst[iNr];

			// Ignore comments.
			if (!strncmp(c + pos, "//", 2))
			{
				while (c[pos] != 0x0a && pos < size) pos++; pos++;
				continue;
			}
			// Read statement.
			if (ReadStatement(c + pos) < 1)
			{
				logDecompileError("Error parsing statement: " + string(c + pos, 80));
				return;
			}
			//if (LogFile && LogDebug) fprintf(LogFile, "parsing statement %s with args %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s\n", statement,
			//	op1, op2, op3, op4, op5, op6, op7, op8, op9, op10, op11, op12, op13, op14, op15);
			//

			if (!strncmp(statement, "vs_", 3) ||
				!strncmp(statement, "ps_", 3) ||
				!strncmp(statement, "cs_", 3))
			{
				mShaderType = statement;
			}
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
				char *strPos = strstr(op1, "cb");
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
						logDecompileError("Error parsing buffer offset: " + string(op1));
						return;
					}
					int bufSize = 0;
					if (sscanf_s(strPos + 1, "%d", &bufSize) != 1)
					{
						logDecompileError("Error parsing buffer size: " + string(op1));
						return;
					}
					CBufferData::iterator i = mCBufferData.find(bufIndex << 16);
					// Create if not existing.
					if (i == mCBufferData.end())
					{
						BufferEntry e;
						e.bt = DT_float4;
						e.matrixRow = 0;
						e.isRowMajor = false;
						sprintf(buffer, "cbuffer cb%d : register(b%d)\n"
							"{\n"
							"  float4 cb%d[%d];\n"
							"}\n\n", bufIndex, bufIndex, bufIndex, bufSize);
						vector<char>::iterator ipos = mOutput.insert(mOutput.begin(), buffer, buffer + strlen(buffer));
						mCodeStartPos += strlen(buffer); ipos += strlen(buffer);
						for (int j = 0; j < bufSize; ++j)
						{
							sprintf(buffer, "cb%d[%d]", bufIndex, j);
							e.Name = buffer;
							mCBufferData[(bufIndex << 16) + j * 16] = e;
						}
					}
				}
			}
			else if (!strcmp(statement, "dcl_sampler"))
			{
				if (op1[0] == 's')
				{
					int bufIndex = 0;
					if (sscanf_s(op1 + 1, "%d", &bufIndex) != 1)
					{
						logDecompileError("Error parsing sampler register index: " + string(op1));
						return;
					}
					// Create if not existing.
					map<int, string>::iterator i = mSamplerNames.find(bufIndex);
					if (i == mSamplerNames.end())
					{
						i = mSamplerComparisonNames.find(bufIndex);
						if (i == mSamplerComparisonNames.end())
						{
							sprintf(buffer, "s%d", bufIndex);
							mSamplerNames[bufIndex] = buffer;
							sprintf(buffer, "SamplerState s%d : register(s%d);\n\n", bufIndex, bufIndex);
							mOutput.insert(mOutput.begin(), buffer, buffer + strlen(buffer));
							mCodeStartPos += strlen(buffer);
						}
					}
				}
			}
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
					// Create if not existing.
					map<int, string>::iterator i = mTextureNames.find(bufIndex);
					if (i == mTextureNames.end())
					{
						sprintf(buffer, "t%d", bufIndex);
						mTextureNames[bufIndex] = buffer;
						sprintf(buffer, "Texture2D<float4> t%d : register(t%d);\n\n", bufIndex, bufIndex);
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
				int numIndex = 0;
				*strchr(op1, '[') = 0;
				sscanf_s(op2, "%d", &numIndex);
				sprintf(buffer, "  float4 %s[%d];\n", op1, numIndex);
				mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
			}
			else if (!strcmp(statement, "dcl_temps"))
			{
				const char *varDecl = "  float4 ";
				mOutput.insert(mOutput.end(), varDecl, varDecl + strlen(varDecl));
				int numTemps;
				sscanf_s(c + pos, "%s %d", statement, sizeof(statement), &numTemps);
				for (int i = 0; i < numTemps; ++i)
				{
					sprintf(buffer, "r%d,", i);
					mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
				}
				mOutput.pop_back();
				mOutput.push_back(';');
				mOutput.push_back('\n');
				const char *helperDecl = "  uint4 bitmask;\n";
				mOutput.insert(mOutput.end(), helperDecl, helperDecl + strlen(helperDecl));
			}
			else if (!strncmp(statement, "dcl_", 4))
			{
				// Other declarations.
			}
			else
			{
				switch (instr->eOpcode)
				{

					case OPCODE_ITOF:
					case OPCODE_UTOF:
					case OPCODE_MOV:
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = %s;\n", writeTarget(op1), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(%s);\n", writeTarget(op1), ci(op2).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						if (op1[0] == 'o')
						{
							char *dotPos = strchr(op1, '.'); if (dotPos) *dotPos = 0;
							if (!dotPos || dotPos[1] == 'x')
								mOutputRegisterValues[op1] = op2;
						}
						removeBoolean(op1);
						break;

					case OPCODE_NOT:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = ~%s;\n", writeTarget(op1), ci(convertToInt(op2)).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_INEG:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						sprintf(buffer, "  %s = -%s;\n", writeTarget(op1), ci(convertToInt(op2)).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_F32TOF16:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = f32tof16(%s);\n", writeTarget(op1), ci(op2).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_F16TOF32:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = f16tof32(%s);\n", writeTarget(op1), ci(op2).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_FRC:
						remapTarget(op1);
						applySwizzle(op1, op2);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = frac(%s);\n", writeTarget(op1), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(frac(%s));\n", writeTarget(op1), ci(op2).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
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
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
						break;

					case OPCODE_IMUL:
						remapTarget(op2);
						applySwizzle(op2, op3, true);
						applySwizzle(op2, op4, true);
						mMulOperand = strncmp(op3, "int", 3) ? op3 : op4;
						sprintf(buffer, "  %s = %s * %s;\n", writeTarget(op2), ci(convertToInt(op3)).c_str(), ci(convertToInt(op4)).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
						break;

					case OPCODE_DIV:
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = %s / %s;\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						else
							sprintf(buffer, "  %s = saturate(%s / %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
						break;
					case OPCODE_UDIV:
					{
						remapTarget(op1);
						remapTarget(op2);
						char *maskOp = instr->asOperands[0].eType != OPERAND_TYPE_NULL ? op1 : op2;
						applySwizzle(maskOp, fixImm(op3, instr->asOperands[2]), true);
						applySwizzle(maskOp, fixImm(op4, instr->asOperands[3]), true);
						if (instr->asOperands[0].eType != OPERAND_TYPE_NULL)
						{
							if (!instr->bSaturate)
								sprintf(buffer, "  %s = %s / %s;\n", writeTarget(op1), ci(op3).c_str(), ci(op4).c_str());
							else
								sprintf(buffer, "  %s = saturate(%s / %s);\n", writeTarget(op1), ci(op3).c_str(), ci(op4).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						}
						if (instr->asOperands[1].eType != OPERAND_TYPE_NULL)
						{
							if (!instr->bSaturate)
								sprintf(buffer, "  %s = %s %% %s;\n", writeTarget(op2), ci(op3).c_str(), ci(op4).c_str());
							else
								sprintf(buffer, "  %s = saturate(%s %% %s);\n", writeTarget(op2), ci(op3).c_str(), ci(op4).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						}
						removeBoolean(op1);
						break;
					}

					case OPCODE_ADD:
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = %s + %s;\n", writeTarget(op1), ci(op3).c_str(), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(%s + %s);\n", writeTarget(op1), ci(op3).c_str(), ci(op2).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
						break;

					case OPCODE_IADD:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						if (isBoolean(op2) && isBoolean(op3))
						{
							if (op2[0] == '-') strcpy(op2, op2 + 1);
							else if (op3[0] == '-') strcpy(op3, op3 + 1);
							sprintf(buffer, "  %s = (%s ? -1 : 0) + (%s ? 1 : 0);\n", writeTarget(op1), ci(convertToInt(op2)).c_str(), ci(convertToInt(op3)).c_str());
						}
						else
							sprintf(buffer, "  %s = %s + %s;\n", writeTarget(op1), ci(convertToInt(op2)).c_str(), ci(convertToInt(op3)).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

						// AND opcodes were generating bad code, as the hex constants were being converted badly.
						// The most common case was 0x3f800000 being converted directly to integer decimal of 1065353216, instead
						// of the most likely answer of floating point 1.0f.
						// This also happened for conversion of Pi.
						// There are bitmasks used for AND, and those need to stay as Hex constants.
						// But anything used after IF statements/booleans, needs to be converted as float.
						// Rather than modify applySwizzle for this single opcode, it makes more sense to convert them here,
						// before they are seen by applySwizzle as float.  
					case OPCODE_AND:
						remapTarget(op1);
						if (isBoolean(op2) || isBoolean(op3))
						{
							convertHexToFloat(op2);
							convertHexToFloat(op3);
						}
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						if (isBoolean(op2) || isBoolean(op3))
						{
							char *cmp = isBoolean(op2) ? op2 : op3;
							char *arg = isBoolean(op2) ? op3 : op2;
							int idx = 0;
							char *pop1 = strrchr(op1, '.'); *pop1 = 0;
							while (*++pop1)
							{
								sprintf(op4, "%s.%c", op1, *pop1);
								sprintf(buffer, "  %s = %s ? %s : 0;\n", writeTarget(op4), ci(GetSuffix(cmp, idx)).c_str(), ci(GetSuffix(arg, idx)).c_str());
								mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
								++idx;
							}
						}
						else
						{
							sprintf(buffer, "  %s = %s & %s;\n", writeTarget(op1), ci(convertToInt(op2)).c_str(), ci(convertToInt(op3)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						}
						break;

					case OPCODE_OR:
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						sprintf(buffer, "  %s = %s | %s;\n", writeTarget(op1), ci(convertToInt(op2)).c_str(), ci(convertToInt(op3)).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_XOR:
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						sprintf(buffer, "  %s = %s ^ %s;\n", writeTarget(op1), ci(convertToInt(op2)).c_str(), ci(convertToInt(op3)).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
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
						sprintf(buffer, "  %s = %s >> %s;\n", writeTarget(op1), ci(convertToUInt(op2)).c_str(), ci(convertToInt(op3)).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_ISHL:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						sprintf(buffer, "  %s = %s << %s;\n", writeTarget(op1), ci(convertToUInt(op2)).c_str(), ci(convertToInt(op3)).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

						// USHR appears to be documented correctly.
						// But this code was still backwards.
					case OPCODE_USHR:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						sprintf(buffer, "  %s = %s >> %s;\n", writeTarget(op1), ci(convertToUInt(op2)).c_str(), ci(convertToUInt(op3)).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_UBFE:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);	// width
						applySwizzle(op1, op3); // offset
						applySwizzle(op1, op4);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op5, "%s.%c", op1, *pop1);
							sprintf(buffer, "  if (%s == 0) %s = 0;\n"
								"  else if (%s+%s < 32) { %s = (int)%s << (32-(%s + %s)); %s = (uint)%s >> (32-%s); }\n"
								"  else %s = (uint)%s >> %s;\n",
								ci(GetSuffix(op2, idx)).c_str(), writeTarget(op5),
								ci(GetSuffix(op2, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str(), writeTarget(op5), ci(GetSuffix(op4, idx)).c_str(), ci(GetSuffix(op2, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str(), writeTarget(op5), writeTarget(op5), ci(GetSuffix(op2, idx)).c_str(),
								writeTarget(op5), ci(GetSuffix(op4, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
						break;
					}
					case OPCODE_EXP:
						remapTarget(op1);
						applySwizzle(op1, op2);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = exp2(%s);\n", writeTarget(op1), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(exp2(%s));\n", writeTarget(op1), ci(op2).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
						break;

					case OPCODE_LOG:
						remapTarget(op1);
						applySwizzle(op1, op2);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = log2(%s);\n", writeTarget(op1), ci(op2).c_str());
						else
							sprintf(buffer, "  %s = saturate(log2(%s));\n", writeTarget(op1), ci(op2).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
						break;

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
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
						break;

					case OPCODE_MIN:
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = min(%s, %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						else
							sprintf(buffer, "  %s = saturate(min(%s, %s));\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_MAX:
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = max(%s, %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						else
							sprintf(buffer, "  %s = saturate(max(%s, %s));\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_IMIN:
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]), true);
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]), true);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = min(%s, %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						else
							sprintf(buffer, "  %s = saturate(min(%s, %s));\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_IMAX:
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]), true);
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]), true);
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = max(%s, %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						else
							sprintf(buffer, "  %s = saturate(max(%s, %s));\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

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
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
						break;

					case OPCODE_IMAD:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						applySwizzle(op1, op4, true);
						sprintf(buffer, "  %s = mad(%s, %s, %s);\n", writeTarget(op1), ci(convertToInt(op2)).c_str(), ci(convertToInt(op3)).c_str(), ci(convertToInt(op4)).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_UMAD:
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						applySwizzle(op1, op4, true);
						sprintf(buffer, "  %s = mad(%s, %s, %s);\n", writeTarget(op1), ci(convertToUInt(op2)).c_str(), ci(convertToUInt(op3)).c_str(), ci(convertToUInt(op4)).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_DP2:
						remapTarget(op1);
						applySwizzle(".xy", fixImm(op2, instr->asOperands[1]));
						applySwizzle(".xy", fixImm(op3, instr->asOperands[2]));
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = dot(%s, %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						else
							sprintf(buffer, "  %s = saturate(dot(%s, %s));\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
						break;

					case OPCODE_DP3:
						remapTarget(op1);
						applySwizzle(".xyz", fixImm(op2, instr->asOperands[1]));
						applySwizzle(".xyz", fixImm(op3, instr->asOperands[2]));
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = dot(%s, %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						else
							sprintf(buffer, "  %s = saturate(dot(%s, %s));\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
						break;

					case OPCODE_DP4:
						remapTarget(op1);
						applySwizzle(".xyzw", fixImm(op2, instr->asOperands[1]));
						applySwizzle(".xyzw", fixImm(op3, instr->asOperands[2]));
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = dot(%s, %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						else
							sprintf(buffer, "  %s = saturate(dot(%s, %s));\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
						break;

					case OPCODE_RSQ:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op3, "%s.%c", op1, *pop1);
							if (!instr->bSaturate)
								sprintf(buffer, "  %s = rsqrt(%s);\n", writeTarget(op3), ci(GetSuffix(op2, idx)).c_str());
							else
								sprintf(buffer, "  %s = saturate(rsqrt(%s));\n", writeTarget(op3), ci(GetSuffix(op2, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
						removeBoolean(op1);
						break;
					}
					case OPCODE_ROUND_NI:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op3, "%s.%c", op1, *pop1);
							if (!instr->bSaturate)
								sprintf(buffer, "  %s = floor(%s);\n", writeTarget(op3), ci(GetSuffix(op2, idx)).c_str());
							else
								sprintf(buffer, "  %s = saturate(floor(%s));\n", writeTarget(op3), ci(GetSuffix(op2, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
						removeBoolean(op1);
						break;
					}
					case OPCODE_ROUND_PI:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op3, "%s.%c", op1, *pop1);
							if (!instr->bSaturate)
								sprintf(buffer, "  %s = ceil(%s);\n", writeTarget(op3), ci(GetSuffix(op2, idx)).c_str());
							else
								sprintf(buffer, "  %s = saturate(ceil(%s));\n", writeTarget(op3), ci(GetSuffix(op2, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
						removeBoolean(op1);
						break;
					}
					case OPCODE_ROUND_Z:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op3, "%s.%c", op1, *pop1);
							if (!instr->bSaturate)
								sprintf(buffer, "  %s = round(%s);\n", writeTarget(op3), ci(GetSuffix(op2, idx)).c_str());
							else
								sprintf(buffer, "  %s = saturate(round(%s));\n", writeTarget(op3), ci(GetSuffix(op2, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
						removeBoolean(op1);
						break;
					}
					case OPCODE_ROUND_NE:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op3, "%s.%c", op1, *pop1);
							if (!instr->bSaturate)
								sprintf(buffer, "  %s = round(%s * 0.5) * 2;\n", writeTarget(op3), ci(GetSuffix(op2, idx)).c_str());
							else
								sprintf(buffer, "  %s = saturate(round(%s * 0.5) * 2);\n", writeTarget(op3), ci(GetSuffix(op2, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
						removeBoolean(op1);
						break;
					}
					case OPCODE_FTOI:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = %s;\n", writeTarget(op1), ci(convertToInt(op2)).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_FTOU:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = %s;\n", writeTarget(op1), ci(convertToUInt(op2)).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
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
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
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
						if (!instr->bSaturate)
							sprintf(buffer, "  %s = %s ? %s : %s;\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str(), ci(op4).c_str());
						else
							sprintf(buffer, "  %s = saturate(%s ? %s : %s);\n", writeTarget(op1), ci(op2).c_str(), ci(op3).c_str(), ci(op4).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));

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
							//mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						//	++idx;
						//}
						mBooleanRegisters.insert(op1);
						removeBoolean(op1);
						break;
					}
					case OPCODE_NE:
					case OPCODE_INE:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op4, "%s.%c", op1, *pop1);
							sprintf(buffer, "  %s = %s != %s;\n", writeTarget(op4), ci(GetSuffix(op2, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
						mBooleanRegisters.insert(op1);
						break;
					}
					case OPCODE_EQ:
					case OPCODE_IEQ:
					{
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op4, "%s.%c", op1, *pop1);
							sprintf(buffer, "  %s = %s == %s;\n", writeTarget(op4), ci(GetSuffix(op2, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
						mBooleanRegisters.insert(op1);
						break;
					}
					case OPCODE_LT:
					{
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op4, "%s.%c", op1, *pop1);
							sprintf(buffer, "  %s = %s < %s;\n", writeTarget(op4), ci(GetSuffix(op2, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
						mBooleanRegisters.insert(op1);
						break;
					}
					case OPCODE_ILT:
					{
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op4, "%s.%c", op1, *pop1);
							sprintf(buffer, "  %s = (int)%s < (int)%s;\n", writeTarget(op4), ci(GetSuffix(op2, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
						mBooleanRegisters.insert(op1);
						break;
					}
					case OPCODE_ULT:
					{
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op4, "%s.%c", op1, *pop1);
							sprintf(buffer, "  %s = (uint)%s < (uint)%s;\n", writeTarget(op4), ci(GetSuffix(op2, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
						mBooleanRegisters.insert(op1);
						break;
					}
					case OPCODE_GE:
					{
						remapTarget(op1);
						applySwizzle(op1, fixImm(op2, instr->asOperands[1]));
						applySwizzle(op1, fixImm(op3, instr->asOperands[2]));
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op4, "%s.%c", op1, *pop1);
							sprintf(buffer, "  %s = %s >= %s;\n", writeTarget(op4), ci(GetSuffix(op2, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
						mBooleanRegisters.insert(op1);
						break;
					}
					case OPCODE_IGE:
					{
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op4, "%s.%c", op1, *pop1);
							sprintf(buffer, "  %s = (int)%s >= (int)%s;\n", writeTarget(op4), ci(GetSuffix(op2, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
						mBooleanRegisters.insert(op1);
						break;
					}
					case OPCODE_UGE:
					{
						remapTarget(op1);
						applySwizzle(op1, op2, true);
						applySwizzle(op1, op3, true);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op4, "%s.%c", op1, *pop1);
							sprintf(buffer, "  %s = (uint)%s >= (uint)%s;\n", writeTarget(op4), ci(GetSuffix(op2, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
						mBooleanRegisters.insert(op1);
						break;
					}

						// Switch statement in HLSL was missing. Added because AC4 uses it.
					case OPCODE_SWITCH:
						sprintf(buffer, "  switch (%s) {\n", ci(op1).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_CASE:
						sprintf(buffer, "  case %s :", ci(op1).substr(2, 1).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_ENDSWITCH:
						sprintf(buffer, "  }\n");
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_DEFAULT:
						sprintf(buffer, "  default :\n");
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_IF:
						applySwizzle(".x", op1);
						if (instr->eBooleanTestType == INSTRUCTION_TEST_ZERO)
							sprintf(buffer, "  if (%s == 0) {\n", ci(op1).c_str());
						else
							sprintf(buffer, "  if (%s != 0) {\n", ci(op1).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_ELSE:
						sprintf(buffer, "  } else {\n");
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_ENDIF:
						sprintf(buffer, "  }\n");
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_LOOP:
						sprintf(buffer, "  while (true) {\n", op1);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_BREAK:
						sprintf(buffer, "  break;\n");
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_BREAKC:
						applySwizzle(".x", op1);
						if (instr->eBooleanTestType == INSTRUCTION_TEST_ZERO)
							sprintf(buffer, "  if (%s == 0) break;\n", ci(op1).c_str());
						else
							sprintf(buffer, "  if (%s != 0) break;\n", ci(op1).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_ENDLOOP:
						sprintf(buffer, "  }\n", op1);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_SWAPC:
					{
						remapTarget(op1);
						remapTarget(op2);
						applySwizzle(op1, op3);
						applySwizzle(op1, op4);
						applySwizzle(op1, op5);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						char *pop2 = strrchr(op2, '.'); if (pop2) *pop2 = 0;
						char *pop3 = strrchr(op3, '.'); if (pop3) *pop3 = 0;
						while (*++pop1)
						{
							sprintf(op6, "%s.%c", op1, *pop1);
							if (pop2) sprintf(op7, "%s.%c", op2, *++pop2); else sprintf(op7, "%s", op2);
							if (pop3) sprintf(op8, "%s.%c", op3, *++pop3); else sprintf(op8, "%s", op3);
							sprintf(buffer, "  %s = (int)%s ? %s : %s; %s = (int)%s ? %s : %s;\n",
								writeTarget(op6), ci(op8).c_str(), ci(GetSuffix(op5, idx)).c_str(), ci(GetSuffix(op4, idx)).c_str(),
								writeTarget(op7), ci(op8).c_str(), ci(GetSuffix(op4, idx)).c_str(), ci(GetSuffix(op5, idx)).c_str());
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							++idx;
						}
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
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						applySwizzle(op1, op4);
						applySwizzle(op1, op5);
						int idx = 0;
						char *pop1 = strrchr(op1, '.'); *pop1 = 0;
						while (*++pop1)
						{
							sprintf(op6, "%s.%c", op1, *pop1);

							// Fails: bitmask.%c = (((1 << %s) - 1) << %s) & 0xffffffff;

							sprintf(buffer, "  bitmask.%c = ((~(-1 << %s)) << %s) & 0xffffffff;\n"
								"  %s = (((uint)%s << %s) & bitmask.%c) | ((uint)%s & ~bitmask.%c);\n",
								*pop1, ci(GetSuffix(op2, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str(),
								writeTarget(op6), ci(GetSuffix(op4, idx)).c_str(), ci(GetSuffix(op3, idx)).c_str(), *pop1, ci(GetSuffix(op5, idx)).c_str(), *pop1);
							mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						}
						break;
					}
					case OPCODE_SAMPLE:
					{
						//	else if (!strncmp(statement, "sample_indexable", strlen("sample_indexable")))
						remapTarget(op1);
						applySwizzle(".xyzw", op2);
						applySwizzle(op1, op3);
						int textureId, samplerId;
						sscanf_s(op3, "t%d.", &textureId);
						sscanf_s(op4, "s%d", &samplerId);
						truncateTexturePos(op2, mTextureType[textureId].c_str());
						sprintf(buffer, "  %s = %s.Sample(%s, %s)%s;\n", writeTarget(op1),
							mTextureNames[textureId].c_str(), mSamplerNames[samplerId].c_str(), ci(op2).c_str(), strrchr(op3, '.'));
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
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
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
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
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
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
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
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
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
						break;
					}
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
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
						break;
					}
						// This opcode was missing, and used in WatchDogs. 
						// expected code "samplepos r0.xy, t1.xyxx, v1.x" -> "r0.xy = t1.GetSamplePosition(v1.x);"
					case OPCODE_SAMPLE_POS:
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(op1, op3);
						int textureId;
						sscanf_s(op2, "t%d.", &textureId);
						sprintf(buffer, "  %s = %s.GetSamplePosition(%s);\n", writeTarget(op1),
							mTextureNames[textureId].c_str(), ci(op3).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

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
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
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
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
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
						if (!instr->bAddressOffset)
							sprintf(buffer, "  %s = %s.Load(%s)%s;\n", writeTarget(op1), mTextureNames[textureId].c_str(), ci(op2).c_str(), strrchr(op3, '.'));
						else {
							int offsetU = 0, offsetV = 0, offsetW = 0;
							sscanf_s(statement, "ld_aoffimmi(%d,%d,%d", &offsetU, &offsetV, &offsetW);
							sprintf(buffer, "  %s = %s.Load(%s, int3(%d, %d, %d))%s;\n", writeTarget(op1), mTextureNames[textureId].c_str(), ci(op2).c_str(),
								offsetU, offsetV, offsetW, strrchr(op3, '.'));
						}
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
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
						if (!instr->bAddressOffset)
							sprintf(buffer, "  %s = %s.Load(%s,%s)%s;\n", writeTarget(op1), mTextureNames[textureId].c_str(), ci(op2).c_str(), ci(op4).c_str(), strrchr(op3, '.'));
						else{
							int offsetU = 0, offsetV = 0, offsetW = 0;
							sscanf_s(statement, "ld_aoffimmi(%d,%d,%d", &offsetU, &offsetV, &offsetW);
							sprintf(buffer, "  %s = %s.Load(%s, %s, int3(%d, %d, %d))%s;\n", writeTarget(op1), mTextureNames[textureId].c_str(), ci(op2).c_str(), ci(op4).c_str(),
								offsetU, offsetV, offsetW, strrchr(op3, '.'));
						}
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						removeBoolean(op1);
						break;
					}

						// New variant found in Mordor.  Example:
						//   gInstanceBuffer                   texture  struct         r/o    0        1
						//   dcl_resource_structured t0, 16 
						//   ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r1.xyzw, r0.x, l(0), t0.xyzw
						// becomes:
						//   StructuredBuffer<float4> gInstanceBuffer : register(t0);
						//   ...
						//	  float4 c0 = gInstanceBuffer[worldMatrixOffset];

						// Example from Mordor, with bizarre struct offsets:
						// struct BufferSrc
						// {
						//	float3 vposition;              // offset:    0
						//	float3 vvelocity;              // offset:   12
						//	float ftime;                   // offset:   24
						//	float fuserdata;               // offset:   28
						// };                        			// offset:    0 size:    32
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

					case OPCODE_LD_STRUCTURED:
					{
						string dst0, srcAddress, srcByteOffset, src0;
						string swiz = "x";
						int reg;

						dst0 = "r" + std::to_string(instr->asOperands[0].ui32RegisterNumber);
						srcAddress = instr->asOperands[1].specialName;
						srcByteOffset = instr->asOperands[2].specialName;
						src0 = shader->sInfo->psResourceBindings->Name;

						// Output one line for each swizzle in dst0.xyzw that is active.
						for (size_t i = 0; i < 4; i++)
						{
							if (instr->asOperands[0].aui32Swizzle[i])
							{
								sprintf(buffer, "  %s.%s = %s[%s].%s.%s;\n", dst0.c_str(), swiz.c_str(),
									src0.c_str(), srcAddress.c_str(), srcByteOffset.c_str(), swiz.c_str());
								mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
							}
						}
						break;
					}
						//	  gInstanceBuffer[worldMatrixOffset] = x.y;
					case OPCODE_STORE_STRUCTURED:
					{
						remapTarget(op1);
						applySwizzle(".xyzw", op2);	// srcAddress structure
						applySwizzle(op1, op3);		// byteOffset in structure
						int textureId;
						sscanf_s(op4, "t%d.", &textureId);
						sprintf(buffer, "  %s[%s].%s = %s;\n", mTextureNames[textureId].c_str(), ci(op2).c_str(), ci(op3).c_str(), writeTarget(op1));
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					}

					case OPCODE_DISCARD:
						applySwizzle(".x", op1);
						if (instr->eBooleanTestType == INSTRUCTION_TEST_ZERO)
							sprintf(buffer, "  if (%s == 0) discard;\n", ci(op1).c_str());
						else
							sprintf(buffer, "  if (%s != 0) discard;\n", ci(op1).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_RESINFO:
					{
						remapTarget(op1);
						applySwizzle(".x", fixImm(op2, instr->asOperands[1]), true);
						if (instr->asOperands[1].eType == OPERAND_TYPE_IMMEDIATE32)
							strcpy(op2, "bitmask.x");
						int textureId;
						sscanf_s(op3, "t%d.", &textureId);
						sprintf(buffer, "  %s.GetDimensions(%s, %s, %s);\n", mTextureNames[textureId].c_str(), ci(GetSuffix(op1, 0)).c_str(), ci(GetSuffix(op1, 1)).c_str(), ci(op2).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					}

					case OPCODE_EVAL_SAMPLE_INDEX:
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(".x", fixImm(op3, instr->asOperands[2]), true);
						sprintf(buffer, "  %s = EvaluateAttributeAtSample(%s, %s);\n", writeTarget(op1), op2, op3);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_EVAL_CENTROID:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = EvaluateAttributeCentroid(%s);\n", writeTarget(op1), op2);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_EVAL_SNAPPED:
						remapTarget(op1);
						applySwizzle(op1, op2);
						applySwizzle(".xy", fixImm(op3, instr->asOperands[2]), true);
						sprintf(buffer, "  %s = EvaluateAttributeSnapped(%s, %s);\n", writeTarget(op1), op2, op3);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_DERIV_RTX_COARSE:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = ddx_coarse(%s);\n", writeTarget(op1), op2);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_DERIV_RTX_FINE:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = ddx_fine(%s);\n", writeTarget(op1), op2);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_DERIV_RTY_COARSE:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = ddy_coarse(%s);\n", writeTarget(op1), op2);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_DERIV_RTY_FINE:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = ddy_fine(%s);\n", writeTarget(op1), op2);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_DERIV_RTX:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = ddx(%s);\n", writeTarget(op1), op2);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;
					case OPCODE_DERIV_RTY:
						remapTarget(op1);
						applySwizzle(op1, op2);
						sprintf(buffer, "  %s = ddy(%s);\n", writeTarget(op1), op2);
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					case OPCODE_RET:
						sprintf(buffer, "  return;\n");
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

						// Missing opcode needed for WatchDogs. Used as "retc_nz r0.x"
					case OPCODE_RETC:
						applySwizzle(".x", op1);
						if (instr->eBooleanTestType == INSTRUCTION_TEST_ZERO)
							sprintf(buffer, "  if (%s == 0) return;\n", ci(op1).c_str());
						else
							sprintf(buffer, "  if (%s != 0) return;\n", ci(op1).c_str());
						mOutput.insert(mOutput.end(), buffer, buffer + strlen(buffer));
						break;

					default:
						logDecompileError("Unknown statement: " + string(statement));
						return;
				}
				iNr++;
			}

			// Next line.
			while (c[pos] != 0x0a && pos < size) pos++; pos++;
			mLastStatement = instr;
		}

		// Moved this out of Opcode_ret, because it's possible to have more than one ret
		// in a shader.  This is the last of a given shader, which seems more correct.
		// This fixes the double injection of "injectedScreenPos : SV_Position"
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
				while (c[pos] != 0x0a && pos < size) pos++; pos++;
				continue;
			}
			// Read statement.
			if (ReadStatement(c + pos) < 1)
			{
				logDecompileError("Error parsing statement: " + string(c + pos, 80));
				return;
			}
			if (!strncmp(statement, "vs_", 3) ||
				!strncmp(statement, "ps_", 3) ||
				!strncmp(statement, "cs_", 3))
			{
				mShaderType = statement;
				return;
			}

			// Next line.
			while (c[pos] != 0x0a && pos < size) pos++; pos++;
		}
	}

	// The StereoParams are nearly always useful, but the depth buffer texture is rarely used.
	void WriteAddOnDeclarations()
	{
		const char *StereoTextureCode = "\n"
			"Texture2D<float4> StereoParams : register(t125);\n";
		mOutput.insert(mOutput.end(), StereoTextureCode, StereoTextureCode + strlen(StereoTextureCode));

		if (mZRepair_DepthBuffer)
		{
			const char *DepthTextureCode = "Texture2D<float4> InjectedDepthTexture : register(t126);\n";
			mOutput.insert(mOutput.end(), DepthTextureCode, DepthTextureCode + strlen(DepthTextureCode));
		}
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
	d.mFixSvPosition = params.fixSvPosition;
	d.mRecompileVs = params.recompileVs;
	d.mPatched = false;
	d.ZRepair_DepthTexture1 = params.ZRepair_DepthTexture1;
	d.ZRepair_DepthTexture2 = params.ZRepair_DepthTexture2;
	d.ZRepair_DepthTextureReg1 = params.ZRepair_DepthTextureReg1;
	d.ZRepair_DepthTextureReg2 = params.ZRepair_DepthTextureReg2;
	d.ZRepair_Dependencies1 = params.ZRepair_Dependencies1;
	d.ZRepair_Dependencies2 = params.ZRepair_Dependencies2;
	d.ZRepair_ZPosCalc1 = params.ZRepair_ZPosCalc1;
	d.ZRepair_ZPosCalc2 = params.ZRepair_ZPosCalc2;
	d.ZRepair_PositionTexture = params.ZRepair_PositionTexture;
	d.ZRepair_WorldPosCalc = params.ZRepair_WorldPosCalc;
	d.mZRepair_DepthBuffer = params.ZRepair_DepthBuffer;
	d.BackProject_Vector1 = params.BackProject_Vector1;
	d.BackProject_Vector2 = params.BackProject_Vector2;
	d.InvTransforms = params.InvTransforms;
	d.ObjectPos_ID1 = params.ObjectPos_ID1;
	d.ObjectPos_ID2 = params.ObjectPos_ID2;
	d.ObjectPos_MUL1 = params.ObjectPos_MUL1;
	d.ObjectPos_MUL2 = params.ObjectPos_MUL2;
	d.MatrixPos_ID1 = params.MatrixPos_ID1;
	d.MatrixPos_MUL1 = params.MatrixPos_MUL1;

	// Decompile binary.

	// This can crash, because of unknown or unseen syntax, so we wrap it in try/catch
	// block to handle any exceptions and mark the shader as presently bad.
	// In order for this to work, the /EHa option must be enabled for code-generation
	// so that system level exceptions are caught too.
	try
	{
		Shader *shader = DecodeDXBC((uint32_t*)params.bytecode);
		if (!shader) return string();

		d.ReadResourceBindings(params.decompiled, params.decompiledSize);
		d.ParseBufferDefinitions(shader, params.decompiled, params.decompiledSize);
		d.WriteResourceDefinitions();
		d.WriteAddOnDeclarations();
		d.ParseInputSignature(params.decompiled, params.decompiledSize);
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
		if (LogFile) fprintf(LogFile, "   ******* Exception caught while decompiling shader ******\n");

		errorOccurred = true;
		return string();
	}
}
