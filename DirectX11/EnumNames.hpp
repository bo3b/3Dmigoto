#pragma once

// This header file is a minimalist set of template functions and struct.
// The goal here is to keep this as simple as possible, so that we can
// safely include it anywhere.
//
// This has been moved out of util.h, because the use of templates requires
// a circular reference dependency chain, and complicates util.h.  By making
// this stand-alone I can then include only this as needed, and only util.h
// as needed, and fix our circular dependencies once and for all.

// Nothing other than system headers should be included here, otherwise
// we will get circular dependencies.

#include <string>

//--------------------------------------------------------------------------------------------------

// Grant enums sensible powers that were taken away when C++ ignored C
// MS already defines a macro DEFINE_ENUM_FLAG_OPERATORS that goes part way,
// but only does the bitwise operators and returns the result as enum types (so
// a simple 'if (enum & mask)' will still fail). Really what I want is the
// namespacing of an enum class that behaves like an int as an enum does. Not
// sure why C++ had to try to solve two problems at once and in doing so
// created a brand new problem...
// clang-format off
#define SENSIBLE_ENUM(ENUMTYPE) \
inline int       operator | (ENUMTYPE a, ENUMTYPE b)   { return (((int)a) | ((int)b)); } \
inline int       operator & (ENUMTYPE a, ENUMTYPE b)   { return (((int)a) & ((int)b)); } \
inline int       operator ^ (ENUMTYPE a, ENUMTYPE b)   { return (((int)a) ^ ((int)b)); } \
inline int       operator ~ (ENUMTYPE a)               { return (~((int)a)); } \
inline ENUMTYPE &operator |= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((int &)a) |= ((int)b)); } \
inline ENUMTYPE &operator &= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((int &)a) &= ((int)b)); } \
inline ENUMTYPE &operator ^= (ENUMTYPE &a, ENUMTYPE b) { return (ENUMTYPE &)(((int &)a) ^= ((int)b)); } \
inline bool      operator || (ENUMTYPE a,  ENUMTYPE b) { return (((int)a) || ((int)b)); } \
inline bool      operator || (    bool a,  ENUMTYPE b) { return (((int)a) || ((int)b)); } \
inline bool      operator || (ENUMTYPE a,      bool b) { return (((int)a) || ((int)b)); } \
inline bool      operator && (ENUMTYPE a,  ENUMTYPE b) { return (((int)a) && ((int)b)); } \
inline bool      operator && (    bool a,  ENUMTYPE b) { return (((int)a) && ((int)b)); } \
inline bool      operator && (ENUMTYPE a,      bool b) { return (((int)a) && ((int)b)); } \
inline bool      operator ! (ENUMTYPE a) { return (!((int)a)); }
// clang-format on

template <class T1, class T2>
struct Enum_Name_t
{
    T1 name;
    T2 val;
};

// The basic Enum->Name mapping function.
template <class T1, class T2>
T1 lookup_enum_name(struct Enum_Name_t<T1, T2>* enum_names, T2 val);

// Template routines that handle parsing .ini file parameter variants.
template <class T>
T get_ini_enum_class(const wchar_t* section, const wchar_t* key, T def, bool* found, struct Enum_Name_t<const wchar_t*, T>* enum_names);
template <class T>
T get_ini_enum_class(const wchar_t* section, const wchar_t* key, T def, bool* found, struct Enum_Name_t<const char*, T>* enum_names);

// Resource and bind flags.
template <class T1, class T2>
T2 lookup_enum_val(struct Enum_Name_t<T1, T2>* enum_names, T1 name, T2 type, bool* found = nullptr);
template <class T1, class T2>
T2 lookup_enum_val(struct Enum_Name_t<T1, T2>* enum_names, T1 name, size_t len, T2 type, bool* found = nullptr);
template <class T2>
std::wstring lookup_enum_bit_names(struct Enum_Name_t<const wchar_t*, T2>* enum_names, T2 val);

// Template routines for FrameAnalysis, ResourceCopies, and hooks.
template <class T1, class T2, class T3>
T2 parse_enum_option_string(struct Enum_Name_t<T1, T2>* enum_names, T3 option_string, T1* unrecognised);
template <class T1, class T2>
T2 parse_enum_option_string(struct Enum_Name_t<T1, T2>* enum_names, T1 option_string, T1* unrecognised);
template <class T1, class T2>
T2 parse_enum_option_string_prefix(struct Enum_Name_t<T1, T2>* enum_names, T1 option_string, T1* unrecognised);
