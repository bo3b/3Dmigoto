#include "DLLMainHookDX9.h"
#include "Globals.h"
#include "HookedD3DX.h"
//from GeDoSaTo: https://github.com/PeterTh/gedosato
#define GENERATE_INTERCEPT_HEADER(__name, __rettype, __convention, ...) \
typedef __rettype (__convention * __name##_FNType)(__VA_ARGS__); \
__name##_FNType True##__name, __name##Pointer; \
bool completed##__name##Detour = false; \
__rettype __convention Detoured##__name(__VA_ARGS__)
//end GeDoSaTo

// Add in Deviare in-proc for hooking system traps using a Detours approach.  We need access to the
// LoadLibrary call to fix the problem of nvapi.dll bypassing our local patches to the d3d11, when
// it does GetSystemDirectory to get System32, and directly access ..\System32\d3d11.dll
// If we get a failure, we'll just log it, it's not fatal.
//
// Pretty sure this is safe at DLLMain, because we are only accessing kernel32 stuff which is sure
// to be loaded.

// Used for other hooking. extern in the .h file.
// Only one instance of CNktHookLib is allowed for a given process.
CNktHookLib cHookMgr;

// Use this logging when at DLLMain which is too early to do anything with the file system.
#if _DEBUG
bool bLog = true;
#else
bool bLog = false;
#endif
#ifdef UNICODE
static HMODULE(WINAPI *trampoline_LoadLibraryExW)( _In_ LPCWSTR lpLibFileName,
		_Reserved_ HANDLE hFile, _In_ DWORD dwFlags) = LoadLibraryExW;
static HMODULE(WINAPI *trampoline_LoadLibraryW)(_In_ LPCWSTR lpLibFileName) = LoadLibraryW;
#else
static HMODULE(WINAPI *trampoline_LoadLibraryExA)(_In_ LPCWSTR lpLibFileName,
	_Reserved_ HANDLE hFile, _In_ DWORD dwFlags) = LoadLibraryExA;
static HMODULE(WINAPI *trampoline_LoadLibraryA)(_In_ LPCWSTR lpLibFileName) = LoadLibraryA;
#endif

static BOOL(WINAPI *trampoline_IsDebuggerPresent)(VOID) = IsDebuggerPresent;


// ----------------------------------------------------------------------------

static HMODULE ReplaceOnMatch(LPCWSTR lpLibFileName, HANDLE hFile,
		DWORD dwFlags, LPCWSTR our_name, LPCWSTR library)
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
		LogInfoW(L"Hooked_LoadLibraryExW switching to original dll: %s to %s.\n",
			lpLibFileName, fullPath);

		return trampoline_LoadLibraryExW(fullPath, hFile, dwFlags);
	}

	// For this case, we want to see if it's the game loading d3d11 or nvapi directly
	// from the system directory, and redirect it to the game folder if so, by stripping
	// the system path. This is to be case insensitive as we don't know if NVidia will
	// change that and otherwise break it it with a driver upgrade.

	if (_wcsicmp(lpLibFileName, fullPath) == 0)
	{
		LogInfoW(L"Replaced Hooked_LoadLibraryExW for: %s to %s.\n", lpLibFileName, library);

		return trampoline_LoadLibraryExW(library, hFile, dwFlags);
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
static void LogHooks(bool LogInfo_is_safe, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	if (LogInfo_is_safe)
		vLogInfo(fmt, ap);
	else if (bLog)
		NktHookLibHelpers::DebugVPrint(fmt, ap);

	va_end(ap);
}

static int InstallHook(HINSTANCE module, char *func, void **trampoline, void *hook, bool LogInfo_is_safe)
{
	SIZE_T hook_id;
	DWORD dwOsErr;
	void *fnOrig;

	// Early exit with error so the caller doesn't need to explicitly deal
	// with errors getting the module handle:
	if (!module)
		return 1;

	fnOrig = NktHookLibHelpers::GetProcedureAddress(module, func);
	if (fnOrig == NULL) {
		LogHooks(LogInfo_is_safe, "Failed to get address of %s\n", func);
		return 1;
	}

	dwOsErr = cHookMgr.Hook(&hook_id, trampoline, fnOrig, hook);
	if (dwOsErr) {
		LogHooks(LogInfo_is_safe, "Failed to hook %s: 0x%x\n", func, dwOsErr);
		return 1;
	}

	return 0;
}


void hook_d3dx9(HINSTANCE module)
{
	int fail = 0;
	fail |= InstallHook(module, "D3DXComputeNormalMap", (LPVOID*)&trampoline_D3DXComputeNormalMap, Hooked_D3DXComputeNormalMap, false);
	fail |= InstallHook(module, "D3DXCreateCubeTexture", (LPVOID*)&trampoline_D3DXCreateCubeTexture, Hooked_D3DXCreateCubeTexture, false);
#ifdef UNICODE
	fail |= InstallHook(module, "D3DXCreateCubeTextureFromFileW", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromFile, Hooked_D3DXCreateCubeTextureFromFile, false);
	fail |= InstallHook(module, "D3DXCreateCubeTextureFromFileExW", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromFileEx, Hooked_D3DXCreateCubeTextureFromFileEx, false);
	fail |= InstallHook(module, "D3DXCreateCubeTextureFromResourceW", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromResource, Hooked_D3DXCreateCubeTextureFromResource, false);
	fail |= InstallHook(module, "D3DXCreateCubeTextureFromResourceExW", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromResourceEx, Hooked_D3DXCreateCubeTextureFromResourceEx, false);
	fail |= InstallHook(module, "D3DXCreateTextureFromFileW", (LPVOID*)&trampoline_D3DXCreateTextureFromFile, Hooked_D3DXCreateTextureFromFile, false);
	fail |= InstallHook(module, "D3DXCreateTextureFromFileExW", (LPVOID*)&trampoline_D3DXCreateTextureFromFileEx, Hooked_D3DXCreateTextureFromFileEx, false);
	fail |= InstallHook(module, "D3DXCreateTextureFromResourceW", (LPVOID*)&trampoline_D3DXCreateTextureFromResource, Hooked_D3DXCreateTextureFromResource, false);
	fail |= InstallHook(module, "D3DXCreateTextureFromResourceExW", (LPVOID*)&trampoline_D3DXCreateTextureFromResourceEx, Hooked_D3DXCreateTextureFromResourceEx, false);
	fail |= InstallHook(module, "D3DXCreateVolumeTextureFromFileW", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromFile, Hooked_D3DXCreateVolumeTextureFromFile, false);
	fail |= InstallHook(module, "D3DXCreateVolumeTextureFromFileExW", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromFileEx, Hooked_D3DXCreateVolumeTextureFromFileEx, false);
	fail |= InstallHook(module, "D3DXCreateVolumeTextureFromResourceW", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromResource, Hooked_D3DXCreateVolumeTextureFromResource, false);
	fail |= InstallHook(module, "D3DXCreateVolumeTextureFromResourceExW", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromResourceEx, Hooked_D3DXCreateVolumeTextureFromResourceEx, false);
	fail |= InstallHook(module, "D3DXLoadSurfaceFromFileW", (LPVOID*)&trampoline_D3DXLoadSurfaceFromFile, Hooked_D3DXLoadSurfaceFromFile, false);
	fail |= InstallHook(module, "D3DXLoadSurfaceFromResourceW", (LPVOID*)&trampoline_D3DXLoadSurfaceFromResource, Hooked_D3DXLoadSurfaceFromResource, false);
	fail |= InstallHook(module, "D3DXLoadVolumeFromFileW", (LPVOID*)&trampoline_D3DXLoadVolumeFromFile, Hooked_D3DXLoadVolumeFromFile, false);
	fail |= InstallHook(module, "D3DXLoadVolumeFromResourceW", (LPVOID*)&trampoline_D3DXLoadVolumeFromResource, Hooked_D3DXLoadVolumeFromResource, false);
#else
	fail |= InstallHook(module, "D3DXCreateCubeTextureFromFileA", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromFile, Hooked_D3DXCreateCubeTextureFromFile, false);
	fail |= InstallHook(module, "D3DXCreateCubeTextureFromFileExA", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromFileEx, Hooked_D3DXCreateCubeTextureFromFileEx, false);
	fail |= InstallHook(module, "D3DXCreateCubeTextureFromResourceA", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromResource, Hooked_D3DXCreateCubeTextureFromResource, false);
	fail |= InstallHook(module, "D3DXCreateCubeTextureFromResourceExA", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromResourceEx, Hooked_D3DXCreateCubeTextureFromResourceEx, false);
	fail |= InstallHook(module, "D3DXCreateTextureFromFileA", (LPVOID*)&trampoline_D3DXCreateTextureFromFile, Hooked_D3DXCreateTextureFromFile, false);
	fail |= InstallHook(module, "D3DXCreateTextureFromFileExA", (LPVOID*)&trampoline_D3DXCreateTextureFromFileEx, Hooked_D3DXCreateTextureFromFileEx, false);
	fail |= InstallHook(module, "D3DXCreateTextureFromResourceA", (LPVOID*)&trampoline_D3DXCreateTextureFromResource, Hooked_D3DXCreateTextureFromResource, false);
	fail |= InstallHook(module, "D3DXCreateTextureFromResourceExA", (LPVOID*)&trampoline_D3DXCreateTextureFromResourceEx, Hooked_D3DXCreateTextureFromResourceEx, false);
	fail |= InstallHook(module, "D3DXCreateVolumeTextureFromFileA", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromFile, Hooked_D3DXCreateVolumeTextureFromFile, false);
	fail |= InstallHook(module, "D3DXCreateVolumeTextureFromFileExA", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromFileEx, Hooked_D3DXCreateVolumeTextureFromFileEx, false);
	fail |= InstallHook(module, "D3DXCreateVolumeTextureFromResourceA", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromResource, Hooked_D3DXCreateVolumeTextureFromResource, false);
	fail |= InstallHook(module, "D3DXCreateVolumeTextureFromResourceExA", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromResourceEx, Hooked_D3DXCreateVolumeTextureFromResourceEx, false);
	fail |= InstallHook(module, "D3DXLoadSurfaceFromFileA", (LPVOID*)&trampoline_D3DXLoadSurfaceFromFile, Hooked_D3DXLoadSurfaceFromFile, false);
	fail |= InstallHook(module, "D3DXLoadSurfaceFromResourceA", (LPVOID*)&trampoline_D3DXLoadSurfaceFromResource, Hooked_D3DXLoadSurfaceFromResource, false);
	fail |= InstallHook(module, "D3DXLoadVolumeFromFileA", (LPVOID*)&trampoline_D3DXLoadVolumeFromFile, Hooked_D3DXLoadVolumeFromFile, false);
	fail |= InstallHook(module, "D3DXLoadVolumeFromResourceA", (LPVOID*)&trampoline_D3DXLoadVolumeFromResource, Hooked_D3DXLoadVolumeFromResource, false);
#endif

	fail |= InstallHook(module, "D3DXCreateCubeTextureFromFileInMemory", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromFileInMemory, Hooked_D3DXCreateCubeTextureFromFileInMemory, false);
	fail |= InstallHook(module, "D3DXCreateCubeTextureFromFileInMemoryEx", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromFileInMemoryEx, Hooked_D3DXCreateCubeTextureFromFileInMemoryEx, false);

	fail |= InstallHook(module, "D3DXCreateTexture", (LPVOID*)&trampoline_D3DXCreateTexture, Hooked_D3DXCreateTexture, false);

	fail |= InstallHook(module, "D3DXCreateTextureFromFileInMemory", (LPVOID*)&trampoline_D3DXCreateTextureFromFileInMemory, Hooked_D3DXCreateTextureFromFileInMemory, false);
	fail |= InstallHook(module, "D3DXCreateTextureFromFileInMemoryEx", (LPVOID*)&trampoline_D3DXCreateTextureFromFileInMemoryEx, Hooked_D3DXCreateTextureFromFileInMemoryEx, false);

	fail |= InstallHook(module, "D3DXCreateVolumeTexture", (LPVOID*)&trampoline_D3DXCreateVolumeTexture, Hooked_D3DXCreateVolumeTexture, false);

	fail |= InstallHook(module, "D3DXCreateVolumeTextureFromFileInMemory", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromFileInMemory, Hooked_D3DXCreateVolumeTextureFromFileInMemory, false);
	fail |= InstallHook(module, "D3DXCreateVolumeTextureFromFileInMemoryEx", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromFileInMemoryEx, Hooked_D3DXCreateVolumeTextureFromFileInMemoryEx, false);

	fail |= InstallHook(module, "D3DXFillCubeTexture", (LPVOID*)&trampoline_D3DXFillCubeTexture, Hooked_D3DXFillCubeTexture, false);
	fail |= InstallHook(module, "D3DXFillCubeTextureTX", (LPVOID*)&trampoline_D3DXFillCubeTextureTX, Hooked_D3DXFillCubeTextureTX, false);
	fail |= InstallHook(module, "D3DXFillTexture", (LPVOID*)&trampoline_D3DXFillTexture, Hooked_D3DXFillTexture, false);
	fail |= InstallHook(module, "D3DXFillTextureTX", (LPVOID*)&trampoline_D3DXFillTextureTX, Hooked_D3DXFillTextureTX, false);
	fail |= InstallHook(module, "D3DXFillVolumeTexture", (LPVOID*)&trampoline_D3DXFillVolumeTexture, Hooked_D3DXFillVolumeTexture, false);
	fail |= InstallHook(module, "D3DXFillVolumeTextureTX", (LPVOID*)&trampoline_D3DXFillVolumeTextureTX, Hooked_D3DXFillVolumeTextureTX, false);
	fail |= InstallHook(module, "D3DXFilterTexture", (LPVOID*)&trampoline_D3DXFilterTexture, Hooked_D3DXFilterTexture, false);

	fail |= InstallHook(module, "D3DXLoadSurfaceFromFileInMemory", (LPVOID*)&trampoline_D3DXLoadSurfaceFromFileInMemory, Hooked_D3DXLoadSurfaceFromFileInMemory, false);
	fail |= InstallHook(module, "D3DXLoadSurfaceFromMemory", (LPVOID*)&trampoline_D3DXLoadSurfaceFromMemory, Hooked_D3DXLoadSurfaceFromMemory, false);

	fail |= InstallHook(module, "D3DXLoadSurfaceFromSurface", (LPVOID*)&trampoline_D3DXLoadSurfaceFromSurface, Hooked_D3DXLoadSurfaceFromSurface, false);

	fail |= InstallHook(module, "D3DXLoadVolumeFromFileInMemory", (LPVOID*)&trampoline_D3DXLoadVolumeFromFileInMemory, Hooked_D3DXLoadVolumeFromFileInMemory, false);
	fail |= InstallHook(module, "D3DXLoadVolumeFromMemory", (LPVOID*)&trampoline_D3DXLoadVolumeFromMemory, Hooked_D3DXLoadVolumeFromMemory, false);

	fail |= InstallHook(module, "D3DXLoadVolumeFromVolume", (LPVOID*)&trampoline_D3DXLoadVolumeFromVolume, Hooked_D3DXLoadVolumeFromVolume, false);

	// Next hook IsDebuggerPresent to force it false. Same Kernel32.dll
	// fail |= InstallHook(hKernel32, "IsDebuggerPresent", (LPVOID*)&trampoline_IsDebuggerPresent, Hooked_IsDebuggerPresent, false);

	if (fail)
	{
		LogHooks(false, "Failed to install hook for D3D9X function using Deviare in-proc\n");
	}

	LogHooks(false, "Succeeded in installing hooks for D3D9X using Deviare in-proc\n");
}
#ifdef UNICODE
static HMODULE WINAPI Hooked_LoadLibraryW(_In_ LPCWSTR lpLibFileName)
{
	LogDebugW(L"   Hooked_LoadLibraryW load: %s.\n", lpLibFileName);
	// Normal unchanged case.
	return trampoline_LoadLibraryW(lpLibFileName);

}


static HMODULE WINAPI Hooked_LoadLibraryExW(_In_ LPCWSTR lpLibFileName, _Reserved_ HANDLE hFile, _In_ DWORD dwFlags)
{
	HMODULE module;
	static bool hook_enabled = true;

	// This is late enough that we can look for standard logging.
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
			module = ReplaceOnMatch(lpLibFileName, hFile, dwFlags, L"original_d3d9.dll", L"d3d9.dll");
			if (module)
				return module;
		}

		if (G->load_library_redirect > 0)
		{
			module = ReplaceOnMatch(lpLibFileName, hFile, dwFlags, L"original_nvapi64.dll", L"nvapi64.dll");
			if (module)
				return module;

			module = ReplaceOnMatch(lpLibFileName, hFile, dwFlags, L"original_nvapi.dll", L"nvapi.dll");
			if (module)
				return module;
		}
	} else
		hook_enabled = true;

	// Normal unchanged case.
	return trampoline_LoadLibraryExW(lpLibFileName, hFile, dwFlags);
}
#else
static HMODULE WINAPI Hooked_LoadLibraryA(_In_ LPCWSTR lpLibFileName)
{
	// This is late enough that we can look for standard logging.
	LogDebugW(L"   Hooked_LoadLibraryW load: %s.\n", lpLibFileName);
	// Normal unchanged case.
	return trampoline_LoadLibraryA(lpLibFileName);

}
static HMODULE WINAPI Hooked_LoadLibraryExA(_In_ LPCWSTR lpLibFileName, _Reserved_ HANDLE hFile, _In_ DWORD dwFlags)
{
	HMODULE module;
	static bool hook_enabled = true;

	// This is late enough that we can look for standard logging.
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
			module = ReplaceOnMatch(lpLibFileName, hFile, dwFlags, L"original_d3d9.dll", L"d3d9.dll");
			if (module)
				return module;
		}

		if (G->load_library_redirect > 0)
		{
			module = ReplaceOnMatch(lpLibFileName, hFile, dwFlags, L"original_nvapi64.dll", L"nvapi64.dll");
			if (module)
				return module;

			module = ReplaceOnMatch(lpLibFileName, hFile, dwFlags, L"original_nvapi.dll", L"nvapi.dll");
			if (module)
				return module;
		}
	}
	else
		hook_enabled = true;
	// Normal unchanged case.
	return trampoline_LoadLibraryExA(lpLibFileName, hFile, dwFlags);
}
#endif
static bool InstallHooks()
{
	HINSTANCE hKernel32;
	int fail = 0;

	LogHooks(false, "Attempting to hook LoadLibraryExW using Deviare in-proc.\n");
	cHookMgr.SetEnableDebugOutput(bLog);

	hKernel32 = NktHookLibHelpers::GetModuleBaseAddress(L"Kernel32.dll");
	if (hKernel32 == NULL)
	{
		LogHooks(false, "Failed to get Kernel32 module for Loadlibrary hook.\n");
		return false;
	}

#ifdef UNICODE
	fail |= InstallHook(hKernel32, "LoadLibraryExW", (LPVOID*)&trampoline_LoadLibraryExW, Hooked_LoadLibraryExW, false);
	fail |= InstallHook(hKernel32, "LoadLibraryW", (LPVOID*)&trampoline_LoadLibraryW, Hooked_LoadLibraryW, false);
#else
	fail |= InstallHook(hKernel32, "LoadLibraryExA", (LPVOID*)&trampoline_LoadLibraryExW, Hooked_LoadLibraryExA, false);
	fail |= InstallHook(hKernel32, "LoadLibraryA", (LPVOID*)&trampoline_LoadLibraryW, Hooked_LoadLibraryA, false);
#endif

	// Next hook IsDebuggerPresent to force it false. Same Kernel32.dll
	// fail |= InstallHook(hKernel32, "IsDebuggerPresent", (LPVOID*)&trampoline_IsDebuggerPresent, Hooked_IsDebuggerPresent, false);

	if (fail)
	{
		LogHooks(false, "InstallHooks for LoadLibraryExW using Deviare in-proc failed\n");
		return false;
	}

	LogHooks(false, "InstallHooks for LoadLibraryExW using Deviare in-proc succeeded\n");

	return true;
}
// Function to be called whenever real IsDebuggerPresent is called, so that we can force it to false.

//static BOOL WINAPI Hooked_IsDebuggerPresent()
//{
//	return trampoline_IsDebuggerPresent();
//}

static BOOL(WINAPI *trampoline_SetWindowPos)(_In_ HWND hWnd, _In_opt_ HWND hWndInsertAfter,
		_In_ int X, _In_ int Y, _In_ int cx, _In_ int cy, _In_ UINT uFlags)
	= SetWindowPos;

static BOOL WINAPI Hooked_SetWindowPos(
    _In_ HWND hWnd,
    _In_opt_ HWND hWndInsertAfter,
    _In_ int X,
    _In_ int Y,
    _In_ int cx,
    _In_ int cy,
    _In_ UINT uFlags)
{
	LogDebug("  Hooked_SetWindowPos called, width = %d, height = %d \n", cx, cy);

	if (G->SCREEN_UPSCALING != 0) {
		if (cx != 0 && cy != 0) {
			cx = G->SCREEN_WIDTH;
			cy = G->SCREEN_HEIGHT;
			X = 0;
			Y = 0;
		}
	}
	else if (G->SCREEN_FULLSCREEN == 2) {
		// Do nothing - passing this call through could change the game
		// to a borderless window. Needed for The Witness.
		return true;
	}

	return trampoline_SetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

void InstallSetWindowPosHook()
{
	HINSTANCE hUser32;
	static bool hook_installed = false;
	int fail = 0;

	// Only attempt to hook it once:
	if (hook_installed)
		return;
	hook_installed = true;

	hUser32 = NktHookLibHelpers::GetModuleBaseAddress(L"User32.dll");
	fail |= InstallHook(hUser32, "SetWindowPos", (void**)&trampoline_SetWindowPos, Hooked_SetWindowPos, true);

	if (fail) {
		LogInfo("Failed to hook SetWindowPos for full_screen=2\n");
		BeepFailure2();
		return;
	}

	LogInfo("Successfully hooked SetWindowPos for full_screen=2\n");
	return;
}

//////////////////////////// HARDWARE MOUSE CURSOR SUPPRESSION //////////////////////////
// To suppress the hardware mouse cursor you would think we just have to call
// ShowCursor(FALSE) somewhere to decrement the count by one, but doing so from
// DLL initialization permanently disables the cursor (in Dreamfall Chapters at
// least), and calling it from RunFrameActions or elsewhere has no effect
// (though the call does not indicate an error and does seem to affect a
// counter).
//
// My first attempt to solve this was to hook ShowCursor and keep a separate
// counter for the software curser visibility, but it turns out the Steam
// Overlay also hooks this function, but has a bug where it calls the original
// vs hooked versions inconsistently when showing vs hiding the overlay,
// leading to it reading the visibility count of the *hardware* cursor when the
// overlay is shown, then setting the visibility count of the *software* cursor
// to match when the overlay is hidden, leading to the cursor disappearing.
//
// This is a second attempt to suppress the hardware cursor - this time we
// leave the visibility count alone and instead replace the cursor icon with a
// completely invisible one. Since the hardware cursor technically is
// displayed, the visibility counts for software and hardware cursors match, so
// we no longer need to manage them separately. We hook into SetCursor,
// GetCursor and GetCursorInfo to keep a handle of the cursor the game set and
// return it whenever something (including our own software mouse
// implementation) asks for it.

HCURSOR current_cursor = NULL;

typedef LRESULT(WINAPI *lpfnDefWindowProc)(_In_ HWND hWnd,
	_In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam);

static lpfnDefWindowProc trampoline_DefWindowProcA = DefWindowProcA;
static lpfnDefWindowProc trampoline_DefWindowProcW = DefWindowProcW;

static HCURSOR(WINAPI *trampoline_SetCursor)(_In_opt_ HCURSOR hCursor) = SetCursor;
static HCURSOR(WINAPI *trampoline_GetCursor)(void) = GetCursor;
static BOOL(WINAPI *trampoline_GetCursorInfo)(_Inout_ PCURSORINFO pci) = GetCursorInfo;
static BOOL(WINAPI* trampoline_SetCursorPos)(_In_ int X, _In_ int Y) = SetCursorPos;
static BOOL(WINAPI* trampoline_GetCursorPos)(_Out_ LPPOINT lpPoint) = GetCursorPos;
static BOOL(WINAPI* trampoline_ScreenToClient)(_In_ HWND hWnd,LPPOINT lpPoint) = ScreenToClient;
static BOOL(WINAPI* trampoline_GetClientRect)(_In_ HWND hWnd, _Out_ LPRECT lpRect) = GetClientRect;
static BOOL(WINAPI *trampoline_GetWindowRect)(_In_ HWND hWnd, _Out_ LPRECT lpRect)= GetWindowRect;
static int(WINAPI *trampoline_MapWindowPoints)(HWND hWndFrom, HWND hWndTo, LPPOINT lpPoints,UINT cPoints) = MapWindowPoints;

static int(WINAPI *trampoline_ShowCursor)(_In_ BOOL bShow) = ShowCursor;

// This routine creates an invisible cursor that we can set whenever we are
// hiding the cursor. It is static, so will only be created the first time this
// is called.
static HCURSOR InvisibleCursor()
{
	static HCURSOR cursor = NULL;
	int width, height;
	unsigned pitch, size;
	char *and, *xor;

	if (!cursor) {
		width = GetSystemMetrics(SM_CXCURSOR);
		height = GetSystemMetrics(SM_CYCURSOR);
		pitch = ((width + 31) / 32) * 4;
		size = pitch * height;

		and = new char[size];
		xor = new char[size];

		memset(and, 0xff, size);
		memset(xor, 0x00, size);

		cursor = CreateCursor(GetModuleHandle(NULL), 0, 0, width, height, and, xor);

		delete [] and;
		delete [] xor;
	}

	return cursor;
}


static int WINAPI Hooked_ShowCursor(_In_ BOOL bShow) {
	G->SET_CURSOR_UPDATE_REQUIRED(1);
	LogDebug("  Hooked_ShowCursor called \n");
	return trampoline_ShowCursor(bShow);
}

// We hook the SetCursor call so that we can catch the current cursor that the
// game has set and return it in the GetCursorInfo call whenever the software
// cursor is visible but the hardware cursor is not.
static HCURSOR WINAPI Hooked_SetCursor(
    _In_opt_ HCURSOR hCursor)
{
	if (current_cursor != hCursor)
		G->SET_CURSOR_UPDATE_REQUIRED(1);
	current_cursor = hCursor;
	LogDebug("  Hooked_SetCursor called \n");
	if (G->hide_cursor)
		return trampoline_SetCursor(InvisibleCursor());
	else
		return trampoline_SetCursor(hCursor);
}

static HCURSOR WINAPI Hooked_GetCursor(void)
{
	LogDebug("  Hooked_GetCursor called \n");
	if (G->hide_cursor)
		return current_cursor;
	else
		return trampoline_GetCursor();
}

static BOOL WINAPI HideCursor_GetCursorInfo(
    _Inout_ PCURSORINFO pci)
{
	LogDebug(" HideCursor_GetCursorInfo called \n");
	BOOL rc = trampoline_GetCursorInfo(pci);

	if (rc && (pci->flags & CURSOR_SHOWING))
		pci->hCursor = current_cursor;

	return rc;
}

static BOOL WINAPI Hooked_GetCursorInfo(
    _Inout_ PCURSORINFO pci)
{
	LogDebug("  Hooked_GetCursorInfo called \n");
	BOOL rc = HideCursor_GetCursorInfo(pci);
	RECT client;

	if (rc && G->SCREEN_UPSCALING > 0 && trampoline_GetClientRect(G->hWnd(), &client) && client.right && client.bottom)
	{
		pci->ptScreenPos.x = pci->ptScreenPos.x * G->GAME_INTERNAL_WIDTH() / client.right;
		pci->ptScreenPos.y = pci->ptScreenPos.y * G->GAME_INTERNAL_HEIGHT() / client.bottom;

		LogDebug("  right = %d, bottom = %d, game width = %d, game height = %d\n", client.right, client.bottom, G->GAME_INTERNAL_WIDTH(), G->GAME_INTERNAL_HEIGHT());
	}

	return rc;
}

BOOL WINAPI CursorUpscalingBypass_GetCursorInfo(
    _Inout_ PCURSORINFO pci)
{
	if (G->cursor_upscaling_bypass)
	{
		// Still need to process hide_cursor logic:
		return HideCursor_GetCursorInfo(pci);
	}
	return GetCursorInfo(pci);
}

static BOOL WINAPI Hooked_ScreenToClient(_In_ HWND hWnd, LPPOINT lpPoint)
{
	LogDebug("Hooked_ScreenToClient called \n");
	BOOL rc;
	RECT client;
	bool translate = G->SCREEN_UPSCALING > 0 && lpPoint
		&& trampoline_GetClientRect(G->hWnd(), &client)
		&& client.right && client.bottom
		&& G->GAME_INTERNAL_WIDTH() && G->GAME_INTERNAL_HEIGHT();

	if (translate)
	{
		// Scale back to original screen coordinates:
		lpPoint->x = lpPoint->x * client.right / G->GAME_INTERNAL_WIDTH();
		lpPoint->y = lpPoint->y * client.bottom / G->GAME_INTERNAL_HEIGHT();
	}

	rc = trampoline_ScreenToClient(hWnd, lpPoint);

	if (translate)
	{
		// Now scale to fake game coordinates:
		lpPoint->x = lpPoint->x * G->GAME_INTERNAL_WIDTH() / client.right;
		lpPoint->y = lpPoint->y * G->GAME_INTERNAL_HEIGHT() / client.bottom;

		LogDebug("  translate = %d, x = %d, y = %d, right = %d, bottom = %d, game width = %d, game height = %d\n", translate, lpPoint->x, lpPoint->y, client.right, client.bottom, G->GAME_INTERNAL_WIDTH(), G->GAME_INTERNAL_HEIGHT());
	}


	return rc;
}
static int WINAPI Hooked_MapWindowPoints(HWND hWndFrom, HWND hWndTo, LPPOINT lpPoints, UINT cPoints)
{
	LogDebug("  Hooked_MapWindowPoints called \n");
	int rc;
	RECT client;
	bool translate = G->adjust_map_window_points && G->SCREEN_UPSCALING > 0 && (hWndFrom == NULL || hWndFrom == HWND_DESKTOP) && hWndTo != NULL && hWndTo != HWND_DESKTOP && lpPoints
		&& trampoline_GetClientRect(G->hWnd(), &client)
		&& client.right && client.bottom
		&& G->GAME_INTERNAL_WIDTH() && G->GAME_INTERNAL_HEIGHT();

	if (translate)
	{
		//lpPoints[0].x = 1;
		// Scale back to original screen coordinates:
		lpPoints->x = lpPoints->x * client.right / G->GAME_INTERNAL_WIDTH();
		lpPoints->y = lpPoints->y * client.bottom / G->GAME_INTERNAL_HEIGHT();
	}

	rc = trampoline_MapWindowPoints(hWndFrom, hWndTo, lpPoints, cPoints);

	if (translate)
	{
		// Now scale to fake game coordinates:
		lpPoints->x = lpPoints->x * G->GAME_INTERNAL_WIDTH() / client.right;
		lpPoints->y = lpPoints->y * G->GAME_INTERNAL_HEIGHT() / client.bottom;
		LogDebug("  translate = %d, x = %d, y = %d, right = %d, bottom = %d, game width = %d, game height = %d\n", translate, lpPoints->x, lpPoints->y, client.right, client.bottom, G->GAME_INTERNAL_WIDTH(), G->GAME_INTERNAL_HEIGHT());
	}



	return rc;
}

BOOL WINAPI CursorUpscalingBypass_ScreenToClient(_In_ HWND hWnd, LPPOINT lpPoint)
{
	if (G->cursor_upscaling_bypass)
		return trampoline_ScreenToClient(hWnd, lpPoint);
	return ScreenToClient(hWnd, lpPoint);
}

static BOOL WINAPI Hooked_GetClientRect(_In_ HWND hWnd, _Out_ LPRECT lpRect)
{

	BOOL rc = trampoline_GetClientRect(hWnd, lpRect);

	if (G->upscaling_hooks_armed && rc && G->SCREEN_UPSCALING > 0 && lpRect != NULL && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1)
	{
			lpRect->right = G->GAME_INTERNAL_WIDTH();
			lpRect->bottom = G->GAME_INTERNAL_HEIGHT();
	}
	LogDebug("  Hooked_GetClientRect called right = %d, bottom = %d\n", lpRect->right, lpRect->bottom);

	return rc;
}
static BOOL WINAPI Hooked_GetWindowRect(_In_ HWND hWnd, _Out_ LPRECT lpRect)
{

	BOOL rc = trampoline_GetWindowRect(hWnd, lpRect);

	if (G->adjust_get_window_rect && G->upscaling_hooks_armed && rc && G->SCREEN_UPSCALING > 0 && lpRect != NULL && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1)
	{
			lpRect->right = G->GAME_INTERNAL_WIDTH();
			lpRect->bottom = G->GAME_INTERNAL_HEIGHT();
	}
	LogDebug("  Hooked_GetWindowRect called right = %d, bottom = %d\n", lpRect->right, lpRect->bottom);

	return rc;
}

BOOL WINAPI CursorUpscalingBypass_GetClientRect(_In_ HWND hWnd, _Out_ LPRECT lpRect)
{
	if (G->cursor_upscaling_bypass)
		return trampoline_GetClientRect(hWnd, lpRect);
	return GetClientRect(hWnd, lpRect);
}

static BOOL WINAPI Hooked_GetCursorPos(_Out_ LPPOINT lpPoint)
{
	LogDebug("Hooked_GetCursorPos called \n");
	BOOL res = trampoline_GetCursorPos(lpPoint);
	RECT client;

	if (G->adjust_cursor_pos && lpPoint && res && G->SCREEN_UPSCALING > 0 && trampoline_GetClientRect(G->hWnd(), &client) && client.right && client.bottom && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1)
	{
		// This should work with all games that uses this function to gatter the mouse coords
		// Tested with witcher 3 and dreamfall chapters
		// TODO: Maybe there is a better way than use globals for the original game resolution
		lpPoint->x = lpPoint->x * G->GAME_INTERNAL_WIDTH() / client.right;
		lpPoint->y = lpPoint->y * G->GAME_INTERNAL_HEIGHT() / client.bottom;
		LogDebug("  x = %d, y = %d, right = %d, bottom = %d, game width = %d, game height = %d\n", lpPoint->x, lpPoint->y, client.right, client.bottom, G->GAME_INTERNAL_WIDTH(), G->GAME_INTERNAL_HEIGHT());
	}


	return res;
}

static BOOL WINAPI Hooked_SetCursorPos(_In_ int X, _In_ int Y)
{
	RECT client;

	if (G->adjust_cursor_pos && G->SCREEN_UPSCALING > 0 && trampoline_GetClientRect(G->hWnd(), &client) && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1)
	{
		// TODO: Maybe there is a better way than use globals for the original game resolution
		const int new_x = X * client.right / G->GAME_INTERNAL_WIDTH();
		const int new_y = Y * client.bottom / G->GAME_INTERNAL_HEIGHT();
		LogDebug("  Hooked_SetCursorPos called x = %d, y = %d, right = %d, bottom = %d, game width = %d, game height = %d\n", new_x, new_y, client.right, client.bottom, G->GAME_INTERNAL_WIDTH(), G->GAME_INTERNAL_HEIGHT());
		return trampoline_SetCursorPos(new_x, new_y);
	}
	else
		LogDebug("  Hooked_SetCursorPos called x = %d, y = %d", X, Y);
		return trampoline_SetCursorPos(X, Y);
}

// DefWindowProc can bypass our SetCursor hook, which means that some games
// such would continue showing the hardware cursor, and our knowledge of what
// cursor was supposed to be set may be inaccurate (e.g. Akiba's Trip doesn't
// hide the cursor and sometimes the software cursor uses the busy cursor
// instead of the arrow cursor). We fix this by hooking DefWindowProc and
// processing WM_SETCURSOR message just as the original DefWindowProc would
// have done, but without bypassing our SetCursor hook.
//
// An alternative to hooking DefWindowProc in this manner might be to use
// SetWindowsHookEx since it can also hook window messages.
static LRESULT WINAPI Hooked_DefWindowProc(
	_In_ HWND   hWnd,
	_In_ UINT   Msg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam,
	lpfnDefWindowProc trampoline_DefWindowProc)
{

	HWND parent = NULL;
	HCURSOR cursor = NULL;
	LPARAM ret = 0;

	if (Msg == WM_SETCURSOR) {
		// XXX: Should we use GetParent or GetAncestor? GetParent can
		// return an "owner" window, while GetAncestor only returns
		// parents... Not sure which the official DefWindowProc uses,
		// but I suspect the answer is GetAncestor, so go with that:
		parent = GetAncestor(hWnd, GA_PARENT);

		if (parent) {
			// Pass the message to the parent window, just like the
			// real DefWindowProc does. This may call back in here
			// if the parent also doesn't handle this message, and
			// we stop processing if the parent handled it.
			ret = SendMessage(parent, Msg, wParam, lParam);
			if (ret)
				return ret;
		}

		// If the mouse is in the client area and the window class has
		// a cursor associated with it we set that. This will call into
		// our hooked version of SetCursor (whereas the real
		// DefWindowProc would bypass that) so that we can track the
		// current cursor set by the game and force the hardware cursor
		// to remain hidden.
		if ((lParam & 0xffff) == HTCLIENT) {
			cursor = (HCURSOR)GetClassLongPtr(hWnd, GCLP_HCURSOR);
			if (cursor)
				SetCursor(cursor);
		} else {
			// Not in client area. We could continue emulating
			// DefWindowProc by setting an arrow cursor, bypassing
			// our hook to set the *real* hardware cursor, but
			// since the real DefWindowProc already bypasses our
			// hook let's just call that and allow it to take care
			// of any other edge cases we may not know about (like
			// HTERROR):
			return trampoline_DefWindowProc(hWnd, Msg, wParam, lParam);
		}

		// Return false to allow children to set their class cursor:
		return FALSE;
	}

	return trampoline_DefWindowProc(hWnd, Msg, wParam, lParam);
}

static LRESULT WINAPI Hooked_DefWindowProcA(_In_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	return Hooked_DefWindowProc(hWnd, Msg, wParam, lParam, trampoline_DefWindowProcA);
}

static LRESULT WINAPI Hooked_DefWindowProcW(_In_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	return Hooked_DefWindowProc(hWnd, Msg, wParam, lParam, trampoline_DefWindowProcW);
}

//from GeDoSaTo: https://github.com/PeterTh/gedosato
// Display Changing /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {
	template<typename DEVM>
	void fixDevMode(DEVM* lpDevMode) {
		if (lpDevMode->dmPelsWidth == G->GAME_INTERNAL_WIDTH() && lpDevMode->dmPelsHeight == G->GAME_INTERNAL_HEIGHT()) {
			LogDebug(" -> Overriding\n");
			if (G->SCREEN_WIDTH != -1)
				lpDevMode->dmPelsWidth = G->SCREEN_WIDTH;
			if (G->SCREEN_HEIGHT != -1)
				lpDevMode->dmPelsHeight = G->SCREEN_HEIGHT;
			if (G->SCREEN_REFRESH != -1)
				lpDevMode->dmDisplayFrequency = G->SCREEN_REFRESH;
		}
	}
}

GENERATE_INTERCEPT_HEADER(ChangeDisplaySettingsExA, LONG, WINAPI, _In_opt_ LPCSTR lpszDeviceName, _In_opt_ DEVMODEA* lpDevMode, _Reserved_ HWND hwnd, _In_ DWORD dwflags, _In_opt_ LPVOID lParam) {
	LogDebug("ChangeDisplaySettingsExA\n");
	if (!G->adjust_display_settings || !(G->SCREEN_UPSCALING > 0)) return TrueChangeDisplaySettingsExA(lpszDeviceName, lpDevMode, hwnd, dwflags, lParam);
	if (lpDevMode == NULL) return TrueChangeDisplaySettingsExA(lpszDeviceName, NULL, hwnd, dwflags, lParam);
	DEVMODEA copy = *lpDevMode;
	fixDevMode(&copy);
	return TrueChangeDisplaySettingsExA(lpszDeviceName, &copy, hwnd, dwflags, lParam);
}
GENERATE_INTERCEPT_HEADER(ChangeDisplaySettingsExW, LONG, WINAPI, _In_opt_ LPCWSTR lpszDeviceName, _In_opt_ DEVMODEW* lpDevMode, _Reserved_ HWND hwnd, _In_ DWORD dwflags, _In_opt_ LPVOID lParam) {
	LogDebug("ChangeDisplaySettingsExW\n");
	if (!G->adjust_display_settings || !(G->SCREEN_UPSCALING > 0)) return TrueChangeDisplaySettingsExW(lpszDeviceName, lpDevMode, hwnd, dwflags, lParam);
	if (lpDevMode == NULL) return TrueChangeDisplaySettingsExW(lpszDeviceName, NULL, hwnd, dwflags, lParam);
	DEVMODEW copy = *lpDevMode;
	fixDevMode(&copy);
	return TrueChangeDisplaySettingsExW(lpszDeviceName, &copy, hwnd, dwflags, lParam);
}
// System Metrics ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* SystemMetricToString(int metric) {
	switch (metric) {
	case SM_CXSCREEN: return "SM_CXSCREEN";
	case SM_CYSCREEN: return "SM_CYSCREEN";
	case SM_CXVSCROLL: return "SM_CXVSCROLL";
	case SM_CYHSCROLL: return "SM_CYHSCROLL";
	case SM_CYCAPTION: return "SM_CYCAPTION";
	case SM_CXBORDER: return "SM_CXBORDER";
	case SM_CYBORDER: return "SM_CYBORDER";
	case SM_CXDLGFRAME: return "SM_CXDLGFRAME";
	case SM_CYDLGFRAME: return "SM_CYDLGFRAME";
	case SM_CYVTHUMB: return "SM_CYVTHUMB";
	case SM_CXHTHUMB: return "SM_CXHTHUMB";
	case SM_CXICON: return "SM_CXICON";
	case SM_CYICON: return "SM_CYICON";
	case SM_CXCURSOR: return "SM_CXCURSOR";
	case SM_CYCURSOR: return "SM_CYCURSOR";
	case SM_CYMENU: return "SM_CYMENU";
	case SM_CXFULLSCREEN: return "SM_CXFULLSCREEN";
	case SM_CYFULLSCREEN: return "SM_CYFULLSCREEN";
	case SM_CYKANJIWINDOW: return "SM_CYKANJIWINDOW";
	case SM_MOUSEPRESENT: return "SM_MOUSEPRESENT";
	case SM_CYVSCROLL: return "SM_CYVSCROLL";
	case SM_CXHSCROLL: return "SM_CXHSCROLL";
	case SM_DEBUG: return "SM_DEBUG";
	case SM_SWAPBUTTON: return "SM_SWAPBUTTON";
	case SM_RESERVED1: return "SM_RESERVED1";
	case SM_RESERVED2: return "SM_RESERVED2";
	case SM_RESERVED3: return "SM_RESERVED3";
	case SM_RESERVED4: return "SM_RESERVED4";
	case SM_CXMIN: return "SM_CXMIN";
	case SM_CYMIN: return "SM_CYMIN";
	case SM_CXSIZE: return "SM_CXSIZE";
	case SM_CYSIZE: return "SM_CYSIZE";
	case SM_CXFRAME: return "SM_CXFRAME";
	case SM_CYFRAME: return "SM_CYFRAME";
	case SM_CXMINTRACK: return "SM_CXMINTRACK";
	case SM_CYMINTRACK: return "SM_CYMINTRACK";
	case SM_CXDOUBLECLK: return "SM_CXDOUBLECLK";
	case SM_CYDOUBLECLK: return "SM_CYDOUBLECLK";
	case SM_CXICONSPACING: return "SM_CXICONSPACING";
	case SM_CYICONSPACING: return "SM_CYICONSPACING";
	case SM_MENUDROPALIGNMENT: return "SM_MENUDROPALIGNMENT";
	case SM_PENWINDOWS: return "SM_PENWINDOWS";
	case SM_DBCSENABLED: return "SM_DBCSENABLED";
	case SM_CMOUSEBUTTONS: return "SM_CMOUSEBUTTONS";
	case SM_SECURE: return "SM_SECURE";
	case SM_CXEDGE: return "SM_CXEDGE";
	case SM_CYEDGE: return "SM_CYEDGE";
	case SM_CXMINSPACING: return "SM_CXMINSPACING";
	case SM_CYMINSPACING: return "SM_CYMINSPACING";
	case SM_CXSMICON: return "SM_CXSMICON";
	case SM_CYSMICON: return "SM_CYSMICON";
	case SM_CYSMCAPTION: return "SM_CYSMCAPTION";
	case SM_CXSMSIZE: return "SM_CXSMSIZE";
	case SM_CYSMSIZE: return "SM_CYSMSIZE";
	case SM_CXMENUSIZE: return "SM_CXMENUSIZE";
	case SM_CYMENUSIZE: return "SM_CYMENUSIZE";
	case SM_ARRANGE: return "SM_ARRANGE";
	case SM_CXMINIMIZED: return "SM_CXMINIMIZED";
	case SM_CYMINIMIZED: return "SM_CYMINIMIZED";
	case SM_CXMAXTRACK: return "SM_CXMAXTRACK";
	case SM_CYMAXTRACK: return "SM_CYMAXTRACK";
	case SM_CXMAXIMIZED: return "SM_CXMAXIMIZED";
	case SM_CYMAXIMIZED: return "SM_CYMAXIMIZED";
	case SM_NETWORK: return "SM_NETWORK";
	case SM_CLEANBOOT: return "SM_CLEANBOOT";
	case SM_CXDRAG: return "SM_CXDRAG";
	case SM_CYDRAG: return "SM_CYDRAG";
	case SM_SHOWSOUNDS: return "SM_SHOWSOUNDS";
	case SM_CXMENUCHECK: return "SM_CXMENUCHECK";
	case SM_CYMENUCHECK: return "SM_CYMENUCHECK";
	case SM_SLOWMACHINE: return "SM_SLOWMACHINE";
	case SM_MIDEASTENABLED: return "SM_MIDEASTENABLED";
	case SM_MOUSEWHEELPRESENT: return "SM_MOUSEWHEELPRESENT";
	case SM_XVIRTUALSCREEN: return "SM_XVIRTUALSCREEN";
	case SM_YVIRTUALSCREEN: return "SM_YVIRTUALSCREEN";
	case SM_CXVIRTUALSCREEN: return "SM_CXVIRTUALSCREEN";
	case SM_CYVIRTUALSCREEN: return "SM_CYVIRTUALSCREEN";
	case SM_CMONITORS: return "SM_CMONITORS";
	case SM_SAMEDISPLAYFORMAT: return "SM_SAMEDISPLAYFORMAT";
	case SM_IMMENABLED: return "SM_IMMENABLED";
	case SM_CXFOCUSBORDER: return "SM_CXFOCUSBORDER";
	case SM_CYFOCUSBORDER: return "SM_CYFOCUSBORDER";
	case SM_TABLETPC: return "SM_TABLETPC";
	case SM_MEDIACENTER: return "SM_MEDIACENTER";
	case SM_STARTER: return "SM_STARTER";
	case SM_SERVERR2: return "SM_SERVERR2";
	case SM_REMOTESESSION: return "SM_REMOTESESSION";
	case SM_SHUTTINGDOWN: return "SM_SHUTTINGDOWN";
	}
	return "Unknown Metric!";
}
GENERATE_INTERCEPT_HEADER(GetSystemMetrics, int, WINAPI, _In_ int nIndex) {
	LogDebug("DetouredGetSystemMetrics %d - %s\n", nIndex, SystemMetricToString(nIndex));
	int ret = TrueGetSystemMetrics(nIndex);
	if (G->adjust_system_metrics && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		switch (nIndex) {
		case SM_CXSCREEN:
			ret = G->GAME_INTERNAL_WIDTH();
			break;
		case SM_CYSCREEN:
			ret = G->GAME_INTERNAL_HEIGHT();
			break;
		case SM_CXVIRTUALSCREEN:
			ret = G->GAME_INTERNAL_WIDTH();
			break;
		case SM_CYVIRTUALSCREEN:
			ret = G->GAME_INTERNAL_HEIGHT();
			break;
		}
	}
	LogDebug(" -> %d\n", ret);
	return ret;
}

namespace {
	void adjustMonitorInfo(LPMONITORINFO lpmi) {
		lpmi->rcMonitor.right = lpmi->rcMonitor.left + G->GAME_INTERNAL_WIDTH();
		lpmi->rcMonitor.bottom = lpmi->rcMonitor.top + G->GAME_INTERNAL_HEIGHT();
	}
}

GENERATE_INTERCEPT_HEADER(GetMonitorInfoA, BOOL, WINAPI, _In_ HMONITOR hMonitor, _Inout_ LPMONITORINFO lpmi) {
	LogDebug("DetouredGetMonitorInfoA %p\n", hMonitor);
	BOOL ret = TrueGetMonitorInfoA(hMonitor, lpmi);
	if (G->adjust_monitor_info && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1)
		adjustMonitorInfo(lpmi);
	return ret;
}
GENERATE_INTERCEPT_HEADER(GetMonitorInfoW, BOOL, WINAPI, _In_ HMONITOR hMonitor, _Inout_ LPMONITORINFO lpmi) {
	LogDebug("DetouredGetMonitorInfoW %p\n", hMonitor);
	BOOL ret = TrueGetMonitorInfoW(hMonitor, lpmi);
	if (G->adjust_monitor_info && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1)
		adjustMonitorInfo(lpmi);
	return ret;
}
// Mouse stuff //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
GENERATE_INTERCEPT_HEADER(ClipCursor, BOOL, WINAPI, __in_opt CONST RECT *lpRect) {
	LogDebug("ClipCursor\n");
	if (G->adjust_clip_cursor && G->SCREEN_UPSCALING > 0) {
		LogDebug(" -> Overriding\n");
		return TrueClipCursor(NULL);
	}
	return TrueClipCursor(lpRect);
}

GENERATE_INTERCEPT_HEADER(WindowFromPoint, HWND, WINAPI, _In_ POINT Point) {
	LogDebug("DetouredWindowFromPointA\n");
	if (G->adjust_window_from_point && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		LogDebug("-> Adjusting position\n");
		Point.x = Point.x * G->SCREEN_WIDTH / G->GAME_INTERNAL_WIDTH();
		Point.y = Point.y * G->SCREEN_HEIGHT / G->GAME_INTERNAL_HEIGHT();
	}
	return TrueWindowFromPoint(Point);
}
// Messages /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

GENERATE_INTERCEPT_HEADER(PeekMessageA, BOOL, WINAPI, _Out_ LPMSG lpMsg, _In_opt_ HWND hWnd, _In_ UINT wMsgFilterMin, _In_ UINT wMsgFilterMax, _In_ UINT wRemoveMsg) {
	LogDebug("DetouredPeekMessageA\n");
	BOOL ret = TruePeekMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
	if (ret && G->adjust_message_pt && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		LogDebug("-> Adjusting position\n");
		lpMsg->pt.x = lpMsg->pt.x * G->GAME_INTERNAL_WIDTH() / G->SCREEN_WIDTH;
		lpMsg->pt.x = lpMsg->pt.x * G->GAME_INTERNAL_HEIGHT() / G->SCREEN_HEIGHT;
	}
	return ret;
}
GENERATE_INTERCEPT_HEADER(GetMessageA, BOOL, WINAPI, _Out_ LPMSG lpMsg, _In_opt_ HWND hWnd, _In_ UINT wMsgFilterMin, _In_ UINT wMsgFilterMax) {
	LogDebug("DetouredGetMessageA\n");
	BOOL ret = TrueGetMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
	if (ret && G->adjust_message_pt && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		LogDebug("-> Adjusting position\n");
		lpMsg->pt.x = lpMsg->pt.x * G->GAME_INTERNAL_WIDTH() / G->SCREEN_WIDTH;
		lpMsg->pt.x = lpMsg->pt.x * G->GAME_INTERNAL_HEIGHT() / G->SCREEN_HEIGHT;
	}
	return ret;
}

GENERATE_INTERCEPT_HEADER(GetMessagePos, DWORD, WINAPI) {
	LogDebug("DetouredGetMessagePos\n");
	DWORD ret = TrueGetMessagePos();
	return ret;
}

// WindowProc ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* WindowLongOffsetToString(int nIndex) {
	switch (nIndex) {
	case GWL_WNDPROC: return "GWL_WNDPROC";
	case GWL_HINSTANCE: return "GWL_HINSTANCE";
	case GWL_HWNDPARENT: return "GWL_HWNDPARENT";
	case GWL_STYLE: return "GWL_STYLE";
	case GWL_EXSTYLE: return "GWL_EXSTYLE";
	case GWL_USERDATA: return "GWL_USERDATA";
	case GWL_ID: return "GWL_ID";
	}
	return "Unknown Offset!";
}
#pragma region WindowProc
namespace {
	std::map<HWND, WNDPROC> prevWndProcs;
	LRESULT CALLBACK InterceptWindowProc(_In_  HWND hwnd, _In_  UINT uMsg, _In_  WPARAM wParam, _In_  LPARAM lParam) {
		LogDebug("InterceptWindowProc hwnd: %p\n", hwnd);
		if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) {
			POINTS p = MAKEPOINTS(lParam);
			p.x = p.x * (SHORT)G->GAME_INTERNAL_WIDTH() / G->SCREEN_WIDTH;
			p.y = p.y * (SHORT)G->GAME_INTERNAL_HEIGHT() / G->SCREEN_HEIGHT;
			return CallWindowProc(prevWndProcs[hwnd], hwnd, uMsg, wParam, MAKELPARAM(p.x, p.y));
		}
		LogDebug(" -> calling original: %d\n", prevWndProcs[hwnd]);
		LRESULT res = CallWindowProc(prevWndProcs[hwnd], hwnd, uMsg, wParam, lParam);
		return res;
	}
}
#ifndef _WIN64
GENERATE_INTERCEPT_HEADER(GetWindowLongA, LONG, WINAPI, _In_ HWND hWnd, _In_ int nIndex) {
	LogDebug("DetouredGetWindowLongA hwnd: %p -- index: %s\n", hWnd, WindowLongOffsetToString(nIndex));
	if (nIndex == GWL_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0) {
		LogDebug(" -> return from table\n");
		if (prevWndProcs.count(hWnd)>0) return (LONG)prevWndProcs[hWnd];
	}
	return TrueGetWindowLongA(hWnd, nIndex);
}
GENERATE_INTERCEPT_HEADER(GetWindowLongW, LONG, WINAPI, _In_ HWND hWnd, _In_ int nIndex) {
	LogDebug("DetouredGetWindowLongW hwnd: %p -- index: %s\n", hWnd, WindowLongOffsetToString(nIndex));
	if (nIndex == GWL_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0) {
		LogDebug(" -> return from table\n");
		if (prevWndProcs.count(hWnd)>0) return (LONG)prevWndProcs[hWnd];
	}
	return TrueGetWindowLongW(hWnd, nIndex);
}

GENERATE_INTERCEPT_HEADER(SetWindowLongA, LONG, WINAPI, _In_ HWND hWnd, _In_ int nIndex, _In_ LONG dwNewLong) {
	LogDebug("DetouredSetWindowLongA hwnd: %p -- index: %s -- value: %d\n", hWnd, WindowLongOffsetToString(nIndex), dwNewLong);
	LONG ret = TrueSetWindowLongA(hWnd, nIndex, dwNewLong);
	if (nIndex == GWL_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		prevWndProcs[hWnd] = (WNDPROC)dwNewLong;
		if (prevWndProcs.size() > 10) LogDebug("More than 10 prevWndProcs!\n");
		TrueSetWindowLongA(hWnd, GWL_WNDPROC, (LONG)&InterceptWindowProc);
	}
	return ret;
}
GENERATE_INTERCEPT_HEADER(SetWindowLongW, LONG, WINAPI, _In_ HWND hWnd, _In_ int nIndex, _In_ LONG dwNewLong) {
	LogDebug("DetouredSetWindowLongW hwnd: %p -- index: %s -- value: %d\n", hWnd, WindowLongOffsetToString(nIndex), dwNewLong);
	LONG ret = TrueSetWindowLongW(hWnd, nIndex, dwNewLong);
	if (nIndex == GWL_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		prevWndProcs[hWnd] = (WNDPROC)dwNewLong;
		if (prevWndProcs.size() > 10) LogDebug("More than 10 prevWndProcs!\n");
		TrueSetWindowLongW(hWnd, GWL_WNDPROC, (LONG)&InterceptWindowProc);
	}
	return ret;
}
#else
GENERATE_INTERCEPT_HEADER(GetWindowLongPtrA, LONG_PTR, WINAPI, _In_ HWND hWnd, _In_ int nIndex) {
	LogDebug("DetouredGetWindowLongA hwnd: %p -- index: %s\n", hWnd, WindowLongOffsetToString(nIndex));
	if (nIndex == GWL_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0) {
		LogDebug(" -> return from table\n");
		if (prevWndProcs.count(hWnd)>0) return (LONG_PTR)prevWndProcs[hWnd];
	}
	return TrueGetWindowLongPtrA(hWnd, nIndex);
}
GENERATE_INTERCEPT_HEADER(GetWindowLongPtrW, LONG_PTR, WINAPI, _In_ HWND hWnd, _In_ int nIndex) {
	LogDebug("DetouredGetWindowLongW hwnd: %p -- index: %s\n", hWnd, WindowLongOffsetToString(nIndex));
	if (nIndex == GWL_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0) {
		LogDebug(" -> return from table\n");
		if (prevWndProcs.count(hWnd)>0) return (LONG_PTR)prevWndProcs[hWnd];
	}
	return TrueGetWindowLongPtrW(hWnd, nIndex);
}

GENERATE_INTERCEPT_HEADER(SetWindowLongPtrA, LONG_PTR, WINAPI, _In_ HWND hWnd, _In_ int nIndex, _In_ LONG_PTR dwNewLong) {
	LogDebug("DetouredSetWindowLongA hwnd: %p -- index: %s -- value: %d\n", hWnd, WindowLongOffsetToString(nIndex), dwNewLong);
	LONG_PTR ret = TrueSetWindowLongPtrA(hWnd, nIndex, dwNewLong);
	if (nIndex == GWL_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		prevWndProcs[hWnd] = (WNDPROC)dwNewLong;
		if (prevWndProcs.size() > 10) LogDebug("More than 10 prevWndProcs!\n");
		TrueSetWindowLongPtrA(hWnd, GWL_WNDPROC, (LONG_PTR)&InterceptWindowProc);
	}
	return ret;
}
GENERATE_INTERCEPT_HEADER(SetWindowLongPtrW, LONG_PTR, WINAPI, _In_ HWND hWnd, _In_ int nIndex, _In_ LONG_PTR dwNewLong) {
	LogDebug("DetouredSetWindowLongW hwnd: %p -- index: %s -- value: %d\n", hWnd, WindowLongOffsetToString(nIndex), dwNewLong);
	LONG_PTR ret = TrueSetWindowLongPtrW(hWnd, nIndex, dwNewLong);
	if (nIndex == GWL_WNDPROC && G->intercept_window_proc && G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		prevWndProcs[hWnd] = (WNDPROC)dwNewLong;
		if (prevWndProcs.size() > 10) LogDebug("More than 10 prevWndProcs!\n");
		TrueSetWindowLongPtrW(hWnd, GWL_WNDPROC, (LONG_PTR)&InterceptWindowProc);
	}
	return ret;
}
#endif // _WIN64

#pragma endregion
//end GeDoSaTo: https://github.com/PeterTh/gedosato

void InstallMouseHooks(bool hide)
{
	HINSTANCE hUser32;
	static bool hook_installed = false;
	int fail = 0;

	// Only attempt to hook it once:
	if (hook_installed)
		return;
	hook_installed = true;

	//InstallCreateWindowHook();
	// Init our handle to the current cursor now before installing the
	// hooks, and from now on it will be kept up to date from SetCursor:
	current_cursor = GetCursor();
	if (hide)
		SetCursor(InvisibleCursor());

	hUser32 = NktHookLibHelpers::GetModuleBaseAddress(L"User32.dll");
	fail |= InstallHook(hUser32, "SetCursor", (void**)&trampoline_SetCursor, Hooked_SetCursor, true);
	fail |= InstallHook(hUser32, "GetCursor", (void**)&trampoline_GetCursor, Hooked_GetCursor, true);

	//fail |= InstallHook(hUser32, "ShowCursor", (void**)&trampoline_ShowCursor, Hooked_ShowCursor, true);

	fail |= InstallHook(hUser32, "GetCursorInfo", (void**)&trampoline_GetCursorInfo, Hooked_GetCursorInfo, true);
	fail |= InstallHook(hUser32, "DefWindowProcA", (void**)&trampoline_DefWindowProcA, Hooked_DefWindowProcA, true);
	fail |= InstallHook(hUser32, "DefWindowProcW", (void**)&trampoline_DefWindowProcW, Hooked_DefWindowProcW, true);

	fail |= InstallHook(hUser32, "SetCursorPos", (void**)&trampoline_SetCursorPos, Hooked_SetCursorPos, true);
	fail |= InstallHook(hUser32, "GetCursorPos", (void**)&trampoline_GetCursorPos, Hooked_GetCursorPos, true);

	fail |= InstallHook(hUser32, "ScreenToClient", (void**)&trampoline_ScreenToClient, Hooked_ScreenToClient, true);
	fail |= InstallHook(hUser32, "MapWindowPoints", (void**)&trampoline_MapWindowPoints, Hooked_MapWindowPoints, true);
	fail |= InstallHook(hUser32, "WindowFromPoint", (void**)&TrueWindowFromPoint, DetouredWindowFromPoint, true);

	fail |= InstallHook(hUser32, "GetClientRect", (void**)&trampoline_GetClientRect, Hooked_GetClientRect, true);
	fail |= InstallHook(hUser32, "GetWindowRect", (void**)&trampoline_GetWindowRect, Hooked_GetWindowRect, true);
	fail |= InstallHook(hUser32, "ClipCursor", (void**)&TrueClipCursor, DetouredClipCursor, true);

#ifndef _WIN64
	fail |= InstallHook(hUser32, "SetWindowLongA", (void**)&TrueSetWindowLongA, DetouredSetWindowLongA, true);
	fail |= InstallHook(hUser32, "SetWindowLongW", (void**)&TrueSetWindowLongW, DetouredSetWindowLongW, true);
	fail |= InstallHook(hUser32, "GetWindowLongA", (void**)&TrueGetWindowLongA, DetouredGetWindowLongA, true);
	fail |= InstallHook(hUser32, "GetWindowLongW", (void**)&TrueGetWindowLongW, DetouredGetWindowLongW, true);
#else
	fail |= InstallHook(hUser32, "SetWindowLongPtrA", (void**)&TrueSetWindowLongPtrA, DetouredSetWindowLongPtrA, true);
	fail |= InstallHook(hUser32, "SetWindowLongPtrW", (void**)&TrueSetWindowLongPtrW, DetouredSetWindowLongPtrW, true);
	fail |= InstallHook(hUser32, "GetWindowLongPtrA", (void**)&TrueGetWindowLongPtrA, DetouredGetWindowLongPtrA, true);
	fail |= InstallHook(hUser32, "GetWindowLongPtrW", (void**)&TrueGetWindowLongPtrW, DetouredGetWindowLongPtrW, true);
#endif
	fail |= InstallHook(hUser32, "PeekMessageA", (void**)&TruePeekMessageA, DetouredPeekMessageA, true);
	fail |= InstallHook(hUser32, "GetMessageA", (void**)&TrueGetMessageA, DetouredGetMessageA, true);
	fail |= InstallHook(hUser32, "GetMessagePos", (void**)&TrueGetMessagePos, DetouredGetMessagePos, true);

	fail |= InstallHook(hUser32, "ChangeDisplaySettingsExA", (void**)&TrueChangeDisplaySettingsExA, DetouredChangeDisplaySettingsExA, true);
	fail |= InstallHook(hUser32, "ChangeDisplaySettingsExW", (void**)&TrueChangeDisplaySettingsExW, DetouredChangeDisplaySettingsExW, true);

	fail |= InstallHook(hUser32, "GetSystemMetrics", (void**)&TrueGetSystemMetrics, DetouredGetSystemMetrics, true);

	fail |= InstallHook(hUser32, "GetMonitorInfoA", (void**)&TrueGetMonitorInfoA, DetouredGetMonitorInfoA, true);
	fail |= InstallHook(hUser32, "GetMonitorInfoW", (void**)&TrueGetMonitorInfoW, DetouredGetMonitorInfoW, true);

	if (fail) {
		LogInfo("Failed to hook mouse cursor functions - hide_cursor will not work\n");
		BeepFailure2();
		return;
	}

	LogInfo("Successfully hooked mouse cursor functions for hide_cursor\n");
}
static HWND(WINAPI *trampoline_CreateWindowEx)(_In_     DWORD     dwExStyle,
	_In_opt_ LPCTSTR   lpClassName,
	_In_opt_ LPCTSTR   lpWindowName,
	_In_     DWORD     dwStyle,
	_In_     int       x,
	_In_     int       y,
	_In_     int       nWidth,
	_In_     int       nHeight,
	_In_opt_ HWND      hWndParent,
	_In_opt_ HMENU     hMenu,
	_In_opt_ HINSTANCE hInstance,
	_In_opt_ LPVOID    lpParam) = CreateWindowExW;

static HWND WINAPI Hooked_CreateWindowEx(
	_In_     DWORD     dwExStyle,
	_In_opt_ LPCTSTR   lpClassName,
	_In_opt_ LPCTSTR   lpWindowName,
	_In_     DWORD     dwStyle,
	_In_     int       x,
	_In_     int       y,
	_In_     int       nWidth,
	_In_     int       nHeight,
	_In_opt_ HWND      hWndParent,
	_In_opt_ HMENU     hMenu,
	_In_opt_ HINSTANCE hInstance,
	_In_opt_ LPVOID    lpParam)
{
	LogDebug("  Hooked_CreateWindowEx called, width = %d, height = %d \n", nWidth, nHeight);
	return trampoline_CreateWindowEx(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}
//
void InstallCreateWindowHook()
{
	HINSTANCE hUser32;
	static bool create_window_hook_installed = false;
	int fail = 0;

	// Only attempt to hook it once:
	if (create_window_hook_installed)
		return;
	create_window_hook_installed = true;

	hUser32 = NktHookLibHelpers::GetModuleBaseAddress(L"User32.dll");
	fail |= InstallHook(hUser32, "CreateWindowExW", (void**)&trampoline_CreateWindowEx, Hooked_CreateWindowEx, true);
	if (fail) {
		LogInfo("Failed to hook create window \n");
		BeepFailure2();
		return;
	}

	LogInfo("Successfully hooked create window \n");
}
static void RemoveHooks()
{
	cHookMgr.UnhookAll();
}


// Now doing hooking for every build, x32 and x64.  Not positive this is the
// best idea, but so far it's been stable and does not seem to introduce any
// problems. The original thought was that this was only useful for some games.
//
// The fact that we are hooking to install DXGI now makes this necessary for
// all builds.

BOOL WINAPI DllMain(
	_In_  HINSTANCE hinstDLL,
	_In_  DWORD fdwReason,
	_In_  LPVOID lpvReserved)
{
	bool result = true;

	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
#if(_WIN64)
#define NVAPI_DLL L"nvapi64.dll"
#else
#define NVAPI_DLL L"nvapi.dll"
#endif
			LoadLibrary(NVAPI_DLL);
			result = InstallHooks();
			break;

		case DLL_PROCESS_DETACH:
			RemoveHooks();
			break;

		case DLL_THREAD_ATTACH:
			// Do thread-specific initialization.
			break;

		case DLL_THREAD_DETACH:
			// Do thread-specific cleanup.
			break;
	}

	return result;
}
