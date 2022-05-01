// These templates are problematic because templates that are used in multiple
// compilation units require the code to remain in the .h file, which goes
// against best practices of not having code in .h files.  But because of the
// poor implementation in C++, code is required at compile time.
// This causes knock-on problems of having to include other headers that are
// otherwise unnecessary and undesirable, and for these specific templates
// it sets up nasty circular dependencies on the headers, which are only
// accidentally avoided because of header order.  Which is a very fragile
// and undesirable approach.
//
// So, rather that put up with all that, let's define the templates as
// prototypes in a EnumNames.hpp file, which will act as forward declarations
// wherever the template is used, but not have any code behind it that
// can require other includes.  Then, we'll put the implemenations of
// those prototypes in this .cpp file, where we are free to include any
// headers necessary.  Lastly, we need to define the actual routines
// so that at link time we can find the implementations. This is done
// by doing explicit instantations of the templates we are using. This
// negates a small advantage of the templates by having the names/types
// specified when used, but it seems a small price to pay to avoid
// circular header dependencies. In addition, it works as documentation
// for how the templates are used.
//
// A better design pattern here would be use an actual object-oriented
// approach, and make a base class with the fundamental name look ups, and
// have subclasses for the various types of Enum_Name_t variants to handle
// the polymorphism, and avoid these nasty template problems.  Not worth
// changing, this is already working, even as it has cost me untold amount
// of problems and time so far.

#include "EnumNames.hpp"

#include "CommandList.hpp"
#include "FrameAnalysis.hpp"
#include "HackerDevice.hpp"
#include "HackerDXGI.hpp"
#include "Hunting.hpp"
#include "IniHandler.h"
#include "log.h"
#include "Overlay.hpp"
#include "Override.hpp"

#include <string>

//--------------------------------------------------------------------------------------------------

template <class T1, class T2>
T1 lookup_enum_name(
    struct Enum_Name_t<T1, T2>* enum_names,
    T2                          val)
{
    for (; enum_names->name; enum_names++)
    {
        if (val == enum_names->val)
            return enum_names->name;
    }

    return nullptr;
}

// We do these explicit template specializations for every usage
// of the lookup_enum_name template, so this will generate a matching
// function that will then be seen by the linker. This allows us
// to keep the implementations out of the header files.
template const wchar_t* lookup_enum_name(struct Enum_Name_t<const wchar_t*, CustomResourceType>* enum_names, CustomResourceType val);
template const wchar_t* lookup_enum_name(struct Enum_Name_t<const wchar_t*, ParamOverrideType>* enum_names, ParamOverrideType val);
template const wchar_t* lookup_enum_name(struct Enum_Name_t<const wchar_t*, MarkingMode>* enum_names, MarkingMode val);
template const char*    lookup_enum_name(struct Enum_Name_t<const char*, Transition_Type>* enum_names, Transition_Type val);

// To use this function be sure to terminate an Enum_Name_t list with {nullptr, 0}
// as it cannot use ArraySize on passed in arrays.
template <class T1, class T2>
T2 lookup_enum_val(
    struct Enum_Name_t<T1, T2>* enum_names,
    T1                          name,
    T2                          type,
    bool*                       found)
{
    for (; enum_names->name; enum_names++)
    {
        if (!_autoicmp(name, enum_names->name))
        {
            if (found)
                *found = true;
            return enum_names->val;
        }
    }

    if (found)
        *found = false;

    return type;
}

// More explicit template instantiations, so that the compiler will know
// to generate these routines, and the linker will then pick these up.
template ParamOverrideType       lookup_enum_val(struct Enum_Name_t<const wchar_t*, ParamOverrideType>* enum_names, const wchar_t* name, ParamOverrideType type, bool* found);
template CustomResourceBindFlags lookup_enum_val(struct Enum_Name_t<const wchar_t*, CustomResourceBindFlags>* enum_names, const wchar_t* name, CustomResourceBindFlags type, bool* found);
template ResourceCPUAccessFlags  lookup_enum_val(struct Enum_Name_t<const wchar_t*, ResourceCPUAccessFlags>* enum_names, const wchar_t* name, ResourceCPUAccessFlags type, bool* found);
template ResourceMiscFlags       lookup_enum_val(struct Enum_Name_t<const wchar_t*, ResourceMiscFlags>* enum_names, const wchar_t* name, ResourceMiscFlags type, bool* found);
template Transition_Type         lookup_enum_val(struct Enum_Name_t<const char*, Transition_Type>* enum_names, const char* name, Transition_Type type, bool* found);

// This one seems unused?
template <class T1, class T2>
T2 lookup_enum_val(
    struct Enum_Name_t<T1, T2>* enum_names,
    T1                          name,
    size_t                      len,
    T2                          type,
    bool*                       found)
{
    for (; enum_names->name; enum_names++)
    {
        if (!_wcsnicmp(name, enum_names->name, len))
        {
            if (found)
                *found = true;
            return enum_names->val;
        }
    }

    if (found)
        *found = false;

    return type;
}

template <class T2>
std::wstring lookup_enum_bit_names(
    struct Enum_Name_t<const wchar_t*, T2>* enum_names,
    T2                                      val)
{
    std::wstring ret;
    T2           remaining = val;

    for (; enum_names->name; enum_names++)
    {
        if (static_cast<T2>(val & enum_names->val) == enum_names->val)
        {
            if (!ret.empty())
                ret += L' ';
            ret += enum_names->name;
            remaining = static_cast<T2>(remaining & static_cast<T2>(~enum_names->val));
        }
    }

    if (remaining != static_cast<T2>(0))
    {
        wchar_t buf[20] {};
        wsprintf(buf, L"%x", remaining);
        if (!ret.empty())
            ret += L' ';
        ret += L"unknown:0x";
        ret += buf;
    }

    return ret;
}

// Since we do these explicit template specializations for every possible usage
// of the lookup_enum_bit_names template, we will generate a function that will
// then be seen by the linker.
template std::wstring lookup_enum_bit_names(struct Enum_Name_t<const wchar_t*, CustomResourceBindFlags>* enum_names, CustomResourceBindFlags val);
template std::wstring lookup_enum_bit_names(struct Enum_Name_t<const wchar_t*, ResourceCPUAccessFlags>* enum_names, ResourceCPUAccessFlags val);
template std::wstring lookup_enum_bit_names(struct Enum_Name_t<const wchar_t*, ResourceMiscFlags>* enum_names, ResourceMiscFlags val);

// Parses an option string of names given by enum_names. The enum used with
// this function should have an INVALID entry, other flags declared as powers
// of two, and the SENSIBLE_ENUM macro used to enable the bitwise and logical
// operators. As above, the Enum_Name_t list must be terminated with {nullptr, 0}
//
// If you wish to parse an option string that contains exactly one unrecognised
// argument, provide a pointer to a pointer in the 'unrecognised' field and the
// unrecognised option will be returned. Multiple unrecognised options are
// still considered errors.
template <class T1, class T2, class T3>
T2 parse_enum_option_string(
    struct Enum_Name_t<T1, T2>* enum_names,
    T3                          option_string,
    T1*                         unrecognised)
{
    T3 ptr = option_string, cur;
    T2 ret = static_cast<T2>(0);
    T2 tmp = T2::INVALID;

    if (unrecognised)
        *unrecognised = nullptr;

    while (*ptr)
    {
        // Skip over whitespace:
        for (; *ptr == L' '; ptr++)
        {
        }

        // Mark start of current entry:
        cur = ptr;

        // Scan until the next whitespace or end of string:
        for (; *ptr && *ptr != L' '; ptr++)
        {
        }

        if (*ptr)
        {
            // NULL terminate the current entry and advance pointer:
            *ptr = L'\0';
            ptr++;
        }

        // Lookup the value of the current entry:
        tmp = lookup_enum_val<T1, T2>(enum_names, cur, T2::INVALID);
        if (tmp != T2::INVALID)
        {
            ret |= tmp;
        }
        else
        {
            if (unrecognised && !(*unrecognised))
            {
                *unrecognised = cur;
            }
            else
            {
                log_overlay_w(overlay::log::warning, L"WARNING: Unknown option: %s\n", cur);
                ret |= T2::INVALID;
            }
        }
    }
    return ret;
}

// Deliberate instantiations of the templates using the params in the code.
// Generates functions that will then be seen by the linker.
template CustomResourceBindFlags parse_enum_option_string(struct Enum_Name_t<const wchar_t*, CustomResourceBindFlags>* enum_names, wchar_t* option_string, const wchar_t** unrecognised);
template ResourceMiscFlags       parse_enum_option_string(struct Enum_Name_t<const wchar_t*, ResourceMiscFlags>* enum_names, wchar_t* option_string, const wchar_t** unrecognised);
template D3DCompileFlags         parse_enum_option_string(struct Enum_Name_t<const wchar_t*, D3DCompileFlags>* enum_names, wchar_t* option_string, const wchar_t** unrecognised);
template MarkingAction           parse_enum_option_string(struct Enum_Name_t<const wchar_t*, MarkingAction>* enum_names, wchar_t* option_string, const wchar_t** unrecognised);

// Two template argument version is the typical case for now. We probably want
// to start adding the 'const' modifier in a bunch of places as we work towards
// migrating to C++ strings, since .c_str() always returns a const string.
// Since the parse_enum_option_string currently modified one of its inputs, it
// cannot use const, so the three argument template version above is to allow
// both const and non-const types passed in.
template <class T1, class T2>
T2 parse_enum_option_string(
    struct Enum_Name_t<T1, T2>* enum_names,
    T1                          option_string,
    T1*                         unrecognised)
{
    return parse_enum_option_string<T1, T2, T1>(enum_names, option_string, unrecognised);
}

// Deliberate instantiations of the templates using the params in the code.
// Generates functions that will then be seen by the linker.
template FrameAnalysisOptions parse_enum_option_string(struct Enum_Name_t<wchar_t*, FrameAnalysisOptions>* enum_names, wchar_t* option_string, wchar_t** unrecognised);
template ResourceCopyOptions  parse_enum_option_string(struct Enum_Name_t<wchar_t*, ResourceCopyOptions>* enum_names, wchar_t* option_string, wchar_t** unrecognised);
template EnableHooks          parse_enum_option_string(struct Enum_Name_t<wchar_t*, EnableHooks>* enum_names, wchar_t* option_string, wchar_t** unrecognised);

// This is similar to the above, but stops parsing when it hits an unrecognised
// keyword and returns the position without throwing any errors. It also
// doesn't modify the option_string, allowing it to be used with C++ strings.
template <class T1, class T2>
T2 parse_enum_option_string_prefix(
    struct Enum_Name_t<T1, T2>* enum_names,
    T1                          option_string,
    T1*                         unrecognised)
{
    T1     ptr = option_string, cur;
    T2     ret = static_cast<T2>(0);
    T2     tmp = T2::INVALID;
    size_t len;

    if (unrecognised)
        *unrecognised = nullptr;

    while (*ptr)
    {
        // Skip over whitespace:
        for (; *ptr == L' '; ptr++)
        {
        }

        // Mark start of current entry:
        cur = ptr;

        // Scan until the next whitespace or end of string:
        for (; *ptr && *ptr != L' '; ptr++)
        {
        }

        // Note word length:
        len = ptr - cur;

        // Advance pointer if not at end of string:
        if (*ptr)
            ptr++;

        // Lookup the value of the current entry:
        tmp = lookup_enum_val<T1, T2>(enum_names, cur, len, T2::INVALID);
        if (tmp != T2::INVALID)
        {
            ret |= tmp;
        }
        else
        {
            if (unrecognised)
                *unrecognised = cur;
            return ret;
        }
    }
    return ret;
}

// Deliberate instantiations of the templates using the params in the code.
// Generates functions that will then be seen by the linker.
template VariableFlags parse_enum_option_string_prefix(struct Enum_Name_t<const wchar_t*, VariableFlags>* enum_names, const wchar_t* option_string, const wchar_t** unrecognised);

//--------------------------------------------------------------------------------------------------

// wchar_t* specialisation. Has character limit
// Want to remove this eventually, though since MarkingMode uses it and
// the DirectXTK API uses wide characters we might keep it around.
template <class T>
T get_ini_enum_class(
    const wchar_t*                         section,
    const wchar_t*                         key,
    T                                      def,
    bool*                                  found,
    struct Enum_Name_t<const wchar_t*, T>* enum_names)
{
    wchar_t val[MAX_PATH];
    T       ret = def;
    bool    tmp_found;

    if (found)
        *found = false;

    if (get_ini_string(section, key, nullptr, val, MAX_PATH))
    {
        ret = lookup_enum_val<const wchar_t*, T>(enum_names, val, def, &tmp_found);
        if (tmp_found)
        {
            if (found)
                *found = tmp_found;
            LOG_INFO("  %S=%S\n", key, val);
        }
        else
        {
            INI_WARNING("WARNING: Unknown %S=%S\n", key, val);
        }
    }

    return ret;
}

// We do these explicit template specializations for every usage
// of the lookup_enum_name template, so this will generate a matching
// function that will then be seen by the linker. This allows us
// to keep the implementations out of the header files.
template MarkingMode        get_ini_enum_class(const wchar_t* section, const wchar_t* key, MarkingMode def, bool* found, struct Enum_Name_t<const wchar_t*, MarkingMode>* enum_names);
template Key_Override_Type  get_ini_enum_class(const wchar_t* section, const wchar_t* key, Key_Override_Type def, bool* found, struct Enum_Name_t<const wchar_t*, Key_Override_Type>* enum_names);
template CustomResourceType get_ini_enum_class(const wchar_t* section, const wchar_t* key, CustomResourceType def, bool* found, struct Enum_Name_t<const wchar_t*, CustomResourceType>* enum_names);
template CustomResourceMode get_ini_enum_class(const wchar_t* section, const wchar_t* key, CustomResourceMode def, bool* found, struct Enum_Name_t<const wchar_t*, CustomResourceMode>* enum_names);
template DepthBufferFilter  get_ini_enum_class(const wchar_t* section, const wchar_t* key, DepthBufferFilter def, bool* found, struct Enum_Name_t<const wchar_t*, DepthBufferFilter>* enum_names);
template GetResolutionFrom  get_ini_enum_class(const wchar_t* section, const wchar_t* key, GetResolutionFrom def, bool* found, struct Enum_Name_t<const wchar_t*, GetResolutionFrom>* enum_names);
template ShaderHashType     get_ini_enum_class(const wchar_t* section, const wchar_t* key, ShaderHashType def, bool* found, struct Enum_Name_t<const wchar_t*, ShaderHashType>* enum_names);

// char* specialisation of the above. No character limit
template <class T>
T get_ini_enum_class(
    const wchar_t*                      section,
    const wchar_t*                      key,
    T                                   def,
    bool*                               found,
    struct Enum_Name_t<const char*, T>* enum_names)
{
    std::string val;
    T           ret = def;
    bool        tmp_found;

    if (found)
        *found = false;

    if (get_ini_string(section, key, nullptr, &val))
    {
        ret = lookup_enum_val<const char*, T>(enum_names, val.c_str(), def, &tmp_found);
        if (tmp_found)
        {
            if (found)
                *found = tmp_found;
            LOG_INFO("  %S=%s\n", key, val.c_str());
        }
        else
        {
            INI_WARNING("WARNING: Unknown %S=%s\n", key, val.c_str());
        }
    }

    return ret;
}

// Specific instantiation of the template to generate a function
// that will be picked up by the linker.
template Transition_Type get_ini_enum_class(const wchar_t* section, const wchar_t* key, Transition_Type def, bool* found, struct Enum_Name_t<const char*, Transition_Type>* enum_names);
