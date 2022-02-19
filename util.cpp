#include "util.h"

#include <sddl.h>
#include <io.h>
#include <fcntl.h>
#include <Dbghelp.h>

#include "DirectX11\lock.h"

// FIXME: Move any dependencies from these headers into common:
#if MIGOTO_DX == 9
    #include "DirectX9\Overlay.hpp"
#elif MIGOTO_DX == 11
    #include "DirectX11\HackerDevice.hpp"
    #include "DirectX11\HackerContext.hpp"
#endif  // MIGOTO_DX

// Sometimes game directories get funny permissions that cause us problems. I
// have no clue how or why this happens, and the usual way to deal with it is
// to recursively reset the permissions and ownership on the game directory
// with something like:
//
//     takeown /F <path> /R
//     icacls <path> /T /Q /C /RESET
//
// But, I'd like to see if we can do better and handle this from 3DMigoto to
// ensure that we always have access to the files and directories we create if
// at all possible. I don't fully understand windows filesystem permissions,
// but then I doubt many people really and truly do - the ACL complexity is
// where this problem stems from after all (I would say give me UNIX
// permissions over this any day, but then some masochist went and created
// SELinux so now we have a similar headache over there who's only saving grace
// is that we can turn it off), so this is partially (and possibly naively)
// based on this MSDN article:
//
//   https://msdn.microsoft.com/en-us/library/windows/desktop/ms717798(v=vs.85).aspx
//

static SECURITY_ATTRIBUTES* init_security_attributes(SECURITY_ATTRIBUTES* sa)
{
    sa->nLength = sizeof(SECURITY_ATTRIBUTES);
    sa->bInheritHandle = FALSE;
    sa->lpSecurityDescriptor = NULL;

    if (ConvertStringSecurityDescriptorToSecurityDescriptor(
            L"D:"  // Discretionary ACL
            // Removed string from MSDN that denies guests/anonymous users
            L"(A;OICI;GRGX;;;WD)"  // Give everyone read/execute access
            L"(A;OICI;GA;;;AU)"    // Allow full control to authenticated users (GRGWGX is not enough to delete contents?)
            // Using "CO" for Creator/Owner instead of "AU" seems ineffective
            L"(A;OICI;GA;;;BA)"  // Allow full control to administrators
            ,
            SDDL_REVISION_1, &sa->lpSecurityDescriptor, NULL))
    {
        return sa;
    }

    LOG_INFO("ConvertStringSecurityDescriptorToSecurityDescriptor failed\n");
    return NULL;
}


// Wrapped in try/catch because it can crash in Dirt Rally,
// because of noncontiguous or non-mapped memory for the texture.  Not sure this
// is the best strategy.

// Now switching to use crc32_append instead of fnv_64_buf for performance. This
// implementation of crc32c uses the SSE 4.2 instructions in the CPU to calculate,
// and is some 30x faster than fnv_64_buf.
//
// Not changing shader hash calculation as there are thousands of shaders already
// in the field, and there is no known bottleneck for that calculation.
uint32_t crc32c_hw(uint32_t seed, const void* buffer, size_t length)
{
    try
    {
        const uint8_t* cast_buffer = static_cast<const uint8_t*>(buffer);

        return crc32c_append(seed, cast_buffer, length);
    }
    catch (...)
    {
        // Fatal error, but catch it and return null for hash.
        LOG_INFO("   ******* Exception caught while calculating crc32c_hw hash ******\n");
        return 0;
    }
}

// Primary hash calculation for all shader file names.
UINT64 fnv_64_buf(const void* buf, size_t len)
{
    UINT64 hval = 0;
    unsigned const char* bp = (unsigned const char*)buf; /* start of buffer */
    unsigned const char* be = bp + len;                  /* beyond end of buffer */

    // FNV-1 hash each octet of the buffer
    while (bp < be)
    {
        // multiply by the 64 bit FNV magic prime mod 2^64 */
        hval *= FNV_64_PRIME;
        // xor the bottom with the current octet
        hval ^= (UINT64)*bp++;
    }
    return hval;
}

// Strip spaces from the right of a string.
// Returns a pointer to the last non-NULL character of the truncated string.
char* right_strip_a(char* buf)
{
    char* end = buf + strlen(buf) - 1;
    while (end > buf && isspace(*end))
        end--;
    *(end + 1) = 0;
    return end;
}

wchar_t* right_strip_w(wchar_t* buf)
{
    wchar_t* end = buf + wcslen(buf) - 1;
    while (end > buf && iswspace(*end))
        end--;
    *(end + 1) = 0;
    return end;
}

char* read_string_parameter(wchar_t* val)
{
    static char buf[MAX_PATH];
    wcstombs(buf, val, MAX_PATH);
    right_strip_a(buf);
    char* start = buf;
    while (isspace(*start))
        start++;
    return start;
}

void beep_success()
{
    // High beep for success
    Beep(1800, 400);
}

void beep_short()
{
    // Short High beep
    Beep(1800, 100);
}

void beep_failure()
{
    // Bonk sound for failure.
    Beep(200, 150);
}

void beep_sad_failure()
{
    // Brnk, dunk sound for failure.
    Beep(300, 200);
    Beep(200, 150);
}

void beep_profile_fail()
{
    // Brnk, du-du-dunk sound to signify the profile failed to install.
    // This is more likely to hit than the others for an end user (e.g. if
    // they denied admin privileges), so use a unique tone to make it
    // easier to identify.
    Beep(300, 300);
    Beep(200, 100);
    Beep(200, 100);
    Beep(200, 200);
}

void double_beep_exit()
{
    // Fatal error somewhere, known to crash, might as well exit cleanly
    // with some notification.
    beep_sad_failure();
    Sleep(500);
    beep_sad_failure();
    Sleep(200);
    if (LogFile)
    {
        // Make sure the log is written out so we see the failure message
        fclose(LogFile);
        LogFile = 0;
    }
    ExitProcess(0xc0000135);
}

int _autoicmp(const wchar_t* s1, const wchar_t* s2)
{
    return _wcsicmp(s1, s2);
}

int _autoicmp(const char* s1, const char* s2)
{
    return _stricmp(s1, s2);
}

// -----------------------------------------------------------------------------------------------

// http://msdn.microsoft.com/en-us/library/windows/desktop/bb173059(v=vs.85).aspx
static char* DXGIFormats[] = {
    "UNKNOWN",
    "R32G32B32A32_TYPELESS",
    "R32G32B32A32_FLOAT",
    "R32G32B32A32_UINT",
    "R32G32B32A32_SINT",
    "R32G32B32_TYPELESS",
    "R32G32B32_FLOAT",
    "R32G32B32_UINT",
    "R32G32B32_SINT",
    "R16G16B16A16_TYPELESS",
    "R16G16B16A16_FLOAT",
    "R16G16B16A16_UNORM",
    "R16G16B16A16_UINT",
    "R16G16B16A16_SNORM",
    "R16G16B16A16_SINT",
    "R32G32_TYPELESS",
    "R32G32_FLOAT",
    "R32G32_UINT",
    "R32G32_SINT",
    "R32G8X24_TYPELESS",
    "D32_FLOAT_S8X24_UINT",
    "R32_FLOAT_X8X24_TYPELESS",
    "X32_TYPELESS_G8X24_UINT",
    "R10G10B10A2_TYPELESS",
    "R10G10B10A2_UNORM",
    "R10G10B10A2_UINT",
    "R11G11B10_FLOAT",
    "R8G8B8A8_TYPELESS",
    "R8G8B8A8_UNORM",
    "R8G8B8A8_UNORM_SRGB",
    "R8G8B8A8_UINT",
    "R8G8B8A8_SNORM",
    "R8G8B8A8_SINT",
    "R16G16_TYPELESS",
    "R16G16_FLOAT",
    "R16G16_UNORM",
    "R16G16_UINT",
    "R16G16_SNORM",
    "R16G16_SINT",
    "R32_TYPELESS",
    "D32_FLOAT",
    "R32_FLOAT",
    "R32_UINT",
    "R32_SINT",
    "R24G8_TYPELESS",
    "D24_UNORM_S8_UINT",
    "R24_UNORM_X8_TYPELESS",
    "X24_TYPELESS_G8_UINT",
    "R8G8_TYPELESS",
    "R8G8_UNORM",
    "R8G8_UINT",
    "R8G8_SNORM",
    "R8G8_SINT",
    "R16_TYPELESS",
    "R16_FLOAT",
    "D16_UNORM",
    "R16_UNORM",
    "R16_UINT",
    "R16_SNORM",
    "R16_SINT",
    "R8_TYPELESS",
    "R8_UNORM",
    "R8_UINT",
    "R8_SNORM",
    "R8_SINT",
    "A8_UNORM",
    "R1_UNORM",
    "R9G9B9E5_SHAREDEXP",
    "R8G8_B8G8_UNORM",
    "G8R8_G8B8_UNORM",
    "BC1_TYPELESS",
    "BC1_UNORM",
    "BC1_UNORM_SRGB",
    "BC2_TYPELESS",
    "BC2_UNORM",
    "BC2_UNORM_SRGB",
    "BC3_TYPELESS",
    "BC3_UNORM",
    "BC3_UNORM_SRGB",
    "BC4_TYPELESS",
    "BC4_UNORM",
    "BC4_SNORM",
    "BC5_TYPELESS",
    "BC5_UNORM",
    "BC5_SNORM",
    "B5G6R5_UNORM",
    "B5G5R5A1_UNORM",
    "B8G8R8A8_UNORM",
    "B8G8R8X8_UNORM",
    "R10G10B10_XR_BIAS_A2_UNORM",
    "B8G8R8A8_TYPELESS",
    "B8G8R8A8_UNORM_SRGB",
    "B8G8R8X8_TYPELESS",
    "B8G8R8X8_UNORM_SRGB",
    "BC6H_TYPELESS",
    "BC6H_UF16",
    "BC6H_SF16",
    "BC7_TYPELESS",
    "BC7_UNORM",
    "BC7_UNORM_SRGB",
    "AYUV",
    "Y410",
    "Y416",
    "NV12",
    "P010",
    "P016",
    "420_OPAQUE",
    "YUY2",
    "Y210",
    "Y216",
    "NV11",
    "AI44",
    "IA44",
    "P8",
    "A8P8",
    "B4G4R4A4_UNORM"
};

char* tex_format_str(unsigned int format)
{
    if (format < sizeof(DXGIFormats) / sizeof(DXGIFormats[0]))
        return DXGIFormats[format];
    return "UNKNOWN";
}

DXGI_FORMAT parse_format_string(const char* fmt, bool allow_numeric_format)
{
    size_t num_formats = sizeof(DXGIFormats) / sizeof(DXGIFormats[0]);
    unsigned format;
    int nargs, end;

    if (allow_numeric_format)
    {
        // Try parsing format string as decimal:
        nargs = sscanf_s(fmt, "%u%n", &format, &end);
        if (nargs == 1 && end == strlen(fmt))
            return (DXGI_FORMAT)format;
    }

    if (!_strnicmp(fmt, "DXGI_FORMAT_", 12))
        fmt += 12;

    // Look up format string:
    for (format = 0; format < num_formats; format++)
    {
        if (!_strnicmp(fmt, DXGIFormats[format], 30))
            return (DXGI_FORMAT)format;
    }

    // UNKNOWN/0 is a valid format (e.g. for structured buffers), so return
    // -1 cast to a DXGI_FORMAT to signify an error:
    return (DXGI_FORMAT)-1;
}

DXGI_FORMAT parse_format_string(const wchar_t* wfmt, bool allow_numeric_format)
{
    char afmt[42];

    wcstombs(afmt, wfmt, 42);
    afmt[41] = '\0';

    return parse_format_string(afmt, allow_numeric_format);
}

// From DirectXTK with extra formats added
DXGI_FORMAT ensure_not_typeless(DXGI_FORMAT fmt)
{
    // Assumes UNORM or FLOAT; doesn't use UINT or SINT
    switch (fmt)
    {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case DXGI_FORMAT_R32G32B32_TYPELESS:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
            return DXGI_FORMAT_R16G16B16A16_UNORM;
        case DXGI_FORMAT_R32G32_TYPELESS:
            return DXGI_FORMAT_R32G32_FLOAT;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
            return DXGI_FORMAT_R10G10B10A2_UNORM;
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_R16G16_TYPELESS:
            return DXGI_FORMAT_R16G16_UNORM;
        case DXGI_FORMAT_R32_TYPELESS:
            return DXGI_FORMAT_R32_FLOAT;
        case DXGI_FORMAT_R8G8_TYPELESS:
            return DXGI_FORMAT_R8G8_UNORM;
        case DXGI_FORMAT_R16_TYPELESS:
            return DXGI_FORMAT_R16_UNORM;
        case DXGI_FORMAT_R8_TYPELESS:
            return DXGI_FORMAT_R8_UNORM;
        case DXGI_FORMAT_BC1_TYPELESS:
            return DXGI_FORMAT_BC1_UNORM;
        case DXGI_FORMAT_BC2_TYPELESS:
            return DXGI_FORMAT_BC2_UNORM;
        case DXGI_FORMAT_BC3_TYPELESS:
            return DXGI_FORMAT_BC3_UNORM;
        case DXGI_FORMAT_BC4_TYPELESS:
            return DXGI_FORMAT_BC4_UNORM;
        case DXGI_FORMAT_BC5_TYPELESS:
            return DXGI_FORMAT_BC5_UNORM;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
            return DXGI_FORMAT_B8G8R8X8_UNORM;
        case DXGI_FORMAT_BC7_TYPELESS:
            return DXGI_FORMAT_BC7_UNORM;
            // Extra depth/stencil buffer formats not covered in DirectXTK (discards
            // stencil buffer to allow binding to a shader resource, alternatively we could
            // discard the depth buffer if we ever needed the stencil buffer):
        case DXGI_FORMAT_R32G8X24_TYPELESS:
            return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        case DXGI_FORMAT_R24G8_TYPELESS:
            return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        default:
            return fmt;
    }
}

// Is there already a utility function that does this?
UINT dxgi_format_size(DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return 16;
        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
            return 12;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
            return 8;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
            return 4;
        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_SINT:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_B5G6R5_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
            return 2;
        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
            return 1;
        default:
            return 0;
    }
}

const char* type_name(IUnknown* object)
{
    ID3D11Device1* device;
    ID3D11DeviceContext1* context;

    // Seems that not even try / catch is safe in all cases of this
    // (grumble grumble poorly designed grumble...). The only cases where
    // we should be called on an object without type information is while
    // hooking the device and/or context, so check if it is one of those
    // cases:

    device = lookup_hooked_device((ID3D11Device1*)object);
    if (device)
        return "Hooked_ID3D11Device";
    context = lookup_hooked_context((ID3D11DeviceContext1*)object);
    if (context)
        return "Hooked_ID3D11DeviceContext";

    try
    {
        return typeid(*object).name();
    }
    catch (std::__non_rtti_object)
    {
        return "<NO_RTTI>";
    }
    catch (std::bad_typeid)
    {
        return "<NULL>";
    }
}

#if MIGOTO_DX == 9
const char* type_name_dx9(IUnknown* object)
{
    IDirect3DDevice9* device;

    // Seems that not even try / catch is safe in all cases of this
    // (grumble grumble poorly designed grumble...). The only cases where
    // we should be called on an object without type information is while
    // hooking the device and/or context, so check if it is one of those
    // cases:

    device = lookup_hooked_device_dx9((IDirect3DDevice9Ex*)object);
    if (device)
        return "Hooked_IDirect3DDevice9";

    try
    {
        return typeid(*object).name();
    }
    catch (std::__non_rtti_object)
    {
        return "<NO_RTTI>";
    }
    catch (std::bad_typeid)
    {
        return "<NULL>";
    }
}
#endif  // MIGOTO_DX == 9

// Common routine to handle disassembling binary shaders to asm text.
// This is used whenever we need the Asm text.

// New version using Flugan's wrapper around D3DDisassemble to replace the
// problematic %f floating point values with %.9e, which is enough that a 32bit
// floating point value will be reproduced exactly:
std::string binary_to_asm_text(const void* shader_bytecode, size_t bytecode_length, bool patch_cb_offsets, bool disassemble_undecipherable_data, int hexdump, bool d3dcompiler_46_compat)
{
    std::string comments;
    std::vector<byte> byte_code(bytecode_length);
    std::vector<byte> disassembly;
    HRESULT r;

    comments = "//   using 3Dmigoto v" + std::string(VER_FILE_VERSION_STR) + " on " + log_time() + "//\n";
    memcpy(byte_code.data(), shader_bytecode, bytecode_length);

    #if MIGOTO_DX == 9
    r = disassemblerDX9(&byte_code, &disassembly, comments.c_str());
    #elif MIGOTO_DX == 11
    r = disassembler(&byte_code, &disassembly, comments.c_str(), hexdump, d3dcompiler_46_compat, disassemble_undecipherable_data, patch_cb_offsets);
    #endif  // MIGOTO_DX
    if (FAILED(r))
    {
        LOG_INFO("  disassembly failed. Error: %x\n", r);
        return "";
    }

    return std::string(disassembly.begin(), disassembly.end());
}

// Get the shader model from the binary shader bytecode.
//
// This used to disassemble, then search for the text string, but if we are going to
// do all that work, we might as well use the James-Jones decoder to get it.
// The other reason to do it this way is that we have seen multiple shader versions
// in Unity games, and the old technique of searching for the first uncommented line
// would fail.
std::string get_shader_model(const void* shader_bytecode, size_t bytecode_length)
{
    std::string asm_text = binary_to_asm_text(shader_bytecode, bytecode_length, false);
    if (asm_text.empty())
        return "";

    // Read shader model. This is the first not commented line.
    char* pos = (char*)asm_text.data();
    char* end = pos + asm_text.size();
    while ((pos[0] == '/' || pos[0] == '\n') && pos < end)
    {
        while (pos[0] != 0x0a && pos < end)
            pos++;
        pos++;
    }
    // Extract model.
    char* eol = pos;
    while (eol[0] != 0x0a && pos < end)
        eol++;
    std::string shader_model(pos, eol);

    return shader_model;
}


// This is an interesting idea, but doesn't work well here because of project structure.
// for the moment, let's leave this here, but use the disassemble search approach.

// static string GetShaderModel(const void *shader_bytecode)
//{
//     Shader *shader = DecodeDXBC((uint32_t*)shader_bytecode);
//     if (shader == nullptr)
//         return "";
//
//     string shaderModel;
//
//     switch (shader->eShaderType)
//     {
//     case PIXEL_SHADER:
//         shaderModel = "ps";
//         break;
//     case VERTEX_SHADER:
//         shaderModel = "vs";
//         break;
//     case GEOMETRY_SHADER:
//         shaderModel = "gs";
//         break;
//     case HULL_SHADER:
//         shaderModel = "hs";
//         break;
//     case DOMAIN_SHADER:
//         shaderModel = "ds";
//         break;
//     case COMPUTE_SHADER:
//         shaderModel = "cs";
//         break;
//     default:
//         return "";        // Failure.
//     }
//
//     shaderModel += "_" + shader->ui32MajorVersion;
//     shaderModel += "_" + shader->ui32MinorVersion;
//
//     delete shader;
//
//     return shaderModel;
// }

// Create a text file containing text for the string specified.  Can be Asm or HLSL.
// If the file already exists and the caller did not specify overwrite (used
// for reassembled text), return that as an error to avoid overwriting previous
// work.

// We previously would overwrite the file only after checking if the contents were different,
// this relaxes that to just being same file name.
HRESULT create_text_file(wchar_t* full_path, std::string* asm_text, bool overwrite)
{
    FILE* f;

    if (!overwrite)
    {
        _wfopen_s(&f, full_path, L"rb");
        if (f)
        {
            fclose(f);
            LOG_INFO_W(L"    CreateTextFile error: file already exists %s\n", full_path);
            return ERROR_FILE_EXISTS;
        }
    }

    _wfopen_s(&f, full_path, L"wb");
    if (f)
    {
        fwrite(asm_text->data(), 1, asm_text->size(), f);
        fclose(f);
    }

    return S_OK;
}

// Get shader type from asm, first non-commented line.  CS, PS, VS.
// Not sure this works on weird Unity variant with embedded types.

// Specific variant to name files consistently, so we know they are Asm text.
HRESULT create_asm_text_file(wchar_t* file_directory, UINT64 hash, const wchar_t* shader_type, const void* shader_bytecode, size_t bytecode_length, bool patch_cb_offsets)
{
    std::string asm_text = binary_to_asm_text(shader_bytecode, bytecode_length, patch_cb_offsets);
    if (asm_text.empty())
        return E_OUTOFMEMORY;

    wchar_t full_path[MAX_PATH];
    swprintf_s(full_path, MAX_PATH, L"%ls\\%016llx-%ls.txt", file_directory, hash, shader_type);

    HRESULT hr = create_text_file(full_path, &asm_text, false);

    if (SUCCEEDED(hr))
        LOG_INFO_W(L"    storing disassembly to %s\n", full_path);
    else
        LOG_INFO_W(L"    error: %x, storing disassembly to %s\n", hr, full_path);

    return hr;
}

HRESULT create_hlsl_text_file(UINT64 hash, std::string hlsl_text)
{
    return 0;
}

// -----------------------------------------------------------------------------------------------

// Parses the name of one of the IniParam constants: x, y, z, w, x1, y1, ..., z7, w7
bool parse_ini_param_name(const wchar_t* name, int* idx, float DirectX::XMFLOAT4::** component)
{
    int ret, len1, len2;
    wchar_t component_chr;
    size_t length = wcslen(name);

    ret = swscanf_s(name, L"%lc%n%u%n", &component_chr, 1, &len1, idx, &len2);

    // May or may not have matched index. Make sure entire string was
    // matched either way and check index is valid if it was matched:
    if (ret == 1 && len1 == length)
    {
        *idx = 0;
    }
    else if (ret == 2 && len2 == length)
    {
#if MIGOTO_DX == 9
        // Added gating for this DX9 specific limitation that we definitely do
        // not want to enforce in DX11 as that would break a bunch of mods -DSS
        if (*idx >= 225)
            return false;
#endif  // MIGOTO_DX == 9
    }
    else
    {
        return false;
    }

    switch (towlower(component_chr))
    {
        case L'x':
            *component = &DirectX::XMFLOAT4::x;
            return true;
        case L'y':
            *component = &DirectX::XMFLOAT4::y;
            return true;
        case L'z':
            *component = &DirectX::XMFLOAT4::z;
            return true;
        case L'w':
            *component = &DirectX::XMFLOAT4::w;
            return true;
    }

    return false;
}

// -----------------------------------------------------------------------------------------------

BOOL create_directory_ensuring_access(LPCWSTR path)
{
    SECURITY_ATTRIBUTES sa, *psa = NULL;
    BOOL ret = false;

    psa = init_security_attributes(&sa);

    ret = CreateDirectory(path, psa);

    LocalFree(sa.lpSecurityDescriptor);

    return ret;
}

// Replacement for _wfopen_s that ensures the permissions will be set so we can
// read it back later.
errno_t wfopen_ensuring_access(FILE** pFile, const wchar_t* filename, const wchar_t* mode)
{
    SECURITY_ATTRIBUTES sa, *psa = NULL;
    HANDLE fh = NULL;
    int fd = -1;
    FILE* fp = NULL;
    int osf_flags = 0;

    *pFile = NULL;

    if (wcsstr(mode, L"w") == NULL)
    {
        // This function is for creating new files for now. We could
        // make it do some heroics on read/append as well, but I don't
        // want to push this further than we need to.
        LOG_INFO("FIXME: wfopen_ensuring_access only supports opening for write\n");
        double_beep_exit();
    }

    if (wcsstr(mode, L"b") == NULL)
        osf_flags |= _O_TEXT;

    // We use _wfopen_s so that we can use formatted print routines, but to
    // set security attributes at creation time to make sure the
    // permissions give us read access we need to use CreateFile, and
    // convert the resulting handle into a C file descriptor, then a FILE*
    // that can be used as per usual.
    psa = init_security_attributes(&sa);
    fh = CreateFile(filename, GENERIC_WRITE, 0, psa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    LocalFree(sa.lpSecurityDescriptor);
    if (fh == INVALID_HANDLE_VALUE)
    {
        // FIXME: Map GetLastError() to appropriate errno
        return EIO;
    }

    // Convert the HANDLE into a file descriptor.
    fd = _open_osfhandle((intptr_t)fh, osf_flags);
    if (fd == -1)
    {
        CloseHandle(fh);
        return EIO;
    }

    // From this point on, we do not use CloseHandle(fh), as it will be
    // implicitly closed with close(fd)

    // Convert the file descriptor into a file pointer.
    fp = _wfdopen(fd, mode);
    if (!fp)
    {
        _close(fd);
        return EIO;
    }

    // From this point on, we do not use CloseHandle(fh) or close(fd) as it
    // will be implicitly closed with fclose(fp). Convenient for us,
    // because it means the caller doesn't have to care about the fh or fd.

    *pFile = fp;
    return 0;
}

void set_file_last_write_time(wchar_t* path, FILETIME* ft_write, DWORD flags)
{
    HANDLE f;

    f = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | flags, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;

    SetFileTime(f, NULL, NULL, ft_write);
    CloseHandle(f);
}

void touch_file(wchar_t* path, DWORD flags)
{
    FILETIME ft;
    SYSTEMTIME st;

    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);
    set_file_last_write_time(path, &ft, flags);
}

void touch_dir(wchar_t* path)
{
    touch_file(path, FILE_FLAG_BACKUP_SEMANTICS);
}

// -----------------------------------------------------------------------------------------------
// When logging, it's not very helpful to have long sequences of hex instead of
// the actual names of the objects in question.
// e.g.
// DEFINE_GUID(IID_IDXGIFactory,0x7b7166ec,0x21c7,0x44ae,0xb2,0x1a,0xc9,0xae,0x32,0x1a,0xe3,0x69);
//

std::string name_from_IID(IID id)
{
    // Adding every MIDL_INTERFACE from d3d11_1.h to make this reporting complete.
    // Doesn't seem useful to do every object from d3d11.h itself.

    if (__uuidof(IUnknown) == id)
        return "IUnknown";

        // FIXME: We should probably have these IIDs defined regardless of target
        // to catch potential cases where multiple versions of 3DMigoto are
        // coexisting and the devices get mixed up
#if MIGOTO_DX == 11
    if (IID_HackerDevice == id)
        return "HackerDevice";
    if (IID_HackerContext == id)
        return "HackerContext";
#elif MIGOTO_DX == 9
        // FIXME: DX9 GUIDs are not using the correct macros, and need verification
        // that they haven't been copy + pasted
        // if (IID_D3D9Wrapper_IDirect3DDevice9 == id)
        //    return "3DMigotoDevice9";
#endif

#ifdef _D3D9_H_
    if (__uuidof(IDirect3DDevice9) == id)
        return "IDirect3DDevice9";
#endif  // _D3D9_H_

#ifdef __d3d10_h__
    if (__uuidof(ID3D10Multithread) == id)
        return "ID3D10Multithread";
    if (__uuidof(ID3D10Device) == id)
        return "ID3D10Device";
#endif  // __d3d10_h__

#ifdef __d3d11_h__
    if (__uuidof(ID3D11Device) == id)
        return "ID3D11Device";
    if (__uuidof(ID3D11DeviceContext) == id)
        return "ID3D11DeviceContext";
    if (__uuidof(ID3D11DeviceChild) == id)
        return "ID3D11DeviceChild";
    if (__uuidof(ID3D11BlendState) == id)
        return "ID3D11BlendState";
    if (__uuidof(ID3D11RasterizerState) == id)
        return "ID3D11RasterizerState";
    if (__uuidof(ID3D11Texture2D) == id)  // Used to fetch backbuffer
        return "ID3D11Texture2D";
#endif  // __d3d11_h__

#ifdef __d3d11_1_h__
    if (__uuidof(ID3D11BlendState1) == id)
        return "ID3D11BlendState1";
    if (__uuidof(ID3D11Device1) == id)
        return "ID3D11Device1";
    if (__uuidof(ID3D11DeviceContext1) == id)
        return "ID3D11DeviceContext1";
    if (__uuidof(ID3D11RasterizerState1) == id)
        return "ID3D11RasterizerState1";
    if (__uuidof(ID3DDeviceContextState) == id)
        return "ID3DDeviceContextState";
    if (__uuidof(ID3DUserDefinedAnnotation) == id)
        return "ID3DUserDefinedAnnotation";
#endif  // __d3d11_1_h__

    // XXX: From newer Windows SDK than we are using. Defined in util.h for now
    if (__uuidof(ID3D11Device2) == id)  // d3d11_2.h when the time comes
        return "ID3D11Device2";
    if (__uuidof(ID3D11DeviceContext2) == id)  // d3d11_2.h when the time comes
        return "ID3D11DeviceContext2";

#ifdef __d3d11sdklayers_h__
    if (__uuidof(ID3D11InfoQueue) == id)
        return "ID3D11InfoQueue";
#endif

        // All the DXGI interfaces from dxgi.h, and dxgi1_2.h
#ifdef __dxgi_h__
    if (__uuidof(IDXGIAdapter) == id)
        return "IDXGIAdapter";
    if (__uuidof(IDXGIAdapter1) == id)
        return "IDXGIAdapter1";
    if (__uuidof(IDXGIDevice) == id)
        return "IDXGIDevice";
    if (__uuidof(IDXGIDevice1) == id)
        return "IDXGIDevice1";
    if (__uuidof(IDXGIDeviceSubObject) == id)
        return "IDXGIDeviceSubObject";
    if (__uuidof(IDXGIFactory) == id)
        return "IDXGIFactory";
    if (__uuidof(IDXGIFactory1) == id)
        return "IDXGIFactory1";
    if (__uuidof(IDXGIKeyedMutex) == id)
        return "IDXGIKeyedMutex";
    if (__uuidof(IDXGIObject) == id)
        return "IDXGIObject";
    if (__uuidof(IDXGIOutput) == id)
        return "IDXGIOutput";
    if (__uuidof(IDXGIResource) == id)
        return "IDXGIResource";
    if (__uuidof(IDXGISurface) == id)
        return "IDXGISurface";
    if (__uuidof(IDXGISurface1) == id)
        return "IDXGISurface1";
    if (__uuidof(IDXGISwapChain) == id)
        return "IDXGISwapChain";
#endif  // __dxgi_h__

#ifdef __dxgi1_2_h__
    if (__uuidof(IDXGIAdapter2) == id)
        return "IDXGIAdapter2";
    if (__uuidof(IDXGIDevice2) == id)
        return "IDXGIDevice2";
    if (__uuidof(IDXGIDisplayControl) == id)
        return "IDXGIDisplayControl";
    if (__uuidof(IDXGIFactory2) == id)
        return "IDXGIFactory2";
    if (__uuidof(IDXGIOutput1) == id)
        return "IDXGIOutput1";
    if (__uuidof(IDXGIOutputDuplication) == id)
        return "IDXGIOutputDuplication";
    if (__uuidof(IDXGIResource1) == id)
        return "IDXGIResource1";
    if (__uuidof(IDXGISurface2) == id)
        return "IDXGISurface2";
    if (__uuidof(IDXGISwapChain1) == id)
        return "IDXGISwapChain1";
#endif  // __dxgi1_2_h__

    // XXX: From newer Windows SDK than we are using. Defined in util.h for now
    if (__uuidof(IDXGISwapChain2) == id)  // dxgi1_3 A8BE2AC4-199F-4946-B331-79599FB98DE7
        return "IDXGISwapChain2";
    if (__uuidof(IDXGISwapChain3) == id)  // dxgi1_4 94D99BDB-F1F8-4AB0-B236-7DA0170EDAB1
        return "IDXGISwapChain3";
    if (__uuidof(IDXGISwapChain4) == id)  // dxgi1_5 3D585D5A-BD4A-489E-B1F4-3DBCB6452FFB
        return "IDXGISwapChain4";

    // For unknown IIDs lets return the hex string.
    // Converting from wchar_t to string using stackoverflow suggestion.

    std::string iid_string;
    wchar_t wiid[128];
    if (SUCCEEDED(StringFromGUID2(id, wiid, 128)))
    {
        std::wstring convert = std::wstring(wiid);
        iid_string = std::string(convert.begin(), convert.end());
    }
    else
    {
        iid_string = "unknown";
    }

    return iid_string;
}

static void warn_if_conflicting_file_exists(wchar_t* path, wchar_t* conflicting_path, const char* message)
{
    DWORD attrib = GetFileAttributes(conflicting_path);

    if (attrib == INVALID_FILE_ATTRIBUTES)
        return;

    LogOverlay(LOG_DIRE, "WARNING: %s\"%S\" conflicts with \"%S\"\n", message, conflicting_path, path);
}

void warn_if_conflicting_shader_exists(wchar_t* orig_path, const char* message)
{
    wchar_t conflicting_path[MAX_PATH], *postfix;

    wcscpy_s(conflicting_path, MAX_PATH, orig_path);

    // If we're using a HLSL shader, make sure there are no conflicting
    // assembly shaders, either text or binary:
    postfix = wcsstr(conflicting_path, L"_replace");
    if (postfix)
    {
        wcscpy_s(postfix, conflicting_path + MAX_PATH - postfix, L".txt");
        warn_if_conflicting_file_exists(orig_path, conflicting_path, message);
        wcscpy_s(postfix, conflicting_path + MAX_PATH - postfix, L".bin");
        warn_if_conflicting_file_exists(orig_path, conflicting_path, message);
        return;
    }

    // If we're using an assembly shader, make sure there are no
    // conflicting HLSL shaders, either text or binary:
    postfix = wcsstr(conflicting_path, L".");
    if (postfix)
    {
        wcscpy_s(postfix, conflicting_path + MAX_PATH - postfix, L"_replace.txt");
        warn_if_conflicting_file_exists(orig_path, conflicting_path, message);
        wcscpy_s(postfix, conflicting_path + MAX_PATH - postfix, L"_replace.bin");
        warn_if_conflicting_file_exists(orig_path, conflicting_path, message);
        return;
    }
}

#if MIGOTO_DX == 9
void save_om_state(IDirect3DDevice9* device, struct OMState* state)
{
    DWORD i;

    // OMGetRenderTargetAndUnorderedAccessViews is a poorly designed API as
    // to use it properly to get all RTVs and UAVs we need to pass it some
    // information that we don't know. So, we have to do a few extra steps
    // to find that info.
    D3DCAPS9 caps;
    device->GetDeviceCaps(&caps);
    if (state->rtvs.size() != caps.NumSimultaneousRTs)
        state->rtvs.resize(caps.NumSimultaneousRTs);
    state->NumRTVs = 0;
    for (i = 0; i < caps.NumSimultaneousRTs; i++)
    {
        IDirect3DSurface9* rt = NULL;
        device->GetRenderTarget(i, &rt);
        state->rtvs[i] = rt;
        if (rt)
        {
            state->NumRTVs = i + 1;
        }
    }
    device->GetDepthStencilSurface(&state->dsv);
}

void restore_om_state(IDirect3DDevice9* device, struct OMState* state)
{
    UINT i;
    for (i = 0; i < state->NumRTVs; i++)
    {
        device->SetRenderTarget(i, state->rtvs[i]);
        if (state->rtvs[i])
            state->rtvs[i]->Release();
    }
    device->SetDepthStencilSurface(state->dsv);
    if (state->dsv)
        state->dsv->Release();
}
#elif MIGOTO_DX == 11
void save_om_state(ID3D11DeviceContext* context, struct OMState* state)
{
    int i;

    // OMGetRenderTargetAndUnorderedAccessViews is a poorly designed API as
    // to use it properly to get all RTVs and UAVs we need to pass it some
    // information that we don't know. So, we have to do a few extra steps
    // to find that info.

    context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, state->rtvs, &state->dsv);

    state->NumRTVs = 0;
    for (i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    {
        if (state->rtvs[i])
            state->NumRTVs = i + 1;
    }

    state->UAVStartSlot = state->NumRTVs;
    // Set NumUAVs to the max to retrieve them all now, and so that later
    // when rebinding them we will unbind any others that the command list
    // bound in the meantime
    state->NumUAVs = D3D11_PS_CS_UAV_REGISTER_COUNT - state->UAVStartSlot;

    // Finally get all the UAVs. Since we already retrieved the RTVs and
    // DSV we can skip getting them:
    context->OMGetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, state->UAVStartSlot, state->NumUAVs, state->uavs);
}

void restore_om_state(ID3D11DeviceContext* context, struct OMState* state)
{
    static const UINT uav_counts[D3D11_PS_CS_UAV_REGISTER_COUNT] = { (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1 };
    UINT i;

    context->OMSetRenderTargetsAndUnorderedAccessViews(state->NumRTVs, state->rtvs, state->dsv, state->UAVStartSlot, state->NumUAVs, state->uavs, uav_counts);

    for (i = 0; i < state->NumRTVs; i++)
    {
        if (state->rtvs[i])
            state->rtvs[i]->Release();
    }

    if (state->dsv)
        state->dsv->Release();

    for (i = 0; i < state->NumUAVs; i++)
    {
        if (state->uavs[i])
            state->uavs[i]->Release();
    }
}

// -----------------------------------------------------------------------------------------------

static std::map<int, char*> d3d_formats = {
    { 0, "UNKNOWN" },
    { 20, "R8G8B8" },
    { 21, "A8R8G8B8" },
    { 22, "X8R8G8B8" },
    { 23, "R5G6B5" },
    { 24, "X1R5G5B5" },
    { 25, "A1R5G5B5" },
    { 26, "A4R4G4B4" },
    { 27, "R3G3B2" },
    { 28, "A8" },
    { 29, "A8R3G3B2" },
    { 30, "X4R4G4B4" },
    { 31, "A2B10G10R10" },
    { 32, "A8B8G8R8" },
    { 33, "X8B8G8R8" },
    { 34, "G16R16" },
    { 35, "A2R10G10B10" },
    { 36, "A16B16G16R16" },
    { 40, "A8P8" },
    { 41, "P8" },
    { 50, "L8" },
    { 51, "A8L8" },
    { 52, "A4L4" },
    { 60, "V8U8" },
    { 61, "L6V5U5" },
    { 62, "X8L8V8U8" },
    { 63, "Q8W8V8U8" },
    { 64, "V16U16" },
    { 67, "A2W10V10U10" },
    { 70, "D16_LOCKABLE" },
    { 71, "D32" },
    { 73, "D15S1" },
    { 75, "D24S8" },
    { 77, "D24X8" },
    { 79, "D24X4S4" },
    { 80, "D16" },
    { 82, "D32F_LOCKABLE" },
    { 83, "D24FS8" },
    { 84, "D32_LOCKABLE" },
    { 85, "S8_LOCKABLE" },
    { 81, "L16" },
    { 100, "VERTEXDATA" },
    { 101, "INDEX16" },
    { 102, "INDEX32" },
    { 110, "Q16W16V16U16" },
    { 111, "R16F" },
    { 112, "G16R16F" },
    { 113, "A16B16G16R16F" },
    { 114, "R32F" },
    { 115, "G32R32F" },
    { 116, "A32B32G32R32F" },
    { 117, "CxV8U8" },
    { 118, "A1" },
    { 119, "A2B10G10R10_XR_BIAS" },
    { 199, "BINARYBUFFER " }
};

char* tex_format_str_dx9(D3DFORMAT format)
{
    switch (format)
    {
        case MAKEFOURCC('U', 'Y', 'V', 'Y'):
            return "UYVY";
        case MAKEFOURCC('R', 'G', 'B', 'G'):
            return "R8G8_B8G8";
        case MAKEFOURCC('Y', 'U', 'Y', '2'):
            return "YUY2";
        case MAKEFOURCC('G', 'R', 'G', 'B'):
            return "G8R8_G8B8";
        case MAKEFOURCC('D', 'X', 'T', '1'):
            return "DXT1";
        case MAKEFOURCC('D', 'X', 'T', '2'):
            return "DXT2";
        case MAKEFOURCC('D', 'X', 'T', '3'):
            return "DXT3";
        case MAKEFOURCC('D', 'X', 'T', '4'):
            return "DXT4";
        case MAKEFOURCC('D', 'X', 'T', '5'):
            return "DXT5";
        case MAKEFOURCC('M', 'E', 'T', '1'):
            return "MULTI2_ARGB8";
        default:
            std::map<int, char*>::iterator it;
            it = d3d_formats.find(format);
            if (it != d3d_formats.end())
                return it->second;
            return "UNKNOWN";
    }
}

D3DFORMAT parse_format_string_dx9(const char* fmt, bool allow_numeric_format)
{
    size_t num_formats = d3d_formats.size();
    unsigned format;
    int nargs, end;

    if (allow_numeric_format)
    {
        // Try parsing format string as decimal:
        nargs = sscanf_s(fmt, "%u%n", &format, &end);
        if (nargs == 1 && end == strlen(fmt))
            return (D3DFORMAT)format;
    }

    if (!_strnicmp(fmt, "D3DFMT_", 7))
        fmt += 7;

    // Look up format string:
    std::map<int, char*>::iterator it;
    for (it = d3d_formats.begin(); it != d3d_formats.end(); it++)
    {
        if (!_strnicmp(fmt, it->second, 30))
            return (D3DFORMAT)it->first;
    }
    // UNKNOWN/0 is a valid format (e.g. for structured buffers), so return
    // -1 cast to a DXGI_FORMAT to signify an error:
    return (D3DFORMAT)-1;
}

D3DFORMAT parse_format_string_dx9(const wchar_t* wfmt, bool allow_numeric_format)
{
    char afmt[42];

    wcstombs(afmt, wfmt, 42);
    afmt[41] = '\0';

    return parse_format_string_dx9(afmt, allow_numeric_format);
}

inline size_t bits_per_pixel(D3DFORMAT fmt)
{
    switch (fmt)
    {
        case D3DFMT_A32B32G32R32F:
        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
            128;
        case D3DFMT_A16B16G16R16:
        case D3DFMT_Q16W16V16U16:
        case D3DFMT_A16B16G16R16F:
        case D3DFMT_G32R32F:
        case D3DFMT_DXT1:
            64;
        case D3DFMT_A2B10G10R10:
        case D3DFMT_A8B8G8R8:
        case D3DFMT_X8B8G8R8:
        case D3DFMT_G16R16:
        case D3DFMT_A2R10G10B10:
        case D3DFMT_V16U16:
        case D3DFMT_A2W10V10U10:
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
        case D3DFMT_X8L8V8U8:
        case D3DFMT_Q8W8V8U8:
        case D3DFMT_D32:
        case D3DFMT_D24S8:
        case D3DFMT_D24X8:
        case D3DFMT_D24X4S4:
        case D3DFMT_D32F_LOCKABLE:
        case D3DFMT_D24FS8:
        case D3DFMT_D32_LOCKABLE:
        case D3DFMT_INDEX32:
        case D3DFMT_G16R16F:
        case D3DFMT_R32F:
        case D3DFMT_A2B10G10R10_XR_BIAS:
        case D3DFMT_UYVY:
        case D3DFMT_YUY2:
            32;
        case D3DFMT_R8G8B8:
            24;
        case D3DFMT_R5G6B5:
        case D3DFMT_X1R5G5B5:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_A4R4G4B4:
        case D3DFMT_A8R3G3B2:
        case D3DFMT_X4R4G4B4:
        case D3DFMT_A8P8:
        case D3DFMT_A8L8:
        case D3DFMT_V8U8:
        case D3DFMT_L6V5U5:
        case D3DFMT_D16_LOCKABLE:
        case D3DFMT_D15S1:
        case D3DFMT_D16:
        case D3DFMT_L16:
        case D3DFMT_INDEX16:
        case D3DFMT_R16F:
        case D3DFMT_CxV8U8:
        case D3DFMT_R8G8_B8G8:
        case D3DFMT_G8R8_G8B8:
            16;
        case D3DFMT_R3G3B2:
        case D3DFMT_A8:
        case D3DFMT_P8:
        case D3DFMT_L8:
        case D3DFMT_A4L4:
        case D3DFMT_S8_LOCKABLE:
            8;
        default:
            return 0;
    }
}

UINT d3d_format_bytes(D3DFORMAT format)
{
    switch (format)
    {
        case D3DFMT_A32B32G32R32F:
        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
            16;
        case D3DFMT_A16B16G16R16:
        case D3DFMT_Q16W16V16U16:
        case D3DFMT_A16B16G16R16F:
        case D3DFMT_G32R32F:
        case D3DFMT_DXT1:
            8;
        case D3DFMT_A2B10G10R10:
        case D3DFMT_A8B8G8R8:
        case D3DFMT_X8B8G8R8:
        case D3DFMT_G16R16:
        case D3DFMT_A2R10G10B10:
        case D3DFMT_V16U16:
        case D3DFMT_A2W10V10U10:
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
        case D3DFMT_X8L8V8U8:
        case D3DFMT_Q8W8V8U8:
        case D3DFMT_D32:
        case D3DFMT_D24S8:
        case D3DFMT_D24X8:
        case D3DFMT_D24X4S4:
        case D3DFMT_D32F_LOCKABLE:
        case D3DFMT_D24FS8:
        case D3DFMT_D32_LOCKABLE:
        case D3DFMT_INDEX32:
        case D3DFMT_G16R16F:
        case D3DFMT_R32F:
        case D3DFMT_A2B10G10R10_XR_BIAS:
        case D3DFMT_UYVY:
        case D3DFMT_YUY2:
            4;
        case D3DFMT_R8G8B8:
            3;
        case D3DFMT_R5G6B5:
        case D3DFMT_X1R5G5B5:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_A4R4G4B4:
        case D3DFMT_A8R3G3B2:
        case D3DFMT_X4R4G4B4:
        case D3DFMT_A8P8:
        case D3DFMT_A8L8:
        case D3DFMT_V8U8:
        case D3DFMT_L6V5U5:
        case D3DFMT_D16_LOCKABLE:
        case D3DFMT_D15S1:
        case D3DFMT_D16:
        case D3DFMT_L16:
        case D3DFMT_INDEX16:
        case D3DFMT_R16F:
        case D3DFMT_CxV8U8:
        case D3DFMT_R8G8_B8G8:
        case D3DFMT_G8R8_G8B8:
            2;
        case D3DFMT_R3G3B2:
        case D3DFMT_A8:
        case D3DFMT_P8:
        case D3DFMT_L8:
        case D3DFMT_A4L4:
        case D3DFMT_S8_LOCKABLE:
            1;
        default:
            return 0;
    }
}

UINT byte_size_from_d3d_type(D3DDECLTYPE type)
{
    switch (type)
    {
        case D3DDECLTYPE_FLOAT1:
            return sizeof(float);
        case D3DDECLTYPE_FLOAT2:
            return 2 * sizeof(float);
        case D3DDECLTYPE_FLOAT3:
            return 3 * sizeof(float);
        case D3DDECLTYPE_FLOAT4:
            return 4 * sizeof(float);
        case D3DDECLTYPE_D3DCOLOR:
        case D3DDECLTYPE_UBYTE4:
        case D3DDECLTYPE_UBYTE4N:
            return 4 * sizeof(BYTE);
        case D3DDECLTYPE_SHORT2:
        case D3DDECLTYPE_SHORT2N:
        case D3DDECLTYPE_USHORT2N:
        case D3DDECLTYPE_FLOAT16_2:
            return 2 * sizeof(short int);
        case D3DDECLTYPE_SHORT4:
        case D3DDECLTYPE_SHORT4N:
        case D3DDECLTYPE_USHORT4N:
        case D3DDECLTYPE_FLOAT16_4:
            return 4 * sizeof(short int);
        case D3DDECLTYPE_UDEC3:
        case D3DDECLTYPE_DEC3N:
            return 3 * sizeof(short int);
        case D3DDECLTYPE_UNUSED:
            return 0;
        default:
            return NULL;
    }
}

DWORD decl_type_to_FVF(D3DDECLTYPE type, D3DDECLUSAGE usage, BYTE usage_index, int n_weights)
{
    switch (type)
    {
        case D3DDECLTYPE_FLOAT3:
            switch (usage)
            {
                case D3DDECLUSAGE_POSITION:
                    return D3DFVF_XYZ;
                case D3DDECLUSAGE_NORMAL:
                    return D3DFVF_NORMAL;
                default:
                    return NULL;
            }
        case D3DDECLTYPE_FLOAT4:
            if (usage == D3DDECLUSAGE_POSITIONT)
                return D3DFVF_XYZRHW;
            return NULL;
        case D3DDECLTYPE_UBYTE4:
            if (usage == D3DDECLUSAGE_BLENDINDICES)
                switch (n_weights)
                {
                    case 0:
                        return D3DFVF_XYZB1;
                    case 1:
                        return D3DFVF_XYZB2;
                    case 2:
                        return D3DFVF_XYZB3;
                    case 3:
                        return D3DFVF_XYZB4;
                    case 4:
                        return D3DFVF_XYZB5;
                    default:
                        return D3DFVF_XYZB1;
                }
        case D3DDECLTYPE_FLOAT1:
            if (usage == D3DDECLUSAGE_PSIZE)
                return D3DFVF_PSIZE;
            return NULL;
        case D3DDECLTYPE_D3DCOLOR:
            if (usage == D3DDECLUSAGE_COLOR)
            {
                switch (usage_index)
                {
                    case 0:
                        return D3DFVF_DIFFUSE;
                    case 1:
                        return D3DFVF_SPECULAR;
                    default:
                        return NULL;
                }
            }
            else
            {
                return NULL;
            }
        default:
            return NULL;
    }
}

D3DDECLTYPE d3d_format_to_decl_type(D3DFORMAT format)
{
    switch (format)
    {
        case D3DFMT_A32B32G32R32F:
            return D3DDECLTYPE_FLOAT4;
        case D3DFMT_A16B16G16R16:
            return D3DDECLTYPE_SHORT4;
        case D3DFMT_Q16W16V16U16:
            return D3DDECLTYPE_SHORT4;
        case D3DFMT_A16B16G16R16F:
            return D3DDECLTYPE_FLOAT16_4;
        case D3DFMT_G32R32F:
            return D3DDECLTYPE_FLOAT2;
        case D3DFMT_A2B10G10R10:
            return D3DDECLTYPE_UDEC3;
        case D3DFMT_A8B8G8R8:
            return D3DDECLTYPE_UBYTE4;
        case D3DFMT_X8B8G8R8:
            return D3DDECLTYPE_UBYTE4;
        case D3DFMT_G16R16:
            return D3DDECLTYPE_USHORT2N;
        case D3DFMT_A2R10G10B10:
            return D3DDECLTYPE_UDEC3;
        case D3DFMT_V16U16:
            return D3DDECLTYPE_SHORT2;
        case D3DFMT_A2W10V10U10:
        case D3DFMT_A8R8G8B8:
            return D3DDECLTYPE_D3DCOLOR;
        case D3DFMT_X8R8G8B8:
            return D3DDECLTYPE_UBYTE4;
        case D3DFMT_X8L8V8U8:
            return D3DDECLTYPE_UBYTE4;
        case D3DFMT_Q8W8V8U8:
            return D3DDECLTYPE_UBYTE4;
        case D3DFMT_D32:
        case D3DFMT_D24S8:
        case D3DFMT_D24X8:
        case D3DFMT_D24X4S4:
        case D3DFMT_D32F_LOCKABLE:
        case D3DFMT_D24FS8:
        case D3DFMT_D32_LOCKABLE:
        case D3DFMT_INDEX32:
        case D3DFMT_G16R16F:
            return D3DDECLTYPE_FLOAT16_2;
        case D3DFMT_R32F:
        case D3DFMT_A2B10G10R10_XR_BIAS:
            return D3DDECLTYPE_UDEC3;
        case D3DFMT_R8G8B8:
        case D3DFMT_R5G6B5:
        case D3DFMT_X1R5G5B5:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_A4R4G4B4:
        case D3DFMT_A8R3G3B2:
        case D3DFMT_X4R4G4B4:
        case D3DFMT_A8P8:
        case D3DFMT_A8L8:
        case D3DFMT_V8U8:
        case D3DFMT_L6V5U5:
        case D3DFMT_D16_LOCKABLE:
        case D3DFMT_D15S1:
        case D3DFMT_D16:
        case D3DFMT_L16:
        case D3DFMT_INDEX16:
        case D3DFMT_R16F:
        case D3DFMT_CxV8U8:
        case D3DFMT_R8G8_B8G8:
        case D3DFMT_G8R8_G8B8:
        case D3DFMT_R3G3B2:
        case D3DFMT_A8:
        case D3DFMT_P8:
        case D3DFMT_L8:
        case D3DFMT_A4L4:
        case D3DFMT_S8_LOCKABLE:
        default:
            return (D3DDECLTYPE)-1;
    }
}

UINT strideForFVF(DWORD FVF)
{
    UINT total_bytes = 0;

    if (FVF & D3DFVF_XYZ)
        total_bytes += 3 * sizeof(float);
    if (FVF & D3DFVF_XYZRHW)
        total_bytes += 4 * sizeof(float);
    if (FVF & D3DFVF_XYZW)
        total_bytes += 4 * sizeof(float);
    if (FVF & D3DFVF_XYZB5)
    {
        total_bytes += 8 * sizeof(float);
    }
    if (FVF & D3DFVF_LASTBETA_UBYTE4)
    {
        total_bytes += 8 * sizeof(float);
    }
    if (FVF & D3DFVF_LASTBETA_D3DCOLOR)
    {
        total_bytes += 8 * sizeof(float);
    }
    if (FVF & D3DFVF_XYZB4)
    {
        total_bytes += 7 * sizeof(float);
    }
    if (FVF & D3DFVF_XYZB3)
    {
        total_bytes += 6 * sizeof(float);
    }
    if (FVF & D3DFVF_XYZB2)
    {
        total_bytes += 5 * sizeof(float);
    }
    if (FVF & D3DFVF_XYZB1)
    {
        total_bytes += 4 * sizeof(float);
    }
    if (FVF & D3DFVF_NORMAL)
    {
        total_bytes += 3 * sizeof(float);
    }
    if (FVF & D3DFVF_PSIZE)
    {
        total_bytes += sizeof(float);
    }
    if (FVF & D3DFVF_DIFFUSE)
    {
        total_bytes += sizeof(float);
    }
    if (FVF & D3DFVF_SPECULAR)
    {
        total_bytes += sizeof(float);
    }

    for (int x = 1; x < 8; x++)
    {
        if (FVF & D3DFVF_TEXCOORDSIZE1(x))
        {
            total_bytes += sizeof(float);
        }
        if (FVF & D3DFVF_TEXCOORDSIZE2(x))
        {
            total_bytes += 2 * sizeof(float);
        }
        if (FVF & D3DFVF_TEXCOORDSIZE3(x))
        {
            total_bytes += 3 * sizeof(float);
        }
        if (FVF & D3DFVF_TEXCOORDSIZE4(x))
        {
            total_bytes += 4 * sizeof(float);
        }
    }

    return total_bytes;
}

UINT draw_vertices_count_to_primitive_count(UINT ver_count, D3DPRIMITIVETYPE prim_type)
{
    switch (prim_type)
    {
        case D3DPT_POINTLIST:
            return ver_count;
        case D3DPT_LINELIST:
            return ver_count / 2;
        case D3DPT_LINESTRIP:
            return ver_count - 1;
        case D3DPT_TRIANGLELIST:
            return ver_count / 3;
        case D3DPT_TRIANGLESTRIP:
            return ver_count - 2;
        case D3DPT_TRIANGLEFAN:
            return ver_count - 2;
        case D3DPT_FORCE_DWORD:
            return ver_count - 2;
        default:
            return ver_count - 2;
    }
}

UINT draw_primitive_count_to_vertices_count(UINT ver_count, D3DPRIMITIVETYPE prim_type)
{
    switch (prim_type)
    {
        case D3DPT_POINTLIST:
            return ver_count;
        case D3DPT_LINELIST:
            return ver_count * 2;
        case D3DPT_LINESTRIP:
            return ver_count + 1;
        case D3DPT_TRIANGLELIST:
            return ver_count * 3;
        case D3DPT_TRIANGLESTRIP:
            return ver_count + 2;
        case D3DPT_TRIANGLEFAN:
            return ver_count + 2;
        case D3DPT_FORCE_DWORD:
            return ver_count + 2;
        default:
            return ver_count + 2;
    }
}

// -----------------------------------------------------------------------------------------------

IDXGISwapChain* last_fullscreen_swap_chain;
static CRITICAL_SECTION crash_handler_lock;
static int crash_handler_level;

static DWORD WINAPI crash_handler_switch_to_window(_In_ LPVOID lpParameter)
{
    // Debugging is a pain in exclusive full screen, especially without a
    // second monitor attached (and even with one if you don't know about
    // the win+arrow or alt+space shortcuts you may be stuck - alt+tab to
    // the debugger, either win+down or alt+space and choose "Restore",
    // either win+left/right several times to move it to the other monitor
    // or alt+space again and choose "Move", press any arrow key to start
    // moving and *then* you can use the mouse to move the window to the
    // other monitor)... Try to switch to windowed mode to make our lives a
    // lot easier, but depending on the crash this might just hang (DirectX
    // might get stuck waiting on a lock or the window message queue might
    // not be pumping), so we do this in a new thread to allow the main
    // crash handler to continue responding to other keys:
    //
    // TODO: See if we can find a way to make this more reliable
    //
    if (last_fullscreen_swap_chain)
    {
        LOG_INFO("Attempting emergency switch to windowed mode on swap chain %p\n", last_fullscreen_swap_chain);

        last_fullscreen_swap_chain->SetFullscreenState(FALSE, NULL);
        // last_fullscreen_swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    }

    if (LogFile)
        fflush(LogFile);

    return 0;
}

static LONG WINAPI migoto_exception_filter(_In_ struct _EXCEPTION_POINTERS* ExceptionInfo)
{
    wchar_t path[MAX_PATH];
    tm timestruct;
    time_t ltime;
    LONG ret = EXCEPTION_CONTINUE_EXECUTION;

    // SOS
    Beep(250, 100);
    Beep(250, 100);
    Beep(250, 100);
    Beep(200, 300);
    Beep(200, 200);
    Beep(200, 200);
    Beep(250, 100);
    Beep(250, 100);
    Beep(250, 100);

    // Before anything else, flush the log file and log exception info

    if (LogFile)
    {
        fflush(LogFile);

        LOG_INFO(
            "\n\n ######################################\n"
            " ### 3DMigoto Crash Handler Invoked ###\n");

        int i = 0;
        for (auto record = ExceptionInfo->ExceptionRecord; record; record = record->ExceptionRecord, i++)
        {
            LOG_INFO(
                " ######################################\n"
                " ### Exception Record %i\n"
                " ###    ExceptionCode: 0x%08x\n"
                " ###   ExceptionFlags: 0x%08x\n"
                " ### ExceptionAddress: 0x%p\n"
                " ### NumberParameters: 0x%u\n"
                " ###",
                i,
                record->ExceptionCode,
                record->ExceptionFlags,
                record->ExceptionAddress,
                record->NumberParameters);
            for (unsigned j = 0; j < record->NumberParameters; j++)
                LOG_INFO(" %08Ix", record->ExceptionInformation[j]);
            LOG_INFO("\n");
        }

        fflush(LogFile);
    }

    // Next, write a minidump file so we can examine this in a debugger
    // later. Note that if the stack is corrupt there is some possibility
    // this could fail - if we really want a robust crash handler we could
    // bring in something like breakpad

    ltime = time(NULL);
    localtime_s(&timestruct, &ltime);
    wcsftime(path, MAX_PATH, L"3DM-%Y%m%d%H%M%S.dmp", &timestruct);

    // If multiple threads crash only allow one to write the crash dump and
    // the rest stop here:
    ENTER_CRITICAL_SECTION(&crash_handler_lock);

    auto fp = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fp != INVALID_HANDLE_VALUE)
    {
        LOG_INFO("Writing minidump to %S...\n", path);

        MINIDUMP_EXCEPTION_INFORMATION dump_info = { GetCurrentThreadId(), ExceptionInfo, FALSE };

        if (MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), fp, MiniDumpWithHandleData, &dump_info, NULL, NULL))
            LOG_INFO("Succeeded\n");
        else
            LOG_INFO("Failed :(\n");

        CloseHandle(fp);
    }
    else
        LOG_INFO("Error creating minidump file \"%S\": %d\n", path, GetLastError());

    if (LogFile)
        fflush(LogFile);

    // If crash is set to 2 instead of continuing we will stop and start
    // responding to various key bindings, sounding a reminder tone every
    // 5 seconds. All key bindings in this mode are prefixed with Ctrl+Alt
    // to prevent them being accidentally triggered.
    if (crash_handler_level == 2)
    {
        if (LogFile)
        {
            LOG_INFO("3DMigoto interactive crash handler invoked:\n");
            LOG_INFO(" Ctrl+Alt+Q: Quit (execute exception handler)\n");
            LOG_INFO(" Ctrl+Alt+K: Kill process\n");
            LOG_INFO(" Ctrl+Alt+C: Continue execution\n");
            LOG_INFO(" Ctrl+Alt+B: Break into the debugger (make sure one is attached)\n");
            LOG_INFO(" Ctrl+Alt+W: Attempt to switch to Windowed mode\n");
            LOG_INFO("\n");
            fflush(LogFile);
        }
        while (1)
        {
            Beep(500, 100);
            for (int i = 0; i < 50; i++)
            {
                Sleep(100);
                if (GetAsyncKeyState(VK_CONTROL) < 0 &&
                    GetAsyncKeyState(VK_MENU) < 0)
                {
                    if (GetAsyncKeyState('C') < 0)
                    {
                        LOG_INFO("Attempting to continue...\n");
                        fflush(LogFile);
                        Beep(1000, 100);
                        ret = EXCEPTION_CONTINUE_EXECUTION;
                        goto unlock;
                    }

                    if (GetAsyncKeyState('Q') < 0)
                    {
                        LOG_INFO("Executing exception handler...\n");
                        fflush(LogFile);
                        Beep(1000, 100);
                        ret = EXCEPTION_EXECUTE_HANDLER;
                        goto unlock;
                    }

                    if (GetAsyncKeyState('K') < 0)
                    {
                        LOG_INFO("Killing process...\n");
                        fflush(LogFile);
                        Beep(1000, 100);
                        ExitProcess(0x3D819070);
                    }

                    // TODO:
                    // S = Suspend all other threads
                    // R = Resume all other threads

                    if (GetAsyncKeyState('B') < 0)
                    {
                        LOG_INFO("Dropping to debugger...\n");
                        fflush(LogFile);
                        Beep(1000, 100);
                        __debugbreak();
                        goto unlock;
                    }

                    if (GetAsyncKeyState('W') < 0)
                    {
                        LOG_INFO("Attempting to switch to windowed mode...\n");
                        fflush(LogFile);
                        Beep(1000, 100);
                        CreateThread(NULL, 0, crash_handler_switch_to_window, NULL, 0, NULL);
                        Sleep(1000);
                    }
                }
            }
        }
    }

unlock:
    LEAVE_CRITICAL_SECTION(&crash_handler_lock);

    return ret;
}

static DWORD WINAPI exception_keyboard_monitor(_In_ LPVOID lpParameter)
{
    while (1)
    {
        Sleep(1000);
        if (GetAsyncKeyState(VK_CONTROL) < 0 &&
            GetAsyncKeyState(VK_MENU) < 0 &&
            GetAsyncKeyState(VK_F11) < 0)
        {
            // User must be really committed to this to invoke the
            // crash handler, and this is a simple measure against
            // accidentally invoking it multiple times in a row:
            Sleep(3000);
            if (GetAsyncKeyState(VK_CONTROL) < 0 &&
                GetAsyncKeyState(VK_MENU) < 0 &&
                GetAsyncKeyState(VK_F11) < 0)
            {
                // Make sure 3DMigoto's exception handler is
                // still installed and trigger it:
                SetUnhandledExceptionFilter(migoto_exception_filter);
                RaiseException(0x3D819070, 0, 0, NULL);
            }
        }
    }
}

void install_crash_handler(int level)
{
    LPTOP_LEVEL_EXCEPTION_FILTER old_handler;
    UINT old_mode;

    crash_handler_level = level;

    old_handler = SetUnhandledExceptionFilter(migoto_exception_filter);
    // TODO: Call set_terminate() on every thread to catch unhandled C++
    // exceptions as well

    if (old_handler == migoto_exception_filter)
    {
        LOG_INFO("  > 3DMigoto crash handler already installed\n");
        return;
    }

    INIT_CRITICAL_SECTION(&crash_handler_lock);

    old_mode = SetErrorMode(SEM_FAILCRITICALERRORS);

    LOG_INFO("  > Installed 3DMigoto crash handler, previous exception filter: %p, previous error mode: %x\n", old_handler, old_mode);

    // Spawn a thread to monitor for a keyboard salute to trigger the
    // exception handler in the event of a hang/deadlock:
    CreateThread(NULL, 0, exception_keyboard_monitor, NULL, 0, NULL);
}
#endif
