// cmd_Decompiler.cpp : Defines the entry point for the console application.
//

#include "DecompileHLSL.h"
#include "log.h"
#include "stdafx.h"
#include "version.h"

#include <D3Dcompiler.h>
#define MIGOTO_DX 11 // Selects the DX11 disassembler in util.h - the DX9 dis/assembler is not very
                     // interesting since it is just Microsoft's - we can add it later, but low priority.
                     // The DX9 decompiler is more interesting, which is unrelated to this flag.
#include "shader.h"
#include "util.h"

using namespace std;

FILE *LogFile = stderr; // Log to stderr by default
bool gLogDebug = false;

static void PrintHelp(int argc, char *argv[])
{
    LOG_INFO("usage: %s [OPTION] FILE...\n\n", argv[0]);

    LOG_INFO("  -D, --decompile\n");
    LOG_INFO("\t\t\tDecompile binary shaders with 3DMigoto's decompiler\n");

    // We can do this via fxc easily enough for now, and would need to pass in the shader model:
    LOG_INFO("  -C, --compile\n");
    LOG_INFO("\t\t\tCompile binary shaders with Microsoft's compiler\n");
    LOG_INFO("\t\t\Use the fxc compiler in VS cmd window.\n");

    LOG_INFO("  -d, --disassemble, --disassemble-flugan\n");
    LOG_INFO("\t\t\tDisassemble binary shaders with Flugan's disassembler\n");

    LOG_INFO("  -x, --disassemble-hexdump\n");
    LOG_INFO("\t\t\tIntermix the disassembly with the raw hex codes (implies --disassemble-flugan)\n");

    LOG_INFO("  --disassemble-ms\n");
    LOG_INFO("\t\t\tDisassemble binary shaders with Microsoft's disassembler\n");

    // Only applicable to the vs2017 branch / d3dcompiler_47 version:
    LOG_INFO("  -6, --disassemble-46\n");
    LOG_INFO("\t\t\tApply backwards compatibility formatting patch to disassembler output\n");

    LOG_INFO("  -16, --patch-cb-offsets\n");
    LOG_INFO("\t\t\tReplace constant buffer byte offsets with indices when disassembling\n");

    LOG_INFO("  -a, --assemble\n");
    LOG_INFO("\t\t\tAssemble shaders with Flugan's assembler\n");

    LOG_INFO("  --copy-reflection FILE\n");
    LOG_INFO("\t\t\t\tCopy reflection & signature sections from FILE when assembling\n");

    // TODO (at the moment we always force):
    // LOG_INFO("  -f, --force\n");
    // LOG_INFO("\t\t\tOverwrite existing files\n");

    // Call this validate not verify, because it's impossible to machine
    // verify the decompiler:
    LOG_INFO("  -V, --validate\n");
    LOG_INFO("\t\t\tRun a validation pass after decompilation / disassembly\n");

    LOG_INFO("  --lenient\n");
    LOG_INFO("\t\t\tDon't fail shader validation for certain types of section mismatches\n");

    LOG_INFO("  -S, --stop-on-failure\n");
    LOG_INFO("\t\t\tStop processing files if an error occurs\n");

    LOG_INFO("  -v, --verbose\n");
    LOG_INFO("\t\t\tVerbose debugging output\n");

    exit(EXIT_FAILURE);
}

static void PrintVersion()
{
    LOG_INFO("3DMigoto cmd_Decompiler version %s\n", VER_FILE_VERSION_STR);

    exit(EXIT_SUCCESS);
}

static struct {
    std::vector<std::string> files;
    bool decompile;
    bool compile;
    bool disassemble_ms;
    bool disassemble_flugan;
    int disassemble_hexdump;
    bool disassemble_46;
    bool patch_cb_offsets;
    std::string reflection_reference;
    bool assemble;
    bool force;
    bool validate;
    bool lenient;
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
            if (!strcmp(arg, "--version")) {
                PrintVersion(); // Does not return
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
            //     args.compile = true;
            //     continue;
            // }
            if (!strcmp(arg, "-d") || !strcmp(arg, "--disassemble") || !strcmp(arg, "--disassemble-flugan")) {
                args.disassemble_flugan = true;
                continue;
            }
            if (!strcmp(arg, "-x") || !strcmp(arg, "--disassemble-hexdump")) {
                args.disassemble_hexdump = 1;
                continue;
            }
            if (!strcmp(arg, "-6") || !strcmp(arg, "--disassemble-46")) {
                args.disassemble_46 = true;
                continue;
            }
            if (!strcmp(arg, "-16") || !strcmp(arg, "--patch-cb-offsets")) {
                args.patch_cb_offsets = true;
                continue;
            }

            if (!strcmp(arg, "--copy-reflection")) {
                if (++i >= argc)
                    PrintHelp(argc, argv);
                args.reflection_reference = argv[i];
                continue;
            }
            if (!strcmp(arg, "--disassemble-ms")) {
                args.disassemble_ms = true;
                continue;
            }
            if (!strcmp(arg, "-a") || !strcmp(arg, "--assemble")) {
                args.assemble = true;
                continue;
            }
            // if (!strcmp(arg, "-f") || !strcmp(arg, "--force")) {
            //     args.force = true;
            //     continue;
            // }
            if (!strcmp(arg, "-V") || !strcmp(arg, "--validate")) {
                args.validate = true;
                continue;
            }
            if (!strcmp(arg, "--lenient")) {
                args.lenient = true;
                continue;
            }
            if (!strcmp(arg, "-S") || !strcmp(arg, "--stop-on-failure")) {
                args.stop = true;
                continue;
            }
            if (!strcmp(arg, "-v") || !strcmp(arg, "--verbose")) {
                gLogDebug = true;
                continue;
            }
            LOG_INFO("Unrecognised argument: %s\n", arg);
            PrintHelp(argc, argv); // Does not return
        }
        args.files.push_back(arg);
    }

    if (args.decompile + args.compile
            + args.disassemble_ms
            + args.disassemble_flugan
            + args.disassemble_hexdump
            + args.disassemble_46
            + args.assemble < 1) {
        LOG_INFO("No action specified\n");
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
    string comments = "//   using 3Dmigoto command line v" + string(VER_FILE_VERSION_STR) + " on " + log_time() + "//\n";

    HRESULT hr = D3DDisassemble(pShaderBytecode, BytecodeLength, flags, comments.c_str(), &disassembly);
    if (FAILED(hr)) {
        LOG_INFO("  disassembly failed. Error: %x\n", hr);
        return hr;
    }

    // Successfully disassembled into a Blob.  Let's turn it into a C++ std::string
    // so that we don't have a null byte as a terminator.  If written to a file,
    // the null bytes otherwise cause Git diffs to fail.
    *asmText = string(static_cast<char*>(disassembly->GetBufferPointer()));

    disassembly->Release();
    return S_OK;
}

// Because of complicated header includes, switching this routine to just directly disassemble instead
// of using binary_to_asm_text from Util.

static HRESULT DisassembleFlugan(const void *pShaderBytecode, size_t BytecodeLength, string *asmText,
        int hexdump, bool d3dcompiler_46_compat)
{
    std::string       comments;
    std::vector<byte> byte_code(BytecodeLength);
    std::vector<byte> disassembly;
    HRESULT           r;

    comments = "//   using 3Dmigoto v" + std::string(VER_FILE_VERSION_STR) + " on " + log_time() + "//\n";
    memcpy(byte_code.data(), pShaderBytecode, BytecodeLength);

    r = disassembler(&byte_code, &disassembly, comments.c_str(), hexdump, d3dcompiler_46_compat, true, args.patch_cb_offsets);

    if (FAILED(r))
    {
        LOG_INFO("  disassembly failed. Error: %x\n", r);
        return E_FAIL;
    }

    *asmText = std::string(disassembly.begin(), disassembly.end());

    return S_OK;
}

static int validate_section(char section[4], unsigned char *old_section, unsigned char *new_section, size_t size, struct dxbc_header *old_dxbc)
{
    unsigned char *p1 = old_section, *p2 = new_section;
    int rc = 0;
    size_t pos;
    size_t off = (size_t)(old_section - (unsigned char*)old_dxbc);

    for (pos = 0; pos < size; pos++, p1++, p2++) {
        if (*p1 == *p2)
            continue;

        if (!rc)
            LOG_INFO("\n*** Assembly verification pass failed: mismatch in section %.4s:\n", section);
        LOG_INFO("  %.4s+0x%04Ix (0x%08Ix): expected 0x%02x, found 0x%02x\n",
                section, pos, off+pos, *p1, *p2);
        rc = 1;
    }

    return rc;
}

static int validate_assembly(string *assembly, vector<char> *old_shader)
{
    vector<char> assembly_vec(assembly->begin(), assembly->end());
    vector<byte> new_shader;
    struct dxbc_header *old_dxbc_header = NULL, *new_dxbc_header = NULL;
    struct section_header *old_section_header = NULL, *new_section_header = NULL;
    uint32_t *old_section_offset_ptr = NULL, *new_section_offset_ptr = NULL;
    unsigned char *old_section = NULL, *new_section = NULL;
    uint32_t size;
    unsigned i, j;
    int rc = 0;
    HRESULT hret;

    // Assemble the disassembly and compare it to the original shader. We
    // use the version that reconstructs the signature sections from the
    // assembly text so that we can check the signature parsing separately
    // from the assembler. FIXME: We really need to clean up how the
    // buffers are passed between these functions
    try {
        hret = AssembleFluganWithSignatureParsing(&assembly_vec, &new_shader);
        if (FAILED(hret)) {
            LOG_INFO("\n*** Assembly verification pass failed: Reassembly failed 0x%x\n", hret);
            return 1;
        }
    } catch (AssemblerParseError &e) {
        string disassembly;

        LOG_INFO("\n%s\n\n", e.what());

        // Assembler threw a parse error. Switch to disassembly with
        // hexdump mode 2 enabled to identify bad instructions:
        hret = DisassembleFlugan(old_shader->data(), old_shader->size(), &disassembly, 2, false);
        if (SUCCEEDED(hret))
            LOG_INFO("%s\n", disassembly.c_str());
        return 1;
    }

    // Get some useful pointers into the buffers:
    old_dxbc_header = (struct dxbc_header*)old_shader->data();
    new_dxbc_header = (struct dxbc_header*)new_shader.data();

    old_section_offset_ptr = (uint32_t*)((char*)old_dxbc_header + sizeof(struct dxbc_header));
    for (i = 0; i < old_dxbc_header->num_sections; i++, old_section_offset_ptr++) {
        old_section_header = (struct section_header*)((char*)old_dxbc_header + *old_section_offset_ptr);

        // Find the matching section in the new shader:
        new_section_offset_ptr = (uint32_t*)((char*)new_dxbc_header + sizeof(struct dxbc_header));
        for (j = 0; j < new_dxbc_header->num_sections; j++, new_section_offset_ptr++) {
            new_section_header = (struct section_header*)((char*)new_dxbc_header + *new_section_offset_ptr);

            if (memcmp(old_section_header->signature, new_section_header->signature, 4)) {
                // If it's a mismatch between SHDR and SHEX
                // (SHader EXtension) we'll flag a failure and
                // warn, but still compare since the sections
                // are identical
                if ((!strncmp(old_section_header->signature, "SHDR", 4) &&
                     !strncmp(new_section_header->signature, "SHEX", 4)) ||
                    (!strncmp(old_section_header->signature, "SHEX", 4) &&
                     !strncmp(new_section_header->signature, "SHDR", 4))) {
                    if (args.lenient) {
                        LOG_INFO("Notice: SHDR / SHEX mismatch\n");
                    } else {
                        LOG_INFO("\n*** Assembly verification pass failed: SHDR / SHEX mismatch ***\n");
                        rc = 1;
                    }
                } else
                    continue;
            }

            LOG_DEBUG_NO_NL(" Checking section %.4s...", old_section_header->signature);

            size = min(old_section_header->size, new_section_header->size);
            old_section = (unsigned char*)old_section_header + sizeof(struct section_header);
            new_section = (unsigned char*)new_section_header + sizeof(struct section_header);

            if (validate_section(old_section_header->signature, old_section, new_section, size, old_dxbc_header)) {
                rc = 1;

                // If the failure was in a bytecode section,
                // output the disassembly with hexdump enabled:
                if (!strncmp(old_section_header->signature, "SHDR", 4) ||
                    !strncmp(old_section_header->signature, "SHEX", 4)) {
                    string disassembly;
                    hret = DisassembleFlugan(old_shader->data(), old_shader->size(), &disassembly, 2, false);
                    if (SUCCEEDED(hret))
                        LOG_INFO("\n%s\n", disassembly.c_str());
                }
            } else
                LOG_DEBUG(" OK\n");

            if (old_section_header->size != new_section_header->size) {
                LOG_INFO("\n*** Assembly verification pass failed: size mismatch in section %.4s, expected %i, found %i\n",
                        old_section_header->signature, old_section_header->size, new_section_header->size);
                rc = 1;
            }

            break;
        }
        if (j == new_dxbc_header->num_sections) {
            // Whitelist sections that are okay to be missed:
            if (!args.lenient &&
                strncmp(old_section_header->signature, "STAT", 4) && // Compiler Statistics
                strncmp(old_section_header->signature, "RDEF", 4) && // Resource Definitions
                strncmp(old_section_header->signature, "SDBG", 4) && // Debug Info
                strncmp(old_section_header->signature, "Aon9", 4)) { // Level 9 shader bytecode
                //strncmp(old_section_header->signature, "SFI0", 4)) { // Subtarget Feature Info (not yet sure if this is critical or not)
                LOG_INFO("*** Assembly verification pass failed: Reassembled shader missing %.4s section (not whitelisted)\n", old_section_header->signature);
                rc = 1;
            } else
                LOG_INFO("Reassembled shader missing %.4s section\n", old_section_header->signature);
        }
    }

    // List any sections in the new shader that weren't in the old (e.g. section version mismatches):
    new_section_offset_ptr = (uint32_t*)((char*)new_dxbc_header + sizeof(struct dxbc_header));
    for (i = 0; i < new_dxbc_header->num_sections; i++, new_section_offset_ptr++) {
        new_section_header = (struct section_header*)((char*)new_dxbc_header + *new_section_offset_ptr);

        old_section_offset_ptr = (uint32_t*)((char*)old_dxbc_header + sizeof(struct dxbc_header));
        for (j = 0; j < old_dxbc_header->num_sections; j++, old_section_offset_ptr++) {
            old_section_header = (struct section_header*)((char*)old_dxbc_header + *old_section_offset_ptr);
            if (!memcmp(old_section_header->signature, new_section_header->signature, 4))
                break;
        }
        if (j == old_dxbc_header->num_sections)
            LOG_INFO("Reassembled shader contains %.4s section not in original\n", new_section_header->signature);
    }

    if (!rc)
        LOG_INFO("    Assembly verification pass succeeded\n");
    return rc;
}


static HRESULT Decompile(const void *pShaderBytecode, size_t BytecodeLength, string *hlslText, string *shaderModel)
{
    // Set all to zero, so we only init the ones we are using here:
    ParseParameters p = {};
    DecompilerSettings d;
    bool patched = false;
    bool errorOccurred = false;
    string disassembly;
    HRESULT hret;

    hret = DisassembleMS(pShaderBytecode, BytecodeLength, &disassembly);
    if (FAILED(hret))
        return E_FAIL;

    LOG_INFO("    creating HLSL representation\n");

    p.bytecode = pShaderBytecode;
    p.decompiled = disassembly.c_str(); // XXX: Why do we call this "decompiled" when it's actually disassembled?
    p.decompiledSize = disassembly.size();
    p.G = &d;

    // Disable IniParams and StereoParams registers. This avoids inserting
    // these in a shader that already has them, such as some of our test
    // cases. Also, while cmd_Decompiler is part of 3DMigoto, it is NOT
    // 3DMigoto so it doesn't really make sense that it should add 3DMigoto
    // registers, and if someone wants these registers there is nothing
    // stopping them from adding them by hand. May break scripts that use
    // cmd_Decompiler and expect these to be here, but those scripts can be
    // updated to add them or they can keep using an old version.
    d.IniParamsReg = -1;
    d.StereoParamsReg = -1;

    *hlslText = DecompileBinaryHLSL(p, patched, *shaderModel, errorOccurred);
    if (!hlslText->size() || errorOccurred) {
        LOG_INFO("    error while decompiling\n");
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
        LOG_INFO("--------------------------------------------- BEGIN ---------------------------------------------\n");
        fwrite(errMsg, 1, errSize - 1, LogFile);
        LOG_INFO("---------------------------------------------- END ----------------------------------------------\n");
        pErrorMsgs->Release();
    }

    if (FAILED(hr)) {
        LOG_INFO("\n*** Decompiler validation pass failed!\n");
        return EXIT_FAILURE;
    }

    // Not a guarantee that the decompilation result is correct, but at
    // least it compiled...

    LOG_INFO("    Decompiler validation pass succeeded\n");
    return EXIT_SUCCESS;
}

template<typename T>
static int ReadInput(vector<T> *srcData, string const *filename)
{
    DWORD srcDataSize;
    DWORD readSize;
    BOOL bret;
    HANDLE fp;

    // TODO: Handle reading from stdin for use in a pipeline

    fp = CreateFileA(filename->c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fp == INVALID_HANDLE_VALUE) {
        LOG_INFO("    Shader not found: %s\n", filename->c_str());
        return EXIT_FAILURE;
    }

    srcDataSize = GetFileSize(fp, 0);
    srcData->resize(srcDataSize);

    bret = ReadFile(fp, srcData->data(), srcDataSize, &readSize, 0);
    CloseHandle(fp);
    if (!bret || srcDataSize != readSize) {
        LOG_INFO("    Error reading input file\n");
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
    LOG_INFO("  -> %s\n", out_filename.c_str());

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
        LOG_INFO("Disassembling (MS) %s...\n", filename->c_str());
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

    if (args.disassemble_flugan || args.disassemble_hexdump || args.disassemble_46) {
        LOG_INFO("Disassembling (Flugan) %s...\n", filename->c_str());
        hret = DisassembleFlugan(srcData.data(), srcData.size(), &output, args.disassemble_hexdump, args.disassemble_46);
        if (FAILED(hret))
            return EXIT_FAILURE;

        if (args.validate) {
            if (validate_assembly(&output, &srcData))
                return EXIT_FAILURE;
            // TODO: Validate signature parsing instead of binary identical files
        }

        if (WriteOutput(filename, ".asm", &output))
            return EXIT_FAILURE;

    }

    if (args.assemble) {
        LOG_INFO("Assembling %s...\n", filename->c_str());
        vector<byte> new_bytecode;
        if (args.reflection_reference.empty()) {
            hret = AssembleFluganWithSignatureParsing(&srcData, &new_bytecode);
            if (FAILED(hret))
                return EXIT_FAILURE;
        } else {
            vector<byte> refData;
            if (ReadInput(&refData, &args.reflection_reference))
                return EXIT_FAILURE;
            new_bytecode = AssembleFluganWithOptionalSignatureParsing(&srcData, false, &refData);
        }

        // TODO:
        // if (args.validate)
        // disassemble again and perform fuzzy compare

        output = string(new_bytecode.begin(), new_bytecode.end());
        if (WriteOutput(filename, ".shdr", &output))
            return EXIT_FAILURE;

    }

    if (args.decompile) {
        LOG_INFO("Decompiling %s...\n", filename->c_str());
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
            LOG_INFO("\n*** UNHANDLED EXCEPTION: %s\n", e.what());
            rc = EXIT_FAILURE;
        }

        if (rc && args.stop)
            return rc;
    }

    if (rc)
        LOG_INFO("\n*** At least one error occurred during run ***\n");

    return rc;
}

