#pragma once

#include <d3d11_1.h>
#include <DirectXMath.h>
#include <dxgi1_2.h>
#include <string>
#include <Windows.h>

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
// Use the pretty lock debugging version if Lock.h is included first, otherwise
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
char*    right_strip_a(char* buf);
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

BOOL    create_directory_ensuring_access(LPCWSTR path);
errno_t wfopen_ensuring_access(FILE** file, const wchar_t* filename, const wchar_t* mode);
void    set_file_last_write_time(wchar_t* path, FILETIME* ft_write, DWORD flags = 0);
void    touch_file(wchar_t* path, DWORD flags = 0);
void    touch_dir(wchar_t* path);

void               warn_if_conflicting_shader_exists(wchar_t* orig_path, const char* message = "");
static const char* end_user_conflicting_shader_msg =
    "Conflicting shaders present - please use uninstall.bat and reinstall the fix.\n";

struct om_state
{
    UINT NumRTVs;
#if MIGOTO_DX == 9
    std::vector<IDirect3DSurface9*> rtvs;
    IDirect3DSurface9*              dsv;
#elif MIGOTO_DX == 11
    ID3D11RenderTargetView*    rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
    ID3D11DepthStencilView*    dsv;
    UINT                       UAVStartSlot;
    UINT                       NumUAVs;
    ID3D11UnorderedAccessView* uavs[D3D11_PS_CS_UAV_REGISTER_COUNT];
#endif  // MIGOTO_DX
};

// TODO: Could use DX version specific typedefs for these differences
#if MIGOTO_DX == 9
void save_om_state(IDirect3DDevice9* device, struct om_state* state);
void restore_om_state(IDirect3DDevice9* device, struct om_state* state);
#elif MIGOTO_DX == 11
void save_om_state(ID3D11DeviceContext* context, struct om_state* state);
void restore_om_state(ID3D11DeviceContext* context, struct om_state* state);
#endif  // MIGOTO_DX

// -----------------------------------------------------------------------------------------------

#if MIGOTO_DX == 9
char* tex_format_str_dx9(D3DFORMAT format);

D3DFORMAT parse_format_string_dx9(const char* fmt, bool allow_numeric_format);

D3DFORMAT     parse_format_string_dx9(const wchar_t* wfmt, bool allow_numeric_format);
inline size_t bits_per_pixel(_In_ D3DFORMAT fmt);
UINT          d3d_format_bytes(D3DFORMAT format);
UINT          byte_size_from_d3d_type(D3DDECLTYPE type);

DWORD decl_type_to_FVF(D3DDECLTYPE type, D3DDECLUSAGE usage, BYTE usage_index, int n_weights);

D3DDECLTYPE d3d_format_to_decl_type(D3DFORMAT format);

UINT stride_for_FVF(DWORD FVF);
UINT draw_vertices_count_to_primitive_count(UINT ver_count, D3DPRIMITIVETYPE prim_type);
UINT draw_primitive_count_to_vertices_count(UINT ver_count, D3DPRIMITIVETYPE prim_type);
#endif  // MIGOTO_DX == 9

#if MIGOTO_DX == 11
extern IDXGISwapChain* last_fullscreen_swap_chain;
#endif  // MIGOTO_DX == 11

void install_crash_handler(int level);
