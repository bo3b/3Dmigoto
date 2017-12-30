#include "DLLMainHook.h"

#include "HookedDXGI.h"
#include "D3D11Wrapper.h"

// ----------------------------------------------------------------------------
// Add in Deviare in-proc for hooking system traps using a Detours approach.  We need access to the
// LoadLibrary call to fix the problem of nvapi.dll bypassing our local patches to the d3d11, when
// it does GetSystemDirectory to get System32, and directly access ..\System32\d3d11.dll
// If we get a failure, we'll just log it, it's not fatal.
//
// Pretty sure this is safe at DLLMain, because we are only accessing kernel32 stuff which is sure
// to be loaded.
//
// It's important to note this will be called from DLLMain, where there are a 
// lot of restrictions on what can be called here.  Avoid everything possible.
// Anything that might change the load order of the dlls will make it crash or hang.
//
// Specifically- we cannot legally call LoadLibrary here.
// For libraries we need at DLLMain load time, they need to be linked to the
// d3d11.dll directly using the appropriate .lib file.
// ----------------------------------------------------------------------------


// Used for other hooking. extern in the .h file.
// Only one instance of CNktHookLib is allowed for a given process.
// Automatically instantiated by C++
CNktHookLib cHookMgr;

// ----------------------------------------------------------------------------
// Use this logging when at DLLMain which is too early to do anything with the file system.
#if _DEBUG
bool bLog = true;
#else
bool bLog = false;
#endif

// Special logging for this strange moment at runtime.
// We cannot log to our normal file, because this is too early, in DLLMain.
// Nektra provides a safe log though, so we will use this when debugging.

static void LogHooking(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	if (bLog)
		NktHookLibHelpers::DebugVPrint(fmt, ap);

	va_end(ap);
}


// ----------------------------------------------------------------------------
static HRESULT InstallHook(LPCWSTR moduleName, char *func, void **trampoline, void *hook)
{
	HINSTANCE module;
	SIZE_T hook_id;
	DWORD dwOsErr;
	void *fnOrig;

	module = NktHookLibHelpers::GetModuleBaseAddress(moduleName);
	if (module == NULL)
	{
		LogHooking("*** Failed to GetModuleBaseAddress for %s\n", moduleName);
		return E_FAIL;
	}

	fnOrig = NktHookLibHelpers::GetProcedureAddress(module, func);
	if (fnOrig == NULL) {
		LogHooking("*** Failed to get address of %s\n", func);
		return E_FAIL;
	}

	dwOsErr = cHookMgr.Hook(&hook_id, trampoline, fnOrig, hook);
	if (dwOsErr != ERROR_SUCCESS) {
		LogHooking("*** Failed to hook %s: 0x%x\n", func, dwOsErr);
		return E_FAIL;
	}

	return NOERROR;
}


// ----------------------------------------------------------------------------
// Only ExW version for now, used by nvapi.
// Safe: Kernel32.dll known to be linked directly to our d3d11.dll

static HRESULT HookLoadLibraryExW()
{
	HRESULT hr = InstallHook(L"Kernel32.dll", "LoadLibraryExW", (LPVOID*)&pOrigLoadLibraryExW, Hooked_LoadLibraryExW);
	if (FAILED(hr))
		return E_FAIL;

	return NOERROR;
}


// ----------------------------------------------------------------------------
// Object			OS				DXGI version	Feature level
// IDXGIFactory		Win7			1.0				11.0
// IDXGIFactory1	Win7			1.0				11.0
// IDXGIFactory2	Platform update	1.2				11.1
// IDXGIFactory3	Win8.1			1.3
// IDXGIFactory4					1.4
// IDXGIFactory5					1.5
//
// IDXGIFactory2 is *not* exported until Win8.1 DLLs, and is specifically
// not part of a Win7 platform_update runtime.

static HRESULT HookDXGIFactories()
{
	HRESULT hr;

	hr = InstallHook(L"dxgi.dll", "CreateDXGIFactory", (LPVOID*)&pOrigCreateDXGIFactory, Hooked_CreateDXGIFactory);
	if (FAILED(hr))
		return E_FAIL;

	hr = InstallHook(L"dxgi.dll", "CreateDXGIFactory1", (LPVOID*)&pOrigCreateDXGIFactory1, Hooked_CreateDXGIFactory1);
	if (FAILED(hr))
		return E_FAIL;

	return NOERROR;
}


// ----------------------------------------------------------------------------
static void RemoveHooks()
{
	cHookMgr.UnhookAll();
}


// ----------------------------------------------------------------------------
// Now doing hooking for every build, x32 and x64.  Release and Debug.
// Originally created to solve a problem Nvidia introduced by changing the
// dll search path, this is also now used for DXGI Present hooking.
//
// If we return false here, then the game will error out and not run.

BOOL WINAPI DllMain(
	_In_  HINSTANCE hinstDLL,
	_In_  DWORD fdwReason,
	_In_  LPVOID lpvReserved)
{
	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
			cHookMgr.SetEnableDebugOutput(bLog);

			if (FAILED(HookLoadLibraryExW()))
				return false;
			if (FAILED(HookDXGIFactories()))
				return false;
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

	return true;
}
