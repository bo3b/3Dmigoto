#pragma once

#include <ctype.h>
#include <wchar.h>
#include <string>

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <D3Dcompiler.h>
#include <d3d9.h>

#include "version.h"
#include "log.h"


// -----------------------------------------------------------------------------------------------

// Primary hash calculation for all shader file names, all textures.

// 64 bit magic FNV-0 and FNV-1 prime
#define FNV_64_PRIME ((UINT64)0x100000001b3ULL)
static UINT64 fnv_64_buf(const void *buf, size_t len)
{
	UINT64 hval = 0;
	unsigned const char *bp = (unsigned const char *)buf;	/* start of buffer */
	unsigned const char *be = bp + len;		/* beyond end of buffer */

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


// -----------------------------------------------------------------------------------------------

static UINT64 CalcTexture2DDescHash(const D3D11_TEXTURE2D_DESC *desc,
	UINT64 initial_hash, int override_width, int override_height)
{
	UINT64 hash = initial_hash;

	// It concerns me that CreateTextureND can use an override if it
	// matches screen resolution, but when we record render target / shader
	// resource stats we don't use the same override.
	//
	// For textures made with CreateTextureND and later used as a render
	// target it's probably fine since the hash will still be stored, but
	// it could be a problem if we need the hash of a render target not
	// created directly with that. I don't know enough about the DX11 API
	// to know if this is an issue, but it might be worth using the screen
	// resolution override in all cases. -DarkStarSword
	if (override_width)
		hash ^= override_width;
	else
		hash ^= desc->Width;
	hash *= FNV_64_PRIME;

	if (override_height)
		hash ^= override_height;
	else
		hash ^= desc->Height;
	hash *= FNV_64_PRIME;

	hash ^= desc->MipLevels; hash *= FNV_64_PRIME;
	hash ^= desc->ArraySize; hash *= FNV_64_PRIME;
	hash ^= desc->Format; hash *= FNV_64_PRIME;
	hash ^= desc->SampleDesc.Count;
	hash ^= desc->SampleDesc.Quality;
	hash ^= desc->Usage; hash *= FNV_64_PRIME;
	hash ^= desc->BindFlags; hash *= FNV_64_PRIME;
	hash ^= desc->CPUAccessFlags; hash *= FNV_64_PRIME;
	hash ^= desc->MiscFlags;

	return hash;
}

static UINT64 CalcTexture3DDescHash(const D3D11_TEXTURE3D_DESC *desc,
	UINT64 initial_hash, int override_width, int override_height)
{
	UINT64 hash = initial_hash;

	// Same comment as in CalcTexture2DDescHash above - concerned about
	// inconsistent use of these resolution overrides
	if (override_width)
		hash ^= override_width;
	else
		hash ^= desc->Width;
	hash *= FNV_64_PRIME;

	if (override_height)
		hash ^= override_height;
	else
		hash ^= desc->Height;
	hash *= FNV_64_PRIME;

	hash ^= desc->Depth; hash *= FNV_64_PRIME;
	hash ^= desc->MipLevels; hash *= FNV_64_PRIME;
	hash ^= desc->Format; hash *= FNV_64_PRIME;
	hash ^= desc->Usage; hash *= FNV_64_PRIME;
	hash ^= desc->BindFlags; hash *= FNV_64_PRIME;
	hash ^= desc->CPUAccessFlags; hash *= FNV_64_PRIME;
	hash ^= desc->MiscFlags;

	return hash;
}

// -----------------------------------------------------------------------------------------------

// Strip spaces from the right of a string.
// Returns a pointer to the last non-NULL character of the truncated string.
static char *RightStripA(char *buf)
{
	char *end = buf + strlen(buf) - 1;
	while (end > buf && isspace(*end))
		end--;
	*(end + 1) = 0;
	return end;
}
static wchar_t *RightStripW(wchar_t *buf)
{
	wchar_t *end = buf + wcslen(buf) - 1;
	while (end > buf && iswspace(*end))
		end--;
	*(end + 1) = 0;
	return end;
}

static char *readStringParameter(wchar_t *val)
{
	static char buf[MAX_PATH];
	wcstombs(buf, val, MAX_PATH);
	RightStripA(buf);
	char *start = buf; while (isspace(*start)) start++;
	return start;
}

static void BeepSuccess() 
{
	// High beep for success
	Beep(1800, 400);
}

static void BeepShort() 
{
	// Short High beep
	Beep(1800, 100);
}

static void BeepFailure() 
{
	// Bonk sound for failure.
	Beep(200, 150);
}

static void BeepFailure2() 
{
	// Brnk, dunk sound for failure.
	Beep(300, 200); Beep(200, 150);
}

static void DoubleBeepExit() 
{
	// Fatal error somewhere, known to crash, might as well exit cleanly
	// with some notification.
	BeepFailure2();
	Sleep(500);
	BeepFailure2();
	Sleep(200);
	if (LogFile) {
		// Make sure the log is written out so we see the failure message
		fclose(LogFile);
		LogFile = 0;
	}
	ExitProcess(0xc0000135);
}


// -----------------------------------------------------------------------------------------------

template <class T1, class T2>
struct EnumName_t {
	T1 name;
	T2 val;
};

// To use this function be sure to terminate an EnumName_t list with {NULL, 0}
// as it cannot use ArraySize on passed in arrays.
template <class T1, class T2>
static T2 lookup_enum_val(struct EnumName_t<T1, T2> *enum_names, T1 name, T2 default)
{
	for (; enum_names->name; enum_names++) {
		if (!_wcsicmp(name, enum_names->name))
			return enum_names->val;
	}

	return default;
}
template <class T1, class T2>
static T1 lookup_enum_name(struct EnumName_t<T1, T2> *enum_names, T2 val)
{
	for (; enum_names->name; enum_names++) {
		if (val == enum_names->val)
			return enum_names->name;
	}

	return NULL;
}

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

// Parses an option string of names given by enum_names. The enum used with
// this function should have an INVALID=0, other flags declared as powers of
// two, and the SENSIBLE_ENUM macro used to enable the bitwise and logical
// operators. As above, the EnumName_t list must be terminated with {NULL, 0}
template <class T1, class T2>
static T2 parse_enum_option_string(struct EnumName_t<T1, T2> *enum_names, T1 option_string)
{
	wchar_t *ptr = option_string, *cur;
	T2 ret = T2::INVALID;
	T2 tmp = T2::INVALID;

	while (*ptr) {
		// Skip over whitespace:
		for (; *ptr == L' '; ptr++) {}

		// Mark start of current entry:
		cur = ptr;

		// Scan until the next whitespace or end of string:
		for (; *ptr && *ptr != L' '; ptr++) {}

		if (*ptr) {
			// NULL terminate the current entry and advance pointer:
			*ptr = L'\0';
			ptr++;
		}

		// Lookup the value of the current entry:
		tmp = lookup_enum_val<T1, T2> (enum_names, cur, T2::INVALID);
		if (tmp == T2::INVALID) {
			LogInfoW(L"WARNING: Unknown option: %s\n", cur);
			BeepFailure2();
		}
		ret |= tmp;
	}
	return ret;
}

// http://msdn.microsoft.com/en-us/library/windows/desktop/bb173059(v=vs.85).aspx
static char *DXGIFormats[] = {
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

static char *TexFormatStr(unsigned int format)
{
	if (format < sizeof(DXGIFormats) / sizeof(DXGIFormats[0]))
		return DXGIFormats[format];
	return "UNKNOWN";
}

// From DirectXTK with extra formats added
static DXGI_FORMAT EnsureNotTypeless( DXGI_FORMAT fmt )
{
    // Assumes UNORM or FLOAT; doesn't use UINT or SINT
    switch( fmt )
    {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:    return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case DXGI_FORMAT_R32G32B32_TYPELESS:       return DXGI_FORMAT_R32G32B32_FLOAT;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:    return DXGI_FORMAT_R16G16B16A16_UNORM;
    case DXGI_FORMAT_R32G32_TYPELESS:          return DXGI_FORMAT_R32G32_FLOAT;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:     return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_R16G16_TYPELESS:          return DXGI_FORMAT_R16G16_UNORM;
    case DXGI_FORMAT_R32_TYPELESS:             return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R8G8_TYPELESS:            return DXGI_FORMAT_R8G8_UNORM;
    case DXGI_FORMAT_R16_TYPELESS:             return DXGI_FORMAT_R16_UNORM;
    case DXGI_FORMAT_R8_TYPELESS:              return DXGI_FORMAT_R8_UNORM;
    case DXGI_FORMAT_BC1_TYPELESS:             return DXGI_FORMAT_BC1_UNORM;
    case DXGI_FORMAT_BC2_TYPELESS:             return DXGI_FORMAT_BC2_UNORM;
    case DXGI_FORMAT_BC3_TYPELESS:             return DXGI_FORMAT_BC3_UNORM;
    case DXGI_FORMAT_BC4_TYPELESS:             return DXGI_FORMAT_BC4_UNORM;
    case DXGI_FORMAT_BC5_TYPELESS:             return DXGI_FORMAT_BC5_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:        return DXGI_FORMAT_B8G8R8X8_UNORM;
    case DXGI_FORMAT_BC7_TYPELESS:             return DXGI_FORMAT_BC7_UNORM;
// Extra depth/stencil buffer formats not covered in DirectXTK (discards
// stencil buffer to allow binding to a shader resource, alternatively we could
// discard the depth buffer if we ever needed the stencil buffer):
    case DXGI_FORMAT_R32G8X24_TYPELESS:        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    case DXGI_FORMAT_R24G8_TYPELESS:           return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    default:                                   return fmt;
    }
}

// When logging, it's not very helpful to have long sequences of hex instead of
// the actual names of the objects in question.
// e.g.
// DEFINE_GUID(IID_IDXGIFactory,0x7b7166ec,0x21c7,0x44ae,0xb2,0x1a,0xc9,0xae,0x32,0x1a,0xe3,0x69);

static std::string NameFromIID(IID id)
{
	if (__uuidof(IUnknown) == id)
		return "IUnknown";

	if (__uuidof(ID3D11Device) == id)
		return "ID3D11Device";
	if (__uuidof(ID3D10Device) == id)
		return "ID3D10Device";
	if (__uuidof(IDirect3DDevice9) == id)
		return "IDirect3DDevice9";

	if (__uuidof(ID3D11Device1) == id)
		return "ID3D11Device1";
	if (__uuidof(ID3D11DeviceContext1) == id)
		return "ID3D11DeviceContext1";
	
	if (__uuidof(ID3D11InfoQueue) == id)
		return "ID3D11InfoQueue ";

	// All the DXGI interfaces

	if (__uuidof(IDXGIFactory) == id)
		return "IDXGIFactory";
	if (__uuidof(IDXGIFactory1) == id)
		return "IDXGIFactory1";
	if (__uuidof(IDXGIFactory2) == id)
		return "IDXGIFactory2";

	if (__uuidof(IDXGIDevice) == id)
		return "IDXGIDevice";
	if (__uuidof(IDXGIDevice1) == id)
		return "IDXGIDevice1";
	if (__uuidof(IDXGIDevice2) == id)
		return "IDXGIDevice2";

	if (__uuidof(IDXGISwapChain) == id)
		return "IDXGISwapChain";
	if (__uuidof(IDXGISwapChain1) == id)
		return "IDXGISwapChain1";

	if (__uuidof(IDXGIAdapter) == id)
		return "IDXGIAdapter";
	if (__uuidof(IDXGIAdapter1) == id)
		return "IDXGIAdapter1";
	if (__uuidof(IDXGIAdapter2) == id)
		return "IDXGIAdapter2";

	if (__uuidof(IDXGIObject) == id)
		return "IDXGIObject";

	if (__uuidof(ID3D11Texture2D) == id)	// Used to fetch backbuffer
		return "ID3D11Texture2D";

	// For unknown IIDs lets return the hex string.
	// Converting from wchar_t to string using stackoverflow suggestion.

	std::string iidString;
	wchar_t wiid[128];
	if (SUCCEEDED(StringFromGUID2(id, wiid, 128)))
	{
		std::wstring convert = std::wstring(wiid);
		iidString = std::string(convert.begin(), convert.end());
	}
	else
	{
		iidString = "unknown";
	}

	return iidString;
}


// -----------------------------------------------------------------------------------------------

// Common routine to handle disassembling binary shaders to asm text.
// This is used whenever we need the Asm text.

static string BinaryToAsmText(const void *pShaderBytecode, size_t BytecodeLength)
{
	ID3DBlob *disassembly = nullptr;
	UINT flags = D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS;
	string comments = "//   using 3Dmigoto v" + string(VER_FILE_VERSION_STR) + " on " + LogTime() + "//\n";

	HRESULT r = D3DDisassemble(pShaderBytecode, BytecodeLength, flags, comments.c_str(), 
		&disassembly);
	if (FAILED(r))
	{
		LogInfo("  disassembly failed. Error: %x \n", r);
		return "";
	}

	// Successfully disassembled into a Blob.  Let's turn it into a C++ std::string
	// so that we don't have a null byte as a terminator.  If written to a file,
	// the null bytes otherwise cause Git diffs to fail.
	string asmText = string(static_cast<char*>(disassembly->GetBufferPointer()));

	disassembly->Release();
	return asmText;
}

// Get the shader model from the binary shader bytecode.
//
// This used to disassemble, then search for the text string, but if we are going to
// do all that work, we might as well use the James-Jones decoder to get it.
// The other reason to do it this way is that we have seen multiple shader versions
// in Unity games, and the old technique of searching for the first uncommented line
// would fail.

// This is an interesting idea, but doesn't work well here because of project structure.
// for the moment, let's leave this here, but use the disassemble search approach.

//static string GetShaderModel(const void *pShaderBytecode)
//{
//	Shader *shader = DecodeDXBC((uint32_t*)pShaderBytecode);
//	if (shader == nullptr)
//		return "";
//
//	string shaderModel;
//	
//	switch (shader->eShaderType)
//	{
//	case PIXEL_SHADER:
//		shaderModel = "ps";
//		break;
//	case VERTEX_SHADER:
//		shaderModel = "vs";
//		break;
//	case GEOMETRY_SHADER:
//		shaderModel = "gs";
//		break;
//	case HULL_SHADER:
//		shaderModel = "hs";
//		break;
//	case DOMAIN_SHADER:
//		shaderModel = "ds";
//		break;
//	case COMPUTE_SHADER:
//		shaderModel = "cs";
//		break;
//	default:
//		return "";		// Failure.
//	}
//
//	shaderModel += "_" + shader->ui32MajorVersion;
//	shaderModel += "_" + shader->ui32MinorVersion;
//
//	delete shader;
//
//	return shaderModel;
//}

static string GetShaderModel(const void *pShaderBytecode, size_t bytecodeLength)
{
	string asmText = BinaryToAsmText(pShaderBytecode, bytecodeLength);
	if (asmText.empty())
		return "";

	// Read shader model. This is the first not commented line.
	char *pos = (char *)asmText.data();
	char *end = pos + asmText.size();
	while (pos[0] == '/' && pos < end)
	{
		while (pos[0] != 0x0a && pos < end) pos++;
		pos++;
	}
	// Extract model.
	char *eol = pos;
	while (eol[0] != 0x0a && pos < end) eol++;
	string shaderModel(pos, eol);

	return shaderModel;
}

// Create a text file containing text for the string specified.  Can be Asm or HLSL.
// If the file already exists and the caller did not specify overwrite (used
// for reassembled text), return that as an error to avoid overwriting previous
// work.

// We previously would overwrite the file only after checking if the contents were different,
// this relaxes that to just being same file name.

static HRESULT CreateTextFile(wchar_t* fullPath, string asmText, bool overwrite)
{
	FILE *f;

	if (!overwrite) {
		_wfopen_s(&f, fullPath, L"rb");
		if (f)
		{
			fclose(f);
			LogInfoW(L"    CreateTextFile error: file already exists %s \n", fullPath);
			return ERROR_FILE_EXISTS;
		}
	}

	_wfopen_s(&f, fullPath, L"wb");
	if (f)
	{
		fwrite(asmText.data(), 1, asmText.size(), f);
		fclose(f);
	}

	return S_OK;
}

// Get shader type from asm, first non-commented line.  CS, PS, VS.
// Not sure this works on weird Unity variant with embedded types.


// Specific variant to name files consistently, so we know they are Asm text.
// ToDo: Remove all use of the obsolete wsprintf.

static HRESULT CreateAsmTextFile(wchar_t* fileDirectory, UINT64 hash, const wchar_t* shaderType, 
	const void *pShaderBytecode, size_t bytecodeLength)
{
	string asmText = BinaryToAsmText(pShaderBytecode, bytecodeLength);
	if (asmText.empty())
	{
		return E_OUTOFMEMORY;
	}

	wchar_t fullPath[MAX_PATH];
	swprintf_s(fullPath, MAX_PATH, L"%ls\\%016llx-%ls.txt", fileDirectory, hash, shaderType);

	HRESULT hr = CreateTextFile(fullPath, asmText, false);

	if (SUCCEEDED(hr))
		LogInfoW(L"    storing disassembly to %s \n", fullPath);
	else
		LogInfoW(L"    error: %x, storing disassembly to %s \n", hr, fullPath);

	return hr;
}

// Specific variant to name files, so we know they are HLSL text.

static HRESULT CreateHLSLTextFile(UINT64 hash, string hlslText)
{

}

