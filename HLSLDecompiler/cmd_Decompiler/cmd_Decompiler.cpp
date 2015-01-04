// cmd_Decompiler.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <iostream>     // console output 

#include "..\DecompileHLSL.h"
#include "..\..\log.h"

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

	bool patched = false;
	string shaderModel;
	bool errorOccurred = false;

	// Set all to zero, so we only init the ones we are using here.
	ParseParameters p = {};

	p.bytecode = pShaderByteCode->GetBufferPointer();
	p.decompiled = (const char *)disassembly->GetBufferPointer();
	p.decompiledSize = disassembly->GetBufferSize();
	const string decompiledCode = DecompileBinaryHLSL(p, patched, shaderModel, errorOccurred);

	if (!decompiledCode.size())
	{
		if (LogFile) fprintf(LogFile, "    error while decompiling.\n");
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

