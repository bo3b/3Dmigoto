#pragma once
#include <vector>
#include <string>
#include <cstdint>

enum EREGISTER_SET
{
	RS_BOOL,
	RS_INT4,
	RS_FLOAT4,
	RS_SAMPLER
};

struct ConstantDesc
{
	std::string Name;
	EREGISTER_SET RegisterSet;
	int RegisterIndex;
	int RegisterCount;
	int Rows;
	int Columns;
	int Elements;
	int StructMembers;
	size_t Bytes;
};

class ConstantTable
{
public:
	bool Create(const void* data);

	size_t GetConstantCount() const { return m_constants.size(); }
	const std::string& GetCreator() const { return m_creator; }

	const ConstantDesc* GetConstantByIndex(size_t i) const { return &m_constants[i]; }
	const ConstantDesc* GetConstantByName(const std::string& name) const;
	size_t GetConstantCountOfType(EREGISTER_SET regSet) const {
		size_t count = 0;
		for (size_t x = 0; x < GetConstantCount(); x++) {
			const ConstantDesc* c = GetConstantByIndex(x);
			if (c->RegisterSet == regSet) {
				count++;
			}
		}
		return count;
	}

	std::string ToString();

private:
	std::vector<ConstantDesc> m_constants;
	std::string m_creator;
};

// Structs
struct CTHeader
{
	uint32_t Size;
	uint32_t Creator;
	uint32_t Version;
	uint32_t Constants;
	uint32_t ConstantInfo;
	uint32_t Flags;
	uint32_t Target;
};

struct CTInfo
{
	uint32_t Name;
	uint16_t RegisterSet;
	uint16_t RegisterIndex;
	uint16_t RegisterCount;
	uint16_t Reserved;
	uint32_t TypeInfo;
	uint32_t DefaultValue;
};

struct CTType
{
	uint16_t Class;
	uint16_t Type;
	uint16_t Rows;
	uint16_t Columns;
	uint16_t Elements;
	uint16_t StructMembers;
	uint32_t StructMemberInfo;
};

// Shader instruction opcodes
const uint32_t SIO_COMMENT = 0x0000FFFE;
const uint32_t SIO_END = 0x0000FFFF;
const uint32_t SI_OPCODE_MASK = 0x0000FFFF;
const uint32_t SI_COMMENTSIZE_MASK = 0x7FFF0000;
const uint32_t CTAB_CONSTANT = 0x42415443;
