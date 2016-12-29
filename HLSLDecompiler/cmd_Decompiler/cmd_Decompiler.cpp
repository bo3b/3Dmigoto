// cmd_Decompiler.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <iostream>     // console output

#include <D3Dcompiler.h>
#include "DecompileHLSL.h"
#include "version.h"
#include "log.h"
#include "util.h"
#include "shader.h"

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

	LogInfo("  -a, --assemble\n");
	LogInfo("\t\t\tAssemble shaders with Flugan's assembler\n");

	// TODO (at the moment we always force):
	// LogInfo("  -f, --force\n");
	// LogInfo("\t\t\tOverwrite existing files\n");

	// Call this validate not verify, because it's impossible to machine
	// verify the decompiler:
	LogInfo("  -V, --validate\n");
	LogInfo("\t\t\tRun a validation pass after decompilation / disassembly\n");

	LogInfo("  -S, --stop-on-failure\n");
	LogInfo("\t\t\tStop processing files if an error occurs\n");

	LogInfo("  -v, --verbose\n");
	LogInfo("\t\t\tVerbose debugging output\n");

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
			if (!strcmp(arg, "-a") || !strcmp(arg, "--assemble")) {
				args.assemble = true;
				continue;
			}
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
			if (!strcmp(arg, "-v") || !strcmp(arg, "--verbose")) {
				gLogDebug = true;
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

class ParseError : public exception {} parseError;

static string next_line(string *shader, size_t *pos)
{
	size_t start_pos = *pos;
	size_t end_pos;

	// Skip preceeding whitespace:
	start_pos = shader->find_first_not_of(" \t\r", start_pos);

	// Blank line at end of file:
	if (start_pos == shader->npos) {
		*pos = shader->npos;
		return "";
	}

	// Find newline, update parent pointer:
	*pos = shader->find('\n', start_pos);

	// Skip trailing whitespace (will pad it later during parsing, but
	// can't rely on whitespace here so still strip it):
	end_pos = shader->find_last_not_of(" \t\n\r", *pos) + 1;

	if (*pos != shader->npos)
		(*pos)++; // Brackets are important

	return shader->substr(start_pos, end_pos - start_pos);
}

struct format_type {
	uint32_t format;
	uint32_t min_precision;
	char *name;
};

static struct format_type format_names[] = {
	{0, 0, "unknown" },
	{1, 0, "uint"    },
	{1, 5, "min16u"  }, // min16uint
	{2, 0, "int"     },
	{2, 4, "min16i"  }, // min16int *AND* min12int as of d3dcompiler47.dll
	{3, 0, "float"   },
	{3, 1, "min16f"  }, // min16float
	{3, 2, "min2_8f" }, // min10float
};

static void parse_format(char *format, uint32_t *format_number, uint32_t *min_precision)
{
	uint32_t i;

	for (i = 0; i < ARRAYSIZE(format_names); i++) {
		if (!strcmp(format, format_names[i].name)) {
			*format_number = format_names[i].format;
			*min_precision = format_names[i].min_precision;
			return;
		}
	}

	throw parseError;
}

struct svt {
	uint32_t val;
	char *short_name;
};

static struct svt system_value_abbreviations[] = {
	{  0, "NONE",      }, // TEXCOORDs, input position to VS
	{  0, "TARGET",    }, // SV_Target

	{  1, "POS",       }, // SV_Position
	{  2, "CLIPDST",   }, // SV_ClipDistance
	{  3, "CULLDST",   }, // SV_CullDistance
	{  4, "RTINDEX",   }, // SV_RenderTargetArrayIndex
	{  5, "VPINDEX",   }, // SV_ViewportArrayIndex
	{  6, "VERTID",    }, // SV_VertexID
	{  7, "PRIMID",    }, // SV_PrimitiveID, register = "   primID"
	{  8, "INSTID",    }, // SV_InstanceID
	{  9, "FFACE",     }, // SV_IsFrontFace
	{ 10, "SAMPLE",    }, // SV_SampleIndex

	// Tesselation domain/hull shaders. XXX: These numbers don't match up
	// with BinaryDecompiler, but I got them myself, so I am 100% certain
	// they are right, but perhaps I am missing something (Update - now
	// pretty sure the values in BinaryDecompiler are those used in the
	// bytecode, whereas these values are used in the signatures):
	{ 11, "QUADEDGE",  }, // SV_TessFactor with [domain("quad")]
	{ 12, "QUADINT",   }, // SV_InsideTessFactor with [domain("quad")]
	{ 13, "TRIEDGE",   }, // SV_TessFactor with [domain("tri")]
	{ 14, "TRIINT",    }, // SV_InsideTessFactor with [domain("tri")]
	{ 15, "LINEDET",   },
	{ 16, "LINEDEN",   },

	// System values using SPRs (special purpose registers). These only
	// ever seem to show up in the output signature - any used as inputs is
	// not in the input signature, and uses a 'v' prefix instead of 'o'.
	// Mask = "N/A" (0x1)
	// Register = "oSomething" (0xffffffff)
	// Used = "YES" (0xe) / "NO" (0x0)
	{  0, "DEPTH"      }, // SV_Depth, oDepth
	{  0, "COVERAGE",  }, // SV_Coverage, oMask
	{  0, "DEPTHGE"    }, // SV_DepthGreaterEqual, oDepthGE
	{  0, "DEPTHLE"    }, // SV_DepthLessEqual, oDepthLE

	// Only available in DX12 / shader model 5.1 / d3dcompiler47.dll:
	{  0, "STENCILREF" }, // SV_StencilRef, oStencilRef
	{  0, "INNERCOV"   }, // SV_InnerCoverage, not sure what register is listed as - "special"?

	// Some other semantics which don't appear here (e.g. SV_GSInstanceID,
	// any of the compute shader thread IDs) are not present in these
	// sections.
};

static uint32_t parse_system_value(char *sv)
{
	uint32_t i;

	for (i = 0; i < ARRAYSIZE(system_value_abbreviations); i++) {
		if (!strcmp(sv, system_value_abbreviations[i].short_name))
			return system_value_abbreviations[i].val;
	}

	throw parseError;
}

static uint8_t parse_mask(char mask[8], bool invert)
{
	uint8_t xor_val = (invert ? 0xf : 0);
	uint8_t ret = 0;
	int i;

	// This allows for more flexible processing rather than just limiting
	// it to what the disassembler generates

	for (i = 0; i < 8 && mask[i]; i++) {
		switch (mask[i]) {
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
			case 'Y': // YES
				return 0x1 ^ xor_val;
			case 'N': // NO or N/A - wait for next character
				break;
			case 'O': // NO
				return 0x0 ^ xor_val;
			case '/': // N/A
				return 0x1 ^ xor_val;
			default:
				throw parseError;
		}
	}
	return ret ^ xor_val;
}

static uint32_t pad(uint32_t size, uint32_t multiple)
{
	return (multiple - size % multiple) % multiple;
}

static void* serialise_signature_section(char *section24, char *section28, char *section32, int entry_size,
		vector<struct sgn_entry_unserialised> *entries, uint32_t name_len)
{
	void *section;
	uint32_t section_size, padding, alloc_size, name_off;
	char *name_ptr = NULL;
	void *padding_ptr = NULL;
	struct section_header *section_header = NULL;
	struct sgn_header *sgn_header = NULL;
	sgn_entry_serialiased *entryn = NULL;
	sg5_entry_serialiased *entry5 = NULL;
	sg1_entry_serialiased *entry1 = NULL;

	// Geometry shader 5 never uses OSGN, bump to OSG5:
	if (entry_size == 24 && section24 == NULL)
		entry_size = 28;

	// Only OSG5 exists in version 5, so bump ISG & PSG to version 5.1:
	if (entry_size == 28 && section28 == NULL)
		entry_size = 32;

	// Calculate various offsets and sizes:
	name_off = (uint32_t)(sizeof(struct sgn_header) + (entry_size * entries->size()));
	section_size = name_off + name_len;
	padding = pad(section_size, 4);
	alloc_size = section_size + sizeof(struct section_header) + padding;

	LogDebug("name_off: %u, name_len: %u, section_size: %u, padding: %u, alloc_size: %u\n",
			name_off, name_len, section_size, padding, alloc_size);

	// Allocate entire section, including room for section header and padding:
	section = malloc(alloc_size);
	if (!section) {
		LogInfo("Out of memory\n");
		return NULL;
	}

	// Pointers to useful data structures and offsets in the buffer:
	section_header = (struct section_header*)section;
	sgn_header = (struct sgn_header*)((char*)section_header + sizeof(struct section_header));
	padding_ptr = (void*)((char*)sgn_header + section_size);
	// Only one of these will be used as the base address depending on the
	// structure version, but pointers to the older versions will also be
	// updated during the iteration:
	entryn = (struct sgn_entry_serialiased*)((char*)sgn_header + sizeof(struct sgn_header));
	entry5 = (struct sg5_entry_serialiased*)entryn;
	entry1 = (struct sg1_entry_serialiased*)entryn;

	LogDebug("section: 0x%p, section_header: 0x%p, sgn_header: 0x%p, padding_ptr: 0x%p, entry: 0x%p\n",
			section, (char*)section_header, sgn_header, padding_ptr, entryn);

	switch (entry_size) {
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
			throw parseError;
	}
	section_header->size = section_size + padding;

	sgn_header->num_entries = (uint32_t)entries->size();
	sgn_header->unknown = sizeof(struct sgn_header); // Not confirmed, but seems likely. Always 8

	// Fill out entries:
	for (struct sgn_entry_unserialised const &unserialised : *entries) {
		switch (entry_size) {
			case 32:
				entry1->min_precision = unserialised.min_precision;
				entry5 = &entry1->sg5;
				entry1++;
				// Fall through
			case 28:
				entry5->stream = unserialised.stream;
				entryn = &entry5->sgn;
				entry5++;
				// Fall through
			case 24:
				entryn->name_offset = name_off + unserialised.name_offset;
				name_ptr = (char*)sgn_header + entryn->name_offset;
				memcpy(name_ptr, unserialised.name.c_str(), unserialised.name.size() + 1);
				memcpy(&entryn->common, &unserialised.common, sizeof(struct sgn_entry_common));
				entryn++;
		}
	}

	memset(padding_ptr, 0xab, padding);

	return section;
}

static void* parse_signature_section(char *section24, char *section28, char *section32, string *shader, size_t *pos, bool invert_used)
{
	string line;
	size_t old_pos = *pos;
	int numRead;
	uint32_t name_off = 0;
	int entry_size = 24; // We use the size, because in MS's usual wisdom version 1 is higher than version 5 :facepalm:
	char semantic_name[64]; // Semantic names are limited to 63 characters in fxc
	char semantic_name2[64];
	char system_value[16]; // More than long enough for even "STENCILREF"
	char reg[16]; // More than long enough for even "oStencilRef"
	char mask[8], used[8]; // We read 7 characters - check the reason below
	char format[16]; // Long enough for even "unknown"
	vector<struct sgn_entry_unserialised> entries;
	struct sgn_entry_unserialised entry;

	while (*pos != shader->npos) {
		line = next_line(shader, pos);

		LogDebug("%s\n", line.c_str());

		if (line == "//"
		 || line == "// Name                 Index   Mask Register SysValue  Format   Used"
		 || line == "// -------------------- ----- ------ -------- -------- ------- ------") {
			continue;
		}

		if (line == "// no Input"
		 || line == "// no Output"
		 || line == "// no Patch Constant") {
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

		numRead = sscanf_s((line + "       ").c_str(),
				"// %s %d%7c %s %s %s%7c",
				semantic_name, (unsigned)ARRAYSIZE(semantic_name),
				&entry.common.semantic_index,
				mask, (unsigned)ARRAYSIZE(mask),
				&reg, (unsigned)ARRAYSIZE(reg),
				system_value, (unsigned)ARRAYSIZE(system_value),
				format, (unsigned)ARRAYSIZE(format),
				used, (unsigned)ARRAYSIZE(used));

		if (numRead != 7) {
			// I really would love to throw parseError here to
			// catch typos, but since this is in a comment I can't
			// be certain that this is part of the signature
			// declaration, so I have to assume this is the end of
			// the section :(
			break;
		}

		// Try parsing the semantic name with streams, and bump the
		// section version if sucessful:
		numRead = sscanf_s(semantic_name, "m%u:%s",
				&entry.stream,
				semantic_name2, (unsigned)ARRAYSIZE(semantic_name2));
		if (numRead == 2) {
			entry_size = max(entry_size, 28);
			entry.name = semantic_name2;
		} else {
			entry.stream = 0;
			entry.name = semantic_name;
		}

		// Parse the format. If it is one of the minimum precision
		// formats, bump the section version:
		parse_format(format, &entry.common.format, &entry.min_precision);
		if (entry.min_precision)
			entry_size = max(entry_size, 32);

		// Try parsing register as a decimal number. If it is not, it
		// is a special purpose register, in which case we store -1:
		if (numRead = sscanf_s(reg, "%d", &entry.common.reg) == 0)
			entry.common.reg = 0xffffffff;

		entry.common.system_value = parse_system_value(system_value);
		entry.common.mask = parse_mask(mask, false);
		entry.common.used = parse_mask(used, invert_used);
		entry.common.zero = 0;

		// Check if a previous entry used the same semantic name
		for (struct sgn_entry_unserialised const &prev_entry : entries) {
			if (prev_entry.name == entry.name) {
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
		} // else { ;-)
			entry.name_offset = name_off;
			name_off += (uint32_t)entry.name.size() + 1;
		// }
name_already_used:

		LogDebug("Stream: %i, Name: %s, Index: %i, Mask: 0x%x, Register: %i, SysValue: %i, Format: %i, Used: 0x%x, Precision: %i\n",
				entry.stream, entry.name.c_str(),
				entry.common.semantic_index, entry.common.mask,
				entry.common.reg, entry.common.system_value,
				entry.common.format, entry.common.used,
				entry.min_precision);

		entries.push_back(entry);
		old_pos = *pos;
	}
	// Wind the pos pointer back to the start of the line in case it is
	// another section that the caller will need to parse:
	*pos = old_pos;

	return serialise_signature_section(section24, section28, section32, entry_size, &entries, name_off);
}

static void* manufacture_empty_section(char *section_name)
{
	void *section;

	LogInfo("Manufacturing placeholder %s section...\n", section_name);

	section = malloc(8);
	if (!section) {
		LogInfo("Out of memory\n");
		return NULL;
	}

	memcpy(section, section_name, 4);
	memset((char*)section + 4, 0, 4);

	return section;
}

static bool is_hull_shader(string *shader, size_t start_pos) {
	string line;
	size_t pos = start_pos;

	while (pos != shader->npos) {
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

static bool is_geometry_shader_5(string *shader, size_t start_pos) {
	string line;
	size_t pos = start_pos;

	while (pos != shader->npos) {
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

static bool parse_section(string *line, string *shader, size_t *pos, void **section)
{
	*section = NULL;

	if (!strncmp(line->c_str() + 1, "s_4_", 4)) {
		*section = manufacture_empty_section("SHDR");
		return true;
	}
	if (!strncmp(line->c_str() + 1, "s_5_", 4)) {
		*section = manufacture_empty_section("SHEX");
		return true;
	}

	if (!strncmp(line->c_str(), "// Patch Constant signature:", 28)) {
		LogInfo("Parsing Patch Constant Signature section...\n",);
		*section = parse_signature_section("PCSG", NULL, "PSG1", shader, pos, is_hull_shader(shader, *pos));
	} else if (!strncmp(line->c_str(), "// Input signature:", 19)) {
		LogInfo("Parsing Input Signature section...\n",);
		*section = parse_signature_section("ISGN", NULL, "ISG1", shader, pos, false);
	} else if (!strncmp(line->c_str(), "// Output signature:", 20)) {
		LogInfo("Parsing Output Signature section...\n",);
		char *section24 = "OSGN";
		if (is_geometry_shader_5(shader, *pos))
			section24 = NULL;
		*section = parse_signature_section(section24, "OSG5", "OSG1", shader, pos, true);
	}

	return false;
}

static void serialise_shader_binary(vector<void*> *sections, uint32_t all_sections_size, vector<byte> *bytecode)
{
	struct dxbc_header *header = NULL;
	uint32_t *section_offset_ptr = NULL;
	void *section_ptr = NULL;
	uint32_t section_size;
	uint32_t shader_size;

	// Calculate final size of shader binary:
	shader_size = (uint32_t)(sizeof(struct dxbc_header) + 4 * sections->size() + all_sections_size);

	bytecode->resize(shader_size);

	// Get some useful pointers into the buffer:
	header = (struct dxbc_header*)bytecode->data();
	section_offset_ptr = (uint32_t*)((char*)header + sizeof(struct dxbc_header));
	section_ptr = (void*)(section_offset_ptr + sections->size());

	memcpy(header->signature, "DXBC", 4);
	memset(header->hash, 0, sizeof(header->hash)); // Will be filled in by assembler
	header->one = 1;
	header->size = shader_size;
	header->num_sections = (uint32_t)sections->size();

	for (void *section : *sections) {
		section_size = *((uint32_t*)section + 1) + sizeof(section_header);
		memcpy(section_ptr, section, section_size);
		*section_offset_ptr = (uint32_t)((char*)section_ptr - (char*)header);
		section_offset_ptr++;
		section_ptr = (char*)section_ptr + section_size;
	}
}

static HRESULT manufacture_shader_binary(const void *pShaderAsm, size_t AsmLength, vector<byte> *bytecode)
{
	string shader_str((const char*)pShaderAsm, AsmLength);
	string line;
	size_t pos = 0;
	bool done = false;
	vector<void*> sections;
	uint32_t section_size, all_sections_size = 0;
	void *section;
	HRESULT hr = E_FAIL;

	while (!done && pos != shader_str.npos) {
		line = next_line(&shader_str, &pos);
		//LogInfo("%s\n", line.c_str());

		done = parse_section(&line, &shader_str, &pos, &section);
		if (section) {
			sections.push_back(section);
			section_size = *((uint32_t*)section + 1) + sizeof(section_header);
			all_sections_size += section_size;

			if (gLogDebug) {
				LogInfo("Constructed section size=%u:\n", section_size);
				for (uint32_t i = 0; i < section_size; i++) {
					if (i && i % 16 == 0)
						LogInfo("\n");
					LogInfo("%02x ", ((unsigned char*)section)[i]);
				}
				LogInfo("\n");
			}
		}
	}

	if (!done) {
		LogInfo("Did not find an assembly text section!");
		goto out_free;
	}

	serialise_shader_binary(&sections, all_sections_size, bytecode);

	hr = S_OK;
out_free:
	for (void * const &section : sections)
		free(section);
	return hr;
}

static HRESULT AssembleFlugan(vector<char> *assembly, string *bytecode)
{
	vector<byte> new_bytecode;
	vector<byte> manufactured_bytecode;
	HRESULT hr;

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

	new_bytecode = assembler(*reinterpret_cast<vector<byte>*>(assembly), manufactured_bytecode);

	*bytecode = string(new_bytecode.begin(), new_bytecode.end());

	return S_OK;
}

static int validate_section(char section[4], unsigned char *old_section, unsigned char *new_section, size_t size)
{
	unsigned char *p1 = old_section, *p2 = new_section;
	int rc = 0;
	size_t pos;

	for (pos = 0; pos < size; pos++, p1++, p2++) {
		if (*p1 == *p2)
			continue;

		if (!rc)
			LogInfo("\n*** Assembly verification pass failed: mismatch in section %.4s:\n", section);
		LogInfo("  0x%08Ix: expected 0x%02x, found 0x%02x\n", pos, *p1, *p2);
		rc = 1;
	}

	return rc;
}

static int validate_assembly(string *assembly, vector<char> *old_shader)
{
	vector<char> assembly_vec(assembly->begin(), assembly->end());
	string new_assembly;
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
	hret = AssembleFlugan(&assembly_vec, &new_assembly);
	if (FAILED(hret)) {
		LogInfo("\n*** Assembly verification pass failed: Reassembly failed 0x%x\n", hret);
		return 1;
	}

	vector<char> new_shader(new_assembly.begin(), new_assembly.end());

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

			if (memcmp(old_section_header->signature, new_section_header->signature, 4))
				continue;

			LogDebug(" Checking section %.4s...", old_section_header->signature);

			size = min(old_section_header->size, new_section_header->size);
			old_section = (unsigned char*)old_section_header + sizeof(struct section_header);
			new_section = (unsigned char*)new_section_header + sizeof(struct section_header);

			if (validate_section(old_section_header->signature, old_section, new_section, size))
				rc = 1;
			else
				LogDebug(" OK\n");

			if (old_section_header->size != new_section_header->size) {
				LogInfo("\n*** Assembly verification pass failed: size mismatch in section %.4s\n",
						old_section_header->signature);
				rc = 1;
			}

			break;
		}
		if (j == new_dxbc_header->num_sections)
			LogInfo("Reassembled shader missing %.4s section\n", old_section_header->signature);
	}

	if (!rc)
		LogInfo("    Assembly verification pass succeeded\n");
	return rc;
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
			// TODO: Validate signature parsing instead of binary identical files
		}

		if (WriteOutput(filename, ".asm", &output))
			return EXIT_FAILURE;

	}

	if (args.assemble) {
		LogInfo("Assembling %s...\n", filename->c_str());
		hret = AssembleFlugan(&srcData, &output);
		if (FAILED(hret))
			return EXIT_FAILURE;

		// TODO:
		// if (args.validate)
		// disassemble again and perform fuzzy compare

		if (WriteOutput(filename, ".shdr", &output))
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

