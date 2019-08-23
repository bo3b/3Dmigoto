// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>



// TODO: reference additional headers your program requires here
#include <string>
#include <vector>

using namespace std;

//struct shader_ins
//{
//	unsigned opcode : 11;
//	unsigned _11_23 : 13;
//	unsigned length : 7;
//	unsigned extended : 1;
//};
//struct token_operand
//{
//	unsigned comps_enum : 2; /* sm4_operands_comps */
//	unsigned mode : 2; /* sm4_operand_mode */
//	unsigned sel : 8;
//	unsigned file : 8; /* SM_FILE */
//	unsigned num_indices : 2;
//	unsigned index0_repr : 3; /* sm4_operand_index_repr */
//	unsigned index1_repr : 3; /* sm4_operand_index_repr */
//	unsigned index2_repr : 3; /* sm4_operand_index_repr */
//	unsigned extended : 1;
//};

vector<DWORD> assembleIns(string s);
vector<byte> readFile(string fileName);
vector<DWORD> ComputeHash(byte const* input, DWORD size);
vector<string> stringToLines(char* start, int size);

void assembler(string asmFile, vector<byte> & bc);