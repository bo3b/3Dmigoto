#pragma once

struct dxbc_header {
	char signature[4]; // DXCB
	uint32_t hash[4]; // Not quite MD5
	uint32_t one; // File version? Always 1
	uint32_t size;
	uint32_t num_sections;
};

struct section_header {
	char signature[4];
	uint32_t size;
};

struct sgn_header {
	uint32_t num_entries;
	uint32_t unknown; // Always 0x00000008? Probably the offset to the sgn_entry array
};

struct sgn_entry_common {
	uint32_t semantic_index;
	uint32_t system_value;
	uint32_t format;
	uint32_t reg;
	uint8_t  mask;
	uint8_t  used;
	uint16_t zero; // 0x0000
};

struct sgn_entry_serialiased { // Base version - 24 bytes
	uint32_t name_offset; // Relative to the start of sgn_header
	struct sgn_entry_common common;
	// Followed by an unpadded array of null-terminated names
	// Whole structure padded to a multiple of 4 bytes with 0xAB
};

struct sg5_entry_serialiased { // Version "5" (only exists for geometry shader outputs) - 28 bytes
	uint32_t stream;
	struct sgn_entry_serialiased sgn;
};

struct sg1_entry_serialiased { // "Version "1" (most recent - I assume that's 5.1?) - 32 bytes
	struct sg5_entry_serialiased sg5;
	uint32_t min_precision;
};

struct sgn_entry_unserialised {
	uint32_t stream;
	string name;
	uint32_t name_offset; // Relative to start of the name list
	struct sgn_entry_common common;
	uint32_t min_precision;
};
