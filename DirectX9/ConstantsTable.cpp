// Member functions
#include "ConstantsTable.h"
bool ConstantTable::Create(const void* data)
{
	const uint32_t* ptr = static_cast<const uint32_t*>(data);
	while (*++ptr != SIO_END)
	{
		if ((*ptr & SI_OPCODE_MASK) == SIO_COMMENT)
		{
			// Check for CTAB comment
			uint32_t comment_size = (*ptr & SI_COMMENTSIZE_MASK) >> 16;
			if (*(ptr + 1) != CTAB_CONSTANT)
			{
				ptr += comment_size;
				continue;
			}

			// Read header
			const char* ctab = reinterpret_cast<const char*>(ptr + 2);
			size_t ctab_size = (comment_size - 1) * 4;

			const CTHeader* header = reinterpret_cast<const CTHeader*>(ctab);
			if (ctab_size < sizeof(*header) || header->Size != sizeof(*header))
				return false;
			m_creator = ctab + header->Creator;

			// Read constants
			m_constants.reserve(header->Constants);
			const CTInfo* info = reinterpret_cast<const CTInfo*>(ctab + header->ConstantInfo);
			for (uint32_t i = 0; i < header->Constants; ++i)
			{
				const CTType* type = reinterpret_cast<const CTType*>(ctab + info[i].TypeInfo);

				// Fill struct
				ConstantDesc desc;
				desc.Name = ctab + info[i].Name;
				desc.RegisterSet = static_cast<EREGISTER_SET>(info[i].RegisterSet);
				desc.RegisterIndex = info[i].RegisterIndex;
				desc.RegisterCount = info[i].RegisterCount;
				desc.Rows = type->Rows;
				desc.Columns = type->Columns;
				desc.Elements = type->Elements;
				desc.StructMembers = type->StructMembers;
				desc.Bytes = 4 * desc.Elements * desc.Rows * desc.Columns;
				m_constants.push_back(desc);
			}
			return true;
		}
	}
	return false;
}

const ConstantDesc* ConstantTable::GetConstantByName(const std::string& name) const
{
	std::vector<ConstantDesc>::const_iterator it;
	for (it = m_constants.begin(); it != m_constants.end(); ++it)
	{
		if (it->Name == name)
			return &(*it);
	}
	return NULL;
}

std::string ConstantTable::ToString()
{
	std::string str;
	//std:size_t maxSize = 0;
	for (auto const& _const : m_constants) {
		std::string const_line;
		switch (_const.RegisterSet) {
		case EREGISTER_SET::RS_BOOL:
			const_line += "bool";
			break;
		case EREGISTER_SET::RS_SAMPLER:
			const_line += "sampler";
			break;
		case EREGISTER_SET::RS_INT4:
			const_line += "int4";
			break;
		case EREGISTER_SET::RS_FLOAT4:
			const_line += "float4";
			break;
		}
		if (_const.RegisterCount > 1) {
			const_line += 'x' + std::to_string(_const.RegisterCount / 4);
		}
		const_line += ' ' + _const.Name;
		if (_const.StructMembers > 1) {
			const_line += '[' + std::to_string(_const.StructMembers) + ']';
		}
		std::size_t lineSize = const_line.size();
		if (lineSize >= 69)
			const_line += "      ";
		else
		{
			size_t i = lineSize;
			while (i < 69) {
				const_line += ' ';
				i++;
			}
		}
		std::string reg;
		switch (_const.RegisterSet) {
		case EREGISTER_SET::RS_BOOL:
			reg += 'b' + std::to_string(_const.RegisterIndex);
			break;
		case EREGISTER_SET::RS_SAMPLER:
			reg += 's' + std::to_string(_const.RegisterIndex);
			break;
		case EREGISTER_SET::RS_INT4:
			reg += 'i' + std::to_string(_const.RegisterIndex);
			break;
		case EREGISTER_SET::RS_FLOAT4:
			reg += 'c' + std::to_string(_const.RegisterIndex);
			break;
		}
		std::size_t regIndexSize = reg.size();
		std::string regCount = std::to_string(_const.RegisterCount);
		std::size_t regCountSize = regCount.size();
		std::size_t i = regIndexSize;
		while (i < (13 - regCountSize)) {
			reg += ' ';
			i++;
		}
		reg += regCount;
		const_line += reg;
		const_line += "\n";
		str += const_line;
	}
	return str;
}
