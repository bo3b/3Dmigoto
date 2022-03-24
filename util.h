#pragma once

#include <cctype>
#include <cwchar>
#include <string>
#include <vector>

#include <d3d11_1.h>

#include <D3Dcompiler.h>
#include <DirectXMath.h>

#include "util_min.h"

#include "D3D_Shaders\stdafx.h"


// Sets the threshold for warning about IniParams size. The larger IniParams is
// the more CPU -> GPU bandwidth we will require to update it, so we want to
// discourage modders from picking arbitrarily high IniParams.
//
// This threshold is somewhat arbitrary and I haven't measured how performance
// actually goes in practice, so we can tweak it as we encounter real world
// performance issues. I've chosen the page size of 4K as a starting point as
// exceeding that will likely add additional performance overheads beyond the
// bandwidth requirements (ideally we would also ensure the IniParams buffer is
// aligned to a page boundary).
//
// If a shaderhacker wants more than 1024 (256x4) IniParams they should
// probably think about using a different storage means anyway, since IniParams
// has other problems such as no meaningful names, no namespacing, etc.
constexpr int ini_params_size_warning = 256;

// -----------------------------------------------------------------------------------------------

// This critical section must be held to avoid race conditions when creating
// any resource. The nvapi functions used to set the resource creation mode
// affect global state, so if multiple threads are creating resources
// simultaneously it is possible for a StereoMode override or stereo/mono copy
// on one thread to affect another. This should be taken before setting the
// surface creation mode and released only after it has been restored. If the
// creation mode is not being set it should still be taken around the actual
// CreateXXX call.
//
// The actual variable definition is in the DX11 project to remind anyone using
// this from another project that they need to InitializeCriticalSection[Pretty]
extern CRITICAL_SECTION resource_creation_mode_lock;

// TODO: Is this necessary? Why not use the pretty version in all cases?
// Use the pretty lock debugging version if lock.h is included first, otherwise
// use the regular EnterCriticalSection:
#ifdef EnterCriticalSectionPretty
    #define LOCK_RESOURCE_CREATION_MODE() \
        ENTER_CRITICAL_SECTION(&resource_creation_mode_lock)
    #define UNLOCK_RESOURCE_CREATION_MODE() \
        LEAVE_CRITICAL_SECTION(&resource_creation_mode_lock)
#else
    #define LOCK_RESOURCE_CREATION_MODE() \
        EnterCriticalSection(&resource_creation_mode_lock)
    #define UNLOCK_RESOURCE_CREATION_MODE() \
        LeaveCriticalSection(&resource_creation_mode_lock)
#endif

// -----------------------------------------------------------------------------------------------

// Create hash code for textures or buffers.

uint32_t crc32c_hw(uint32_t seed, const void* buffer, size_t length);

// -----------------------------------------------------------------------------------------------

// Primary hash calculation for all shader file names.

// 64 bit magic FNV-0 and FNV-1 prime
#define FNV_64_PRIME ((UINT64)0x100000001b3ULL)
UINT64 fnv_64_buf(const void* buf, size_t len);

// -----------------------------------------------------------------------------------------------

// Strip spaces from the right of a string.
// Returns a pointer to the last non-NULL character of the truncated string.
char* right_strip_a(char* buf);
wchar_t* right_strip_w(wchar_t* buf);

char* read_string_parameter(wchar_t* val);

// -----------------------------------------------------------------------------------------------

void beep_success();

void beep_short();

void beep_failure();
void beep_sad_failure();
void beep_profile_fail();

DECLSPEC_NORETURN void double_beep_exit();

// -----------------------------------------------------------------------------------------------

int _autoicmp(const wchar_t* s1, const wchar_t* s2);
int _autoicmp(const char* s1, const char* s2);

// -----------------------------------------------------------------------------------------------

// To use this function be sure to terminate an EnumName_t list with {NULL, 0}
// as it cannot use ArraySize on passed in arrays.
template <class T1, class T2>
static T2 lookup_enum_val(struct EnumName_t<T1, T2>* enum_names, T1 name, T2 type, bool* found = NULL)
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
template <class T1, class T2>
static T2 lookup_enum_val(struct EnumName_t<T1, T2>* enum_names, T1 name, size_t len, T2 type, bool* found = NULL)
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
template <class T1, class T2>
static T1 lookup_enum_name(struct EnumName_t<T1, T2>* enum_names, T2 val)
{
    for (; enum_names->name; enum_names++)
    {
        if (val == enum_names->val)
            return enum_names->name;
    }

    return NULL;
}

template <class T2>
static std::wstring lookup_enum_bit_names(struct EnumName_t<const wchar_t*, T2>* enum_names, T2 val)
{
    std::wstring ret;
    T2 remaining = val;

    for (; enum_names->name; enum_names++)
    {
        if ((T2)(val & enum_names->val) == enum_names->val)
        {
            if (!ret.empty())
                ret += L' ';
            ret += enum_names->name;
            remaining = (T2)(remaining & (T2)~enum_names->val);
        }
    }

    if (remaining != (T2)0)
    {
        wchar_t buf[20]{};
        wsprintf(buf, L"%x", remaining);
        if (!ret.empty())
            ret += L' ';
        ret += L"unknown:0x";
        ret += buf;
    }

    return ret;
}

// Parses an option string of names given by enum_names. The enum used with
// this function should have an INVALID entry, other flags declared as powers
// of two, and the SENSIBLE_ENUM macro used to enable the bitwise and logical
// operators. As above, the EnumName_t list must be terminated with {NULL, 0}
//
// If you wish to parse an option string that contains exactly one unrecognised
// argument, provide a pointer to a pointer in the 'unrecognised' field and the
// unrecognised option will be returned. Multiple unrecognised options are
// still considered errors.
template <class T1, class T2, class T3>
static T2 parse_enum_option_string(struct EnumName_t<T1, T2>* enum_names, T3 option_string, T1* unrecognised)
{
    T3 ptr = option_string, cur;
    T2 ret = (T2)0;
    T2 tmp = T2::INVALID;

    if (unrecognised)
        *unrecognised = NULL;

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
                LogOverlayW(LOG_WARNING, L"WARNING: Unknown option: %s\n", cur);
                ret |= T2::INVALID;
            }
        }
    }
    return ret;
}

// Two template argument version is the typical case for now. We probably want
// to start adding the 'const' modifier in a bunch of places as we work towards
// migrating to C++ strings, since .c_str() always returns a const string.
// Since the parse_enum_option_string currently modified one of its inputs, it
// cannot use const, so the three argument template version above is to allow
// both const and non-const types passed in.
template <class T1, class T2>
static T2 parse_enum_option_string(struct EnumName_t<T1, T2>* enum_names, T1 option_string, T1* unrecognised)
{
    return parse_enum_option_string<T1, T2, T1>(enum_names, option_string, unrecognised);
}

// This is similar to the above, but stops parsing when it hits an unrecognised
// keyword and returns the position without throwing any errors. It also
// doesn't modify the option_string, allowing it to be used with C++ strings.
template <class T1, class T2>
static T2 parse_enum_option_string_prefix(struct EnumName_t<T1, T2>* enum_names, T1 option_string, T1* unrecognised)
{
    T1 ptr = option_string, cur;
    T2 ret = (T2)0;
    T2 tmp = T2::INVALID;
    size_t len;

    if (unrecognised)
        *unrecognised = NULL;

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

// -----------------------------------------------------------------------------------------------

#if MIGOTO_DX == 11
const char* tex_format_str(unsigned int format);

DXGI_FORMAT parse_format_string(const char* fmt, bool allow_numeric_format);
DXGI_FORMAT parse_format_string(const wchar_t* wfmt, bool allow_numeric_format);

// From DirectXTK with extra formats added
DXGI_FORMAT ensure_not_typeless(DXGI_FORMAT fmt);

// Is there already a utility function that does this?
UINT dxgi_format_size(DXGI_FORMAT format);

const char* type_name(IUnknown* object);
#endif  // MIGOTO_DX == 11

#if MIGOTO_DX == 9
const char* type_name_dx9(IUnknown* object);
#endif  // MIGOTO_DX == 9

// -----------------------------------------------------------------------------------------------

// Common routine to handle disassembling binary shaders to asm text.
// This is used whenever we need the Asm text.

std::string binary_to_asm_text(const void* shader_bytecode, size_t bytecode_length, bool patch_cb_offsets, bool disassemble_undecipherable_data = true, int hexdump = 0, bool d3dcompiler_46_compat = true);

std::string get_shader_model(const void* shader_bytecode, size_t bytecode_length);

// Create a text file containing text for the string specified.  Can be Asm or HLSL.
// If the file already exists and the caller did not specify overwrite (used
// for reassembled text), return that as an error to avoid overwriting previous
// work.

HRESULT create_text_file(wchar_t* full_path, std::string* asm_text, bool overwrite);

// Get shader type from asm, first non-commented line.  CS, PS, VS.
// Not sure this works on weird Unity variant with embedded types.

HRESULT create_asm_text_file(wchar_t* file_directory, UINT64 hash, const wchar_t* shader_type, const void* shader_bytecode, size_t bytecode_length, bool patch_cb_offsets);

// Specific variant to name files, so we know they are HLSL text.

HRESULT create_hlsl_text_file(UINT64 hash, std::string hlsl_text);

// -----------------------------------------------------------------------------------------------

// Parses the name of one of the IniParam constants: x, y, z, w, x1, y1, ..., z7, w7
bool parse_ini_param_name(const wchar_t* name, int* idx, float DirectX::XMFLOAT4::** component);

// -----------------------------------------------------------------------------------------------

BOOL create_directory_ensuring_access(LPCWSTR path);
errno_t wfopen_ensuring_access(FILE** pFile, const wchar_t* filename, const wchar_t* mode);
void set_file_last_write_time(wchar_t* path, FILETIME* ft_write, DWORD flags = 0);
void touch_file(wchar_t* path, DWORD flags = 0);
void touch_dir(wchar_t* path);

bool check_interface_supported(IUnknown* unknown, REFIID riid);
void analyse_iunknown(IUnknown* unknown);

// TODO: For the time being, since we are not setup to use the Win10 SDK, we'll add
// these manually. Some games under Win10 are requesting these.

struct _declspec(uuid("9d06dffa-d1e5-4d07-83a8-1bb123f2f841")) ID3D11Device2;
struct _declspec(uuid("420d5b32-b90c-4da4-bef0-359f6a24a83a")) ID3D11DeviceContext2;
struct _declspec(uuid("A8BE2AC4-199F-4946-B331-79599FB98DE7")) IDXGISwapChain2;
struct _declspec(uuid("94D99BDB-F1F8-4AB0-B236-7DA0170EDAB1")) IDXGISwapChain3;
struct _declspec(uuid("3D585D5A-BD4A-489E-B1F4-3DBCB6452FFB")) IDXGISwapChain4;

std::string name_from_IID(IID id);

void warn_if_conflicting_shader_exists(wchar_t* orig_path, const char* message = "");
static const char* end_user_conflicting_shader_msg =
    "Conflicting shaders present - please use uninstall.bat and reinstall the fix.\n";

struct OMState
{
    UINT NumRTVs;
#if MIGOTO_DX == 9
    std::vector<IDirect3DSurface9*> rtvs;
    IDirect3DSurface9* dsv;
#elif MIGOTO_DX == 11
    ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
    ID3D11DepthStencilView* dsv;
    UINT UAVStartSlot;
    UINT NumUAVs;
    ID3D11UnorderedAccessView* uavs[D3D11_PS_CS_UAV_REGISTER_COUNT];
#endif  // MIGOTO_DX
};

// TODO: Could use DX version specific typedefs for these differences
#if MIGOTO_DX == 9
void save_om_state(IDirect3DDevice9* device, struct OMState* state);
void restore_om_state(IDirect3DDevice9* device, struct OMState* state);
#elif MIGOTO_DX == 11
void save_om_state(ID3D11DeviceContext* context, struct OMState* state);
void restore_om_state(ID3D11DeviceContext* context, struct OMState* state);
#endif  // MIGOTO_DX

// -----------------------------------------------------------------------------------------------

#if MIGOTO_DX == 9
char* tex_format_str_dx9(D3DFORMAT format);

D3DFORMAT parse_format_string_dx9(const char* fmt, bool allow_numeric_format);

D3DFORMAT parse_format_string_dx9(const wchar_t* wfmt, bool allow_numeric_format);
inline size_t bits_per_pixel(_In_ D3DFORMAT fmt);
UINT d3d_format_bytes(D3DFORMAT format);
UINT byte_size_from_d3d_type(D3DDECLTYPE type);

DWORD decl_type_to_FVF(D3DDECLTYPE type, D3DDECLUSAGE usage, BYTE usage_index, int n_weights);

D3DDECLTYPE d3d_format_to_decl_type(D3DFORMAT format);

UINT strideForFVF(DWORD FVF);
UINT draw_vertices_count_to_primitive_count(UINT ver_count, D3DPRIMITIVETYPE prim_type);
UINT draw_primitive_count_to_vertices_count(UINT ver_count, D3DPRIMITIVETYPE prim_type);
#endif  // MIGOTO_DX == 9

#if MIGOTO_DX == 11
extern IDXGISwapChain* last_fullscreen_swap_chain;
#endif  // MIGOTO_DX == 11

void install_crash_handler(int level);
