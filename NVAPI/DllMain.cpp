#include "Main.h"

namespace D3D9Base
{
#include <dxgi.h>
#include <d3d9.h>
#include "../nvapi.h"
}

#include <XInput.h>
#include "../DirectInput.h"

using namespace std;

extern "C"
{
	typedef HRESULT (__stdcall *DllCanUnloadNowType)(void);
	static DllCanUnloadNowType DllCanUnloadNowPtr;
	typedef HRESULT (__stdcall *DllGetClassObjectType)(void);
	static DllGetClassObjectType DllGetClassObjectPtr;
	typedef HRESULT (__stdcall *DllRegisterServerType)(void);
	static DllRegisterServerType DllRegisterServerPtr;
	typedef HRESULT (__stdcall *DllUnregisterServerType)(void);
	static DllUnregisterServerType DllUnregisterServerPtr;
	typedef int *(__cdecl *nvapi_QueryInterfaceType)(unsigned int offset);
	static nvapi_QueryInterfaceType nvapi_QueryInterfacePtr;

	typedef int (__cdecl *tNvAPI_Stereo_GetConvergence)(D3D9Base::StereoHandle stereoHandle, float *pConvergence);
	static tNvAPI_Stereo_GetConvergence _NvAPI_Stereo_GetConvergence;
	typedef int (__cdecl *tNvAPI_Stereo_SetConvergence)(D3D9Base::StereoHandle stereoHandle, float newConvergence);
	static tNvAPI_Stereo_SetConvergence _NvAPI_Stereo_SetConvergence;
	typedef int (__cdecl *tNvAPI_Stereo_GetSeparation)(D3D9Base::StereoHandle stereoHandle, float *pSeparationPercentage);
	static tNvAPI_Stereo_GetSeparation _NvAPI_Stereo_GetSeparation;
	typedef int (__cdecl *tNvAPI_Stereo_SetSeparation)(D3D9Base::StereoHandle stereoHandle, float newSeparationPercentage);
	static tNvAPI_Stereo_SetSeparation _NvAPI_Stereo_SetSeparation;
	typedef int (__cdecl *tNvAPI_Stereo_Disable)();
	static tNvAPI_Stereo_Disable _NvAPI_Stereo_Disable;
	typedef int (__cdecl *tNvAPI_D3D9_VideoSetStereoInfo)(D3D9Base::IDirect3DDevice9 *pDev,
                                              D3D9Base::NV_DX_VIDEO_STEREO_INFO *pStereoInfo);
	static tNvAPI_D3D9_VideoSetStereoInfo _NvAPI_D3D9_VideoSetStereoInfo;
	typedef int (__cdecl *tNvAPI_Stereo_CreateConfigurationProfileRegistryKey)(
		D3D9Base::NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType);
	static tNvAPI_Stereo_CreateConfigurationProfileRegistryKey _NvAPI_Stereo_CreateConfigurationProfileRegistryKey;
	typedef int (__cdecl *tNvAPI_Stereo_DeleteConfigurationProfileRegistryKey)(
		D3D9Base::NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType);
	static tNvAPI_Stereo_DeleteConfigurationProfileRegistryKey _NvAPI_Stereo_DeleteConfigurationProfileRegistryKey;
	typedef int (__cdecl *tNvAPI_Stereo_SetConfigurationProfileValue)(
		D3D9Base::NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType, 
		D3D9Base::NV_STEREO_REGISTRY_ID valueRegistryID, void *pValue);
	static tNvAPI_Stereo_SetConfigurationProfileValue _NvAPI_Stereo_SetConfigurationProfileValue;
	typedef int (__cdecl *tNvAPI_Stereo_DeleteConfigurationProfileValue)(
		D3D9Base::NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType, 
		D3D9Base::NV_STEREO_REGISTRY_ID valueRegistryID);
	static tNvAPI_Stereo_DeleteConfigurationProfileValue _NvAPI_Stereo_DeleteConfigurationProfileValue;
	typedef int (__cdecl *tNvAPI_Stereo_Enable)(void);
	static tNvAPI_Stereo_Enable _NvAPI_Stereo_Enable;
	typedef int (__cdecl *tNvAPI_Stereo_IsEnabled)(D3D9Base::NvU8 *pIsStereoEnabled);
	static tNvAPI_Stereo_IsEnabled _NvAPI_Stereo_IsEnabled;
	typedef int (__cdecl *tNvAPI_Stereo_GetStereoSupport)(
		__in D3D9Base::NvMonitorHandle hMonitor, __out D3D9Base::NVAPI_STEREO_CAPS *pCaps);
	static tNvAPI_Stereo_GetStereoSupport _NvAPI_Stereo_GetStereoSupport;
	typedef int (__cdecl *tNvAPI_Stereo_CreateHandleFromIUnknown)(
		IUnknown *pDevice, D3D9Base::StereoHandle *pStereoHandle);
	static tNvAPI_Stereo_CreateHandleFromIUnknown _NvAPI_Stereo_CreateHandleFromIUnknown;
	typedef int (__cdecl *tNvAPI_Stereo_DestroyHandle)(D3D9Base::StereoHandle stereoHandle);
	static tNvAPI_Stereo_DestroyHandle _NvAPI_Stereo_DestroyHandle;
	typedef int (__cdecl *tNvAPI_Stereo_Activate)(D3D9Base::StereoHandle stereoHandle);
	static tNvAPI_Stereo_Activate _NvAPI_Stereo_Activate;
	typedef int (__cdecl *tNvAPI_Stereo_Deactivate)(D3D9Base::StereoHandle stereoHandle);
	static tNvAPI_Stereo_Deactivate _NvAPI_Stereo_Deactivate;
	typedef int (__cdecl *tNvAPI_Stereo_IsActivated)(D3D9Base::StereoHandle stereoHandle, D3D9Base::NvU8 *pIsStereoOn);
	static tNvAPI_Stereo_IsActivated _NvAPI_Stereo_IsActivated;
	typedef int (__cdecl *tNvAPI_Stereo_DecreaseSeparation)(D3D9Base::StereoHandle stereoHandle);
	static tNvAPI_Stereo_DecreaseSeparation _NvAPI_Stereo_DecreaseSeparation;
	typedef int (__cdecl *tNvAPI_Stereo_IncreaseSeparation)(D3D9Base::StereoHandle stereoHandle);
	static tNvAPI_Stereo_IncreaseSeparation _NvAPI_Stereo_IncreaseSeparation;
	typedef int (__cdecl *tNvAPI_Stereo_DecreaseConvergence)(D3D9Base::StereoHandle stereoHandle);
	static tNvAPI_Stereo_DecreaseConvergence _NvAPI_Stereo_DecreaseConvergence;
	typedef int (__cdecl *tNvAPI_Stereo_IncreaseConvergence)(D3D9Base::StereoHandle stereoHandle);
	static tNvAPI_Stereo_IncreaseConvergence _NvAPI_Stereo_IncreaseConvergence;
	typedef int (__cdecl *tNvAPI_Stereo_GetFrustumAdjustMode)(D3D9Base::StereoHandle stereoHandle, 
		D3D9Base::NV_FRUSTUM_ADJUST_MODE *pFrustumAdjustMode);
	static tNvAPI_Stereo_GetFrustumAdjustMode _NvAPI_Stereo_GetFrustumAdjustMode;
	typedef int (__cdecl *tNvAPI_Stereo_SetFrustumAdjustMode)(D3D9Base::StereoHandle stereoHandle, 
		D3D9Base::NV_FRUSTUM_ADJUST_MODE newFrustumAdjustModeValue);
	static tNvAPI_Stereo_SetFrustumAdjustMode _NvAPI_Stereo_SetFrustumAdjustMode;
	typedef int (__cdecl *tNvAPI_Stereo_InitActivation)(__in D3D9Base::StereoHandle hStereoHandle, 
		__in D3D9Base::NVAPI_STEREO_INIT_ACTIVATION_FLAGS flags);
	static tNvAPI_Stereo_InitActivation _NvAPI_Stereo_InitActivation;
	typedef int (__cdecl *tNvAPI_Stereo_Trigger_Activation)(__in D3D9Base::StereoHandle hStereoHandle);
	static tNvAPI_Stereo_Trigger_Activation _NvAPI_Stereo_Trigger_Activation;
	typedef int (__cdecl *tNvAPI_Stereo_ReverseStereoBlitControl)(D3D9Base::StereoHandle hStereoHandle, D3D9Base::NvU8 TurnOn);
	static tNvAPI_Stereo_ReverseStereoBlitControl _NvAPI_Stereo_ReverseStereoBlitControl;
	typedef int (__cdecl *tNvAPI_Stereo_SetActiveEye)(D3D9Base::StereoHandle hStereoHandle, 
		D3D9Base::NV_STEREO_ACTIVE_EYE StereoEye);
	static tNvAPI_Stereo_SetActiveEye _NvAPI_Stereo_SetActiveEye;
	typedef int (__cdecl *tNvAPI_Stereo_SetDriverMode)(D3D9Base::NV_STEREO_DRIVER_MODE mode);
	static tNvAPI_Stereo_SetDriverMode _NvAPI_Stereo_SetDriverMode;
	typedef int (__cdecl *tNvAPI_Stereo_GetEyeSeparation)(D3D9Base::StereoHandle hStereoHandle,  float *pSeparation );
	static tNvAPI_Stereo_GetEyeSeparation _NvAPI_Stereo_GetEyeSeparation;
	typedef int (__cdecl *tNvAPI_Stereo_SetSurfaceCreationMode)(__in D3D9Base::StereoHandle hStereoHandle, 
		__in D3D9Base::NVAPI_STEREO_SURFACECREATEMODE creationMode);
	static tNvAPI_Stereo_SetSurfaceCreationMode _NvAPI_Stereo_SetSurfaceCreationMode;
	typedef int (__cdecl *tNvAPI_Stereo_GetSurfaceCreationMode)(__in D3D9Base::StereoHandle hStereoHandle, 
		__in D3D9Base::NVAPI_STEREO_SURFACECREATEMODE* pCreationMode);
	static tNvAPI_Stereo_GetSurfaceCreationMode _NvAPI_Stereo_GetSurfaceCreationMode;
	typedef int (__cdecl *tNvAPI_D3D1x_CreateSwapChain)(D3D9Base::StereoHandle hStereoHandle,
											D3D9Base::DXGI_SWAP_CHAIN_DESC* pDesc,
											D3D9Base::IDXGISwapChain** ppSwapChain,
                                            D3D9Base::NV_STEREO_SWAPCHAIN_MODE mode);
	static tNvAPI_D3D1x_CreateSwapChain _NvAPI_D3D1x_CreateSwapChain;

	typedef int (__cdecl *tNvAPI_D3D9_CreateSwapChain)(D3D9Base::StereoHandle hStereoHandle,
                                           D3D9Base::D3DPRESENT_PARAMETERS *pPresentationParameters,
                                           D3D9Base::IDirect3DSwapChain9 **ppSwapChain,
                                           D3D9Base::NV_STEREO_SWAPCHAIN_MODE mode);
	static tNvAPI_D3D9_CreateSwapChain _NvAPI_D3D9_CreateSwapChain;

	typedef int(__cdecl *tNvAPI_D3D_GetCurrentSLIState)(__in IUnknown *pDevice, __in D3D9Base::NV_GET_CURRENT_SLI_STATE *pSliState);
	static tNvAPI_D3D_GetCurrentSLIState _NvAPI_D3D_GetCurrentSLIState;
}

static HMODULE nvDLL = 0;
static bool NoStereoDisable=0;
static bool ForceAutomaticStereo=0;
static float UserConvergence, SetConvergence = -1e30f, GetConvergence = -1e30f;
static float UserSeparation, SetSeparation = -1e30f, GetSeparation = -1e30f;
static map<float, float> GameConvergenceMap, GameConvergenceMapInv;
static float ActionConvergence = -1e30f, ActionSeparation = -1e30f;
static bool gDirectXOverride = false;
static int gSurfaceCreateMode = -1;

static int SCREEN_WIDTH = -1;
static int SCREEN_HEIGHT = -1;
static int SCREEN_REFRESH = -1;
static int SCREEN_FULLSCREEN = -1;

static bool LogConvergence = false;
static bool LogSeparation = false;
static bool LogCalls = false;
static bool LogDebug = false;
bool LogInput = false;
FILE *LogFile = 0;

static bool CallsLogging()
{
	if (!LogCalls) return false;
	if (!LogFile) fopen_s(&LogFile, "nvapi_log.txt", "w");
	return true;
}
static bool SeparationLogging()
{
	if (!LogSeparation) return false;
	if (!LogFile) fopen_s(&LogFile, "nvapi_log.txt", "w");
	return true;
}
static bool ConvergenceLogging()
{
	if (!LogConvergence) return false;
	if (!LogFile) fopen_s(&LogFile, "nvapi_log.txt", "w");
	return true;
}

// Ignore the warnings for secure function here.  Too much trouble for no risk.
// Better bet is to use logging api like Log4C.
#pragma warning( disable : 4996 )
static char *LogTime()
{
	time_t ltime = time(0);
	char *timeStr = asctime(localtime(&ltime));
	timeStr[strlen(timeStr) - 1] = 0;
	return timeStr;
}
#pragma warning( default : 4996 )

static void loadDll()
{
	if (!nvDLL)
	{
		wchar_t sysDir[MAX_PATH];
		SHGetFolderPath(0, CSIDL_SYSTEM, 0, SHGFP_TYPE_CURRENT, sysDir);
		wcscat(sysDir, L"\\nvapi.dll");
		nvDLL = LoadLibrary(sysDir);

		DllCanUnloadNowPtr = (DllCanUnloadNowType)GetProcAddress(nvDLL, "DllCanUnloadNow");
		DllGetClassObjectPtr = (DllGetClassObjectType)GetProcAddress(nvDLL, "DllGetClassObject");
		DllRegisterServerPtr = (DllRegisterServerType)GetProcAddress(nvDLL, "DllRegisterServer");
		DllUnregisterServerPtr = (DllUnregisterServerType)GetProcAddress(nvDLL, "DllUnregisterServer");
		nvapi_QueryInterfacePtr = (nvapi_QueryInterfaceType)GetProcAddress(nvDLL, "nvapi_QueryInterface");

		GetModuleFileName(0, sysDir, MAX_PATH);
		wcsrchr(sysDir, L'\\')[1] = 0;
		wcscat(sysDir, L"d3dx.ini");
		for (int i = 1;; ++i)
		{
			wchar_t id[] = L"Mapxxx", val[MAX_PATH];
			_itow_s(i, id+3, 3, 10);
			int read = GetPrivateProfileString(L"ConvergenceMap", id, 0, val, MAX_PATH, sysDir);
			if (!read) break;
			unsigned int fromHx;
			float from, to;
			swscanf_s(val, L"from %x to %e", &fromHx, &to);
			from = *reinterpret_cast<float *>(&fromHx);
			GameConvergenceMap[from] = to;
			GameConvergenceMapInv[to] = from;
		}
		LogConvergence = GetPrivateProfileInt(L"Logging", L"convergence", 0, sysDir) == 1;
		LogSeparation = GetPrivateProfileInt(L"Logging", L"separation", 0, sysDir) == 1;
		LogInput = GetPrivateProfileInt(L"Logging", L"input", 0, sysDir) == 1;
		LogCalls = GetPrivateProfileInt(L"Logging", L"calls", 0, sysDir) == 1;
		LogDebug = GetPrivateProfileInt(L"Logging", L"debug", 0, sysDir) == 1;

		if (CallsLogging()) fprintf(LogFile, "\nNVapi DLL starting init  -  %s\n\n", LogTime());

		// Unbuffered logging to remove need for fflush calls, and r/w access to make it easy
		// to open active files.
		int unbuffered = -1;
		if (GetPrivateProfileInt(L"Logging", L"unbuffered", 0, sysDir))
		{
			unbuffered = setvbuf(LogFile, NULL, _IONBF, 0);
			if (CallsLogging()) fprintf(LogFile, "  unbuffered=1  return: %d\n", unbuffered);
		}

		// Set the CPU affinity based upon d3dx.ini setting.  Useful for debugging and shader hunting in AC3.
		if (GetPrivateProfileInt(L"Logging", L"force_cpu_affinity", 0, sysDir))
		{
			DWORD one = 0x01;
			BOOL result = SetProcessAffinityMask(GetCurrentProcess(), one);
			if (CallsLogging()) fprintf(LogFile, "CPU Affinity forced to 1- no multithreading: %s\n", result ? "true" : "false");
		}

		// Device
		wchar_t valueString[MAX_PATH];
		int read = GetPrivateProfileString(L"Device", L"width", 0, valueString, MAX_PATH, sysDir);
		if (read) swscanf_s(valueString, L"%d", &SCREEN_WIDTH);
		read = GetPrivateProfileString(L"Device", L"height", 0, valueString, MAX_PATH, sysDir);
		if (read) swscanf_s(valueString, L"%d", &SCREEN_HEIGHT);
		read = GetPrivateProfileString(L"Device", L"refresh_rate", 0, valueString, MAX_PATH, sysDir);
		if (read) swscanf_s(valueString, L"%d", &SCREEN_REFRESH);
		read = GetPrivateProfileString(L"Device", L"full_screen", 0, valueString, MAX_PATH, sysDir);
		if (read) swscanf_s(valueString, L"%d", &SCREEN_FULLSCREEN);

		// Stereo
		NoStereoDisable = GetPrivateProfileInt(L"Device", L"force_stereo", 0, sysDir) == 1;

		if (CallsLogging()) fprintf(LogFile, "[Stereo]\n");
		ForceAutomaticStereo = GetPrivateProfileInt(L"Stereo", L"automatic_mode", 0, sysDir) == 1;
		gSurfaceCreateMode = GetPrivateProfileInt(L"Stereo", L"surface_createmode", -1, sysDir);

		// DirectInput
		InputDevice[0] = 0;
		GetPrivateProfileString(L"OverrideSettings", L"Input", 0, InputDevice, MAX_PATH, sysDir);
		wchar_t *end = InputDevice + wcslen(InputDevice) - 1; while (end > InputDevice && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"OverrideSettings", L"Action", 0, InputAction[0], MAX_PATH, sysDir);
		end = InputAction[0] + wcslen(InputAction[0]) - 1; while (end > InputAction[0] && iswspace(*end)) end--; *(end + 1) = 0;
		InputDeviceId = GetPrivateProfileInt(L"OverrideSettings", L"DeviceNr", -1, sysDir);
		if (GetPrivateProfileString(L"OverrideSettings", L"Convergence", 0, valueString, MAX_PATH, sysDir))
			swscanf_s(valueString, L"%e", &ActionConvergence);
		if (GetPrivateProfileString(L"OverrideSettings", L"Separation", 0, valueString, MAX_PATH, sysDir))
			swscanf_s(valueString, L"%e", &ActionSeparation);
		InitDirectInput();
		
		// XInput
		XInputDeviceId = GetPrivateProfileInt(L"OverrideSettings", L"XInputDevice", -1, sysDir);		
	}
}

STDAPI DllCanUnloadNow(void)
{
	loadDll();
	return (*DllCanUnloadNowPtr)();
}
STDAPI DllGetClassObject(
  __in   REFCLSID rclsid,
  __in   REFIID riid,
  __out  LPVOID *ppv
)
{
	loadDll();
	return (*DllGetClassObjectPtr)();
}
STDAPI DllRegisterServer(void)
{
	loadDll();
	return (*DllRegisterServerPtr)();
}
STDAPI DllUnregisterServer(void)
{
	loadDll();
	return (*DllUnregisterServerPtr)();
}

static int __cdecl NvAPI_Stereo_GetConvergence(D3D9Base::StereoHandle stereoHandle, float *pConvergence)
{
	// Callback from DX wrapper?
	if ((unsigned int) stereoHandle == 0x77aa8ebc && *pConvergence == 1.23f)
	{
		if (CallsLogging() && LogDebug) fprintf(LogFile, "%s - Callback from DirectX wrapper: override next call.\n", LogTime());
		gDirectXOverride = true;
		return 0xeecc34ab;
	}

	int ret = (*_NvAPI_Stereo_GetConvergence)(stereoHandle, pConvergence);
	if (ConvergenceLogging() && GetConvergence != *pConvergence)
	{
		fprintf(LogFile, "%s - GetConvergence value=%e, hex=%x\n", LogTime(), GetConvergence = *pConvergence, *reinterpret_cast<unsigned int *>(pConvergence));
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_SetConvergence(D3D9Base::StereoHandle stereoHandle, float newConvergence)
{
	if (ConvergenceLogging() && SetConvergence != newConvergence)
	{
		fprintf(LogFile, "%s - Request SetConvergence to %e, hex=%x\n", LogTime(), SetConvergence = newConvergence, *reinterpret_cast<unsigned int *>(&newConvergence));
	}
	UpdateInputState();
	// Save current user convergence value.
	float currentConvergence;
	_NvAPI_Stereo_GetConvergence = (tNvAPI_Stereo_GetConvergence)(*nvapi_QueryInterfacePtr)(0x4ab00934);
	(*_NvAPI_Stereo_GetConvergence)(stereoHandle, &currentConvergence);
	if (GameConvergenceMapInv.find(currentConvergence) == GameConvergenceMapInv.end() && currentConvergence != ActionConvergence)
		UserConvergence = currentConvergence;
	// Map special convergence value?
	map<float, float>::iterator i = GameConvergenceMap.find(newConvergence);
	if (i != GameConvergenceMap.end())
		newConvergence = i->second;
	else
		// Normal convergence value. Replace with user value.
		newConvergence = UserConvergence;
	// Action key mapping?
	if (Action && ActionConvergence != -1e30f)
	{
		newConvergence = ActionConvergence;
		if (ActionSeparation != -1e30f)
		{
			float currentSeparation;
			_NvAPI_Stereo_GetSeparation = (tNvAPI_Stereo_GetSeparation)(*nvapi_QueryInterfacePtr)(0x451f2134);
			(*_NvAPI_Stereo_GetSeparation)(stereoHandle, &currentSeparation);
			if (currentSeparation != ActionSeparation)
			{
				UserSeparation = currentSeparation;
				_NvAPI_Stereo_SetSeparation = (tNvAPI_Stereo_SetSeparation)(*nvapi_QueryInterfacePtr)(0x5c069fa3);
				(*_NvAPI_Stereo_SetSeparation)(stereoHandle, ActionSeparation);
			}
		}
	}
	else if (!Action && ActionSeparation != -1e30f)
	{
		float currentSeparation;
		_NvAPI_Stereo_GetSeparation = (tNvAPI_Stereo_GetSeparation)(*nvapi_QueryInterfacePtr)(0x451f2134);
		(*_NvAPI_Stereo_GetSeparation)(stereoHandle, &currentSeparation);
		if (currentSeparation == ActionSeparation)
		{
			_NvAPI_Stereo_SetSeparation = (tNvAPI_Stereo_SetSeparation)(*nvapi_QueryInterfacePtr)(0x5c069fa3);
			(*_NvAPI_Stereo_SetSeparation)(stereoHandle, UserSeparation);			
		}
	}
	// Update needed?
	if (currentConvergence == newConvergence) 
		return 0;
	if (ConvergenceLogging() && SetConvergence != newConvergence)
	{
		fprintf(LogFile, "%s - Remap SetConvergence to %e, hex=%x\n", LogTime(), SetConvergence = newConvergence, *reinterpret_cast<unsigned int *>(&newConvergence));
	}
	return (*_NvAPI_Stereo_SetConvergence)(stereoHandle, newConvergence);
}
static int __cdecl NvAPI_Stereo_GetSeparation(D3D9Base::StereoHandle stereoHandle, float *pSeparationPercentage)
{
	int ret = (*_NvAPI_Stereo_GetSeparation)(stereoHandle, pSeparationPercentage);
	if (SeparationLogging() && GetSeparation != *pSeparationPercentage)
	{
		fprintf(LogFile, "%s - GetSeparation value=%e, hex=%x\n", LogTime(), GetSeparation = *pSeparationPercentage, *reinterpret_cast<unsigned int *>(pSeparationPercentage));
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_SetSeparation(D3D9Base::StereoHandle stereoHandle, float newSeparationPercentage)
{
	if (gDirectXOverride)
	{
		if (CallsLogging() && LogDebug)	fprintf(LogFile, "%s - Stereo_SetSeparation called from DirectX wrapper: ignoring user overrides.\n", LogTime());
		gDirectXOverride = false;
		return (*_NvAPI_Stereo_SetSeparation)(stereoHandle, newSeparationPercentage);
	}

	// Action key mapping?
	if (Action && ActionSeparation != -1e30f)
		newSeparationPercentage = ActionSeparation;
	else
		UserSeparation = ActionSeparation;
	int ret = (*_NvAPI_Stereo_SetSeparation)(stereoHandle, newSeparationPercentage);
	if (SeparationLogging() && SetSeparation != newSeparationPercentage)
	{
		fprintf(LogFile, "%s - SetSeparation to %e, hex=%x\n", LogTime(), SetSeparation = newSeparationPercentage, *reinterpret_cast<unsigned int *>(&newSeparationPercentage));
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_Disable()
{
	if (CallsLogging()) fprintf(LogFile, "%s - Stereo_Disable called.\n", LogTime());
	if (NoStereoDisable)
	{
		if (CallsLogging()) fprintf(LogFile, "  Stereo_Disable ignored.\n");
		return 0;
	}
	return (*_NvAPI_Stereo_Disable)();
}
static int __cdecl NvAPI_D3D9_VideoSetStereoInfo(D3D9Base::IDirect3DDevice9 *pDev,
                                              D3D9Base::NV_DX_VIDEO_STEREO_INFO *pStereoInfo)
{
	if (CallsLogging()) 
	{
		fprintf(LogFile, "%s - D3D9_VideoSetStereoInfo called width\n", LogTime());
		fprintf(LogFile, "  IDirect3DDevice9 = %x\n", pDev);
		fprintf(LogFile, "  hSurface = %x\n", pStereoInfo->hSurface);
		fprintf(LogFile, "  Format = %x\n", pStereoInfo->eFormat);
		fprintf(LogFile, "  StereoEnable = %d\n", pStereoInfo->bStereoEnable);
	}
	return (*_NvAPI_D3D9_VideoSetStereoInfo)(pDev, pStereoInfo);
}
static int __cdecl NvAPI_Stereo_CreateConfigurationProfileRegistryKey(
		D3D9Base::NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType)
{
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_CreateConfigurationProfileRegistryKey called width type = %d\n", LogTime(),
			registryProfileType);
	}
	return (*_NvAPI_Stereo_CreateConfigurationProfileRegistryKey)(registryProfileType);
}
static int __cdecl NvAPI_Stereo_DeleteConfigurationProfileRegistryKey(
		D3D9Base::NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType)
{
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_DeleteConfigurationProfileRegistryKey called width type = %d\n", LogTime(),
			registryProfileType);
	}
	return (*_NvAPI_Stereo_DeleteConfigurationProfileRegistryKey)(registryProfileType);
}
static int __cdecl NvAPI_Stereo_SetConfigurationProfileValue(
		D3D9Base::NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType, 
		D3D9Base::NV_STEREO_REGISTRY_ID valueRegistryID, void *pValue)
{
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_SetConfigurationProfileValue called width type = %d\n", LogTime(),
			registryProfileType);
		fprintf(LogFile, "  value ID = %x\n", valueRegistryID);
		fprintf(LogFile, "  value = %x\n", *(int*)pValue);
	}
	return (*_NvAPI_Stereo_DeleteConfigurationProfileRegistryKey)(registryProfileType);
}
static int __cdecl NvAPI_Stereo_DeleteConfigurationProfileValue(
		D3D9Base::NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType, 
		D3D9Base::NV_STEREO_REGISTRY_ID valueRegistryID)
{
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_SetConfigurationProfileValue called width type = %d\n", LogTime(),
			registryProfileType);
		fprintf(LogFile, "  value ID = %x\n", valueRegistryID);
	}
	return (*_NvAPI_Stereo_DeleteConfigurationProfileValue)(registryProfileType, valueRegistryID);
}
static int __cdecl NvAPI_Stereo_Enable()
{
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_Enable called\n", LogTime());
	}
	return (*_NvAPI_Stereo_Enable)();
}
static int __cdecl NvAPI_Stereo_IsEnabled(D3D9Base::NvU8 *pIsStereoEnabled)
{
	int ret = (*_NvAPI_Stereo_IsEnabled)(pIsStereoEnabled);
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - NvAPI_Stereo_IsEnabled called. Returns IsStereoEnabled = %d, Result = %d\n", LogTime(),
			*pIsStereoEnabled, ret);
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_GetStereoSupport(
		__in D3D9Base::NvMonitorHandle hMonitor, __out D3D9Base::NVAPI_STEREO_CAPS *pCaps)
{
	int ret = (*_NvAPI_Stereo_GetStereoSupport)(hMonitor, pCaps);
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_GetStereoSupportStereo_Enable called with hMonitor = %x. Returns:\n", LogTime(),
			hMonitor);
		fprintf(LogFile, "  result = %d\n", ret);
		fprintf(LogFile, "  version = %d\n", pCaps->version);
		fprintf(LogFile, "  supportsWindowedModeOff = %d\n", pCaps->supportsWindowedModeOff);
		fprintf(LogFile, "  supportsWindowedModeAutomatic = %d\n", pCaps->supportsWindowedModeAutomatic);
		fprintf(LogFile, "  supportsWindowedModePersistent = %d\n", pCaps->supportsWindowedModePersistent);
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_CreateHandleFromIUnknown(
		IUnknown *pDevice, D3D9Base::StereoHandle *pStereoHandle)
{
	int ret = (*_NvAPI_Stereo_CreateHandleFromIUnknown)(pDevice, pStereoHandle);
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_CreateHandleFromIUnknown called with device = %x. Result = %d\n", LogTime(), pDevice, ret);
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_DestroyHandle(D3D9Base::StereoHandle stereoHandle)
{
	int ret = (*_NvAPI_Stereo_DestroyHandle)(stereoHandle);
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_DestroyHandle called. Result = %d\n", LogTime(), ret);
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_Activate(D3D9Base::StereoHandle stereoHandle)
{
	int ret = (*_NvAPI_Stereo_Activate)(stereoHandle);
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_Activate called. Result = %d\n", LogTime(), ret);
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_Deactivate(D3D9Base::StereoHandle stereoHandle)
{
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_Deactivate called.\n", LogTime());
	}
	if (NoStereoDisable)
	{
		if (CallsLogging()) fprintf(LogFile, "  Stereo_Deactivate ignored.\n");
		return 0;
	}
	int ret = (*_NvAPI_Stereo_Deactivate)(stereoHandle);
	if (CallsLogging())
	{
		fprintf(LogFile, "  Result = %d\n", LogTime(), ret);
	}
	return ret;	
}
static int __cdecl NvAPI_Stereo_IsActivated(D3D9Base::StereoHandle stereoHandle, D3D9Base::NvU8 *pIsStereoOn)
{
	int ret = (*_NvAPI_Stereo_IsActivated)(stereoHandle, pIsStereoOn);
	if (CallsLogging() && LogDebug)
	{
		fprintf(LogFile, "%s - Stereo_IsActivated called. Result = %d, IsStereoOn = %d\n", LogTime(), ret, *pIsStereoOn);
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_DecreaseSeparation(D3D9Base::StereoHandle stereoHandle)
{
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_DecreaseSeparation called.\n", LogTime());
	}
	return (*_NvAPI_Stereo_DecreaseSeparation)(stereoHandle);	
}
static int __cdecl NvAPI_Stereo_IncreaseSeparation(D3D9Base::StereoHandle stereoHandle)
{
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_IncreaseSeparation called.\n", LogTime());
	}
	return (*_NvAPI_Stereo_IncreaseSeparation)(stereoHandle);	
}
static int __cdecl NvAPI_Stereo_DecreaseConvergence(D3D9Base::StereoHandle stereoHandle)
{
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_DecreaseConvergence called.\n", LogTime());
	}
	return (*_NvAPI_Stereo_DecreaseConvergence)(stereoHandle);	
}
static int __cdecl NvAPI_Stereo_IncreaseConvergence(D3D9Base::StereoHandle stereoHandle)
{
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_IncreaseConvergence called.\n", LogTime());
	}
	return (*_NvAPI_Stereo_IncreaseConvergence)(stereoHandle);	
}
static int __cdecl NvAPI_Stereo_GetFrustumAdjustMode(D3D9Base::StereoHandle stereoHandle, 
		D3D9Base::NV_FRUSTUM_ADJUST_MODE *pFrustumAdjustMode)
{
	int ret = (*_NvAPI_Stereo_GetFrustumAdjustMode)(stereoHandle, pFrustumAdjustMode);
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_GetFrustumAdjustMode called. Result = %d, returns:\n", LogTime(), ret);
		fprintf(LogFile, "  FrustumAdjustMode = %d\n", *pFrustumAdjustMode);
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_SetFrustumAdjustMode(D3D9Base::StereoHandle stereoHandle, 
		D3D9Base::NV_FRUSTUM_ADJUST_MODE newFrustumAdjustModeValue)
{
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_SetFrustumAdjustMode called with FrustumAdjustMode = %d\n", LogTime(), newFrustumAdjustModeValue);
	}
	return (*_NvAPI_Stereo_SetFrustumAdjustMode)(stereoHandle, newFrustumAdjustModeValue);
}
static int __cdecl NvAPI_Stereo_InitActivation(__in D3D9Base::StereoHandle hStereoHandle, 
		__in D3D9Base::NVAPI_STEREO_INIT_ACTIVATION_FLAGS flags)
{
	int ret = (*_NvAPI_Stereo_InitActivation)(hStereoHandle, flags);
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_InitActivation called with flags = %d. Result = %d\n", LogTime(), 
			flags, ret);
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_Trigger_Activation(__in D3D9Base::StereoHandle hStereoHandle)
{
	int ret = (*_NvAPI_Stereo_Trigger_Activation)(hStereoHandle);
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_Trigger_Activation called. Result = %d\n", LogTime(), ret);
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_ReverseStereoBlitControl(D3D9Base::StereoHandle hStereoHandle, D3D9Base::NvU8 TurnOn)
{
	int ret = (*_NvAPI_Stereo_ReverseStereoBlitControl)(hStereoHandle, TurnOn);
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_Trigger_Activation called with TurnOn = %d. Result = %d\n", LogTime(), 
			TurnOn, ret);
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_SetActiveEye(D3D9Base::StereoHandle hStereoHandle, 
		D3D9Base::NV_STEREO_ACTIVE_EYE StereoEye)
{
	int ret = (*_NvAPI_Stereo_SetActiveEye)(hStereoHandle, StereoEye);
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_SetActiveEye called with StereoEye = %d. Result = %d\n", LogTime(), 
			StereoEye, ret);
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_SetDriverMode(D3D9Base::NV_STEREO_DRIVER_MODE mode)
{
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_SetDriverMode called with mode = %d. Result = %d\n", LogTime(), mode);
		switch (mode)
		{
			case D3D9Base::NVAPI_STEREO_DRIVER_MODE_AUTOMATIC:
				fprintf(LogFile, "  mode %d means automatic mode\n", mode);
				break;
			case D3D9Base::NVAPI_STEREO_DRIVER_MODE_DIRECT:
				fprintf(LogFile, "  mode %d means direct mode\n", mode);
				break;
		}
	}
	if (ForceAutomaticStereo && mode != D3D9Base::NVAPI_STEREO_DRIVER_MODE_AUTOMATIC)
	{
		if (CallsLogging())
		{
			fprintf(LogFile, "    mode forced to automatic mode\n");
		}
		mode = D3D9Base::NVAPI_STEREO_DRIVER_MODE_AUTOMATIC;
	}
	int ret = (*_NvAPI_Stereo_SetDriverMode)(mode);
	if (CallsLogging())
	{
		fprintf(LogFile, "  Result = %d\n", ret);
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_GetEyeSeparation(D3D9Base::StereoHandle hStereoHandle,  float *pSeparation )
{
	int ret = (*_NvAPI_Stereo_GetEyeSeparation)(hStereoHandle, pSeparation);
	if (SeparationLogging())
	{
		fprintf(LogFile, "%s - Stereo_GetEyeSeparation called. Result = %d, Separation = %f\n", LogTime(), 
			ret, *pSeparation);
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_SetSurfaceCreationMode(__in D3D9Base::StereoHandle hStereoHandle, 
		__in D3D9Base::NVAPI_STEREO_SURFACECREATEMODE creationMode)
{
	if (gDirectXOverride)
	{
		if (CallsLogging())
		{
			fprintf(LogFile, "%s - Stereo_SetSurfaceCreationMode called from DirectX wrapper: ignoring user overrides.\n", LogTime());
		}
		gDirectXOverride = false;
	}
	else if (gSurfaceCreateMode >= 0)
	{
		creationMode = (D3D9Base::NVAPI_STEREO_SURFACECREATEMODE)gSurfaceCreateMode;
	}

	int ret = (*_NvAPI_Stereo_SetSurfaceCreationMode)(hStereoHandle, creationMode);
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_SetSurfaceCreationMode called with CreationMode = %d. Result = %d\n", LogTime(), 
			creationMode, ret);
	}
	return ret;
}
static int __cdecl NvAPI_Stereo_GetSurfaceCreationMode(__in D3D9Base::StereoHandle hStereoHandle, 
		__in D3D9Base::NVAPI_STEREO_SURFACECREATEMODE* pCreationMode)
{
	int ret = (*_NvAPI_Stereo_GetSurfaceCreationMode)(hStereoHandle, pCreationMode);
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - Stereo_GetSurfaceCreationMode called. Result = %d, CreationMode = %d\n", LogTime(), 
			ret, *pCreationMode);
	}
	return ret;
}
static int __cdecl NvAPI_D3D1x_CreateSwapChain(D3D9Base::StereoHandle hStereoHandle,
											D3D9Base::DXGI_SWAP_CHAIN_DESC* pDesc,
											D3D9Base::IDXGISwapChain** ppSwapChain,
                                            D3D9Base::NV_STEREO_SWAPCHAIN_MODE mode)
{
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - D3D1x_CreateSwapChain called with parameters\n", LogTime());
		fprintf(LogFile, "  Width = %d\n", pDesc->BufferDesc.Width);
		fprintf(LogFile, "  Height = %d\n", pDesc->BufferDesc.Height);
		fprintf(LogFile, "  Refresh rate = %f\n", 
			(float) pDesc->BufferDesc.RefreshRate.Numerator / (float) pDesc->BufferDesc.RefreshRate.Denominator);
		fprintf(LogFile, "  Windowed = %d\n", pDesc->Windowed);
	}

	if (SCREEN_REFRESH >= 0)
	{
		pDesc->BufferDesc.RefreshRate.Numerator = SCREEN_REFRESH;
		pDesc->BufferDesc.RefreshRate.Denominator = 1;
	}
	if (SCREEN_WIDTH >= 0) pDesc->BufferDesc.Width = SCREEN_WIDTH;
	if (SCREEN_HEIGHT >= 0) pDesc->BufferDesc.Height = SCREEN_HEIGHT;
	if (SCREEN_FULLSCREEN >= 0) pDesc->Windowed = !SCREEN_FULLSCREEN;

	int ret = (*_NvAPI_D3D1x_CreateSwapChain)(hStereoHandle, pDesc, ppSwapChain, mode);
	if (CallsLogging())
	{
		fprintf(LogFile, "  returned %d\n", ret);
	}
	return ret;
}
static int __cdecl NvAPI_D3D9_CreateSwapChain(D3D9Base::StereoHandle hStereoHandle,
                                           D3D9Base::D3DPRESENT_PARAMETERS *pPresentationParameters,
                                           D3D9Base::IDirect3DSwapChain9 **ppSwapChain,
                                           D3D9Base::NV_STEREO_SWAPCHAIN_MODE mode)
{
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - D3D9_CreateSwapChain called with parameters\n", LogTime());
		fprintf(LogFile, "  Width = %d\n", pPresentationParameters->BackBufferWidth);
		fprintf(LogFile, "  Height = %d\n", pPresentationParameters->BackBufferHeight);
		fprintf(LogFile, "  Refresh rate = %d\n", pPresentationParameters->FullScreen_RefreshRateInHz);
		fprintf(LogFile, "  Windowed = %d\n", pPresentationParameters->Windowed);
	}
	if (SCREEN_REFRESH >= 0) 
	{
		if (CallsLogging()) fprintf(LogFile, "    overriding refresh rate = %d\n", SCREEN_REFRESH);
		pPresentationParameters->FullScreen_RefreshRateInHz = SCREEN_REFRESH;
	}
	if (SCREEN_WIDTH >= 0) 
	{
		if (CallsLogging()) fprintf(LogFile, "    overriding width = %d\n", SCREEN_WIDTH);
		pPresentationParameters->BackBufferWidth = SCREEN_WIDTH;
	}
	if (SCREEN_HEIGHT >= 0) 
	{
		if (CallsLogging()) fprintf(LogFile, "    overriding height = %d\n", SCREEN_HEIGHT);
		pPresentationParameters->BackBufferHeight = SCREEN_HEIGHT;
	}
	if (SCREEN_FULLSCREEN >= 0)
	{
		if (CallsLogging()) fprintf(LogFile, "    overriding full screen = %d\n", SCREEN_FULLSCREEN);
		pPresentationParameters->Windowed = !SCREEN_FULLSCREEN;
	}
	int ret = (*_NvAPI_D3D9_CreateSwapChain)(hStereoHandle, pPresentationParameters, ppSwapChain, mode);
	if (CallsLogging())
	{
		fprintf(LogFile, "  returned %d\n", ret);
	}
	return ret;
}

static int __cdecl NvAPI_D3D_GetCurrentSLIState(__in IUnknown *pDevice, __in D3D9Base::NV_GET_CURRENT_SLI_STATE *pSliState)
{
	int ret = (*_NvAPI_D3D_GetCurrentSLIState)(pDevice, pSliState);
	if (CallsLogging())
	{
		fprintf(LogFile, "%s - NvAPI_D3D_GetCurrentSLIState called with device = %x. Result = %d\n", LogTime(), pDevice, ret);
	}
	return ret;
}

extern "C" __declspec(dllexport) int * __cdecl nvapi_QueryInterface(unsigned int offset)
{
	loadDll();
	int *ptr = (*nvapi_QueryInterfacePtr)(offset);
	switch (offset)
	{
		case 0x4ab00934:
			_NvAPI_Stereo_GetConvergence = (tNvAPI_Stereo_GetConvergence)ptr;
			ptr = (int *)NvAPI_Stereo_GetConvergence;
			break;
		case 0x3dd6b54b:
			_NvAPI_Stereo_SetConvergence = (tNvAPI_Stereo_SetConvergence)ptr;
			ptr = (int *)NvAPI_Stereo_SetConvergence;
			break;
		case 0x451f2134: 
			_NvAPI_Stereo_GetSeparation = (tNvAPI_Stereo_GetSeparation)ptr;
			ptr = (int *)NvAPI_Stereo_GetSeparation;
			break;
		case 0x5c069fa3: 
			_NvAPI_Stereo_SetSeparation = (tNvAPI_Stereo_SetSeparation)ptr;
			ptr = (int *)NvAPI_Stereo_SetSeparation;
			break;
		case 0x2ec50c2b:
			_NvAPI_Stereo_Disable = (tNvAPI_Stereo_Disable)ptr;
			ptr = (int *)NvAPI_Stereo_Disable;
			break;
		case 0xB852F4DB:
			_NvAPI_D3D9_VideoSetStereoInfo = (tNvAPI_D3D9_VideoSetStereoInfo)ptr;
			ptr = (int *)NvAPI_D3D9_VideoSetStereoInfo;
			break;
		case 0xBE7692EC:
			_NvAPI_Stereo_CreateConfigurationProfileRegistryKey = (tNvAPI_Stereo_CreateConfigurationProfileRegistryKey)ptr;
			ptr = (int *)NvAPI_Stereo_CreateConfigurationProfileRegistryKey;
			break;
		case 0xF117B834:
			_NvAPI_Stereo_DeleteConfigurationProfileRegistryKey = (tNvAPI_Stereo_DeleteConfigurationProfileRegistryKey)ptr;
			ptr = (int *)NvAPI_Stereo_DeleteConfigurationProfileRegistryKey;
			break;
		case 0x24409F48:
			_NvAPI_Stereo_SetConfigurationProfileValue = (tNvAPI_Stereo_SetConfigurationProfileValue)ptr;
			ptr = (int *)NvAPI_Stereo_SetConfigurationProfileValue;
			break;
		case 0x49BCEECF:
			_NvAPI_Stereo_DeleteConfigurationProfileValue = (tNvAPI_Stereo_DeleteConfigurationProfileValue)ptr;
			ptr = (int *)NvAPI_Stereo_DeleteConfigurationProfileValue;
			break;
		case 0x239C4545:
			_NvAPI_Stereo_Enable = (tNvAPI_Stereo_Enable)ptr;
			ptr = (int *)NvAPI_Stereo_Enable;
			break;
		case 0x348FF8E1:
			_NvAPI_Stereo_IsEnabled = (tNvAPI_Stereo_IsEnabled)ptr;
			ptr = (int *)NvAPI_Stereo_IsEnabled;
			break;
		case 0x296C434D:
			_NvAPI_Stereo_GetStereoSupport = (tNvAPI_Stereo_GetStereoSupport)ptr;
			ptr = (int *)NvAPI_Stereo_GetStereoSupport;
			break;
		case 0xAC7E37F4:
			_NvAPI_Stereo_CreateHandleFromIUnknown = (tNvAPI_Stereo_CreateHandleFromIUnknown)ptr;
			ptr = (int *)NvAPI_Stereo_CreateHandleFromIUnknown;
			break;
		case 0x3A153134:
			_NvAPI_Stereo_DestroyHandle = (tNvAPI_Stereo_DestroyHandle)ptr;
			ptr = (int *)NvAPI_Stereo_DestroyHandle;
			break;
		case 0xF6A1AD68:
			_NvAPI_Stereo_Activate = (tNvAPI_Stereo_Activate)ptr;
			ptr = (int *)NvAPI_Stereo_Activate;
			break;
		case 0x2D68DE96:
			_NvAPI_Stereo_Deactivate = (tNvAPI_Stereo_Deactivate)ptr;
			ptr = (int *)NvAPI_Stereo_Deactivate;
			break;
		case 0x1FB0BC30:
			_NvAPI_Stereo_IsActivated = (tNvAPI_Stereo_IsActivated)ptr;
			ptr = (int *)NvAPI_Stereo_IsActivated;
			break;
		case 0xDA044458:
			_NvAPI_Stereo_DecreaseSeparation = (tNvAPI_Stereo_DecreaseSeparation)ptr;
			ptr = (int *)NvAPI_Stereo_DecreaseSeparation;
			break;
		case 0xC9A8ECEC:
			_NvAPI_Stereo_IncreaseSeparation = (tNvAPI_Stereo_IncreaseSeparation)ptr;
			ptr = (int *)NvAPI_Stereo_IncreaseSeparation;
			break;
		case 0x4C87E317:
			_NvAPI_Stereo_DecreaseConvergence = (tNvAPI_Stereo_DecreaseConvergence)ptr;
			ptr = (int *)NvAPI_Stereo_DecreaseConvergence;
			break;
		case 0xA17DAABE:
			_NvAPI_Stereo_IncreaseConvergence = (tNvAPI_Stereo_IncreaseConvergence)ptr;
			ptr = (int *)NvAPI_Stereo_IncreaseConvergence;
			break;
		case 0xE6839B43:
			_NvAPI_Stereo_GetFrustumAdjustMode = (tNvAPI_Stereo_GetFrustumAdjustMode)ptr;
			ptr = (int *)NvAPI_Stereo_GetFrustumAdjustMode;
			break;
		case 0x7BE27FA2:
			_NvAPI_Stereo_SetFrustumAdjustMode = (tNvAPI_Stereo_SetFrustumAdjustMode)ptr;
			ptr = (int *)NvAPI_Stereo_SetFrustumAdjustMode;
			break;
		case 0xC7177702:
			_NvAPI_Stereo_InitActivation = (tNvAPI_Stereo_InitActivation)ptr;
			ptr = (int *)NvAPI_Stereo_InitActivation;
			break;
		case 0x0D6C6CD2:
			_NvAPI_Stereo_Trigger_Activation = (tNvAPI_Stereo_Trigger_Activation)ptr;
			ptr = (int *)NvAPI_Stereo_Trigger_Activation;
			break;
		case 0x3CD58F89:
			_NvAPI_Stereo_ReverseStereoBlitControl = (tNvAPI_Stereo_ReverseStereoBlitControl)ptr;
			ptr = (int *)NvAPI_Stereo_ReverseStereoBlitControl;
			break;
		case 0x96EEA9F8:
			_NvAPI_Stereo_SetActiveEye = (tNvAPI_Stereo_SetActiveEye)ptr;
			ptr = (int *)NvAPI_Stereo_SetActiveEye;
			break;
		case 0x5E8F0BEC:
			_NvAPI_Stereo_SetDriverMode = (tNvAPI_Stereo_SetDriverMode)ptr;
			ptr = (int *)NvAPI_Stereo_SetDriverMode;
			break;
		case 0xCE653127:
			_NvAPI_Stereo_GetEyeSeparation = (tNvAPI_Stereo_GetEyeSeparation)ptr;
			ptr = (int *)NvAPI_Stereo_GetEyeSeparation;
			break;
		case 0xF5DCFCBA:
			_NvAPI_Stereo_SetSurfaceCreationMode = (tNvAPI_Stereo_SetSurfaceCreationMode)ptr;
			ptr = (int *)NvAPI_Stereo_SetSurfaceCreationMode;
			break;
		case 0x36F1C736:
			_NvAPI_Stereo_GetSurfaceCreationMode = (tNvAPI_Stereo_GetSurfaceCreationMode)ptr;
			ptr = (int *)NvAPI_Stereo_GetSurfaceCreationMode;
			break;
		case 0x1BC21B66:
			_NvAPI_D3D1x_CreateSwapChain = (tNvAPI_D3D1x_CreateSwapChain)ptr;
			ptr = (int *)NvAPI_D3D1x_CreateSwapChain;
			break;
		case 0x1A131E09:
			_NvAPI_D3D9_CreateSwapChain = (tNvAPI_D3D9_CreateSwapChain)ptr;
			ptr = (int *)NvAPI_D3D9_CreateSwapChain;
			break;

			// Informational logging
		case 0x4B708B54:
			_NvAPI_D3D_GetCurrentSLIState = (tNvAPI_D3D_GetCurrentSLIState)ptr;
			ptr = (int *)NvAPI_D3D_GetCurrentSLIState;
			break;
	}
	return ptr;
}

/*  List of all hex API constants from: 
http://stackoverflow.com/questions/13291783/how-to-get-the-id-memory-address-of-dll-function

NvAPI_GetUnAttachedAssociatedDisplayName  -  4888D790
NvAPI_Stereo_Disable  -  2EC50C2B
NvAPI_GPU_GetPCIIdentifiers  -  2DDFB66E
NvAPI_GPU_GetECCErrorInfo  -  C71F85A6
NvAPI_Disp_InfoFrameControl  -  6067AF3F
NvAPI_Mosaic_GetCurrentTopo  -  EC32944E
NvAPI_Unload  -  D22BDD7E
NvAPI_EnableCurrentMosaicTopology  -  74073CC9
NvAPI_DRS_GetNumProfiles  -  1DAE4FBC
NvAPI_DRS_LoadSettingsFromFile  -  D3EDE889
NvAPI_Stereo_SetFrustumAdjustMode  -  7BE27FA2
NvAPI_Mosaic_SetCurrentTopo  -  9B542831
NvAPI_DRS_GetApplicationInfo  -  ED1F8C69
NvAPI_Stereo_Activate  -  F6A1AD68
NvAPI_Stereo_GetFrustumAdjustMode  -  E6839B43
NvAPI_D3D_SetFPSIndicatorState  -  A776E8DB
NvAPI_GetLogicalGPUFromPhysicalGPU  -  ADD604D1
NvAPI_GetAssociatedNvidiaDisplayName  -  22A78B05
NvAPI_GetViewEx  -  DBBC0AF4
NvAPI_Stereo_CapturePngImage  -  8B7E99B5
NvAPI_Stereo_GetSurfaceCreationMode  -  36F1C736
NvAPI_GPU_GetEDID  -  37D32E69
NvAPI_Stereo_CreateConfigurationProfileRegistryKey  -  BE7692EC
NvAPI_VIO_Status  -  0E6CE4F1
NvAPI_DRS_GetCurrentGlobalProfile  -  617BFF9F
NvAPI_VIO_GetPCIInfo  -  B981D935
NvAPI_GetSupportedMosaicTopologies  -  410B5C25
NvAPI_VIO_SetSyncDelay  -  2697A8D1
NvAPI_GPU_SetIllumination  -  0254A187
NvAPI_VIO_GetGamma  -  51D53D06
NvAPI_Disp_ColorControl  -  92F9D80D
NvAPI_GetSupportedViews  -  66FB7FC0
NvAPI_DRS_LoadSettings  -  375DBD6B
NvAPI_DRS_CreateApplication  -  4347A9DE
NvAPI_EnumLogicalGPUs  -  48B3EA59
NvAPI_Stereo_SetSurfaceCreationMode  -  F5DCFCBA
NvAPI_DISP_GetDisplayConfig  -  11ABCCF8
NvAPI_GetCurrentMosaicTopology  -  F60852BD
NvAPI_DisableHWCursor  -  AB163097
NvAPI_D3D9_AliasSurfaceAsTexture  -  E5CEAE41
NvAPI_GPU_GetBusSlotId  -  2A0A350F
NvAPI_GPU_GetTachReading  -  5F608315
NvAPI_Stereo_SetSeparation  -  5C069FA3
NvAPI_GPU_GetECCStatusInfo  -  CA1DDAF3
NvAPI_VIO_IsFrameLockModeCompatible  -  7BF0A94D
NvAPI_Mosaic_EnumDisplayGrids  -  DF2887AF
NvAPI_DISP_SetDisplayConfig  -  5D8CF8DE
NvAPI_DRS_EnumAvailableSettingIds  -  F020614A
NvAPI_VIO_SetConfig  -  0E4EEC07
NvAPI_GPU_GetPerfDecreaseInfo  -  7F7F4600
NvAPI_SYS_GetLidAndDockInfo  -  CDA14D8A
NvAPI_GPU_GetPstates20  -  6FF81213
NvAPI_GPU_GetAllOutputs  -  7D554F8E
NvAPI_GPU_GetConnectedSLIOutputs  -  0680DE09
NvAPI_VIO_IsRunning  -  96BD040E
NvAPI_Initialize  -  0150E828
NvAPI_VIO_Close  -  D01BD237
NvAPI_Stereo_GetStereoSupport  -  296C434D
NvAPI_GPU_GetGPUType  -  C33BAEB1
NvAPI_Stereo_CaptureJpegImage  -  932CB140
NvAPI_DRS_GetProfileInfo  -  61CD6FD6
NvAPI_Stereo_SetConfigurationProfileValue  -  24409F48
NvAPI_VIO_SyncFormatDetect  -  118D48A3
NvAPI_VIO_GetCapabilities  -  1DC91303
NvAPI_GPU_GetCurrentAGPRate  -  C74925A0
NvAPI_I2CWrite  -  E812EB07
NvAPI_Stereo_GetSeparation  -  451F2134
NvAPI_GPU_GetPstatesInfoEx  -  843C0256
NvAPI_DRS_SetCurrentGlobalProfile  -  1C89C5DF
NvAPI_Mosaic_GetTopoGroup  -  CB89381D
NvAPI_GPU_GetCurrentPCIEDownstreamWidth  -  D048C3B1
NvAPI_D3D9_RegisterResource  -  A064BDFC
NvAPI_DRS_RestoreProfileDefaultSetting  -  53F0381E
NvAPI_VIO_GetSyncDelay  -  462214A9
NvAPI_GPU_GetVbiosOEMRevision  -  2D43FB31
NvAPI_GetVBlankCounter  -  67B5DB55
NvAPI_GetDisplayDriverVersion  -  F951A4D1
NvAPI_DRS_EnumSettings  -  AE3039DA
NvAPI_GPU_QueryIlluminationSupport  -  A629DA31
NvAPI_GetLogicalGPUFromDisplay  -  EE1370CF
NvAPI_DRS_EnumApplications  -  7FA2173A
NvAPI_Mosaic_EnableCurrentTopo  -  5F1AA66C
NvAPI_Stereo_IsActivated  -  1FB0BC30
NvAPI_VIO_Stop  -  6BA2A5D6
NvAPI_SYS_GetChipSetInfo  -  53DABBCA
NvAPI_GPU_GetActiveOutputs  -  E3E89B6F
NvAPI_DRS_GetSettingNameFromId  -  D61CBE6E
NvAPI_GetPhysicalGPUFromUnAttachedDisplay  -  5018ED61
NvAPI_Mosaic_GetSupportedTopoInfo  -  FDB63C81
NvAPI_GPU_GetIRQ  -  E4715417
NvAPI_GPU_GetOutputType  -  40A505E4
NvAPI_Stereo_IsEnabled  -  348FF8E1
NvAPI_Stereo_Enable  -  239C4545
NvAPI_GPU_GetSystemType  -  BAAABFCC
NvAPI_GPU_SetEDID  -  E83D6456
NvAPI_GetPhysicalGPUsFromLogicalGPU  -  AEA3FA32
NvAPI_VIO_GetConfig  -  D34A789B
NvAPI_GetInterfaceVersionString  -  01053FA5
NvAPI_GPU_ResetECCErrorInfo  -  C02EEC20
NvAPI_SetCurrentMosaicTopology  -  D54B8989
NvAPI_DISP_GetDisplayIdByDisplayName  -  AE457190
NvAPI_GetView  -  D6B99D89
NvAPI_Stereo_DeleteConfigurationProfileRegistryKey  -  F117B834
NvAPI_DRS_DestroySession  -  DAD9CFF8
NvAPI_GPU_WorkstationFeatureQuery  -  004537DF
NvAPI_VIO_QueryTopology  -  869534E2
NvAPI_DRS_EnumAvailableSettingValues  -  2EC39F90
NvAPI_DRS_GetBaseProfile  -  DA8466A0
NvAPI_OGL_ExpertModeDefaultsGet  -  AE921F12
NvAPI_DRS_DeleteApplicationEx  -  C5EA85A1
NvAPI_D3D1x_CreateSwapChain  -  1BC21B66
NvAPI_GPU_GetConnectedDisplayIds  -  0078DBA2
NvAPI_DRS_FindProfileByName  -  7E4A9A0B
NvAPI_D3D9_UnregisterResource  -  BB2B17AA
NvAPI_DRS_EnumProfiles  -  BC371EE0
NvAPI_VIO_EnumDevices  -  FD7C5557
NvAPI_DRS_CreateProfile  -  CC176068
NvAPI_D3D9_StretchRectEx  -  22DE03AA
NvAPI_DRS_GetSetting  -  73BF8338
NvAPI_Stereo_InitActivation  -  C7177702
NvAPI_EnumNvidiaDisplayHandle  -  9ABDD40D
NvAPI_GPU_GetConnectedSLIOutputsWithLidState  -  96043CC7
NvAPI_Stereo_DecreaseConvergence  -  4C87E317
NvAPI_GPU_GetBusType  -  1BB18724
NvAPI_DRS_FindApplicationByName  -  EEE566B2
NvAPI_D3D9_ClearRT  -  332D3942
NvAPI_GPU_GetVirtualFrameBufferSize  -  5A04B644
NvAPI_GPU_GetAllDisplayIds  -  785210A2
NvAPI_DRS_SetSetting  -  577DD202
NvAPI_Stereo_GetConvergence  -  4AB00934
NvAPI_GPU_GetCurrentPstate  -  927DA4F6
NvAPI_VIO_SetCSC  -  A1EC8D74
NvAPI_CreateDisplayFromUnAttachedDisplay  -  63F9799E
NvAPI_DRS_SaveSettingsToFile  -  2BE25DF8
NvAPI_DRS_DeleteProfile  -  17093206
NvAPI_Stereo_Trigger_Activation  -  0D6C6CD2
NvAPI_GPU_GetThermalSettings  -  E3640A56
NvAPI_Stereo_SetNotificationMessage  -  6B9B409E
NvAPI_Stereo_CreateHandleFromIUnknown  -  AC7E37F4
NvAPI_Stereo_DecreaseSeparation  -  DA044458
NvAPI_GPU_ValidateOutputCombination  -  34C9C2D4
NvAPI_Stereo_ReverseStereoBlitControl  -  3CD58F89
NvAPI_GPU_GetConnectedOutputs  -  1730BFC9
NvAPI_DRS_GetSettingIdFromName  -  CB7309CD
NvAPI_EnumPhysicalGPUs  -  E5AC921F
NvAPI_VIO_GetCSC  -  7B0D72A3
NvAPI_GPU_GetVbiosRevision  -  ACC3DA0A
NvAPI_SYS_GetDriverAndBranchVersion  -  2926AAAD
NvAPI_SetDisplayPort  -  FA13E65A
NvAPI_GPU_GetPhysicalFrameBufferSize  -  46FBEB03
NvAPI_DRS_CreateSession  -  0694D52E
NvAPI_VIO_EnumSignalFormats  -  EAD72FE4
NvAPI_GPU_GetECCConfigurationInfo  -  77A796F3
NvAPI_Mosaic_GetOverlapLimits  -  989685F0
NvAPI_GetHDMISupportInfo  -  6AE16EC3
NvAPI_Mosaic_EnumDisplayModes  -  78DB97D7
NvAPI_Stereo_DeleteConfigurationProfileValue  -  49BCEECF
NvAPI_OGL_ExpertModeSet  -  3805EF7A
NvAPI_GetPhysicalGPUsFromDisplay  -  34EF9506
NvAPI_Mosaic_GetDisplayViewportsByResolution  -  DC6DC8D3
NvAPI_VIO_Open  -  44EE4841
NvAPI_DRS_SaveSettings  -  FCBC7E14
NvAPI_D3D9_CreateSwapChain  -  1A131E09
NvAPI_GPU_GetHDCPSupportStatus  -  F089EEF5
NvAPI_DISP_GetAssociatedUnAttachedNvidiaDisplayHandle  -  A70503B2
NvAPI_Stereo_DestroyHandle  -  3A153134
NvAPI_DRS_RestoreAllDefaults  -  5927B094
NvAPI_VIO_SetGamma  -  964BF452
NvAPI_GPU_GetBoardInfo  -  22D54523
NvAPI_DRS_SetProfileInfo  -  16ABD3A9
NvAPI_DISP_GetGDIPrimaryDisplayId  -  1E9D8A31
NvAPI_Stereo_SetDriverMode  -  5E8F0BEC
NvAPI_D3D_GetCurrentSLIState  -  4B708B54
NvAPI_SetViewEx  -  06B89E68
NvAPI_I2CRead  -  2FDE12C5
NvAPI_DRS_RestoreProfileDefault  -  FA5F6134
NvAPI_GetDisplayPortInfo  -  C64FF367
NvAPI_VIO_Start  -  CDE8E1A3
NvAPI_OGL_ExpertModeGet  -  22ED9516
NvAPI_EnumNvidiaUnAttachedDisplayHandle  -  20DE9260
NvAPI_SYS_GetGpuAndOutputIdFromDisplayId  -  112BA1A5
NvAPI_Stereo_Deactivate  -  2D68DE96
NvAPI_GPU_GetFullName  -  CEEE8E9F
NvAPI_DRS_DeleteProfileSetting  -  E4A26362
NvAPI_OGL_ExpertModeDefaultsSet  -  B47A657E
NvAPI_GetErrorMessage  -  6C2D048C
NvAPI_SetRefreshRateOverride  -  3092AC32
NvAPI_Stereo_IncreaseSeparation  -  C9A8ECEC
NvAPI_GPU_GetGpuCoreCount  -  C7026A87
NvAPI_SYS_GetDisplayIdFromGpuAndOutputId  -  08F2BAB4
NvAPI_GPU_GetIllumination  -  9A1B9365
NvAPI_SetView  -  0957D7B6
NvAPI_GetAssociatedNvidiaDisplayHandle  -  35C29134
NvAPI_GPU_GetBusId  -  1BE0B8E5
NvAPI_DRS_DeleteApplication  -  2C694BC6
NvAPI_Stereo_SetActiveEye  -  96EEA9F8
NvAPI_GPU_GetAGPAperture  -  6E042794
NvAPI_GetAssociatedDisplayOutputId  -  D995937E
NvAPI_EnableHWCursor  -  2863148D
NvAPI_Stereo_GetEyeSeparation  -  CE653127
NvAPI_DISP_GetMonitorCapabilities  -  3B05C7E1
NvAPI_Stereo_SetConvergence  -  3DD6B54B
NvAPI_GPU_WorkstationFeatureSetup  -  6C1F3FE4
NvAPI_GPU_GetConnectedOutputsWithLidState  -  CF8CAF39
NvAPI_Stereo_IncreaseConvergence  -  A17DAABE
NvAPI_GPU_GetDynamicPstatesInfoEx  -  60DED2ED
NvAPI_GPU_GetVbiosVersionString  -  A561FD7D
NvAPI_GPU_SetECCConfiguration  -  1CF639D9
NvAPI_VIO_EnumDataFormats  -  221FA8E8

*/
