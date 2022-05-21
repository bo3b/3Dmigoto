#pragma once
#include "stdafx.h"
#include <unordered_map>

using namespace std;

// VS2013 BUG WORKAROUND: Make sure this class has a unique type name!
class AssemblerParseError : public exception {
public:
	string context, desc, msg;
	int line_no;

	AssemblerParseError(std::string context, string desc);

	void update_msg();

	const char* what() const;
};

struct shader_ins
{
	union {
		struct {
			// XXX Beware that bitfield packing is not defined in
			// the C/C++ standards and this is relying on compiler
			// specific packing. This approach is not recommended.

			unsigned opcode : 11;
			unsigned _11_23 : 13;
			unsigned length : 7;
			unsigned extended : 1;
		};
		DWORD op;
	};
};

HRESULT disassembler(vector<byte>* buffer, vector<byte>* ret, const char* comment, unordered_map<string, vector<DWORD>>& codeBin, int hexdump = 0, bool d3dcompiler_46_compat = false, bool disassemble_undecipherable_data = false, bool patch_cb_offsets = false);

vector<byte> assembler(vector<char>* asmFile, vector<byte> origBytecode, vector<AssemblerParseError>* parse_errors = nullptr);

HRESULT disassemblerDX9(vector<byte>* buffer, vector<byte>* ret, const char* comment);

vector<byte> assemblerDX9(vector<char>* asmFile);

string BinaryToAsmText(const void* pShaderBytecode, size_t BytecodeLength, bool patch_cb_offsets, bool disassemble_undecipherable_data = true, int hexdump = 0, bool d3dcompiler_46_compat = true);

string GetShaderModel(const void* pShaderBytecode, size_t bytecodeLength);

HRESULT CreateAsmTextFile(wchar_t* fileDirectory, UINT64 hash, const wchar_t* shaderType, const void* pShaderBytecode, size_t bytecodeLength, bool patch_cb_offsets);

vector<string> stringToLines(const char* start, size_t size);
