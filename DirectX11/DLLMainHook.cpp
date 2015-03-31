#include "../log.h"
#include "../Nektra/NktHookLib.h"

// Add in Deviare in-proc for hooking system traps using a Detours approach.  We need access to the
// LoadLibrary call to fix the problem of nvapi.dll bypassing our local patches to the d3d11, when
// it does GetSystemDirectory to get System32, and directly access ..\System32\d3d11.dll
// If we get a failure, we'll just log it, it's not fatal.
//
// Pretty sure this is safe at DLLMain, because we are only accessing kernel32 stuff which is sure
// to be loaded.

// Use this logging when at DLLMain which is too early to do anything with the file system.
#if _DEBUG
	bool bLog = true;
#else
	bool bLog = false;
#endif

CNktHookLib cHookMgr;

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

static HMODULE _Hooked_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile,
		DWORD dwFlags, LPCWSTR magic_name, LPCWSTR library)
{
	WCHAR systemPath[MAX_PATH];
	GetSystemDirectoryW(systemPath, ARRAYSIZE(systemPath));
	wcscat_s(systemPath, MAX_PATH, L"\\");
	wcscat_s(systemPath, MAX_PATH, library);

	// This is late enough that we can look for standard logging.
	LogInfoW(L"Call to Hooked_LoadLibraryExW for: %s.\n", lpLibFileName);

	// Bypass the known expected call from our wrapped d3d11 & nvapi64, where it needs to call to the system to get APIs.
	// This is a bit of a hack, but if the string comes in as original_d3d11/nvapi64, that's from us, and needs to switch
	// to the real one. This doesn't need to be case insensitive, because we create the original string, all lower case.
	if (wcsstr(lpLibFileName, magic_name) != NULL)
	{
		LogInfoW(L"Hooked_LoadLibraryExW switching to original dll: %s to %s.\n",
			lpLibFileName, systemPath);

		return sLoadLibraryExW_Hook.fnLoadLibraryExW(systemPath, hFile, dwFlags);
	}

	// This is to be case insenstive as we don't know if NVidia will change that and otherwise break it
	// it with a driver upgrade.  Any direct access to system32\d3d11.dll needs to be reset to us.
	if (_wcsicmp(lpLibFileName, systemPath) == 0)
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
// Looking for: nvapi64.dll	LoadLibraryExW("C:\Windows\system32\d3d11.dll", NULL, 0)
//
// Cleanly fetch system directory, as drive may not be C:, and it doesn't have to be
// "C:\Windows\system32", although that will be the path for both 32 bit and 64 bit OS.

static HMODULE WINAPI Hooked_LoadLibraryExW(_In_ LPCWSTR lpLibFileName, _Reserved_ HANDLE hFile, _In_ DWORD dwFlags)
{
	HMODULE module;

	module = _Hooked_LoadLibraryExW(lpLibFileName, hFile, dwFlags, L"original_d3d11.dll", L"d3d11.dll");
	if (module)
		return module;

	module = _Hooked_LoadLibraryExW(lpLibFileName, hFile, dwFlags, L"original_nvapi64.dll", L"nvapi64.dll");
	if (module)
		return module;

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

	if (bLog) NktHookLibHelpers::DebugPrint("InstallHooks for IsDebuggerPresent using Deviare in-proc: %x\n", dwOsErr);


	return (dwOsErr == 0) ? true : false;
}

static void RemoveHooks()
{
	cHookMgr.UnhookAll();
}


// Only do this hooking for known bad scenarios. Like Watch Dogs and Dragon Age Inquistion.  
// This will only be active if the build target defines HOOK_SYSTEM32, so it's build selectable.

#if (_WIN64 && HOOK_SYSTEM32)
BOOL WINAPI DllMain(
	_In_  HINSTANCE hinstDLL,
	_In_  DWORD fdwReason,
	_In_  LPVOID lpvReserved)
{
	bool result = true;

	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
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
#endif
