// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include "stdint.h"
#include "D3DCompiler.h"
#include <string>
#include <vector>
#include <unordered_map>

using namespace std;

struct shader_ins
{
	// XXX Beware that bitfield packing is not defined in
	// the C/C++ standards and this is relying on compiler
	// specific packing. This approach is not recommended.

	unsigned opcode : 11;
	unsigned _11_23 : 13;
	unsigned length : 7;
	unsigned extended : 1;
};
struct token_operand
{
	union {
		struct {
			// XXX Beware that bitfield packing is not defined in
			// the C/C++ standards and this is relying on compiler
			// specific packing. This approach is not recommended.

			unsigned comps_enum : 2; /* sm4_operands_comps */
			unsigned mode : 2; /* sm4_operand_mode */
			unsigned sel : 8;
			unsigned file : 8; /* SM_FILE */
			unsigned num_indices : 2;
			unsigned index0_repr : 3; /* sm4_operand_index_repr */
			unsigned index1_repr : 3; /* sm4_operand_index_repr */
			unsigned index2_repr : 3; /* sm4_operand_index_repr */
			unsigned extended : 1;
		};
		DWORD op;
	};
};

vector<string> stringToLines(const char* start, size_t size);
HRESULT disassembler(vector<byte> *buffer, vector<byte> *ret, const char *comment, bool hexdump = false);
vector<byte> assembler(vector<char> *asmFile, vector<byte> origBytecode);
void writeLUT();
HRESULT AssembleFluganWithSignatureParsing(vector<char> *assembly, vector<byte> *result_bytecode);
vector<byte> AssembleFluganWithOptionalSignatureParsing(vector<char> *assembly, bool assemble_signatures, vector<byte> *orig_bytecode);
