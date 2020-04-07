#pragma once

#include <ctype.h>
#include <wchar.h>
#include <string>
#include <vector>
#include <map>

#include <d3d11_1.h>
#include <dxgi1_2.h>

#include <D3Dcompiler.h>
#include <d3d9.h>
#include <DirectXMath.h>

#include "version.h"
#include "log.h"
#include "crc32c.h"
#include "util_min.h"

#include "D3D_Shaders\stdafx.h"

#if MIGOTO_DX == 11
#include "DirectX11\HookedDevice.h"
#include "DirectX11\HookedContext.h"
#elif MIGOTO_DX == 9
#include "DirectX9\HookedDeviceDX9.h"
#endif // MIGOTO_DX


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
const int INI_PARAMS_SIZE_WARNING = 256;

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

// Use the pretty lock debugging version if lock.h is included first, otherwise
// use the regular EnterCriticalSection:
#ifdef EnterCriticalSectionPretty
#define LockResourceCreationMode() \
	EnterCriticalSectionPretty(&resource_creation_mode_lock)
#else
#define LockResourceCreationMode() \
	EnterCriticalSection(&resource_creation_mode_lock)
#endif

#define UnlockResourceCreationMode() \
	LeaveCriticalSection(&resource_creation_mode_lock)

// -----------------------------------------------------------------------------------------------

// Create hash code for textures or buffers.  

// Wrapped in try/catch because it can crash in Dirt Rally,
// because of noncontiguous or non-mapped memory for the texture.  Not sure this
// is the best strategy.

// Now switching to use crc32_append instead of fnv_64_buf for performance. This
// implementation of crc32c uses the SSE 4.2 instructions in the CPU to calculate,
// and is some 30x faster than fnv_64_buf.
// 
// Not changing shader hash calculation as there are thousands of shaders already
// in the field, and there is no known bottleneck for that calculation.

static uint32_t crc32c_hw(uint32_t seed, const void *buffer, size_t length)
{
	try
	{
		const uint8_t *cast_buffer = static_cast<const uint8_t*>(buffer);

		return crc32c_append(seed, cast_buffer, length);
	}
	catch (...)
	{
		// Fatal error, but catch it and return null for hash.
		LogInfo("   ******* Exception caught while calculating crc32c_hw hash ******\n");
		return 0;
	}
}


// -----------------------------------------------------------------------------------------------

// Primary hash calculation for all shader file names.

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

static void BeepProfileFail()
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

static DECLSPEC_NORETURN void DoubleBeepExit()
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

static int _autoicmp(const wchar_t *s1, const wchar_t *s2)
{
	return _wcsicmp(s1, s2);
}
static int _autoicmp(const char *s1, const char *s2)
{
	return _stricmp(s1, s2);
}

// To use this function be sure to terminate an EnumName_t list with {NULL, 0}
// as it cannot use ArraySize on passed in arrays.
template <class T1, class T2>
static T2 lookup_enum_val(struct EnumName_t<T1, T2> *enum_names, T1 name, T2 default, bool *found=NULL)
{
	for (; enum_names->name; enum_names++) {
		if (!_autoicmp(name, enum_names->name)) {
			if (found)
				*found = true;
			return enum_names->val;
		}
	}

	if (found)
		*found = false;

	return default;
}
template <class T1, class T2>
static T2 lookup_enum_val(struct EnumName_t<T1, T2> *enum_names, T1 name, size_t len, T2 default, bool *found=NULL)
{
	for (; enum_names->name; enum_names++) {
		if (!_wcsnicmp(name, enum_names->name, len)) {
			if (found)
				*found = true;
			return enum_names->val;
		}
	}

	if (found)
		*found = false;

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

template <class T2>
static wstring lookup_enum_bit_names(struct EnumName_t<const wchar_t*, T2> *enum_names, T2 val)
{
	wstring ret;
	T2 remaining = val;

	for (; enum_names->name; enum_names++) {
		if ((T2)(val & enum_names->val) == enum_names->val) {
			if (!ret.empty())
				ret += L' ';
			ret += enum_names->name;
			remaining = (T2)(remaining & (T2)~enum_names->val);
		}
	}

	if (remaining != (T2)0) {
		wchar_t buf[20];
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
static T2 parse_enum_option_string(struct EnumName_t<T1, T2> *enum_names, T3 option_string, T1 *unrecognised)
{
	T3 ptr = option_string, cur;
	T2 ret = (T2)0;
	T2 tmp = T2::INVALID;

	if (unrecognised)
		*unrecognised = NULL;

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
		if (tmp != T2::INVALID) {
			ret |= tmp;
		} else {
			if (unrecognised && !(*unrecognised)) {
				*unrecognised = cur;
			} else {
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
static T2 parse_enum_option_string(struct EnumName_t<T1, T2> *enum_names, T1 option_string, T1 *unrecognised)
{
	return parse_enum_option_string<T1, T2, T1>(enum_names, option_string, unrecognised);
}

// This is similar to the above, but stops parsing when it hits an unrecognised
// keyword and returns the position without throwing any errors. It also
// doesn't modify the option_string, allowing it to be used with C++ strings.
template <class T1, class T2>
static T2 parse_enum_option_string_prefix(struct EnumName_t<T1, T2> *enum_names, T1 option_string, T1 *unrecognised)
{
	T1 ptr = option_string, cur;
	T2 ret = (T2)0;
	T2 tmp = T2::INVALID;
	size_t len;

	if (unrecognised)
		*unrecognised = NULL;

	while (*ptr) {
		// Skip over whitespace:
		for (; *ptr == L' '; ptr++) {}

		// Mark start of current entry:
		cur = ptr;

		// Scan until the next whitespace or end of string:
		for (; *ptr && *ptr != L' '; ptr++) {}

		// Note word length:
		len = ptr - cur;

		// Advance pointer if not at end of string:
		if (*ptr)
			ptr++;

		// Lookup the value of the current entry:
		tmp = lookup_enum_val<T1, T2> (enum_names, cur, len, T2::INVALID);
		if (tmp != T2::INVALID) {
			ret |= tmp;
		} else {
			if (unrecognised)
				*unrecognised = cur;
			return ret;
		}
	}
	return ret;
}

#if MIGOTO_DX == 11
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

static DXGI_FORMAT ParseFormatString(const char *fmt, bool allow_numeric_format)
{
	size_t num_formats = sizeof(DXGIFormats) / sizeof(DXGIFormats[0]);
	unsigned format;
	int nargs, end;

	if (allow_numeric_format) {
		// Try parsing format string as decimal:
		nargs = sscanf_s(fmt, "%u%n", &format, &end);
		if (nargs == 1 && end == strlen(fmt))
			return (DXGI_FORMAT)format;
	}

	if (!_strnicmp(fmt, "DXGI_FORMAT_", 12))
		fmt += 12;

	// Look up format string:
	for (format = 0; format < num_formats; format++) {
		if (!_strnicmp(fmt, DXGIFormats[format], 30))
			return (DXGI_FORMAT)format;
	}

	// UNKNOWN/0 is a valid format (e.g. for structured buffers), so return
	// -1 cast to a DXGI_FORMAT to signify an error:
	return (DXGI_FORMAT)-1;
}

static DXGI_FORMAT ParseFormatString(const wchar_t *wfmt, bool allow_numeric_format)
{
	char afmt[42];

	wcstombs(afmt, wfmt, 42);
	afmt[41] = '\0';

	return ParseFormatString(afmt, allow_numeric_format);
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

// Is there already a utility function that does this?
static UINT dxgi_format_size(DXGI_FORMAT format)
{
	switch (format) {
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


static const char* type_name(IUnknown *object)
{
	ID3D11Device1 *device;
	ID3D11DeviceContext1 *context;

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

	try {
		return typeid(*object).name();
	} catch (__non_rtti_object) {
		return "<NO_RTTI>";
	} catch(bad_typeid) {
		return "<NULL>";
	}
}
#endif // MIGOTO_DX == 11

#if MIGOTO_DX == 9
static const char* type_name_dx9(IUnknown *object)
{
	IDirect3DDevice9 *device;

	// Seems that not even try / catch is safe in all cases of this
	// (grumble grumble poorly designed grumble...). The only cases where
	// we should be called on an object without type information is while
	// hooking the device and/or context, so check if it is one of those
	// cases:

	device = lookup_hooked_device_dx9((IDirect3DDevice9Ex*)object);
	if (device)
		return "Hooked_IDirect3DDevice9";

	try {
		return typeid(*object).name();
	}
	catch (__non_rtti_object) {
		return "<NO_RTTI>";
	}
	catch (bad_typeid) {
		return "<NULL>";
	}
}
#endif // MIGOTO_DX == 9
// -----------------------------------------------------------------------------------------------

// Common routine to handle disassembling binary shaders to asm text.
// This is used whenever we need the Asm text.


// New version using Flugan's wrapper around D3DDisassemble to replace the
// problematic %f floating point values with %.9e, which is enough that a 32bit
// floating point value will be reproduced exactly:
static string BinaryToAsmText(const void *pShaderBytecode, size_t BytecodeLength,
		bool patch_cb_offsets,
		bool disassemble_undecipherable_data = true,
		int hexdump = 0, bool d3dcompiler_46_compat = true)
{
	string comments;
	vector<byte> byteCode(BytecodeLength);
	vector<byte> disassembly;
	HRESULT r;

	comments = "//   using 3Dmigoto v" + string(VER_FILE_VERSION_STR) + " on " + LogTime() + "//\n";
	memcpy(byteCode.data(), pShaderBytecode, BytecodeLength);

#if MIGOTO_DX == 9
	r = disassemblerDX9(&byteCode, &disassembly, comments.c_str());
#elif MIGOTO_DX == 11
	r = disassembler(&byteCode, &disassembly, comments.c_str(), hexdump,
			d3dcompiler_46_compat, disassemble_undecipherable_data, patch_cb_offsets);
#endif // MIGOTO_DX
	if (FAILED(r)) {
		LogInfo("  disassembly failed. Error: %x\n", r);
		return "";
	}

	return string(disassembly.begin(), disassembly.end());
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
	string asmText = BinaryToAsmText(pShaderBytecode, bytecodeLength, false);
	if (asmText.empty())
		return "";

	// Read shader model. This is the first not commented line.
	char *pos = (char *)asmText.data();
	char *end = pos + asmText.size();
	while ((pos[0] == '/' || pos[0] == '\n') && pos < end)
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

static HRESULT CreateTextFile(wchar_t *fullPath, string *asmText, bool overwrite)
{
	FILE *f;

	if (!overwrite) {
		_wfopen_s(&f, fullPath, L"rb");
		if (f)
		{
			fclose(f);
			LogInfoW(L"    CreateTextFile error: file already exists %s\n", fullPath);
			return ERROR_FILE_EXISTS;
		}
	}

	_wfopen_s(&f, fullPath, L"wb");
	if (f)
	{
		fwrite(asmText->data(), 1, asmText->size(), f);
		fclose(f);
	}

	return S_OK;
}

// Get shader type from asm, first non-commented line.  CS, PS, VS.
// Not sure this works on weird Unity variant with embedded types.


// Specific variant to name files consistently, so we know they are Asm text.

static HRESULT CreateAsmTextFile(wchar_t* fileDirectory, UINT64 hash, const wchar_t* shaderType, 
	const void *pShaderBytecode, size_t bytecodeLength, bool patch_cb_offsets)
{
	string asmText = BinaryToAsmText(pShaderBytecode, bytecodeLength, patch_cb_offsets);
	if (asmText.empty())
	{
		return E_OUTOFMEMORY;
	}

	wchar_t fullPath[MAX_PATH];
	swprintf_s(fullPath, MAX_PATH, L"%ls\\%016llx-%ls.txt", fileDirectory, hash, shaderType);

	HRESULT hr = CreateTextFile(fullPath, &asmText, false);

	if (SUCCEEDED(hr))
		LogInfoW(L"    storing disassembly to %s\n", fullPath);
	else
		LogInfoW(L"    error: %x, storing disassembly to %s\n", hr, fullPath);

	return hr;
}

// Specific variant to name files, so we know they are HLSL text.

static HRESULT CreateHLSLTextFile(UINT64 hash, string hlslText)
{

}

// -----------------------------------------------------------------------------------------------

// Parses the name of one of the IniParam constants: x, y, z, w, x1, y1, ..., z7, w7
static bool ParseIniParamName(const wchar_t *name, int *idx, float DirectX::XMFLOAT4::**component)
{
	int ret, len1, len2;
	wchar_t component_chr;
	size_t length = wcslen(name);

	ret = swscanf_s(name, L"%lc%n%u%n", &component_chr, 1, &len1, idx, &len2);

	// May or may not have matched index. Make sure entire string was
	// matched either way and check index is valid if it was matched:
	if (ret == 1 && len1 == length) {
		*idx = 0;
	} else if (ret == 2 && len2 == length) {
#if MIGOTO_DX == 9
		// Added gating for this DX9 specific limitation that we definitely do
		// not want to enforce in DX11 as that would break a bunch of mods -DSS
		if (*idx >= 225)
			return false;
#endif // MIGOTO_DX == 9
	} else {
		return false;
	}

	switch (towlower(component_chr)) {
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

BOOL CreateDirectoryEnsuringAccess(LPCWSTR path);
errno_t wfopen_ensuring_access(FILE** pFile, const wchar_t *filename, const wchar_t *mode);
void set_file_last_write_time(wchar_t *path, FILETIME *ftWrite, DWORD flags=0);
void touch_file(wchar_t *path, DWORD flags=0);
#define touch_dir(path) touch_file(path, FILE_FLAG_BACKUP_SEMANTICS)

bool check_interface_supported(IUnknown *unknown, REFIID riid);
void analyse_iunknown(IUnknown *unknown);

// For the time being, since we are not setup to use the Win10 SDK, we'll add
// these manually. Some games under Win10 are requesting these.

struct _declspec(uuid("9d06dffa-d1e5-4d07-83a8-1bb123f2f841")) ID3D11Device2;
struct _declspec(uuid("420d5b32-b90c-4da4-bef0-359f6a24a83a")) ID3D11DeviceContext2;
struct _declspec(uuid("A8BE2AC4-199F-4946-B331-79599FB98DE7")) IDXGISwapChain2;
struct _declspec(uuid("94D99BDB-F1F8-4AB0-B236-7DA0170EDAB1")) IDXGISwapChain3;
struct _declspec(uuid("3D585D5A-BD4A-489E-B1F4-3DBCB6452FFB")) IDXGISwapChain4;

std::string NameFromIID(IID id);

void WarnIfConflictingShaderExists(wchar_t *orig_path, const char *message = "");
static const char *end_user_conflicting_shader_msg =
	"Conflicting shaders present - please use uninstall.bat and reinstall the fix.\n";

struct OMState {
	UINT NumRTVs;
#if MIGOTO_DX == 9
	vector<IDirect3DSurface9*> rtvs;
	IDirect3DSurface9 *dsv;
#elif MIGOTO_DX == 11
	ID3D11RenderTargetView *rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	ID3D11DepthStencilView *dsv;
	UINT UAVStartSlot;
	UINT NumUAVs;
	ID3D11UnorderedAccessView *uavs[D3D11_PS_CS_UAV_REGISTER_COUNT];
#endif // MIGOTO_DX
};

// TODO: Could use DX version specific typedefs for these differences
#if MIGOTO_DX == 9
void save_om_state(IDirect3DDevice9 *device, struct OMState *state);
void restore_om_state(IDirect3DDevice9 *device, struct OMState *state);
#elif MIGOTO_DX == 11
void save_om_state(ID3D11DeviceContext *context, struct OMState *state);
void restore_om_state(ID3D11DeviceContext *context, struct OMState *state);
#endif // MIGOTO_DX

// -----------------------------------------------------------------------------------------------
#if MIGOTO_DX == 9
static std::map<int, char*> D3DFORMATS = {
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

static char *TexFormatStrDX9(D3DFORMAT format)
{
	switch (format) {
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
		it = D3DFORMATS.find(format);
		if (it != D3DFORMATS.end())
			return it->second;
		return "UNKNOWN";

	}
}

static D3DFORMAT ParseFormatStringDX9(const char *fmt, bool allow_numeric_format)
{
	size_t num_formats = D3DFORMATS.size();
	unsigned format;
	int nargs, end;

	if (allow_numeric_format) {
		// Try parsing format string as decimal:
		nargs = sscanf_s(fmt, "%u%n", &format, &end);
		if (nargs == 1 && end == strlen(fmt))
			return (D3DFORMAT)format;
	}

	if (!_strnicmp(fmt, "D3DFMT_", 7))
		fmt += 7;

	// Look up format string:
	map<int, char*>::iterator it;
	for (it = D3DFORMATS.begin(); it != D3DFORMATS.end(); it++)
	{
		if (!_strnicmp(fmt, it->second, 30))
			return (D3DFORMAT)it->first;
	}
	// UNKNOWN/0 is a valid format (e.g. for structured buffers), so return
	// -1 cast to a DXGI_FORMAT to signify an error:
	return (D3DFORMAT) - 1;
}

static D3DFORMAT ParseFormatStringDX9(const wchar_t *wfmt, bool allow_numeric_format)
{
	char afmt[42];

	wcstombs(afmt, wfmt, 42);
	afmt[41] = '\0';

	return ParseFormatStringDX9(afmt, allow_numeric_format);
}
inline size_t BitsPerPixel(_In_ D3DFORMAT fmt)
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
static UINT d3d_format_bytes(D3DFORMAT format) {

	switch (format) {
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
static UINT byteSizeFromD3DType(D3DDECLTYPE type) {
	switch (type) {
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

static DWORD decl_type_to_FVF(D3DDECLTYPE type, D3DDECLUSAGE usage, BYTE usageIndex, int nWeights) {
	switch (type) {
	case D3DDECLTYPE_FLOAT3:
		switch (usage) {
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
			switch (nWeights) {
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
		if (usage == D3DDECLUSAGE_COLOR) {
			switch (usageIndex) {
			case 0:
				return D3DFVF_DIFFUSE;
			case 1:
				return D3DFVF_SPECULAR;
			default:
				return NULL;
			}
		}
		else {
			return NULL;
		}
	default:
		return NULL;

	}

}

static D3DDECLTYPE d3d_format_to_decl_type(D3DFORMAT format)
{
	switch (format) {
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
		return 	D3DDECLTYPE_UBYTE4;
	case D3DFMT_X8B8G8R8:
		return 	D3DDECLTYPE_UBYTE4;
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
		return (D3DDECLTYPE) - 1;
	}
}


static UINT strideForFVF(DWORD FVF) {
	UINT totalBytes = 0;

	if (FVF & D3DFVF_XYZ)
		totalBytes += 3 * sizeof(float);
	if (FVF & D3DFVF_XYZRHW)
		totalBytes += 4 * sizeof(float);
	if (FVF & D3DFVF_XYZW)
		totalBytes += 4 * sizeof(float);
	if (FVF & D3DFVF_XYZB5) {
		totalBytes += 8 * sizeof(float);
	}
	if (FVF & D3DFVF_LASTBETA_UBYTE4) {
		totalBytes += 8 * sizeof(float);
	}
	if (FVF & D3DFVF_LASTBETA_D3DCOLOR) {
		totalBytes += 8 * sizeof(float);
	}
	if (FVF & D3DFVF_XYZB4) {
		totalBytes += 7 * sizeof(float);
	}
	if (FVF & D3DFVF_XYZB3) {
		totalBytes += 6 * sizeof(float);
	}
	if (FVF & D3DFVF_XYZB2) {
		totalBytes += 5 * sizeof(float);
	}
	if (FVF & D3DFVF_XYZB1) {
		totalBytes += 4 * sizeof(float);
	}
	if (FVF & D3DFVF_NORMAL) {
		totalBytes += 3 * sizeof(float);
	}
	if (FVF & D3DFVF_PSIZE) {
		totalBytes += sizeof(float);
	}
	if (FVF & D3DFVF_DIFFUSE) {
		totalBytes += sizeof(float);
	}
	if (FVF & D3DFVF_SPECULAR) {
		totalBytes += sizeof(float);
	}

	for (int x = 1; x < 8; x++) {
		if (FVF & D3DFVF_TEXCOORDSIZE1(x)) {
			totalBytes += sizeof(float);
		}
		if (FVF & D3DFVF_TEXCOORDSIZE2(x)) {
			totalBytes += 2 * sizeof(float);
		}
		if (FVF & D3DFVF_TEXCOORDSIZE3(x)) {
			totalBytes += 3 * sizeof(float);
		}
		if (FVF & D3DFVF_TEXCOORDSIZE4(x)) {
			totalBytes += 4 * sizeof(float);
		}
	}

	return totalBytes;

}
static UINT DrawVerticesCountToPrimitiveCount(UINT vCount, D3DPRIMITIVETYPE pType) {

	switch (pType) {
	case D3DPT_POINTLIST:
		return vCount;
	case D3DPT_LINELIST:
		return vCount / 2;
	case D3DPT_LINESTRIP:
		return vCount - 1;
	case D3DPT_TRIANGLELIST:
		return vCount / 3;
	case D3DPT_TRIANGLESTRIP:
		return vCount - 2;
	case D3DPT_TRIANGLEFAN:
		return vCount - 2;
	case D3DPT_FORCE_DWORD:
		return vCount - 2;
	default:
		return vCount - 2;
	}


}
static UINT DrawPrimitiveCountToVerticesCount(UINT pCount, D3DPRIMITIVETYPE pType) {

	switch (pType) {
	case D3DPT_POINTLIST:
		return pCount;
	case D3DPT_LINELIST:
		return pCount * 2;
	case D3DPT_LINESTRIP:
		return pCount + 1;
	case D3DPT_TRIANGLELIST:
		return pCount * 3;
	case D3DPT_TRIANGLESTRIP:
		return pCount + 2;
	case D3DPT_TRIANGLEFAN:
		return pCount + 2;
	case D3DPT_FORCE_DWORD:
		return pCount + 2;
	default:
		return pCount + 2;
	}
}
#endif // MIGOTO_DX == 9

#if MIGOTO_DX == 11
extern IDXGISwapChain *last_fullscreen_swap_chain;
#endif // MIGOTO_DX == 11
void install_crash_handler(int level);
