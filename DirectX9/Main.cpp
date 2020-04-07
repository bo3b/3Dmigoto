#include "Main.h"
// We used to call nvapi_QueryInterface directly, however that puts nvapi.dll /
// nvapi64.dll in our dependencies list and the Windows dynamic linker will
// refuse to load us if that doesn't exist, which is a problem on AMD or Intel
// hardware and a problem for some of our users who are interested in 3DMigoto
// for reasons beyond 3D Vision modding. The way nvapi is supposed to work is
// that we call functions in the nvapi *static* library and it will try to load
// the dynamic library, and gracefully fail if it could not, but directly
// calling nvapi_QueryInterface thwarts that because that call does not come
// from the static library - it is how the static library calls into the
// dynamic library.
//
// Instead we now load nvapi.dll at runtime in the same way that the static
// library does, failing gracefully if we could not.
static HMODULE nvDLL;
static bool nvapi_failed = false;
typedef NvAPI_Status *(__cdecl *nvapi_QueryInterfaceType)(unsigned int offset);
static nvapi_QueryInterfaceType nvapi_QueryInterfacePtr;
void NvAPIOverride()
{
	static bool warned = false;

	if (nvapi_failed)
		return;

	if (!nvDLL) {
		nvDLL = GetModuleHandle(L"nvapi64.dll");
		if (!nvDLL) {
			nvDLL = GetModuleHandle(L"nvapi.dll");
		}
		if (!nvDLL) {
			LogInfo("Can't get nvapi handle\n");
			nvapi_failed = true;
			return;
		}
	}
	if (!nvapi_QueryInterfacePtr) {
		nvapi_QueryInterfacePtr = (nvapi_QueryInterfaceType)GetProcAddress(nvDLL, "nvapi_QueryInterface");
		LogDebug("nvapi_QueryInterfacePtr @ 0x%p\n", nvapi_QueryInterfacePtr);
		if (!nvapi_QueryInterfacePtr) {
			LogInfo("Unable to call NvAPI_QueryInterface\n");
			nvapi_failed = true;
			return;
		}
	}

	// One shot, override custom settings.
	intptr_t ret = (intptr_t)nvapi_QueryInterfacePtr(0xb03bb03b);
	if (!warned && (ret & 0xffffffff) != 0xeecc34ab) {
		LogInfo("  overriding NVAPI wrapper failed.\n");
		warned = true;
	}
}

void NvAPIEnableStereoActiveTracking()
{
	static bool warnedStereoActiveTracking = false;

	if (nvapi_failed)
		return;

	if (!nvDLL) {
		nvDLL = GetModuleHandle(L"nvapi64.dll");
		if (!nvDLL) {
			nvDLL = GetModuleHandle(L"nvapi.dll");
		}
		if (!nvDLL) {
			LogInfo("Can't get nvapi handle\n");
			nvapi_failed = true;
			return;
		}
	}
	if (!nvapi_QueryInterfacePtr) {
		nvapi_QueryInterfacePtr = (nvapi_QueryInterfaceType)GetProcAddress(nvDLL, "nvapi_QueryInterface");
		LogDebug("nvapi_QueryInterfacePtr @ 0x%p\n", nvapi_QueryInterfacePtr);
		if (!nvapi_QueryInterfacePtr) {
			LogInfo("Unable to call NvAPI_QueryInterface\n");
			nvapi_failed = true;
			return;
		}
	}

	intptr_t ret = (intptr_t)nvapi_QueryInterfacePtr(0xa03aa03a);
	if (!warnedStereoActiveTracking && (ret & 0xffffffff) != 0xeecc34ab) {
		LogInfo("  enabling stereo active tracking in NVAPI wrapper failed.\n");
		warnedStereoActiveTracking = true;
	}
}
void NvAPIEnableConvergenceTracking()
{
	static bool warnedConvergenceTracking = false;

	if (nvapi_failed)
		return;

	if (!nvDLL) {
		nvDLL = GetModuleHandle(L"nvapi64.dll");
		if (!nvDLL) {
			nvDLL = GetModuleHandle(L"nvapi.dll");
		}
		if (!nvDLL) {
			LogInfo("Can't get nvapi handle\n");
			nvapi_failed = true;
			return;
		}
	}
	if (!nvapi_QueryInterfacePtr) {
		nvapi_QueryInterfacePtr = (nvapi_QueryInterfaceType)GetProcAddress(nvDLL, "nvapi_QueryInterface");
		LogDebug("nvapi_QueryInterfacePtr @ 0x%p\n", nvapi_QueryInterfacePtr);
		if (!nvapi_QueryInterfacePtr) {
			LogInfo("Unable to call NvAPI_QueryInterface\n");
			nvapi_failed = true;
			return;
		}
	}

	// One shot, override custom settings.
	intptr_t ret = (intptr_t)nvapi_QueryInterfacePtr(0xc03cc03c);
	if (!warnedConvergenceTracking && (ret & 0xffffffff) != 0xeecc34ab) {
		LogInfo("  enabling convergence tracking in NVAPI wrapper failed.\n");
		warnedConvergenceTracking = true;
	}
}
void NvAPIEnableSeparationTracking()
{
	static bool warnedSeparationTracking = false;

	if (nvapi_failed)
		return;

	if (!nvDLL) {
		nvDLL = GetModuleHandle(L"nvapi64.dll");
		if (!nvDLL) {
			nvDLL = GetModuleHandle(L"nvapi.dll");
		}
		if (!nvDLL) {
			LogInfo("Can't get nvapi handle\n");
			nvapi_failed = true;
			return;
		}
	}
	if (!nvapi_QueryInterfacePtr) {
		nvapi_QueryInterfacePtr = (nvapi_QueryInterfaceType)GetProcAddress(nvDLL, "nvapi_QueryInterface");
		LogDebug("nvapi_QueryInterfacePtr @ 0x%p\n", nvapi_QueryInterfacePtr);
		if (!nvapi_QueryInterfacePtr) {
			LogInfo("Unable to call NvAPI_QueryInterface\n");
			nvapi_failed = true;
			return;
		}
	}

	intptr_t ret = (intptr_t)nvapi_QueryInterfacePtr(0xd03dd03d);
	if (!warnedSeparationTracking && (ret & 0xffffffff) != 0xeecc34ab) {
		LogInfo("  enabling separation tracking in NVAPI wrapper failed.\n");
		warnedSeparationTracking = true;
	}
}
void NvAPIEnableEyeSeparationTracking()
{
	static bool warnedEyeSeparationTracking = false;

	if (nvapi_failed)
		return;

	if (!nvDLL) {
		nvDLL = GetModuleHandle(L"nvapi64.dll");
		if (!nvDLL) {
			nvDLL = GetModuleHandle(L"nvapi.dll");
		}
		if (!nvDLL) {
			LogInfo("Can't get nvapi handle\n");
			nvapi_failed = true;
			return;
		}
	}
	if (!nvapi_QueryInterfacePtr) {
		nvapi_QueryInterfacePtr = (nvapi_QueryInterfaceType)GetProcAddress(nvDLL, "nvapi_QueryInterface");
		LogDebug("nvapi_QueryInterfacePtr @ 0x%p\n", nvapi_QueryInterfacePtr);
		if (!nvapi_QueryInterfacePtr) {
			LogInfo("Unable to call NvAPI_QueryInterface\n");
			nvapi_failed = true;
			return;
		}
	}

	intptr_t ret = (intptr_t)nvapi_QueryInterfacePtr(0xe03ee03e);
	if (!warnedEyeSeparationTracking && (ret & 0xffffffff) != 0xeecc34ab) {
		LogInfo("  enabling eye separation tracking in NVAPI wrapper failed.\n");
		warnedEyeSeparationTracking = true;
	}
}
void NvAPIResetStereoActiveTracking()
{
	static bool warnedResetStereoActiveTracking = false;

	if (nvapi_failed)
		return;

	if (!nvDLL) {
		nvDLL = GetModuleHandle(L"nvapi64.dll");
		if (!nvDLL) {
			nvDLL = GetModuleHandle(L"nvapi.dll");
		}
		if (!nvDLL) {
			LogInfo("Can't get nvapi handle\n");
			nvapi_failed = true;
			return;
		}
	}
	if (!nvapi_QueryInterfacePtr) {
		nvapi_QueryInterfacePtr = (nvapi_QueryInterfaceType)GetProcAddress(nvDLL, "nvapi_QueryInterface");
		LogDebug("nvapi_QueryInterfacePtr @ 0x%p\n", nvapi_QueryInterfacePtr);
		if (!nvapi_QueryInterfacePtr) {
			LogInfo("Unable to call NvAPI_QueryInterface\n");
			nvapi_failed = true;
			return;
		}
	}

	intptr_t ret = (intptr_t)nvapi_QueryInterfacePtr(0xf03ff03f);
	if (!warnedResetStereoActiveTracking && (ret & 0xffffffff) != 0xeecc34ab) {
		LogInfo("  reset stereo active tracking in NVAPI wrapper failed.\n");
		warnedResetStereoActiveTracking = true;
	}
}
void NvAPIResetConvergenceTracking()
{
	static bool warnedResetConvergenceTracking = false;

	if (nvapi_failed)
		return;

	if (!nvDLL) {
		nvDLL = GetModuleHandle(L"nvapi64.dll");
		if (!nvDLL) {
			nvDLL = GetModuleHandle(L"nvapi.dll");
		}
		if (!nvDLL) {
			LogInfo("Can't get nvapi handle\n");
			nvapi_failed = true;
			return;
		}
	}
	if (!nvapi_QueryInterfacePtr) {
		nvapi_QueryInterfacePtr = (nvapi_QueryInterfaceType)GetProcAddress(nvDLL, "nvapi_QueryInterface");
		LogDebug("nvapi_QueryInterfacePtr @ 0x%p\n", nvapi_QueryInterfacePtr);
		if (!nvapi_QueryInterfacePtr) {
			LogInfo("Unable to call NvAPI_QueryInterface\n");
			nvapi_failed = true;
			return;
		}
	}

	intptr_t ret = (intptr_t)nvapi_QueryInterfacePtr(0xa03dd03d);
	if (!warnedResetConvergenceTracking && (ret & 0xffffffff) != 0xeecc34ab) {
		LogInfo("  reset convergence tracking in NVAPI wrapper failed.\n");
		warnedResetConvergenceTracking = true;
	}
}
void NvAPIResetSeparationTracking()
{
	static bool warnedResetSeperationTracking = false;

	if (nvapi_failed)
		return;

	if (!nvDLL) {
		nvDLL = GetModuleHandle(L"nvapi64.dll");
		if (!nvDLL) {
			nvDLL = GetModuleHandle(L"nvapi.dll");
		}
		if (!nvDLL) {
			LogInfo("Can't get nvapi handle\n");
			nvapi_failed = true;
			return;
		}
	}
	if (!nvapi_QueryInterfacePtr) {
		nvapi_QueryInterfacePtr = (nvapi_QueryInterfaceType)GetProcAddress(nvDLL, "nvapi_QueryInterface");
		LogDebug("nvapi_QueryInterfacePtr @ 0x%p\n", nvapi_QueryInterfacePtr);
		if (!nvapi_QueryInterfacePtr) {
			LogInfo("Unable to call NvAPI_QueryInterface\n");
			nvapi_failed = true;
			return;
		}
	}

	intptr_t ret = (intptr_t)nvapi_QueryInterfacePtr(0xb03dd03d);
	if (!warnedResetSeperationTracking && (ret & 0xffffffff) != 0xeecc34ab) {
		LogInfo("  reset seperation tracking in NVAPI wrapper failed.\n");
		warnedResetSeperationTracking = true;
	}
}
void NvAPIResetEyeSeparationTracking()
{
	static bool warnedResetEyeSeparationTracking = false;

	if (nvapi_failed)
		return;

	if (!nvDLL) {
		nvDLL = GetModuleHandle(L"nvapi64.dll");
		if (!nvDLL) {
			nvDLL = GetModuleHandle(L"nvapi.dll");
		}
		if (!nvDLL) {
			LogInfo("Can't get nvapi handle\n");
			nvapi_failed = true;
			return;
		}
	}
	if (!nvapi_QueryInterfacePtr) {
		nvapi_QueryInterfacePtr = (nvapi_QueryInterfaceType)GetProcAddress(nvDLL, "nvapi_QueryInterface");
		LogDebug("nvapi_QueryInterfacePtr @ 0x%p\n", nvapi_QueryInterfacePtr);
		if (!nvapi_QueryInterfacePtr) {
			LogInfo("Unable to call NvAPI_QueryInterface\n");
			nvapi_failed = true;
			return;
		}
	}

	intptr_t ret = (intptr_t)nvapi_QueryInterfacePtr(0xc03dd03d);
	if (!warnedResetEyeSeparationTracking && (ret & 0xffffffff) != 0xeecc34ab) {
		LogInfo("  reset eye separation tracking in NVAPI wrapper failed.\n");
		warnedResetEyeSeparationTracking = true;
	}
}

// Based on the same function in the 3DMigoto loader
static bool check_file_description(const char *buf, const wchar_t *module_path)
{
	// https://docs.microsoft.com/en-gb/windows/desktop/api/winver/nf-winver-verqueryvaluea
	struct LANGANDCODEPAGE {
		WORD wLanguage;
		WORD wCodePage;
	} *translate_query;
	char id[50];
	char *file_description = "";
	unsigned int query_size, file_desc_size;
	HRESULT hr;
	unsigned i;

	if (!VerQueryValueA(buf, "\\VarFileInfo\\Translation", (void**)&translate_query, &query_size)) {
		LogInfo("3DMigoto file information query failed\n");
		return false;
	}

	// Look for the 3DMigoto file description in all language blocks... We
	// could likely skip the loop since we know which language it should be
	// in, but for some reason we included it in the German section which
	// we might want to change, so this way we won't need to adjust this
	// code if we do:
	for (i = 0; i < (query_size / sizeof(struct LANGANDCODEPAGE)); i++) {
		hr = _snprintf_s(id, 50, 50, "\\StringFileInfo\\%04x%04x\\FileDescription",
				translate_query[i].wLanguage,
				translate_query[i].wCodePage);
		if (FAILED(hr)) {
			LogInfo("3DMigoto file description query bugged\n");
			return false;
		}

		if (!VerQueryValueA(buf, id, (void**)&file_description, &file_desc_size)) {
			LogInfo("3DMigoto file description query failed\n");
			return false;
		}

		// Only look for the 3Dmigoto prefix. We've had a whitespace
		// error in the description for all this time that we want to
		// ignore, and we later might want to add other 3DMigoto DLLs
		printf("%s description: \"%s\"\n", module_path, file_description);
		if (!strncmp(file_description, "3Dmigoto", 8))
			return true;
	}

	return false;
}

static bool dx11_3dmigoto_present()
{
	// If both 3DMigoto's d3d9.dll and d3d11.dll are loaded into the same
	// process they will have both hooked into LoadLibrary to redirect nvapi,
	// which will result in our wrapper getting a reference to itself when it
	// tries to load the real nvapi. We need to detect this situation and avoid
	// both DLLs handling it. Since there are versions of d3d11.dll in the wild
	// without this detection and we can't rely on our users removing old
	// versions we need to handle this from the d3d9.dll, independently of
	// which DLL has been loaded first.

	static bool present = false;
	if (present)
		return true;

	HMODULE d3d11_handle = GetModuleHandleA("d3d11.dll");
	if (!d3d11_handle)
		return false;

	wchar_t module_path[MAX_PATH];
	GetModuleFileName(d3d11_handle, module_path, MAX_PATH);

	VS_FIXEDFILEINFO *query = NULL;
	DWORD pointless_handle = 0;
	unsigned int size;
	char *buf;

	size = GetFileVersionInfoSize(module_path, &pointless_handle);
	if (!size) {
		LogInfo("3DMigoto version size check failed\n");
		return false;
	}

	buf = new char[size];

	if (!GetFileVersionInfo(module_path, pointless_handle, size, buf)) {
		LogInfo("3DMigoto version info check failed\n");
		goto out_free;
	}

	if (!check_file_description(buf, module_path)) {
		LogInfo("Loaded module \"%ls\" is not 3DMigoto\n", module_path);
		goto out_free;
	}

	LogInfo("Loaded module \"%ls\" is 3DMigoto - disabling nvapi redirect in this DLL\n", module_path);
	present = true;

out_free:
	delete [] buf;
	return present;
}

// -----------------------------------------------------------------------------------------------
// This is our hook for LoadLibraryExW, handling the loading of our original_* libraries.
//
// The hooks are actually installed during load time in DLLMainHook, but called here
// whenever the game or system calls LoadLibraryExW.  These are here because at this
// point in the runtime, it is OK to do normal calls like LoadLibrary, and logging
// is available.  This is normal runtime.


static HMODULE ReplaceOnMatch(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags,
	LPCWSTR our_name, LPCWSTR library, bool check_3dmigoto_dx11)
{
	WCHAR fullPath[MAX_PATH];
	UINT ret;

	// We can use System32 for all cases, because it will be properly rerouted
	// to SysWow64 by LoadLibraryEx itself.

	ret = GetSystemDirectoryW(fullPath, ARRAYSIZE(fullPath));
	if (ret == 0 || ret >= ARRAYSIZE(fullPath))
		return NULL;
	wcscat_s(fullPath, MAX_PATH, L"\\");
	wcscat_s(fullPath, MAX_PATH, library);

	// Bypass the known expected call from our wrapped d3d11 & nvapi, where it needs
	// to call to the system to get APIs. This is a bit of a hack, but if the string
	// comes in as original_d3d11/nvapi/nvapi64, that's from us, and needs to switch
	// to the real one. The test string should have no path attached.

	if (_wcsicmp(lpLibFileName, our_name) == 0)
	{
		if (check_3dmigoto_dx11 && dx11_3dmigoto_present())
			return NULL;

		LogInfoW(L"Hooked_LoadLibraryExW switching to original dll: %s to %s.\n",
			lpLibFileName, fullPath);

		return fnOrigLoadLibraryExW(fullPath, hFile, dwFlags);
	}

	// For this case, we want to see if it's the game loading d3d11 or nvapi directly
	// from the system directory, and redirect it to the game folder if so, by stripping
	// the system path. This is to be case insensitive as we don't know if NVidia will
	// change that and otherwise break it it with a driver upgrade.

	if (_wcsicmp(lpLibFileName, fullPath) == 0)
	{
		if (check_3dmigoto_dx11 && dx11_3dmigoto_present())
			return NULL;

		LogInfoW(L"Replaced Hooked_LoadLibraryExW for: %s to %s.\n", lpLibFileName, library);

		return fnOrigLoadLibraryExW(library, hFile, dwFlags);
	}

	return NULL;
}

// Function called for every LoadLibraryExW call once we have hooked it.
// We want to look for overrides to System32 that we can circumvent.  This only happens
// in the current process, not system wide.
//
// We need to do two things here.  First, we need to bypass all calls that go
// directly to the System32 folder, because that will circumvent our wrapping
// of the d3d11 and nvapi APIs. The nvapi itself does this specifically as fake
// security to avoid proxy DLLs like us.
// Second, because we are now forcing all LoadLibraryExW calls back to the game
// folder, we need somehow to allow us access to the original dlls so that we can
// get the original proc addresses to call.  We do this with the original_* names
// passed in to this routine.
//
// There three use cases:
// x32 game on x32 OS
//	 LoadLibraryExW("C:\Windows\system32\d3d11.dll", NULL, 0)
//	 LoadLibraryExW("C:\Windows\system32\nvapi.dll", NULL, 0)
// x64 game on x64 OS
//	 LoadLibraryExW("C:\Windows\system32\d3d11.dll", NULL, 0)
//	 LoadLibraryExW("C:\Windows\system32\nvapi64.dll", NULL, 0)
// x32 game on x64 OS
//	 LoadLibraryExW("C:\Windows\SysWOW64\d3d11.dll", NULL, 0)
//	 LoadLibraryExW("C:\Windows\SysWOW64\nvapi.dll", NULL, 0)
//
// To be general and simplify the init, we are going to specifically do the bypass
// for all variants, even though we only know of this happening on x64 games.
//
// An important thing to remember here is that System32 is automatically rerouted
// to SysWow64 by the OS as necessary, so we can use System32 in all cases.
//
// It's not clear if we should also hook LoadLibraryW, but we don't have examples
// where we need that yet.


// The storage for the original routine so we can call through.

HMODULE(__stdcall *fnOrigLoadLibraryExW)(
	_In_       LPCWSTR lpLibFileName,
	_Reserved_ HANDLE  hFile,
	_In_       DWORD   dwFlags
	) = LoadLibraryExW;

HMODULE __stdcall Hooked_LoadLibraryExW(_In_ LPCWSTR lpLibFileName, _Reserved_ HANDLE hFile, _In_ DWORD dwFlags)
{
	HMODULE module;
	static bool hook_enabled = true;

	LogDebugW(L"   Hooked_LoadLibraryExW load: %s.\n", lpLibFileName);

	if (_wcsicmp(lpLibFileName, L"SUPPRESS_3DMIGOTO_REDIRECT") == 0) {
		// Something (like Origin's IGO32.dll hook in ntdll.dll
		// LdrLoadDll) is interfering with our hook and the caller is
		// about to attempt the load again using the full path. Disable
		// our redirect for the next call to make sure we don't give
		// them a reference to themselves. Subsequent calls will be
		// armed again in case we still need the redirect.
		hook_enabled = false;
		return NULL;
	}

	// Only do these overrides if they are specified in the d3dx.ini file.
	//  load_library_redirect=0 for off, allowing all through unchanged.
	//  load_library_redirect=1 for nvapi.dll override only, forced to game folder.
	//  load_library_redirect=2 for both d3d11.dll and nvapi.dll forced to game folder.
	// This flag can be set by the proxy loading, because it must be off in that case.
	if (hook_enabled) {

		if (G->load_library_redirect > 1)
		{
			module = ReplaceOnMatch(lpLibFileName, hFile, dwFlags, L"original_d3d9.dll", L"d3d9.dll", false);
			if (module)
				return module;
		}

		if (G->load_library_redirect > 0)
		{
			module = ReplaceOnMatch(lpLibFileName, hFile, dwFlags, L"original_nvapi64.dll", L"nvapi64.dll", true);
			if (module)
				return module;

			module = ReplaceOnMatch(lpLibFileName, hFile, dwFlags, L"original_nvapi.dll", L"nvapi.dll", true);
			if (module)
				return module;
		}
	} else
		hook_enabled = true;

	// Normal unchanged case.
	return fnOrigLoadLibraryExW(lpLibFileName, hFile, dwFlags);
}
