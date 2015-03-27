// cmd_Decompiler.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <iostream>     // console output 
#include <codecvt>

#include "..\DecompileHLSL.h"

#include "Shlwapi.h"

#include "mojoshader.h"

FILE *LogFile = 0;
bool LogInfo = true;

//----------------------------------------------------------------------------- 
// Print help lines on errors. 
//----------------------------------------------------------------------------- 
void PrintHelp()
{
	using std::cout;
	using std::endl;

	cout << endl;
	cout << L"3Dmigoto standalone Decompiler..." << endl;
	cout << endl;
	cout << L"Specify the ASM file name to Decompile, as command line parameter." << endl;
	cout << endl;
}


std::string Decompile(_TCHAR* asmFileName)
{
	using std::cout;
	using std::endl;
	using std::string;

	cout << "    creating HLSL representation.\n" << endl;

	// Given an ASM file, we need to reassemble it to get the binary of the shader, because
	// the Decompiler needs both ASM and binary for the James-Jones side.

	wstring_convert<codecvt_utf8_utf16<wchar_t>, wchar_t> convert;
	string utf8AsmFileName = convert.to_bytes(asmFileName);

	vector<byte> asmCode;
	asmCode = readFile(utf8AsmFileName);

	const MOJOSHADER_parseData * parseData = MOJOSHADER_assemble(utf8AsmFileName.c_str(), (const char *)asmCode.data(), asmCode.size(), NULL, 0, NULL, 0, NULL, 0, NULL, NULL, NULL, NULL, NULL);
	if (parseData->errors != NULL)
	{
		return string();
	}

	FILE * bcFile = NULL;
	char bcFileName[260];
	strcpy_s(bcFileName, 260, utf8AsmFileName.c_str());
	PathRemoveExtensionA(bcFileName);
	sprintf_s(bcFileName, "%s.o", bcFileName);

	if (!fopen_s(&bcFile, bcFileName, "wb"))
	{
		fwrite(parseData->bytecode, 1, parseData->bytecode_len, bcFile);
		fclose(bcFile);
	}

	ID3DBlob * blob = NULL;
	HRESULT hr = D3DDisassemble(parseData->bytecode, parseData->bytecode_len, 0, NULL, &blob);

	if (blob == NULL)
	{
		return string();
	}


	FILE * recreateAsmFile = NULL;
	char recreateAsmFileName[260];
	strcpy_s(recreateAsmFileName, 260, utf8AsmFileName.c_str());
	PathRemoveExtensionA(recreateAsmFileName);
	sprintf_s(recreateAsmFileName, "%s_re.asm", recreateAsmFileName);

	if (!fopen_s(&recreateAsmFile, recreateAsmFileName, "wb"))
	{
		fwrite(blob->GetBufferPointer(), 1, blob->GetBufferSize() - 1, recreateAsmFile);
		fclose(recreateAsmFile);
	}



	vector<byte> asmBuffer = readFile(utf8AsmFileName);


	bool patched = false;
	string shaderModel;
	bool errorOccurred = false;

	// Set all to zero, so we only init the ones we are using here.
	ParseParameters p = {};
	p.bytecode = parseData->bytecode;
	p.decompiled = (const char *)asmBuffer.data();
	p.decompiledSize = asmBuffer.size();

	const string decompiledCode = DecompileBinaryHLSL(p, patched, shaderModel, errorOccurred);

	if (!decompiledCode.size())
	{
		cout << "    error while decompiling.\n" << endl;
	}

	FILE * hlslFile = NULL;
	char hlslFileName[260];
	strcpy_s(hlslFileName, 260, utf8AsmFileName.c_str());
	PathRemoveExtensionA(hlslFileName);
	sprintf_s(hlslFileName, "%s.hlsl", hlslFileName);
	if (!fopen_s(&hlslFile, hlslFileName, "wt"))
	{
		fwrite(decompiledCode.c_str(), 1, decompiledCode.size(), hlslFile);
		fclose(hlslFile);
	}


	return decompiledCode;
}


//----------------------------------------------------------------------------- 
// Console App Entry-Point. 
//----------------------------------------------------------------------------- 
int _tmain(int argc, _TCHAR* argv[])
{
	static const int kExitOk = 0;
	static const int kExitError = 1;

	try
	{
		// Check command line arguments 
		if (argc != 2)
		{
			PrintHelp();
		}
		else
		{
			// Print file hash 
			std::string hlsl = Decompile(argv[1]);
			std::cout << hlsl << std::endl;
		}
	}
	catch (const std::exception & e)
	{
		std::cerr << "\n*** ERROR: " << e.what() << std::endl;
		return kExitError;
	}

	return kExitOk;
}

