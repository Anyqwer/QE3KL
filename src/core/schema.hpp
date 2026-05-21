#pragma once

#define SCHEMA_GET_OFFSET(field) \
	schema::get_offset(fnv1a::hash_const(field))

#define SCHEMA_ADD_STRING(name, field) \
	inline std::string name() \
	{ \
		const auto ptr = reinterpret_cast<uintptr_t>(this); \
		if (ptr < 0x10000) return {}; \
		const auto name_ptr = m_memory->read_t<uintptr_t>(ptr + schema::get_offset(fnv1a::hash_const(field))); \
		if (name_ptr < 0x10000) return {}; \
		return m_memory->read_t<std::string>(name_ptr); \
	}

#define SCHEMA_ADD_STRING_OFFSET(name, offset) \
	inline std::string name() \
	{ \
		const auto ptr = reinterpret_cast<uintptr_t>(this); \
		if (ptr < 0x10000) return {}; \
		const auto name_ptr = m_memory->read_t<uintptr_t>(ptr + offset); \
		if (name_ptr < 0x10000) return {}; \
		return m_memory->read_t<std::string>(name_ptr); \
	}

#define SCHEMA_ADD_OFFSET(type, name, offset) \
	inline type name() \
	{ \
		const auto ptr = reinterpret_cast<uintptr_t>(this); \
		if (ptr < 0x10000) return {}; \
		return m_memory->read_t<type>(ptr + offset); \
	}

#define SCHEMA_ADD_FIELD_OFFSET(type, name, field, offset) \
	SCHEMA_ADD_OFFSET(type, name, schema::get_offset(fnv1a::hash_const(field)) + offset)

#define SCHEMA_ADD_FIELD(type, name, field) \
	SCHEMA_ADD_FIELD_OFFSET(type, name, field, 0)

namespace schema
{
	bool setup();
	uint32_t get_offset(const fnv1a_t hashed_field_name);
}