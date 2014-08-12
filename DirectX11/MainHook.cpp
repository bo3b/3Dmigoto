
#include "../Nektra/NktHookLib.h"

// Add in Deviare in-proc for hooking system traps using a Detours approach.  We need access to the
// LoadLibrary call to fix the problem of nvapi.dll bypassing our local patches to the nvapi, when
// it does GetSystemDirectory to get System32, and directly access ..\System32\nvapi64.dll
// If we get a failure, we'll just log it, it's not fatal.
//
// Pretty sure this is safe at DLLMain, because we are only accessing kernel32 stuff which is sure
// to be loaded.

static BOOL bLog = true;

typedef HMODULE(WINAPI *lpfnLoadLibraryW)(_In_ LPCWSTR lpLibFileName);
static HMODULE WINAPI Hooked_LoadLibraryW(_In_ LPCWSTR lpLibFileName);
static struct
{
	SIZE_T nHookId;
	lpfnLoadLibraryW fnLoadLibraryW;
} sLoadLibraryW_Hook = { 0, NULL };

typedef BOOL(WINAPI *lpfnIsDebuggerPresent)(void);
static BOOL WINAPI Hooked_IsDebuggerPresent(void);
static struct
{
	SIZE_T nHookId;
	lpfnIsDebuggerPresent fnIsDebuggerPresent;
} sIsDebuggerPresent_Hook = { 0, NULL };



BOOL WINAPI DllMain(
	_In_  HINSTANCE hinstDLL,
	_In_  DWORD fdwReason,
	_In_  LPVOID lpvReserved
	)
{
	CNktHookLib cHookMgr;
	HINSTANCE hKernel32;
	LPVOID fnOrigLoadLibrary;
	DWORD dwOsErr;

	cHookMgr.SetEnableDebugOutput(bLog);

	if (bLog) NktHookLibHelpers::DebugPrint("Attempting to hook LoadLibraryW using Deviare in-proc.\n");


	hKernel32 = NktHookLibHelpers::GetModuleBaseAddress(L"Kernel32.dll");
	if (hKernel32 == NULL)
	{
	 	if (bLog) NktHookLibHelpers::DebugPrint("Failed to get Kernel32 module for Loadlibrary hook.\n");
		return false;
	}
	// Only W version for now, used by nvapi.
	fnOrigLoadLibrary = NktHookLibHelpers::GetProcedureAddress(hKernel32, "LoadLibraryW");
	{
	 	if (bLog) NktHookLibHelpers::DebugPrint("Failed to get address of LoadLibraryW for Loadlibrary hook.\n");
		return false;
	}

	dwOsErr = cHookMgr.Hook(&(sLoadLibraryW_Hook.nHookId), (LPVOID*)&(sLoadLibraryW_Hook.fnLoadLibraryW),
		fnOrigLoadLibrary, Hooked_LoadLibraryW);

	 if (bLog) NktHookLibHelpers::DebugPrint("Successfully hooked LoadLibraryW using Deviare in-proc: %x\n", dwOsErr);

	// Same thing for IsDebuggerPresent.
	LPVOID fnOrigIsDebuggerPresent;

	 if (bLog) NktHookLibHelpers::DebugPrint("Attempting to hook IsDebuggerPresent using Deviare in-proc.\n");

	hKernel32 = NktHookLibHelpers::GetModuleBaseAddress(L"Kernel32.dll");
	if (hKernel32 == NULL)
	{
		 	if (bLog) NktHookLibHelpers::DebugPrint("Failed to get Kernel32 module for IsDebuggerPresent hook.\n");
		return false;
	}

	fnOrigIsDebuggerPresent = NktHookLibHelpers::GetProcedureAddress(hKernel32, "IsDebuggerPresent");
	if (fnOrigIsDebuggerPresent == NULL)
	{
		 if (bLog) NktHookLibHelpers::DebugPrint("Failed to get address of IsDebuggerPresent for IsDebuggerPresent hook.\n");
		return false;
	}

	dwOsErr = cHookMgr.Hook(&(sIsDebuggerPresent_Hook.nHookId), (LPVOID*)&(sIsDebuggerPresent_Hook.fnIsDebuggerPresent),
		fnOrigIsDebuggerPresent, Hooked_IsDebuggerPresent);

	 if (bLog) NktHookLibHelpers::DebugPrint("Successfully hooked IsDebuggerPresent using Deviare in-proc: %x\n", dwOsErr);

	return true;
}

// Function called for every LoadLibraryW call once we have hooked it.
static HMODULE WINAPI Hooked_LoadLibraryW(_In_ LPCWSTR lpLibFileName)
{
	if (bLog) NktHookLibHelpers::DebugPrint("Call to Hooked_LoadLibraryW for: %s.\n", lpLibFileName);

	return sLoadLibraryW_Hook.fnLoadLibraryW(lpLibFileName);
}

// Function called for every LoadLibraryW call once we have hooked it.
static BOOL WINAPI Hooked_IsDebuggerPresent(void)
{
	 if (bLog) NktHookLibHelpers::DebugPrint("Call to Hooked_IsDebuggerPresent.\n");

	return true;
}