// cmd_Decompiler.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <iostream>     // console output

#include <D3Dcompiler.h>
#include "DecompileHLSL.h"
#include "version.h"
#include "log.h"
#include "util.h"

using namespace std;

FILE *LogFile = stderr; // Log to stderr by default
bool gLogDebug = false;

static void PrintHelp(int argc, char *argv[])
{
	LogInfo("usage: %s [OPTION] FILE...\n\n", argv[0]);

	LogInfo("  -D, --decompile\n");
	LogInfo("\t\t\tDecompile binary shaders with 3DMigoto's decompiler\n");

	// We can do this via fxc easily enough for now, and would need to pass in the shader model:
	// LogInfo("  -C, --compile\n");
	// LogInfo("\t\t\tCompile binary shaders with Microsoft's compiler\n");

	LogInfo("  -d, --disassemble, --disassemble-flugan\n");
	LogInfo("\t\t\tDisassemble binary shaders with Flugan's disassembler\n");

	LogInfo("  --disassemble-ms\n");
	LogInfo("\t\t\tDisassemble binary shaders with Microsoft's disassembler\n");

	// Flugan's assembler cannot be used standalone as it requires an original binary
	// We could provide one in some cases, but we really need to remove that requirement.
	// LogInfo("  -a, --assemble\n");
	// LogInfo("\t\t\tAssemble shaders with Flugan's assembler\n");

	// TODO (at the moment we always force):
	// LogInfo("  -f, --force\n");
	// LogInfo("\t\t\tOverwrite existing files\n");

	// Call this validate not verify, because it's impossible to machine
	// verify the decompiler:
	LogInfo("  -V, --validate\n");
	LogInfo("\t\t\tRun a validation pass after decompilation / disassembly\n");

	LogInfo("  -S, --stop-on-failure\n");
	LogInfo("\t\t\tStop processing files if an error occurs\n");

	exit(EXIT_FAILURE);
}

static struct {
	std::vector<std::string> files;
	bool decompile;
	bool compile;
	bool disassemble_ms;
	bool disassemble_flugan;
	bool assemble;
	bool force;
	bool validate;
	bool stop;
} args;

void parse_args(int argc, char *argv[])
{
	bool terminated = false;
	char *arg;
	int i;

	// I'd prefer to use an existing library for this (e.g. that allows
	// these to be specified declaratively and handles --help, --usage,
	// abbreviations and all the other goodness that these libraries
	// provide), but getopt is absent on Windows and I don't particularly
	// want to drag in all of boost for this one thing, so for now just use
	// fairly simple posix style option parsing that can later be expanded
	// to use a full library

	for (i = 1; i < argc; i++) {
		arg = argv[i];
		if (!terminated && !strncmp(arg, "-", 1)) {
			if (!strcmp(arg, "--help") || !strcmp(arg, "--usage")) {
				PrintHelp(argc, argv); // Does not return
			}
			if (!strcmp(arg, "--")) {
				terminated = true;
				continue;
			}
			if (!strcmp(arg, "-D") || !strcmp(arg, "--decompile")) {
				args.decompile = true;
				continue;
			}
			// if (!strcmp(arg, "-C") || !strcmp(arg, "--compile")) {
			// 	args.compile = true;
			// 	continue;
			// }
			if (!strcmp(arg, "-d") || !strcmp(arg, "--disassemble") || !strcmp(arg, "--disassemble-flugan")) {
				args.disassemble_flugan = true;
				continue;
			}
			if (!strcmp(arg, "--disassemble-ms")) {
				args.disassemble_ms = true;
				continue;
			}
			// if (!strcmp(arg, "-a") || !strcmp(arg, "--assemble")) {
			// 	args.assemble = true;
			// 	continue;
			// }
			// if (!strcmp(arg, "-f") || !strcmp(arg, "--force")) {
			// 	args.force = true;
			// 	continue;
			// }
			if (!strcmp(arg, "-V") || !strcmp(arg, "--validate")) {
				args.validate = true;
				continue;
			}
			if (!strcmp(arg, "-S") || !strcmp(arg, "--stop-on-failure")) {
				args.stop = true;
				continue;
			}
			LogInfo("Unrecognised argument: %s\n", arg);
			PrintHelp(argc, argv); // Does not return
		}
		args.files.push_back(arg);
	}

	if (args.decompile + args.compile + args.disassemble_ms + args.disassemble_flugan + args.assemble < 1) {
		LogInfo("No action specified\n");
		PrintHelp(argc, argv); // Does not return
	}

}

// Old version directly using D3DDisassemble, suffers from precision issues due
// to bug in MS's disassembler that always prints floats with %f, which does
// not have sufficient precision to reproduce a 32bit floating point value
// exactly. Might still be useful for comparison:
static HRESULT DisassembleMS(const void *pShaderBytecode, size_t BytecodeLength, string *asmText)
{
	ID3DBlob *disassembly = nullptr;
	UINT flags = D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS;
	string comments = "//   using 3Dmigoto command line v" + string(VER_FILE_VERSION_STR) + " on " + LogTime() + "//\n";

	HRESULT hr = D3DDisassemble(pShaderBytecode, BytecodeLength, flags, comments.c_str(), &disassembly);
	if (FAILED(hr)) {
		LogInfo("  disassembly failed. Error: %x\n", hr);
		return hr;
	}

	// Successfully disassembled into a Blob.  Let's turn it into a C++ std::string
	// so that we don't have a null byte as a terminator.  If written to a file,
	// the null bytes otherwise cause Git diffs to fail.
	*asmText = string(static_cast<char*>(disassembly->GetBufferPointer()));

	disassembly->Release();
	return S_OK;
}

static HRESULT DisassembleFlugan(const void *pShaderBytecode, size_t BytecodeLength, string *asmText)
{
	// FIXME: This is a bit of a waste - we convert from a vector<char> to
	// a void* + size_t to a vector<byte>

	*asmText = BinaryToAsmText(pShaderBytecode, BytecodeLength);
	if (*asmText == "")
		return E_FAIL;

	return S_OK;
}

static int validate_assembly(string *assembly, vector<char> *orig_bytecode)
{
	vector<byte> new_bytecode(orig_bytecode->size());
	vector<byte> assembly_vec(assembly->begin(), assembly->end());

	// Assemble the disassembly and compare it to the original bytecode
	new_bytecode = assembler(assembly_vec, *reinterpret_cast<vector<byte>*>(orig_bytecode));

	if (memcmp(orig_bytecode->data(), new_bytecode.data(), orig_bytecode->size())) {
		LogInfo("\n*** Assembly verification pass failed!\n");
		return EXIT_FAILURE;
	}

	LogInfo("    Assembly verification pass succeeded\n");
	return EXIT_SUCCESS;
}

static HRESULT Decompile(const void *pShaderBytecode, size_t BytecodeLength, string *hlslText, string *shaderModel)
{
	// Set all to zero, so we only init the ones we are using here:
	ParseParameters p = {0};
	bool patched = false;
	bool errorOccurred = false;
	string disassembly;
	HRESULT hret;

	hret = DisassembleMS(pShaderBytecode, BytecodeLength, &disassembly);
	if (FAILED(hret))
		return E_FAIL;

	LogInfo("    creating HLSL representation\n");

	p.bytecode = pShaderBytecode;
	p.decompiled = disassembly.c_str(); // XXX: Why do we call this "decompiled" when it's actually disassembled?
	p.decompiledSize = disassembly.size();
	// FIXME: We would be better off defining a pre-processor macro for these:
	p.IniParamsReg = 120;
	p.StereoParamsReg = 125;

	*hlslText = DecompileBinaryHLSL(p, patched, *shaderModel, errorOccurred);
	if (!hlslText->size() || errorOccurred) {
		LogInfo("    error while decompiling\n");
		return E_FAIL;
	}

	return S_OK;
}

static int validate_hlsl(string *hlsl, string *shaderModel)
{
	ID3DBlob *ppBytecode = NULL;
	ID3DBlob *pErrorMsgs = NULL;
	HRESULT hr;

	// Using optimisation level 0 for faster verification:
	hr = D3DCompile(hlsl->c_str(), hlsl->size(), "wrapper1349", 0, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main", shaderModel->c_str(), D3DCOMPILE_OPTIMIZATION_LEVEL0, 0, &ppBytecode, &pErrorMsgs);

	if (ppBytecode)
		ppBytecode->Release();

	if (pErrorMsgs && LogFile) { // Check LogFile so the fwrite doesn't crash
		LPVOID errMsg = pErrorMsgs->GetBufferPointer();
		SIZE_T errSize = pErrorMsgs->GetBufferSize();
		LogInfo("--------------------------------------------- BEGIN ---------------------------------------------\n");
		fwrite(errMsg, 1, errSize - 1, LogFile);
		LogInfo("---------------------------------------------- END ----------------------------------------------\n");
		pErrorMsgs->Release();
	}

	if (FAILED(hr)) {
		LogInfo("\n*** Decompiler validation pass failed!\n");
		return EXIT_FAILURE;
	}

	// Not a guarantee that the decompilation result is correct, but at
	// least it compiled...

	LogInfo("    Decompiler validation pass succeeded\n");
	return EXIT_SUCCESS;
}


static int ReadInput(vector<char> *srcData, string const *filename)
{
	DWORD srcDataSize;
	DWORD readSize;
	BOOL bret;
	HANDLE fp;

	// TODO: Handle reading from stdin for use in a pipeline

	fp = CreateFileA(filename->c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fp == INVALID_HANDLE_VALUE) {
		LogInfo("    Shader not found: %s\n", filename->c_str());
		return EXIT_FAILURE;
	}

	srcDataSize = GetFileSize(fp, 0);
	srcData->resize(srcDataSize);

	bret = ReadFile(fp, srcData->data(), srcDataSize, &readSize, 0);
	CloseHandle(fp);
	if (!bret || srcDataSize != readSize) {
		LogInfo("    Error reading input file\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int WriteOutput(string const *in_filename, char const *extension, string const *output)
{
	string out_filename;
	FILE *fp;

	// TODO: Use 3DMigoto style filenames when possible, but remember we
	// have no guarantee that the input filename is already in a form that
	// 3DMigoto can understand, and we should not assume as such. Maybe
	// make it an option.
	//
	// TODO: Handle writing to stdout for use in a pipeline

	// Also handles the case where the file has no extension (well, unless
	// some fool does foo.bar\baz):
	out_filename = in_filename->substr(0, in_filename->rfind(".")) + extension;
	LogInfo("  -> %s\n", out_filename.c_str());

	fopen_s(&fp, out_filename.c_str(), "wb");
	if (!fp)
		return EXIT_FAILURE;

	fwrite(output->c_str(), 1, output->size(), fp);
	fclose(fp);

	return EXIT_SUCCESS;
}

static int process(string const *filename)
{
	HRESULT hret;
	string output;
	vector<char> srcData;
	string model;

	if (ReadInput(&srcData, filename))
		return EXIT_FAILURE;

	if (args.disassemble_ms) {
		LogInfo("Disassembling (MS) %s...\n", filename->c_str());
		hret = DisassembleMS(srcData.data(), srcData.size(), &output);
		if (FAILED(hret))
			return EXIT_FAILURE;

		if (args.validate) {
			if (validate_assembly(&output, &srcData))
				return EXIT_FAILURE;
		}

		if (WriteOutput(filename, ".msasm", &output))
			return EXIT_FAILURE;
	}

	if (args.disassemble_flugan) {
		LogInfo("Disassembling (Flugan) %s...\n", filename->c_str());
		hret = DisassembleFlugan(srcData.data(), srcData.size(), &output);
		if (FAILED(hret))
			return EXIT_FAILURE;

		if (args.validate) {
			if (validate_assembly(&output, &srcData))
				return EXIT_FAILURE;
		}

		if (WriteOutput(filename, ".asm", &output))
			return EXIT_FAILURE;

	}

	if (args.decompile) {
		LogInfo("Decompiling %s...\n", filename->c_str());
		hret = Decompile(srcData.data(), srcData.size(), &output, &model);
		if (FAILED(hret))
			return EXIT_FAILURE;

		if (args.validate) {
			if (validate_hlsl(&output, &model))
				return EXIT_FAILURE;
		}

		if (WriteOutput(filename, ".hlsl", &output))
			return EXIT_FAILURE;

	}

	return EXIT_SUCCESS;
}


//-----------------------------------------------------------------------------
// Console App Entry-Point.
//-----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	int rc = EXIT_SUCCESS;

	parse_args(argc, argv);

	for (string const &filename : args.files) {
		try {
			rc = process(&filename) || rc;
		} catch (const exception & e) {
			LogInfo("\n*** UNHANDLED EXCEPTION: %s\n", e.what());
			rc = EXIT_FAILURE;
		}

		if (rc && args.stop)
			return rc;
	}

	if (rc)
		LogInfo("\n*** At least one error occurred during run ***\n");

	return rc;
}

