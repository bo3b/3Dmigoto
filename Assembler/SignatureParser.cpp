#include "Assembler.h"
#include "log.h"
#include "shader.h"

#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <vector>
#include <Windows.h>

using std::string;
using std::vector;

// -----------------------------------------------------------------------------

#define SFI_RAW_STRUCT_BUF (1LL << 1)
#define SFI_MIN_PRECISION (1LL << 4)

// Force the use of SHader EXtended bytecode when certain features are in use,
// such as partial or double precision. This is likely incomplete.
#define SFI_FORCE_SHEX (SFI_RAW_STRUCT_BUF | SFI_MIN_PRECISION)

// VS2013 BUG WORKAROUND: Make sure this class has a unique type name!
class AsmSignatureParseError : public std::exception
{
} parse_error;

static string next_line(
    string* shader,
    size_t* pos)
{
    size_t start_pos = *pos;
    size_t end_pos;

    // Skip preceeding whitespace:
    start_pos = shader->find_first_not_of(" \t\r", start_pos);

    // Blank line at end of file:
    if (start_pos == shader->npos)
    {
        *pos = shader->npos;
        return "";
    }

    // Find newline, update parent pointer:
    *pos = shader->find('\n', start_pos);

    // Skip trailing whitespace (will pad it later during parsing, but
    // can't rely on whitespace here so still strip it):
    end_pos = shader->find_last_not_of(" \t\n\r", *pos) + 1;

    if (*pos != shader->npos)
        (*pos)++;  // Brackets are important

    return shader->substr(start_pos, end_pos - start_pos);
}

struct format_type
{
    uint32_t format;
    uint32_t min_precision;
    char*    name;
};

static struct format_type format_names[] = {
    { 0, 0, "unknown" },
    { 1, 0, "uint" },
    { 1, 5, "min16u" },  // min16uint
    { 2, 0, "int" },
    { 2, 4, "min16i" },  // min16int *AND* min12int as of d3dcompiler47.dll
    { 3, 0, "float" },
    { 3, 1, "min16f" },   // min16float
    { 3, 2, "min2_8f" },  // min10float
};

static void parse_format(
    char*     format,
    uint32_t* format_number,
    uint32_t* min_precision)
{
    uint32_t i;

    for (i = 0; i < ARRAYSIZE(format_names); i++)
    {
        if (!strcmp(format, format_names[i].name))
        {
            *format_number = format_names[i].format;
            *min_precision = format_names[i].min_precision;
            return;
        }
    }

    throw parse_error;
}

struct svt
{
    uint32_t val;
    char*    short_name;
};

static struct svt system_value_abbreviations[] = {
    {
        0,
        "NONE",
    },  // TEXCOORDs, input position to VS
    {
        0,
        "TARGET",
    },  // SV_Target

    {
        1,
        "POS",
    },  // SV_Position
    {
        2,
        "CLIPDST",
    },  // SV_ClipDistance
    {
        3,
        "CULLDST",
    },  // SV_CullDistance
    {
        4,
        "RTINDEX",
    },  // SV_RenderTargetArrayIndex
    {
        5,
        "VPINDEX",
    },  // SV_ViewportArrayIndex
    {
        6,
        "VERTID",
    },  // SV_VertexID
    {
        7,
        "PRIMID",
    },  // SV_PrimitiveID, register = "   primID"
    {
        8,
        "INSTID",
    },  // SV_InstanceID
    {
        9,
        "FFACE",
    },  // SV_IsFrontFace
    {
        10,
        "SAMPLE",
    },  // SV_SampleIndex

    // Tesselation domain/hull shaders. XXX: These numbers don't match up
    // with BinaryDecompiler, but I got them myself, so I am 100% certain
    // they are right, but perhaps I am missing something (Update - now
    // pretty sure the values in BinaryDecompiler are those used in the
    // bytecode, whereas these values are used in the signatures):
    {
        11,
        "QUADEDGE",
    },  // SV_TessFactor with [domain("quad")]
    {
        12,
        "QUADINT",
    },  // SV_InsideTessFactor with [domain("quad")]
    {
        13,
        "TRIEDGE",
    },  // SV_TessFactor with [domain("tri")]
    {
        14,
        "TRIINT",
    },  // SV_InsideTessFactor with [domain("tri")]
    {
        15,
        "LINEDET",
    },
    {
        16,
        "LINEDEN",
    },

    // System values using SPRs (special purpose registers). These only
    // ever seem to show up in the output signature - any used as inputs is
    // not in the input signature, and uses a 'v' prefix instead of 'o'.
    // Mask = "N/A" (0x1)
    // Register = "oSomething" (0xffffffff)
    // Used = "YES" (0xe) / "NO" (0x0)
    { 0, "DEPTH" },  // SV_Depth, oDepth
    {
        0,
        "COVERAGE",
    },                 // SV_Coverage, oMask
    { 0, "DEPTHGE" },  // SV_DepthGreaterEqual, oDepthGE
    { 0, "DEPTHLE" },  // SV_DepthLessEqual, oDepthLE

    // Only available in DX12 / shader model 5.1 / d3dcompiler47.dll:
    { 0, "STENCILREF" },  // SV_StencilRef, oStencilRef
    { 0, "INNERCOV" },    // SV_InnerCoverage, not sure what register is listed as - "special"?

    // Some other semantics which don't appear here (e.g. SV_GSInstanceID,
    // any of the compute shader thread IDs) are not present in these
    // sections.
};

static uint32_t parse_system_value(
    char* sv)
{
    uint32_t i;

    for (i = 0; i < ARRAYSIZE(system_value_abbreviations); i++)
    {
        if (!strcmp(sv, system_value_abbreviations[i].short_name))
            return system_value_abbreviations[i].val;
    }

    throw parse_error;
}

static uint8_t parse_mask(
    char mask[8],
    bool invert)
{
    uint8_t xor_val = (invert ? 0xf : 0);
    uint8_t ret     = 0;
    int     i;

    // This allows for more flexible processing rather than just limiting
    // it to what the disassembler generates

    for (i = 0; i < 8 && mask[i]; i++)
    {
        switch (mask[i])
        {
            case 'x':
                ret |= 0x1;
                break;
            case 'y':
                ret |= 0x2;
                break;
            case 'z':
                ret |= 0x4;
                break;
            case 'w':
                ret |= 0x8;
                break;
            case ' ':
                break;
            // Special matches for semantics using special purpose registers:
            case 'Y':  // YES
                return 0x1 ^ xor_val;
            case 'N':  // NO or N/A - wait for next character
                break;
            case 'O':  // NO
                return 0x0 ^ xor_val;
            case '/':  // N/A
                return 0x1 ^ xor_val;
            default:
                throw parse_error;
        }
    }
    return ret ^ xor_val;
}

static uint32_t pad(
    uint32_t size,
    uint32_t multiple)
{
    return (multiple - size % multiple) % multiple;
}

static void* serialise_signature_section(
    char*                                  section24,
    char*                                  section28,
    char*                                  section32,
    int                                    entry_size,
    vector<struct sgn_entry_unserialised>* entries,
    uint32_t                               name_len)
{
    void*                  section;
    uint32_t               section_size, padding, alloc_size, name_off;
    char*                  name_ptr       = nullptr;
    void*                  padding_ptr    = nullptr;
    struct section_header* section_header = nullptr;
    struct sgn_header*     sgn_header     = nullptr;
    sgn_entry_serialiased* entryn         = nullptr;
    sg5_entry_serialiased* entry5         = nullptr;
    sg1_entry_serialiased* entry1         = nullptr;

    // Geometry shader 5 never uses OSGN, bump to OSG5:
    if (entry_size == 24 && section24 == nullptr)
        entry_size = 28;

    // Only OSG5 exists in version 5, so bump ISG & PSG to version 5.1:
    if (entry_size == 28 && section28 == nullptr)
        entry_size = 32;

    // Calculate various offsets and sizes:
    name_off     = static_cast<uint32_t>(sizeof(struct sgn_header) + (entry_size * entries->size()));
    section_size = name_off + name_len;
    padding      = pad(section_size, 4);
    alloc_size   = section_size + sizeof(struct section_header) + padding;

    LOG_DEBUG("name_off: %u, name_len: %u, section_size: %u, padding: %u, alloc_size: %u\n", name_off, name_len, section_size, padding, alloc_size);

    // Allocate entire section, including room for section header and padding:
    section = malloc(alloc_size);
    if (!section)
    {
        LOG_INFO("Out of memory\n");
        return nullptr;
    }

    // Pointers to useful data structures and offsets in the buffer:
    section_header = static_cast<::section_header*>(section);
    sgn_header     = reinterpret_cast<struct sgn_header*>(reinterpret_cast<char*>(section_header) + sizeof(struct section_header));
    padding_ptr    = static_cast<void*>(reinterpret_cast<char*>(sgn_header) + section_size);
    // Only one of these will be used as the base address depending on the
    // structure version, but pointers to the older versions will also be
    // updated during the iteration:
    entryn = reinterpret_cast<struct sgn_entry_serialiased*>(reinterpret_cast<char*>(sgn_header) + sizeof(struct sgn_header));
    entry5 = reinterpret_cast<struct sg5_entry_serialiased*>(entryn);
    entry1 = reinterpret_cast<struct sg1_entry_serialiased*>(entryn);

    LOG_DEBUG("section: 0x%p, section_header: 0x%p, sgn_header: 0x%p, padding_ptr: 0x%p, entry: 0x%p\n", section, reinterpret_cast<char*>(section_header), sgn_header, padding_ptr, entryn);

    switch (entry_size)
    {
        case 24:
            memcpy(&section_header->signature, section24, 4);
            break;
        case 28:
            memcpy(&section_header->signature, section28, 4);
            break;
        case 32:
            memcpy(&section_header->signature, section32, 4);
            break;
        default:
            throw parse_error;
    }
    section_header->size = section_size + padding;

    sgn_header->num_entries = static_cast<uint32_t>(entries->size());
    sgn_header->unknown     = sizeof(struct sgn_header);  // Not confirmed, but seems likely. Always 8

    // Fill out entries:
    for (const struct sgn_entry_unserialised& unserialised : *entries)
    {
        switch (entry_size)
        {
            case 32:
                entry1->min_precision = unserialised.min_precision;
                entry5                = &entry1->sg5;
                entry1++;
            // Fall through
            case 28:
                entry5->stream = unserialised.stream;
                entryn         = &entry5->sgn;
                entry5++;
            // Fall through
            case 24:
                entryn->name_offset = name_off + unserialised.name_offset;
                name_ptr            = reinterpret_cast<char*>(sgn_header) + entryn->name_offset;
                memcpy(name_ptr, unserialised.name.c_str(), unserialised.name.size() + 1);
                memcpy(&entryn->common, &unserialised.common, sizeof(struct sgn_entry_common));
                entryn++;
        }
    }

    memset(padding_ptr, 0xab, padding);

    return section;
}

static void* parse_signature_section(
    char*    section24,
    char*    section28,
    char*    section32,
    string*  shader,
    size_t*  pos,
    bool     invert_used,
    uint64_t sfi)
{
    string                                line;
    size_t                                old_pos = *pos;
    int                                   num_read;
    uint32_t                              name_off   = 0;
    int                                   entry_size = 24;    // We use the size, because in MS's usual wisdom version 1 is higher than version 5 :facepalm:
    char                                  semantic_name[64];  // Semantic names are limited to 63 characters in fxc
    char                                  semantic_name2[64];
    char                                  system_value[16];  // More than long enough for even "STENCILREF"
    char                                  reg[16];           // More than long enough for even "oStencilRef"
    char                                  mask[8], used[8];  // We read 7 characters - check the reason below
    char                                  format[16];        // Long enough for even "unknown"
    vector<struct sgn_entry_unserialised> entries;
    struct sgn_entry_unserialised         entry;

    // If minimum precision formats are in use we bump the section versions:
    if (sfi & SFI_MIN_PRECISION)
        entry_size = max(entry_size, 32);

    while (*pos != shader->npos)
    {
        line = next_line(shader, pos);

        LOG_DEBUG("%s\n", line.c_str());

        if (line == "//" || line == "// Name                 Index   Mask Register SysValue  Format   Used" || line == "// -------------------- ----- ------ -------- -------- ------- ------")
        {
            continue;
        }

        if (line == "// no Input" || line == "// no Output" || line == "// no Patch Constant")
        {
            // Empty section, but we still need to manufacture it
            break;
        }

        // Mask and Used can be empty or have spaces in them, so using
        // %s would not match them correctly. Instead, match exactly 7
        // characters, which will include some preceeding whitespace
        // that parse_mask will skip over. But, since we may have
        // stripped trailing whitespace, explicitly pad the string to
        // make sure Usage has 7 characters to match, and make sure
        // they are initialised to ' ':
        memset(mask, ' ', 8);
        memset(used, ' ', 8);

        num_read = sscanf_s((line + "       ").c_str(), "// %s %d%7c %s %s %s%7c", semantic_name, static_cast<unsigned>(ARRAYSIZE(semantic_name)), &entry.common.semantic_index, mask, static_cast<unsigned>(ARRAYSIZE(mask)), &reg, static_cast<unsigned>(ARRAYSIZE(reg)), system_value, static_cast<unsigned>(ARRAYSIZE(system_value)), format, static_cast<unsigned>(ARRAYSIZE(format)), used, static_cast<unsigned>(ARRAYSIZE(used)));

        if (num_read != 7)
        {
            // I really would love to throw parseError here to
            // catch typos, but since this is in a comment I can't
            // be certain that this is part of the signature
            // declaration, so I have to assume this is the end of
            // the section :(
            break;
        }

        // Try parsing the semantic name with streams, and bump the
        // section version if sucessful:
        num_read = sscanf_s(semantic_name, "m%u:%s", &entry.stream, semantic_name2, static_cast<unsigned>(ARRAYSIZE(semantic_name2)));
        if (num_read == 2)
        {
            entry_size = max(entry_size, 28);
            entry.name = semantic_name2;
        }
        else
        {
            entry.stream = 0;
            entry.name   = semantic_name;
        }

        // Parse the format. If it is one of the minimum precision
        // formats, bump the section version (this is probably
        // redundant now that we bump the version based on SFI):
        parse_format(format, &entry.common.format, &entry.min_precision);
        if (entry.min_precision)
            entry_size = max(entry_size, 32);

        // Try parsing register as a decimal number. If it is not, it
        // is a special purpose register, in which case we store -1:
        if (num_read = sscanf_s(reg, "%d", &entry.common.reg) == 0)
            entry.common.reg = 0xffffffff;

        entry.common.system_value = parse_system_value(system_value);
        entry.common.mask         = parse_mask(mask, false);
        entry.common.used         = parse_mask(used, invert_used);
        entry.common.zero         = 0;

        // Check if a previous entry used the same semantic name
        for (const struct sgn_entry_unserialised& prev_entry : entries)
        {
            if (prev_entry.name == entry.name)
            {
                entry.name_offset = prev_entry.name_offset;
                // Why do so few languages have a for else
                // construct? It is incredibly useful, yet
                // pretty much only Python has it! Using a
                // regular for loop I can fake it by checking
                // the final value of the iterator, but here
                // I'm using for each and the iterator will be
                // out of scope, so I'd have to use another
                // "found" variable instead. Screw it, I'm
                // using goto to pretend I have it here too:
                goto name_already_used;
            }
        }  // else { ;-)
        entry.name_offset = name_off;
        name_off += static_cast<uint32_t>(entry.name.size()) + 1;
        // }
name_already_used:
        LOG_DEBUG("Stream: %i, Name: %s, Index: %i, Mask: 0x%x, Register: %i, SysValue: %i, Format: %i, Used: 0x%x, Precision: %i\n", entry.stream, entry.name.c_str(), entry.common.semantic_index, entry.common.mask, entry.common.reg, entry.common.system_value, entry.common.format, entry.common.used, entry.min_precision);

        entries.push_back(entry);
        old_pos = *pos;
    }
    // Wind the pos pointer back to the start of the line in case it is
    // another section that the caller will need to parse:
    *pos = old_pos;

    return serialise_signature_section(section24, section28, section32, entry_size, &entries, name_off);
}

static void* serialise_subshader_feature_info_section(
    uint64_t flags)
{
    void*                  section;
    const uint32_t         section_size   = 8;
    const uint32_t         alloc_size     = sizeof(struct section_header) + section_size;
    struct section_header* section_header = nullptr;
    uint64_t*              flags_ptr      = nullptr;

    if (!flags)
        return nullptr;

    // Allocate entire section, including room for section header and padding:
    section = malloc(alloc_size);
    if (!section)
    {
        LOG_INFO("Out of memory\n");
        return nullptr;
    }

    // Pointers to useful data structures and offsets in the buffer:
    section_header = static_cast<::section_header*>(section);
    memcpy(section_header->signature, "SFI0", 4);
    section_header->size = section_size;

    flags_ptr  = reinterpret_cast<uint64_t*>(reinterpret_cast<char*>(section_header) + sizeof(struct section_header));
    *flags_ptr = flags;

    return section;
}

struct gf_sfi
{
    uint64_t sfi;
    int      len;
    char*    gf;
};

static struct gf_sfi global_flag_sfi_map[] = {
    { 1LL << 0, 29, "enableDoublePrecisionFloatOps" },
    { SFI_RAW_STRUCT_BUF, 29, "enableRawAndStructuredBuffers" },  // Confirmed
    { SFI_MIN_PRECISION, 22, "enableMinimumPrecision" },
    { 1LL << 5, 26, "enable11_1DoubleExtensions" },
    { 1LL << 6, 26, "enable11_1ShaderExtensions" },

    // Does not map to SFI:
    // "refactoringAllowed"
    // "forceEarlyDepthStencil"
    // "skipOptimization"
    // "allResourcesBound"
};

static char* subshader_feature_comments[] = {
    // d3dcompiler_46:
    "Double-precision floating point",
    //"Early depth-stencil", // d3dcompiler46/47 produces this output for [force, but it does *NOT* map to an SFI flag
    "Raw and Structured buffers",  // DirectXShaderCompiler lists this in this position instead, which matches the globalFlag mapping
    "UAVs at every shader stage", "64 UAV slots", "Minimum-precision data types", "Double-precision extensions for 11.1", "Shader extensions for 11.1", "Comparison filtering for feature level 9",
    // d3dcompiler_47:
    "Tiled resources", "PS Output Stencil Ref", "PS Inner Coverage", "Typed UAV Load Additional Formats", "Raster Ordered UAVs", "SV_RenderTargetArrayIndex or SV_ViewportArrayIndex from any shader feeding rasterizer",
    // DX12 DirectXShaderCompiler (tools/clang/tools/dxcompiler/dxcdisassembler.cpp)
    "Wave level operations", "64-Bit integer", "View Instancing", "Barycentrics", "Use native low precision", "Shading Rate"
};

// Parses the globalFlags in the bytecode to derive Subshader Feature Info.
// This is incomplete, as some of the SFI flags are not in globalFlags, but
// must be found from the "shader requires" comment block instead.
uint64_t parse_global_flags_to_sfi(
    string* shader)
{
    uint64_t sfi = 0LL;
    string   line;
    size_t   pos = 0, gf_pos = 16;
    int      i;

    while (pos != shader->npos)
    {
        line = next_line(shader, &pos);
        if (!strncmp(line.c_str(), "dcl_globalFlags ", 16))
        {
            LOG_DEBUG("%s\n", line.c_str());
            while (gf_pos != string::npos)
            {
                for (i = 0; i < ARRAYSIZE(global_flag_sfi_map); i++)
                {
                    if (!line.compare(gf_pos, global_flag_sfi_map[i].len, global_flag_sfi_map[i].gf))
                    {
                        LOG_DEBUG("Mapped %s to Subshader Feature 0x%llx\n", global_flag_sfi_map[i].gf, global_flag_sfi_map[i].sfi);
                        sfi |= global_flag_sfi_map[i].sfi;
                        gf_pos += global_flag_sfi_map[i].len;
                        break;
                    }
                }
                gf_pos = line.find_first_of(" |", gf_pos);
                gf_pos = line.find_first_not_of(" |", gf_pos);
            }
            return sfi;
        }
    }
    return 0;
}

// Parses the SFI comment block. This is not complete, as some of the flags
// come from globalFlags instead of / as well as this.
static uint64_t parse_subshader_feature_info_comment(
    string*  shader,
    size_t*  pos,
    uint64_t flags)
{
    string   line;
    size_t   old_pos = *pos;
    uint32_t i;

    while (*pos != shader->npos)
    {
        line = next_line(shader, pos);

        LOG_DEBUG("%s\n", line.c_str());

        for (i = 0; i < ARRAYSIZE(subshader_feature_comments); i++)
        {
            if (!strcmp(line.c_str() + 9, subshader_feature_comments[i]))
            {
                LOG_DEBUG("Matched Subshader Feature Comment 0x%llx\n", 1LL << i);
                flags |= 1LL << i;
                break;
            }
        }
        if (i == ARRAYSIZE(subshader_feature_comments))
            break;
    }

    // Wind the pos pointer back to the start of the line in case it is
    // another section that the caller will need to parse:
    *pos = old_pos;

    return flags;
}

static void* manufacture_empty_section(
    char* section_name)
{
    void* section;

    LOG_INFO("Manufacturing placeholder %s section...\n", section_name);

    section = malloc(8);
    if (!section)
    {
        LOG_INFO("Out of memory\n");
        return nullptr;
    }

    memcpy(section, section_name, 4);
    memset(static_cast<char*>(section) + 4, 0, 4);

    return section;
}

static bool is_hull_shader(
    string* shader,
    size_t  start_pos)
{
    string line;
    size_t pos = start_pos;

    while (pos != shader->npos)
    {
        line = next_line(shader, &pos);
        if (!strncmp(line.c_str(), "hs_4_", 5))
            return true;
        if (!strncmp(line.c_str(), "hs_5_", 5))
            return true;
        if (!strncmp(line.c_str() + 1, "s_4_", 4))
            return false;
        if (!strncmp(line.c_str() + 1, "s_5_", 4))
            return false;
    }

    return false;
}

static bool is_geometry_shader_5(
    string* shader,
    size_t  start_pos)
{
    string line;
    size_t pos = start_pos;

    while (pos != shader->npos)
    {
        line = next_line(shader, &pos);
        if (!strncmp(line.c_str(), "gs_5_", 5))
            return true;
        if (!strncmp(line.c_str() + 1, "s_4_", 4))
            return false;
        if (!strncmp(line.c_str() + 1, "s_5_", 4))
            return false;
    }

    return false;
}

static bool parse_section(
    string*   line,
    string*   shader,
    size_t*   pos,
    void**    section,
    uint64_t* sfi,
    bool*     force_shex)
{
    *section = nullptr;

    if (!strncmp(line->c_str() + 1, "s_4_", 4))
    {
        if (!!(*sfi & SFI_FORCE_SHEX) || *force_shex)
            *section = manufacture_empty_section("SHEX");
        else
            *section = manufacture_empty_section("SHDR");
        return true;
    }
    if (!strncmp(line->c_str() + 1, "s_5_", 4))
    {
        *section = manufacture_empty_section("SHEX");
        return true;
    }

    if (!strncmp(line->c_str(), "// Patch Constant signature:", 28))
    {
        LOG_INFO("Parsing Patch Constant Signature section...\n");
        *section = parse_signature_section("PCSG", nullptr, "PSG1", shader, pos, is_hull_shader(shader, *pos), *sfi);
    }
    else if (!strncmp(line->c_str(), "// Input signature:", 19))
    {
        LOG_INFO("Parsing Input Signature section...\n");
        *section = parse_signature_section("ISGN", nullptr, "ISG1", shader, pos, false, *sfi);
    }
    else if (!strncmp(line->c_str(), "// Output signature:", 20))
    {
        LOG_INFO("Parsing Output Signature section...\n");
        char* section24 = "OSGN";
        if (is_geometry_shader_5(shader, *pos))
            section24 = nullptr;
        *section = parse_signature_section(section24, "OSG5", "OSG1", shader, pos, true, *sfi);
    }
    else if (!strncmp(line->c_str(), "// Note: shader requires additional functionality:", 50))
    {
        LOG_INFO("Parsing Subshader Feature Info section...\n");
        *sfi = parse_subshader_feature_info_comment(shader, pos, *sfi);
    }
    else if (!strncmp(line->c_str(), "// Note: SHADER WILL ONLY WORK WITH THE DEBUG SDK LAYER ENABLED.", 64))
    {
        *force_shex = true;
    }

    return false;
}

static void serialise_shader_binary(
    vector<void*>* sections,
    uint32_t       all_sections_size,
    vector<byte>*  bytecode)
{
    uint32_t            section_size;
    uint32_t            shader_size;
    struct dxbc_header* header             = nullptr;
    uint32_t*           section_offset_ptr = nullptr;
    void*               section_ptr        = nullptr;

    // Calculate final size of shader binary:
    shader_size = static_cast<uint32_t>(sizeof(struct dxbc_header) + 4 * sections->size() + all_sections_size);

    bytecode->resize(shader_size);

    // Get some useful pointers into the buffer:
    header             = reinterpret_cast<struct dxbc_header*>(bytecode->data());
    section_offset_ptr = reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(header) + sizeof(struct dxbc_header));
    section_ptr        = static_cast<void*>(section_offset_ptr + sections->size());

    memcpy(header->signature, "DXBC", 4);
    memset(header->hash, 0, sizeof(header->hash));  // Will be filled in by assembler
    header->one          = 1;
    header->size         = shader_size;
    header->num_sections = static_cast<uint32_t>(sections->size());

    for (void* section : *sections)
    {
        section_size = *(static_cast<uint32_t*>(section) + 1) + sizeof(section_header);
        memcpy(section_ptr, section, section_size);
        *section_offset_ptr = static_cast<uint32_t>(static_cast<char*>(section_ptr) - reinterpret_cast<char*>(header));
        section_offset_ptr++;
        section_ptr = static_cast<char*>(section_ptr) + section_size;
    }
}

static HRESULT manufacture_shader_binary(
    const void*   shader_asm,
    size_t        asm_length,
    vector<byte>* bytecode)
{
    string        shader_str(static_cast<const char*>(shader_asm), asm_length);
    string        line;
    size_t        pos  = 0;
    bool          done = false;
    vector<void*> sections;
    uint32_t      section_size, all_sections_size = 0;
    void*         section;
    HRESULT       hr         = E_FAIL;
    uint64_t      sfi        = 0LL;
    bool          force_shex = false;

    sfi = parse_global_flags_to_sfi(&shader_str);

    while (!done && pos != shader_str.npos)
    {
        line = next_line(&shader_str, &pos);
        //LOG_INFO("%s\n", line.c_str());

        done = parse_section(&line, &shader_str, &pos, &section, &sfi, &force_shex);
        if (section)
        {
            sections.push_back(section);
            section_size = *(static_cast<uint32_t*>(section) + 1) + sizeof(section_header);
            all_sections_size += section_size;

            if (gLogDebug)
            {
                LOG_INFO("Constructed section size=%u:\n", section_size);
                for (uint32_t i = 0; i < section_size; i++)
                {
                    if (i && i % 16 == 0)
                        LOG_INFO("\n");
                    LOG_INFO_NO_NL("%02x ", (static_cast<unsigned char*>(section))[i]);
                }
                LOG_INFO("\n");
            }
        }
    }

    if (!done)
    {
        LOG_INFO("Did not find an assembly text section!\n");
        goto out_free;
    }

    if (sfi)
    {
        section = serialise_subshader_feature_info_section(sfi);
        sections.insert(sections.begin(), section);
        section_size = *(static_cast<uint32_t*>(section) + 1) + sizeof(section_header);
        all_sections_size += section_size;
        LOG_INFO("Inserted Subshader Feature Info section: 0x%llx\n", sfi);
    }

    serialise_shader_binary(&sections, all_sections_size, bytecode);

    hr = S_OK;
out_free:
    for (void* const& section : sections)
        free(section);
    return hr;
}

HRESULT assemble_flugan_with_signature_parsing(
    vector<char>*                assembly,
    vector<byte>*                result_bytecode,
    vector<AssemblerParseError>* parse_errors)
{
    vector<byte> manufactured_bytecode;
    HRESULT      hr;

    // Flugan's assembler normally cheats and reuses sections from the
    // original binary when replacing a shader from the game, but that
    // restricts what modifications we can do and is not an option when
    // assembling a stand-alone shader. Let's parse the missing sections
    // ourselved and manufacture a binary shader with those section to pass
    // to Flugan's assembler. Later we should refactor this into the
    // assembler itself.

    hr = manufacture_shader_binary(assembly->data(), assembly->size(), &manufactured_bytecode);
    if (FAILED(hr))
        return E_FAIL;

    *result_bytecode = assembler(assembly, manufactured_bytecode, parse_errors);

    return S_OK;
}

vector<byte> assemble_flugan_with_optional_signature_parsing(
    vector<char>*                assembly,
    bool                         assemble_signatures,
    vector<byte>*                orig_bytecode,
    vector<AssemblerParseError>* parse_errors)
{
    vector<byte> new_bytecode;
    HRESULT      hr;

    if (!assemble_signatures)
        return assembler(assembly, *orig_bytecode, parse_errors);

    hr = assemble_flugan_with_signature_parsing(assembly, &new_bytecode, parse_errors);
    if (FAILED(hr))
        throw parse_error;

    return new_bytecode;
}
