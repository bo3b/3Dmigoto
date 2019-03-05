#pragma once
// Less dependencies for inclusion in Hooked*.cpp that cannot include certain headers

// Grant enums sensible powers that were taken away when C++ ignored C
// MS already defines a macro DEFINE_ENUM_FLAG_OPERATORS that goes part way,
// but only does the bitwise operators and returns the result as enum types (so
// a simple 'if (enum & mask)' will still fail). Really what I want is the
// namespacing of an enum class that behaves like an int as an enum does. Not
// sure why C++ had to try to solve two problems at once and in doing so
// created a brand new problem...
#define SENSIBLE_ENUM(ENUMTYPE) \
inline int operator | (ENUMTYPE a, ENUMTYPE b) { return (((int)a) | ((int)b)); } \
inline int operator & (ENUMTYPE a, ENUMTYPE b) { return (((int)a) & ((int)b)); } \
inline int operator ^ (ENUMTYPE a, ENUMTYPE b) { return (((int)a) ^ ((int)b)); } \
inline int operator ~ (ENUMTYPE a) { return (~((int)a)); } \
inline ENUMTYPE &operator |= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((int &)a) |= ((int)b)); } \
inline ENUMTYPE &operator &= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((int &)a) &= ((int)b)); } \
inline ENUMTYPE &operator ^= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((int &)a) ^= ((int)b)); } \
inline bool operator || (ENUMTYPE a,  ENUMTYPE b) { return (((int)a) || ((int)b)); } \
inline bool operator || (    bool a,  ENUMTYPE b) { return (((int)a) || ((int)b)); } \
inline bool operator || (ENUMTYPE a,      bool b) { return (((int)a) || ((int)b)); } \
inline bool operator && (ENUMTYPE a,  ENUMTYPE b) { return (((int)a) && ((int)b)); } \
inline bool operator && (    bool a,  ENUMTYPE b) { return (((int)a) && ((int)b)); } \
inline bool operator && (ENUMTYPE a,      bool b) { return (((int)a) && ((int)b)); } \
inline bool operator ! (ENUMTYPE a) { return (!((int)a)); }

template <class T1, class T2>
struct EnumName_t {
	T1 name;
	T2 val;
};

const char* find_ini_section_lite(const char *buf, const char *section_name);
bool find_ini_setting_lite(const char *buf, const char *setting, char *ret, size_t n);
bool find_ini_bool_lite(const char *buf, const char *setting, bool def);
int find_ini_int_lite(const char *buf, const char *setting, int def);
