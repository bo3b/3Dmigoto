#include "DLLMainHook.h"

#include "log.h"
#include "HookedDXGI.h"

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

typedef HMODULE(WINAPI *lpfnLoadLibraryExW)(_In_ LPCWSTR lpLibFileName, _Reserved_ HANDLE hFile, _In_ DWORD dwFlags);
static HMODULE WINAPI Hooked_LoadLibraryExW(_In_ LPCWSTR lpLibFileName, _Reserved_ HANDLE hFile, _In_ DWORD dwFlags);
static struct
{
	SIZE_T nHookId;
	lpfnLoadLibraryExW fnLoadLibraryExW;
} sLoadLibraryExW_Hook = { 0, NULL };

typedef BOOL(WINAPI *lpfnIsDebuggerPresent)(VOID);
static BOOL WINAPI Hooked_IsDebuggerPresent(VOID);
static struct
{
	SIZE_T nHookId;
	lpfnIsDebuggerPresent fnIsDebuggerPresent;
} sIsDebuggerPresent_Hook = { 0, NULL };


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

		return sLoadLibraryExW_Hook.fnLoadLibraryExW(fullPath, hFile, dwFlags);
	}

	// For this case, we want to see if it's the game loading d3d11 or nvapi directly
	// from the system directory, and redirect it to the game folder if so, by stripping
	// the system path. This is to be case insensitive as we don't know if NVidia will 
	// change that and otherwise break it it with a driver upgrade. 
	
	if (_wcsicmp(lpLibFileName, fullPath) == 0)
	{
		LogInfoW(L"Replaced Hooked_LoadLibraryExW for: %s to %s.\n", lpLibFileName, library);

		return sLoadLibraryExW_Hook.fnLoadLibraryExW(library, hFile, dwFlags);
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

	if (hook_enabled) {
		module = ReplaceOnMatch(lpLibFileName, hFile, dwFlags, L"original_d3d11.dll", L"d3d11.dll");
		if (module)
			return module;

		module = ReplaceOnMatch(lpLibFileName, hFile, dwFlags, L"original_nvapi64.dll", L"nvapi64.dll");
		if (module)
			return module;

		module = ReplaceOnMatch(lpLibFileName, hFile, dwFlags, L"original_nvapi.dll", L"nvapi.dll");
		if (module)
			return module;
	} else
		hook_enabled = true;

	// Normal unchanged case.
	return sLoadLibraryExW_Hook.fnLoadLibraryExW(lpLibFileName, hFile, dwFlags);
}


// Function to be called whenever real IsDebuggerPresent is called, so that we can force it to false.

//static BOOL WINAPI Hooked_IsDebuggerPresent()
//{
//	return sIsDebuggerPresent_Hook.fnIsDebuggerPresent();
//}

static bool InstallHooks()
{
	HINSTANCE hKernel32;
	LPVOID fnOrigLoadLibrary;
	//LPVOID fnOrigIsDebuggerPresent;
	DWORD dwOsErr;

	if (bLog) NktHookLibHelpers::DebugPrint("Attempting to hook LoadLibraryExW using Deviare in-proc.\n");
	cHookMgr.SetEnableDebugOutput(bLog);

	hKernel32 = NktHookLibHelpers::GetModuleBaseAddress(L"Kernel32.dll");
	if (hKernel32 == NULL)
	{
		if (bLog) NktHookLibHelpers::DebugPrint("Failed to get Kernel32 module for Loadlibrary hook.\n");
		return false;
	}

	// Only ExW version for now, used by nvapi.
	fnOrigLoadLibrary = NktHookLibHelpers::GetProcedureAddress(hKernel32, "LoadLibraryExW");
	if (fnOrigLoadLibrary == NULL)
	{
		if (bLog) NktHookLibHelpers::DebugPrint("Failed to get address of LoadLibraryExW for Loadlibrary hook.\n");
		return false;
	}

	dwOsErr = cHookMgr.Hook(&(sLoadLibraryExW_Hook.nHookId), (LPVOID*)&(sLoadLibraryExW_Hook.fnLoadLibraryExW),
		fnOrigLoadLibrary, Hooked_LoadLibraryExW);

	if (bLog) NktHookLibHelpers::DebugPrint("InstallHooks for LoadLibraryExW using Deviare in-proc: %x\n", dwOsErr);

	if (dwOsErr != 0)
		return false;


	// Next hook IsDebuggerPresent to force it false. Same Kernel32.dll
	//fnOrigIsDebuggerPresent = NktHookLibHelpers::GetProcedureAddress(hKernel32, "IsDebuggerPresent");
	//if (fnOrigIsDebuggerPresent == NULL)
	//{
	//	if (bLog) NktHookLibHelpers::DebugPrint("Failed to get address of IsDebuggerPresent for hook.\n");
	//	return false;
	//}

	//dwOsErr = cHookMgr.Hook(&(sIsDebuggerPresent_Hook.nHookId), (LPVOID*)&(sIsDebuggerPresent_Hook.fnIsDebuggerPresent),
	//	fnOrigIsDebuggerPresent, Hooked_IsDebuggerPresent);

	//if (bLog) NktHookLibHelpers::DebugPrint("InstallHooks for IsDebuggerPresent using Deviare in-proc: %x\n", dwOsErr);


	return (dwOsErr == 0) ? true : false;
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
			InstallDXGIHooks();
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
