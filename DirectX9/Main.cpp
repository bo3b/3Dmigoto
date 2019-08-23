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