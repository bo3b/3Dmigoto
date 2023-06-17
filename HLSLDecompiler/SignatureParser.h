#pragma once
#include "stdafx.h"
#include "Assembler.h"

HRESULT AssembleFluganWithSignatureParsing(vector<char>* assembly, vector<byte>* result_bytecode, vector<AssemblerParseError>* parse_errors = nullptr);

vector<byte> AssembleFluganWithOptionalSignatureParsing(vector<char>* assembly, bool assemble_signatures, vector<byte>* orig_bytecode, vector<AssemblerParseError>* parse_errors = nullptr);
