// Assembler.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "d3dcompiler.h"

#include <string>
#include <vector>

// VS2013 BUG WORKAROUND: Make sure this class has a unique type name!
class AssemblerParseError : public std::exception
{
public:
    std::string context, desc, msg;
    int         lineNo;

    AssemblerParseError(std::string context, std::string desc) :
        context(context),
        desc(desc),
        lineNo(0)
    {
        UpdateMsg();
    }

    void UpdateMsg()
    {
        msg = "Assembly parse error";
        if (lineNo > 0)
            msg += std::string(" on line ") + std::to_string(lineNo);
        msg += ", " + desc + ":\n\"" + context + "\"";
    }

    const char* what() const
    {
        return msg.c_str();
    }
};

struct shader_ins
{
    union
    {
        struct
        {
            // XXX Beware that bitfield packing is not defined in
            // the C/C++ standards and this is relying on compiler
            // specific packing. This approach is not recommended.

            unsigned opcode   : 11;
            unsigned _11_23   : 13;
            unsigned length   : 7;
            unsigned extended : 1;
        };
        DWORD op;
    };
};
struct token_operand
{
    union
    {
        struct
        {
            // XXX Beware that bitfield packing is not defined in
            // the C/C++ standards and this is relying on compiler
            // specific packing. This approach is not recommended.

            unsigned comps_enum  : 2; /* sm4_operands_comps */
            unsigned mode        : 2; /* sm4_operand_mode */
            unsigned sel         : 8;
            unsigned file        : 8; /* SM_FILE */
            unsigned num_indices : 2;
            unsigned index0_repr : 3; /* sm4_operand_index_repr */
            unsigned index1_repr : 3; /* sm4_operand_index_repr */
            unsigned index2_repr : 3; /* sm4_operand_index_repr */
            unsigned extended    : 1;
        };
        DWORD op;
    };
};

std::vector<std::string> string_to_lines(const char* start, size_t size);
void                     write_lut();

HRESULT disassembler(std::vector<byte>* buffer, std::vector<byte>* ret, const char* comment, int hexdump = 0, bool d3dcompiler_46_compat = false, bool disassemble_undecipherable_data = false, bool patch_cb_offsets = false);
HRESULT disassembler_dx9(std::vector<byte>* buffer, std::vector<byte>* ret, const char* comment);

std::vector<byte> assembler(std::vector<char>* asm_file, std::vector<byte> orig_bytecode, std::vector<AssemblerParseError>* parse_errors = nullptr);
std::vector<byte> assembler_dx9(std::vector<char>* asm_file);

HRESULT           assemble_flugan_with_signature_parsing(std::vector<char>* assembly, std::vector<byte>* result_bytecode, std::vector<AssemblerParseError>* parse_errors = nullptr);
std::vector<byte> assemble_flugan_with_optional_signature_parsing(std::vector<char>* assembly, bool assemble_signatures, std::vector<byte>* orig_bytecode, std::vector<AssemblerParseError>* parse_errors = nullptr);
