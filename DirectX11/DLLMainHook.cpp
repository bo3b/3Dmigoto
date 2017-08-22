#include "DLLMainHook.h"

#include "log.h"
#include "HookedDXGI.h"
#include "globals.h"

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

typedef BOOL(WINAPI *lpfnSetWindowPos)(_In_ HWND hWnd, _In_opt_ HWND hWndInsertAfter,
		_In_ int X, _In_ int Y, _In_ int cx, _In_ int cy, _In_ UINT uFlags);
lpfnSetWindowPos trampoline_SetWindowPos;

static BOOL WINAPI Hooked_SetWindowPos(
    _In_ HWND hWnd,
    _In_opt_ HWND hWndInsertAfter,
    _In_ int X,
    _In_ int Y,
    _In_ int cx,
    _In_ int cy,
    _In_ UINT uFlags)
{
	/*
		TODO: What about this: (dont know how to test it)
		We install this hook on demand to avoid any possible
		issues with hooking the call when we don't need it:
		Unconfirmed, but possibly related to:
		https://forums.geforce.com/default/topic/685657/3d-vision/3dmigoto-now-open-source-/post/4801159/#4801159
		and do nothing - passing this call through could change the game
		to a borderless window. Needed for The Witness.
	*/
	if (G->SCREEN_UPSCALING != 0) {
		// Force desired upscaled resolution (only when desired resolution is provided!)
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
	void* fnOrigSetWindowPos;
	DWORD dwOsErr;
	SIZE_T hook_id;
	static bool hook_installed = false;

	// Only attempt to hook it once:
	if (hook_installed)
		return;
	hook_installed = true;

	hUser32 = NktHookLibHelpers::GetModuleBaseAddress(L"User32.dll");
	if (!hUser32)
		goto err;

	fnOrigSetWindowPos = NktHookLibHelpers::GetProcedureAddress(hUser32, "SetWindowPos");
	if (fnOrigSetWindowPos == NULL)
		goto err;

	dwOsErr = cHookMgr.Hook(&hook_id, (void**)&trampoline_SetWindowPos, fnOrigSetWindowPos, Hooked_SetWindowPos);
	if (dwOsErr)
		goto err;

	LogInfo("Successfully hooked SetWindowPos for full_screen=2\n");
	return;
err:
	LogInfo("Failed to hook SetWindowPos for full_screen=2\n");
	BeepFailure2();
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
// So, we hook into the ShowCursor() function so that when it is called we will
// be running in the context of the game thread where it will work. We maintain
// our own cursor visibility counter separate from Windows, and from this call
// will synchronise the hardware counter to ours, possibly hiding the cursor if
// requested in the d3dx.ini.
//
// But it doesn't stop there - since we have hidden the hardware cursor
// GetCursorInfo() won't return it and our software mouse implementation will
// fail. So we also hook into SetCursor() to catch the cursor handles from the
// game, and hook GetCursorInfo() so we can return them when the software
// cursor is supposed to be visible, but the hardware cursor is not.

HCURSOR current_cursor = NULL;
int software_cursor_count = 0;

typedef HCURSOR(WINAPI *lpfnSetCursor)(_In_opt_ HCURSOR hCursor);
typedef int(WINAPI *lpfnShowCursor)(_In_ BOOL bShow);
typedef BOOL(WINAPI *lpfnGetCursorInfo)(_Inout_ PCURSORINFO pci);
lpfnSetCursor trampoline_SetCursor = SetCursor;
lpfnShowCursor trampoline_ShowCursor = ShowCursor;
lpfnGetCursorInfo trampoline_GetCursorInfo = GetCursorInfo;

// We hook the SetCursor call so that we can catch the current cursor that the
// game has set and return it in the GetCursorInfo call whenever the software
// cursor is visible but the hardware cursor is not.
static HCURSOR WINAPI Hooked_SetCursor(
    _In_opt_ HCURSOR hCursor)
{
	current_cursor = hCursor;
	return trampoline_SetCursor(hCursor);
}

int SyncMouseCursorVisibility(BOOL show_hint)
{
	int real_count;

	if (G->hide_cursor) {
		// Hiding the cursor - keep the hardware counter negative, but
		// let's not go crazy so just keep it at -1:
		real_count = trampoline_ShowCursor(FALSE);
		while (real_count < -1)
			real_count = trampoline_ShowCursor(TRUE);
	} else {
		// Not hiding the cursor - pass the call through, and if
		// necessary sync the hardware counter to our software counter:
		real_count = trampoline_ShowCursor(show_hint);
		while (real_count != software_cursor_count)
			real_count = trampoline_ShowCursor(real_count < software_cursor_count);
	}

	return real_count;
}

static int WINAPI Hooked_ShowCursor(
    _In_ BOOL bShow)
{
	int real_count;

	// Adjust our software cursor visibility counter.
	//
	// XXX: These (unhooked) functions don't behave as I expect when I call
	// them from a random place, such as RunFrameActions() -
	// ShowCursor(FALSE) seems to decrement a counter, but does not hide
	// the cursor. That leads me to think that there is some additional
	// context associated with the cursor/count that is not part of the
	// call (possibly the calling thread?), and the hiding only works if
	// that context is correct, so maybe there are multiple counters
	// associated with each thread (or something)? If so, having a single
	// global software cursor counter might be insufficient and we might
	// have to revisit this:
	if (bShow)
		software_cursor_count++;
	else
		software_cursor_count--;

	real_count = SyncMouseCursorVisibility(bShow);

	// It is tempting to try to hide the real hardware cursor counter from
	// the game and return our internal software counter instead since that
	// is what it is expecting, but doing so interacts badly with the Steam
	// Overlay. The overlay shows the hardware cursor when it is opened -
	// but we DO NOT see that call. When the overlay is closed we DO see
	// that call - it does a ShowCursor(TRUE); ShowCursor(FALSE); to read
	// the counter, but then if we lie here it will do a second
	// ShowCursor(FALSE); to try to force it hidden, but that will mess up
	// our counters and the cursor will be permanently hidden. So, we have
	// to tell the truth here and return the hardware counter:
	//
	// Option 1: Return software counter
	// Pro: We will return the value the game expects
	// Con: Steam overlay will hide the cursor one too many times
	return software_cursor_count;
	//
	// Option 2: Return hardware counter
	// Pro: Steam overlay does the right thing
	// Con: Witcher 3 menu ==> frozen black screen
	//return real_count;
}

BOOL WINAPI Hooked_GetCursorInfo(
    _Inout_ PCURSORINFO pci)
{
	BOOL rc = trampoline_GetCursorInfo(pci);

	if (rc && G->hide_cursor) {
		pci->flags &= ~CURSOR_SHOWING;
		if (software_cursor_count >= 0) {
			pci->flags |= CURSOR_SHOWING;
			// If the hardware cursor is hidden this will be NULL,
			// but we need to set it to the current cursor:
			if (!pci->hCursor)
				pci->hCursor = current_cursor;
		}
	}

	return rc;
}

void InstallMouseHooks()
{
	HINSTANCE hUser32;
	void* fnOrigSetCursor;
	void* fnOrigShowCursor;
	void* fnOrigGetCursorInfo;
	DWORD dwOsErr;
	SIZE_T hook_id;
	static bool hook_installed = false;

	// Only attempt to hook it once:
	if (hook_installed)
		return;
	hook_installed = true;

	// Init our software counter to the current hardware counter only when
	// we first install these hooks:
	ShowCursor(TRUE);
	software_cursor_count = ShowCursor(FALSE);

	hUser32 = NktHookLibHelpers::GetModuleBaseAddress(L"User32.dll");
	if (!hUser32)
		goto err;

	fnOrigSetCursor = NktHookLibHelpers::GetProcedureAddress(hUser32, "SetCursor");
	if (fnOrigSetCursor == NULL)
		goto err;

	dwOsErr = cHookMgr.Hook(&hook_id, (void**)&trampoline_SetCursor, fnOrigSetCursor, Hooked_SetCursor);
	if (dwOsErr)
		goto err;

	fnOrigShowCursor = NktHookLibHelpers::GetProcedureAddress(hUser32, "ShowCursor");
	if (fnOrigShowCursor == NULL)
		goto err;

	dwOsErr = cHookMgr.Hook(&hook_id, (void**)&trampoline_ShowCursor, fnOrigShowCursor, Hooked_ShowCursor);
	if (dwOsErr)
		goto err;

	fnOrigGetCursorInfo = NktHookLibHelpers::GetProcedureAddress(hUser32, "GetCursorInfo");
	if (fnOrigGetCursorInfo == NULL)
		goto err;

	dwOsErr = cHookMgr.Hook(&hook_id, (void**)&trampoline_GetCursorInfo, fnOrigGetCursorInfo, Hooked_GetCursorInfo);
	if (dwOsErr)
		goto err;

	LogInfo("Successfully hooked mouse cursor functions for hide_cursor\n");
	return;
err:
	LogInfo("Failed to hook mouse cursor functions - hide_cursor will not work\n");
	BeepFailure2();
	return;
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
