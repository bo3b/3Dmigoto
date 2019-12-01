#include "DLLMainHookDX9.h"
#include "Globals.h"
#include "HookedD3DX.h"


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
static HRESULT InstallHookDLLMain(HINSTANCE module, char *func, void **trampoline, void *hook)
{
	SIZE_T hook_id;
	DWORD dwOsErr;
	void *fnOrig;

	// Early exit with error so the caller doesn't need to explicitly deal
	// with errors getting the module handle:
	if (module == NULL)
	{
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

static bool HookLoadLibraryExW()
{
	HINSTANCE hKernel32;
	int fail = 0;

	LogHooking("Attempting to hook LoadLibraryExW using Deviare in-proc.\n");

	hKernel32 = NktHookLibHelpers::GetModuleBaseAddress(L"Kernel32.dll");
	if (hKernel32 == NULL)
	{
		LogHooking("Failed to get Kernel32 module for Loadlibrary hook.\n");
		return false;
	}

	fail |= InstallHookDLLMain(hKernel32, "LoadLibraryExW", (LPVOID*)&fnOrigLoadLibraryExW, Hooked_LoadLibraryExW);

	if (fail)
	{
		LogHooking("InstallHooks for LoadLibraryExW using Deviare in-proc failed\n");
		return false;
	}

	LogHooking("InstallHooks for LoadLibraryExW using Deviare in-proc succeeded\n");
	return true;
}




#if 0 // This function doesn't appear to ever be called
static void hook_d3dx9(HINSTANCE module)
{
	int fail = 0;
	fail |= InstallHookDLLMain(module, "D3DXComputeNormalMap", (LPVOID*)&trampoline_D3DXComputeNormalMap, Hooked_D3DXComputeNormalMap);
	fail |= InstallHookDLLMain(module, "D3DXCreateCubeTexture", (LPVOID*)&trampoline_D3DXCreateCubeTexture, Hooked_D3DXCreateCubeTexture);
#ifdef UNICODE
	fail |= InstallHookDLLMain(module, "D3DXCreateCubeTextureFromFileW", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromFile, Hooked_D3DXCreateCubeTextureFromFile);
	fail |= InstallHookDLLMain(module, "D3DXCreateCubeTextureFromFileExW", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromFileEx, Hooked_D3DXCreateCubeTextureFromFileEx);
	fail |= InstallHookDLLMain(module, "D3DXCreateCubeTextureFromResourceW", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromResource, Hooked_D3DXCreateCubeTextureFromResource);
	fail |= InstallHookDLLMain(module, "D3DXCreateCubeTextureFromResourceExW", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromResourceEx, Hooked_D3DXCreateCubeTextureFromResourceEx);
	fail |= InstallHookDLLMain(module, "D3DXCreateTextureFromFileW", (LPVOID*)&trampoline_D3DXCreateTextureFromFile, Hooked_D3DXCreateTextureFromFile);
	fail |= InstallHookDLLMain(module, "D3DXCreateTextureFromFileExW", (LPVOID*)&trampoline_D3DXCreateTextureFromFileEx, Hooked_D3DXCreateTextureFromFileEx);
	fail |= InstallHookDLLMain(module, "D3DXCreateTextureFromResourceW", (LPVOID*)&trampoline_D3DXCreateTextureFromResource, Hooked_D3DXCreateTextureFromResource);
	fail |= InstallHookDLLMain(module, "D3DXCreateTextureFromResourceExW", (LPVOID*)&trampoline_D3DXCreateTextureFromResourceEx, Hooked_D3DXCreateTextureFromResourceEx);
	fail |= InstallHookDLLMain(module, "D3DXCreateVolumeTextureFromFileW", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromFile, Hooked_D3DXCreateVolumeTextureFromFile);
	fail |= InstallHookDLLMain(module, "D3DXCreateVolumeTextureFromFileExW", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromFileEx, Hooked_D3DXCreateVolumeTextureFromFileEx);
	fail |= InstallHookDLLMain(module, "D3DXCreateVolumeTextureFromResourceW", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromResource, Hooked_D3DXCreateVolumeTextureFromResource);
	fail |= InstallHookDLLMain(module, "D3DXCreateVolumeTextureFromResourceExW", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromResourceEx, Hooked_D3DXCreateVolumeTextureFromResourceEx);
	fail |= InstallHookDLLMain(module, "D3DXLoadSurfaceFromFileW", (LPVOID*)&trampoline_D3DXLoadSurfaceFromFile, Hooked_D3DXLoadSurfaceFromFile);
	fail |= InstallHookDLLMain(module, "D3DXLoadSurfaceFromResourceW", (LPVOID*)&trampoline_D3DXLoadSurfaceFromResource, Hooked_D3DXLoadSurfaceFromResource);
	fail |= InstallHookDLLMain(module, "D3DXLoadVolumeFromFileW", (LPVOID*)&trampoline_D3DXLoadVolumeFromFile, Hooked_D3DXLoadVolumeFromFile);
	fail |= InstallHookDLLMain(module, "D3DXLoadVolumeFromResourceW", (LPVOID*)&trampoline_D3DXLoadVolumeFromResource, Hooked_D3DXLoadVolumeFromResource);
#else // FIXME: This path will never be used, because 3DMigoto is always built with UNICODE support
	fail |= InstallHookDLLMain(module, "D3DXCreateCubeTextureFromFileA", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromFile, Hooked_D3DXCreateCubeTextureFromFile);
	fail |= InstallHookDLLMain(module, "D3DXCreateCubeTextureFromFileExA", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromFileEx, Hooked_D3DXCreateCubeTextureFromFileEx);
	fail |= InstallHookDLLMain(module, "D3DXCreateCubeTextureFromResourceA", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromResource, Hooked_D3DXCreateCubeTextureFromResource);
	fail |= InstallHookDLLMain(module, "D3DXCreateCubeTextureFromResourceExA", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromResourceEx, Hooked_D3DXCreateCubeTextureFromResourceEx);
	fail |= InstallHookDLLMain(module, "D3DXCreateTextureFromFileA", (LPVOID*)&trampoline_D3DXCreateTextureFromFile, Hooked_D3DXCreateTextureFromFile);
	fail |= InstallHookDLLMain(module, "D3DXCreateTextureFromFileExA", (LPVOID*)&trampoline_D3DXCreateTextureFromFileEx, Hooked_D3DXCreateTextureFromFileEx);
	fail |= InstallHookDLLMain(module, "D3DXCreateTextureFromResourceA", (LPVOID*)&trampoline_D3DXCreateTextureFromResource, Hooked_D3DXCreateTextureFromResource);
	fail |= InstallHookDLLMain(module, "D3DXCreateTextureFromResourceExA", (LPVOID*)&trampoline_D3DXCreateTextureFromResourceEx, Hooked_D3DXCreateTextureFromResourceEx);
	fail |= InstallHookDLLMain(module, "D3DXCreateVolumeTextureFromFileA", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromFile, Hooked_D3DXCreateVolumeTextureFromFile);
	fail |= InstallHookDLLMain(module, "D3DXCreateVolumeTextureFromFileExA", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromFileEx, Hooked_D3DXCreateVolumeTextureFromFileEx);
	fail |= InstallHookDLLMain(module, "D3DXCreateVolumeTextureFromResourceA", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromResource, Hooked_D3DXCreateVolumeTextureFromResource);
	fail |= InstallHookDLLMain(module, "D3DXCreateVolumeTextureFromResourceExA", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromResourceEx, Hooked_D3DXCreateVolumeTextureFromResourceEx);
	fail |= InstallHookDLLMain(module, "D3DXLoadSurfaceFromFileA", (LPVOID*)&trampoline_D3DXLoadSurfaceFromFile, Hooked_D3DXLoadSurfaceFromFile);
	fail |= InstallHookDLLMain(module, "D3DXLoadSurfaceFromResourceA", (LPVOID*)&trampoline_D3DXLoadSurfaceFromResource, Hooked_D3DXLoadSurfaceFromResource);
	fail |= InstallHookDLLMain(module, "D3DXLoadVolumeFromFileA", (LPVOID*)&trampoline_D3DXLoadVolumeFromFile, Hooked_D3DXLoadVolumeFromFile);
	fail |= InstallHookDLLMain(module, "D3DXLoadVolumeFromResourceA", (LPVOID*)&trampoline_D3DXLoadVolumeFromResource, Hooked_D3DXLoadVolumeFromResource);
#endif // UNICODE

	fail |= InstallHookDLLMain(module, "D3DXCreateCubeTextureFromFileInMemory", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromFileInMemory, Hooked_D3DXCreateCubeTextureFromFileInMemory);
	fail |= InstallHookDLLMain(module, "D3DXCreateCubeTextureFromFileInMemoryEx", (LPVOID*)&trampoline_D3DXCreateCubeTextureFromFileInMemoryEx, Hooked_D3DXCreateCubeTextureFromFileInMemoryEx);

	fail |= InstallHookDLLMain(module, "D3DXCreateTexture", (LPVOID*)&trampoline_D3DXCreateTexture, Hooked_D3DXCreateTexture);

	fail |= InstallHookDLLMain(module, "D3DXCreateTextureFromFileInMemory", (LPVOID*)&trampoline_D3DXCreateTextureFromFileInMemory, Hooked_D3DXCreateTextureFromFileInMemory);
	fail |= InstallHookDLLMain(module, "D3DXCreateTextureFromFileInMemoryEx", (LPVOID*)&trampoline_D3DXCreateTextureFromFileInMemoryEx, Hooked_D3DXCreateTextureFromFileInMemoryEx);

	fail |= InstallHookDLLMain(module, "D3DXCreateVolumeTexture", (LPVOID*)&trampoline_D3DXCreateVolumeTexture, Hooked_D3DXCreateVolumeTexture);

	fail |= InstallHookDLLMain(module, "D3DXCreateVolumeTextureFromFileInMemory", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromFileInMemory, Hooked_D3DXCreateVolumeTextureFromFileInMemory);
	fail |= InstallHookDLLMain(module, "D3DXCreateVolumeTextureFromFileInMemoryEx", (LPVOID*)&trampoline_D3DXCreateVolumeTextureFromFileInMemoryEx, Hooked_D3DXCreateVolumeTextureFromFileInMemoryEx);

	fail |= InstallHookDLLMain(module, "D3DXFillCubeTexture", (LPVOID*)&trampoline_D3DXFillCubeTexture, Hooked_D3DXFillCubeTexture);
	fail |= InstallHookDLLMain(module, "D3DXFillCubeTextureTX", (LPVOID*)&trampoline_D3DXFillCubeTextureTX, Hooked_D3DXFillCubeTextureTX);
	fail |= InstallHookDLLMain(module, "D3DXFillTexture", (LPVOID*)&trampoline_D3DXFillTexture, Hooked_D3DXFillTexture);
	fail |= InstallHookDLLMain(module, "D3DXFillTextureTX", (LPVOID*)&trampoline_D3DXFillTextureTX, Hooked_D3DXFillTextureTX);
	fail |= InstallHookDLLMain(module, "D3DXFillVolumeTexture", (LPVOID*)&trampoline_D3DXFillVolumeTexture, Hooked_D3DXFillVolumeTexture);
	fail |= InstallHookDLLMain(module, "D3DXFillVolumeTextureTX", (LPVOID*)&trampoline_D3DXFillVolumeTextureTX, Hooked_D3DXFillVolumeTextureTX);
	fail |= InstallHookDLLMain(module, "D3DXFilterTexture", (LPVOID*)&trampoline_D3DXFilterTexture, Hooked_D3DXFilterTexture);

	fail |= InstallHookDLLMain(module, "D3DXLoadSurfaceFromFileInMemory", (LPVOID*)&trampoline_D3DXLoadSurfaceFromFileInMemory, Hooked_D3DXLoadSurfaceFromFileInMemory);
	fail |= InstallHookDLLMain(module, "D3DXLoadSurfaceFromMemory", (LPVOID*)&trampoline_D3DXLoadSurfaceFromMemory, Hooked_D3DXLoadSurfaceFromMemory);

	fail |= InstallHookDLLMain(module, "D3DXLoadSurfaceFromSurface", (LPVOID*)&trampoline_D3DXLoadSurfaceFromSurface, Hooked_D3DXLoadSurfaceFromSurface);

	fail |= InstallHookDLLMain(module, "D3DXLoadVolumeFromFileInMemory", (LPVOID*)&trampoline_D3DXLoadVolumeFromFileInMemory, Hooked_D3DXLoadVolumeFromFileInMemory);
	fail |= InstallHookDLLMain(module, "D3DXLoadVolumeFromMemory", (LPVOID*)&trampoline_D3DXLoadVolumeFromMemory, Hooked_D3DXLoadVolumeFromMemory);

	fail |= InstallHookDLLMain(module, "D3DXLoadVolumeFromVolume", (LPVOID*)&trampoline_D3DXLoadVolumeFromVolume, Hooked_D3DXLoadVolumeFromVolume);

	if (fail)
		LogHooking("Failed to install hook for D3D9X function using Deviare in-proc\n");
	else
		LogHooking("Succeeded in installing hooks for D3D9X using Deviare in-proc\n");
}
#endif // 0

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
	bool result = true;

	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
			// Calling LoadLibrary is explicilty illegal from DLLMain. If this
			// was required to solve a real problem we will need to find an
			// alternative solution. -DSS
			// LoadLibrary(NVAPI_DLL);

			cHookMgr.SetEnableDebugOutput(bLog);

			result = HookLoadLibraryExW();
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
