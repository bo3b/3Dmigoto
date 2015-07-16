#include "Main.h"

#include <dxgi.h>
#include <d3d11.h>

#include "../nvapi.h"
#include "../util.h"
#include "../log.h"

using namespace std;

extern "C"
{
	typedef HRESULT(__stdcall *DllCanUnloadNowType)(void);
	static DllCanUnloadNowType DllCanUnloadNowPtr;
	typedef HRESULT(__stdcall *DllGetClassObjectType)(REFCLSID rclsid, REFIID riid, LPVOID FAR* ppv);
	static DllGetClassObjectType DllGetClassObjectPtr;
	typedef HRESULT(__stdcall *DllRegisterServerType)(void);
	static DllRegisterServerType DllRegisterServerPtr;
	typedef HRESULT(__stdcall *DllUnregisterServerType)(void);
	static DllUnregisterServerType DllUnregisterServerPtr;
	typedef NvAPI_Status *(__cdecl *nvapi_QueryInterfaceType)(unsigned int offset);
	static nvapi_QueryInterfaceType nvapi_QueryInterfacePtr;

	typedef NvAPI_Status(__cdecl *tNvAPI_Initialize)(void);
	static tNvAPI_Initialize _NvAPI_Initialize;

	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_GetConvergence)(StereoHandle stereoHandle, float *pConvergence);
	static tNvAPI_Stereo_GetConvergence _NvAPI_Stereo_GetConvergence;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_SetConvergence)(StereoHandle stereoHandle, float newConvergence);
	static tNvAPI_Stereo_SetConvergence _NvAPI_Stereo_SetConvergence;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_GetSeparation)(StereoHandle stereoHandle, float *pSeparationPercentage);
	static tNvAPI_Stereo_GetSeparation _NvAPI_Stereo_GetSeparation;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_SetSeparation)(StereoHandle stereoHandle, float newSeparationPercentage);
	static tNvAPI_Stereo_SetSeparation _NvAPI_Stereo_SetSeparation;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_Disable)();
	static tNvAPI_Stereo_Disable _NvAPI_Stereo_Disable;
	//typedef NvAPI_Status(__cdecl *tNvAPI_D3D9_VideoSetStereoInfo)(IDirect3DDevice9 *pDev,
	//	NV_DX_VIDEO_STEREO_INFO *pStereoInfo);
	//static tNvAPI_D3D9_VideoSetStereoInfo _NvAPI_D3D9_VideoSetStereoInfo;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_CreateConfigurationProfileRegistryKey)(
		NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType);
	static tNvAPI_Stereo_CreateConfigurationProfileRegistryKey _NvAPI_Stereo_CreateConfigurationProfileRegistryKey;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_DeleteConfigurationProfileRegistryKey)(
		NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType);
	static tNvAPI_Stereo_DeleteConfigurationProfileRegistryKey _NvAPI_Stereo_DeleteConfigurationProfileRegistryKey;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_SetConfigurationProfileValue)(
		NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType,
		NV_STEREO_REGISTRY_ID valueRegistryID, void *pValue);
	static tNvAPI_Stereo_SetConfigurationProfileValue _NvAPI_Stereo_SetConfigurationProfileValue;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_DeleteConfigurationProfileValue)(
		NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType,
		NV_STEREO_REGISTRY_ID valueRegistryID);
	static tNvAPI_Stereo_DeleteConfigurationProfileValue _NvAPI_Stereo_DeleteConfigurationProfileValue;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_Enable)(void);
	static tNvAPI_Stereo_Enable _NvAPI_Stereo_Enable;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_IsEnabled)(NvU8 *pIsStereoEnabled);
	static tNvAPI_Stereo_IsEnabled _NvAPI_Stereo_IsEnabled;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_GetStereoSupport)(
		__in NvMonitorHandle hMonitor, __out NVAPI_STEREO_CAPS *pCaps);
	static tNvAPI_Stereo_GetStereoSupport _NvAPI_Stereo_GetStereoSupport;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_CreateHandleFromIUnknown)(
		IUnknown *pDevice, StereoHandle *pStereoHandle);
	static tNvAPI_Stereo_CreateHandleFromIUnknown _NvAPI_Stereo_CreateHandleFromIUnknown;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_DestroyHandle)(StereoHandle stereoHandle);
	static tNvAPI_Stereo_DestroyHandle _NvAPI_Stereo_DestroyHandle;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_Activate)(StereoHandle stereoHandle);
	static tNvAPI_Stereo_Activate _NvAPI_Stereo_Activate;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_Deactivate)(StereoHandle stereoHandle);
	static tNvAPI_Stereo_Deactivate _NvAPI_Stereo_Deactivate;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_IsActivated)(StereoHandle stereoHandle, NvU8 *pIsStereoOn);
	static tNvAPI_Stereo_IsActivated _NvAPI_Stereo_IsActivated;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_DecreaseSeparation)(StereoHandle stereoHandle);
	static tNvAPI_Stereo_DecreaseSeparation _NvAPI_Stereo_DecreaseSeparation;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_IncreaseSeparation)(StereoHandle stereoHandle);
	static tNvAPI_Stereo_IncreaseSeparation _NvAPI_Stereo_IncreaseSeparation;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_DecreaseConvergence)(StereoHandle stereoHandle);
	static tNvAPI_Stereo_DecreaseConvergence _NvAPI_Stereo_DecreaseConvergence;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_IncreaseConvergence)(StereoHandle stereoHandle);
	static tNvAPI_Stereo_IncreaseConvergence _NvAPI_Stereo_IncreaseConvergence;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_GetFrustumAdjustMode)(StereoHandle stereoHandle,
		NV_FRUSTUM_ADJUST_MODE *pFrustumAdjustMode);
	static tNvAPI_Stereo_GetFrustumAdjustMode _NvAPI_Stereo_GetFrustumAdjustMode;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_SetFrustumAdjustMode)(StereoHandle stereoHandle,
		NV_FRUSTUM_ADJUST_MODE newFrustumAdjustModeValue);
	static tNvAPI_Stereo_SetFrustumAdjustMode _NvAPI_Stereo_SetFrustumAdjustMode;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_InitActivation)(__in StereoHandle hStereoHandle,
		__in NVAPI_STEREO_INIT_ACTIVATION_FLAGS flags);
	static tNvAPI_Stereo_InitActivation _NvAPI_Stereo_InitActivation;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_Trigger_Activation)(__in StereoHandle hStereoHandle);
	static tNvAPI_Stereo_Trigger_Activation _NvAPI_Stereo_Trigger_Activation;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_ReverseStereoBlitControl)(StereoHandle hStereoHandle, NvU8 TurnOn);
	static tNvAPI_Stereo_ReverseStereoBlitControl _NvAPI_Stereo_ReverseStereoBlitControl;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_SetActiveEye)(StereoHandle hStereoHandle,
		NV_STEREO_ACTIVE_EYE StereoEye);
	static tNvAPI_Stereo_SetActiveEye _NvAPI_Stereo_SetActiveEye;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_SetDriverMode)(NV_STEREO_DRIVER_MODE mode);
	static tNvAPI_Stereo_SetDriverMode _NvAPI_Stereo_SetDriverMode;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_GetEyeSeparation)(StereoHandle hStereoHandle, float *pSeparation);
	static tNvAPI_Stereo_GetEyeSeparation _NvAPI_Stereo_GetEyeSeparation;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_SetSurfaceCreationMode)(__in StereoHandle hStereoHandle,
		__in NVAPI_STEREO_SURFACECREATEMODE creationMode);
	static tNvAPI_Stereo_SetSurfaceCreationMode _NvAPI_Stereo_SetSurfaceCreationMode;
	typedef NvAPI_Status(__cdecl *tNvAPI_Stereo_GetSurfaceCreationMode)(__in StereoHandle hStereoHandle,
		__in NVAPI_STEREO_SURFACECREATEMODE* pCreationMode);
	static tNvAPI_Stereo_GetSurfaceCreationMode _NvAPI_Stereo_GetSurfaceCreationMode;
	typedef NvAPI_Status(__cdecl *tNvAPI_D3D1x_CreateSwapChain)(StereoHandle hStereoHandle,
		DXGI_SWAP_CHAIN_DESC* pDesc,
		IDXGISwapChain** ppSwapChain,
		NV_STEREO_SWAPCHAIN_MODE mode);
	static tNvAPI_D3D1x_CreateSwapChain _NvAPI_D3D1x_CreateSwapChain;

	//typedef NvAPI_Status(__cdecl *tNvAPI_D3D9_CreateSwapChain)(StereoHandle hStereoHandle,
	//	D3DPRESENT_PARAMETERS *pPresentationParameters,
	//	IDirect3DSwapChain9 **ppSwapChain,
	//	NV_STEREO_SWAPCHAIN_MODE mode);
	//static tNvAPI_D3D9_CreateSwapChain _NvAPI_D3D9_CreateSwapChain;

	typedef NvAPI_Status(__cdecl *tNvAPI_D3D_GetCurrentSLIState)(__in IUnknown *pDevice, __in NV_GET_CURRENT_SLI_STATE *pSliState);
	static tNvAPI_D3D_GetCurrentSLIState _NvAPI_D3D_GetCurrentSLIState;
}

static HMODULE nvDLL = 0;

static bool	ForceNoNvAPI = 0;
static bool NoStereoDisable = 0;
static bool ForceAutomaticStereo = 0;

static map<float, float> GameConvergenceMap, GameConvergenceMapInv;
static bool gDirectXOverride = false;
static int gSurfaceCreateMode = -1;
static bool UnlockSeparation = false;

static float UserConvergence = -1, SetConvergence = -1, GetConvergence = -1;
static float UserSeparation = -1, SetSeparation = -1, GetSeparation = -1;

// ToDo: Reconcile these with the DXGI version.
static int SCREEN_WIDTH = -1;
static int SCREEN_HEIGHT = -1;
static int SCREEN_REFRESH = -1;
static int SCREEN_FULLSCREEN = -1;

static bool LogConvergence = false;
static bool LogSeparation = false;
static bool LogCalls = false;

bool gLogDebug = false;
FILE *LogFile = 0;


#define LogCall(fmt, ...) \
	do { if (LogCalls) LogInfo(fmt, __VA_ARGS__); } while (0)
#define LogSeparation(fmt, ...) \
	do { if (LogSeparation) LogInfo(fmt, __VA_ARGS__); } while (0)
#define LogConvergence(fmt, ...) \
	do { if (LogConvergence) LogInfo(fmt, __VA_ARGS__); } while (0)


// -----------------------------------------------------------------------------------------------

static void LoadConfigFile()
{
	wchar_t iniFile[MAX_PATH], logFilename[MAX_PATH];
	GetModuleFileName(0, iniFile, MAX_PATH);
	wcsrchr(iniFile, L'\\')[1] = 0;
	wcscpy(logFilename, iniFile);
	wcscat(iniFile, L"d3dx.ini");
	wcscat(logFilename, L"nvapi_log.txt");

	LogConvergence = GetPrivateProfileInt(L"Logging", L"convergence", 0, iniFile) == 1;
	LogSeparation = GetPrivateProfileInt(L"Logging", L"separation", 0, iniFile) == 1;
	LogCalls = GetPrivateProfileInt(L"Logging", L"calls", 0, iniFile) == 1;
	gLogDebug = GetPrivateProfileInt(L"Logging", L"debug", 0, iniFile) == 1;

	if (!LogFile && (LogConvergence || LogSeparation || LogCalls || gLogDebug))
		LogFile = _wfsopen(logFilename, L"w", _SH_DENYNO);

	LogInfo("\nNVapi DLL starting init - v %s -  %s\n\n", VER_FILE_VERSION_STR, LogTime().c_str());

	// Unbuffered logging to remove need for fflush calls, and r/w access to make it easy
	// to open active files.
	int unbuffered = -1;
	if (LogFile && GetPrivateProfileInt(L"Logging", L"unbuffered", 0, iniFile))
		unbuffered = setvbuf(LogFile, NULL, _IONBF, 0);

	// Set the CPU affinity based upon d3dx.ini setting.  Useful for debugging and shader hunting in AC3.
	BOOL affinity = -1;
	if (GetPrivateProfileInt(L"Logging", L"force_cpu_affinity", 0, iniFile)) {
		DWORD one = 0x01;
		affinity = SetProcessAffinityMask(GetCurrentProcess(), one);
	}

	LogInfo("[Logging]\n");
	LogCall("  calls=1\n");
	LogDebug("  debug=1\n");
	LogSeparation("  separation=1\n");
	LogConvergence("  convergence=1\n");
	if (unbuffered != -1) LogInfo("  unbuffered=1  return: %d\n", unbuffered);
	if (affinity != -1) LogInfo("  force_cpu_affinity=1  return: %s\n", affinity ? "true" : "false");

	LogInfo("[ConvergenceMap]\n");
	for (int i = 1;; ++i)
	{
		wchar_t id[] = L"Mapxxx", val[MAX_PATH];
		_itow_s(i, id + 3, 3, 10);
		if (!GetPrivateProfileString(L"ConvergenceMap", id, 0, val, MAX_PATH, iniFile))
			break;
		unsigned int fromHx;
		float from, to;
		swscanf_s(val, L"from %x to %e", &fromHx, &to);
		from = *reinterpret_cast<float *>(&fromHx);
		GameConvergenceMap[from] = to;
		GameConvergenceMapInv[to] = from;
		LogInfoW(L"  %ls=from %08x to %f\n", id, fromHx, to);
	}

	// Device
	wchar_t valueString[MAX_PATH];
	if (GetPrivateProfileString(L"Device", L"width", 0, valueString, MAX_PATH, iniFile))
		swscanf_s(valueString, L"%d", &SCREEN_WIDTH);
	if (GetPrivateProfileString(L"Device", L"height", 0, valueString, MAX_PATH, iniFile))
		swscanf_s(valueString, L"%d", &SCREEN_HEIGHT);
	if (GetPrivateProfileString(L"Device", L"refresh_rate", 0, valueString, MAX_PATH, iniFile))
		swscanf_s(valueString, L"%d", &SCREEN_REFRESH);
	if (GetPrivateProfileString(L"Device", L"full_screen", 0, valueString, MAX_PATH, iniFile))
		swscanf_s(valueString, L"%d", &SCREEN_FULLSCREEN);

	LogInfo("[Device]\n");
	if (SCREEN_WIDTH != -1) LogInfo("  width=%d\n", SCREEN_WIDTH);
	if (SCREEN_HEIGHT != -1) LogInfo("  height=%d\n", SCREEN_HEIGHT);
	if (SCREEN_REFRESH != -1) LogInfo("  refresh_rate=%d\n", SCREEN_REFRESH);
	if (SCREEN_FULLSCREEN) LogInfo("  full_screen=1\n");

	// Stereo
	ForceNoNvAPI = GetPrivateProfileInt(L"Stereo", L"force_no_nvapi", 0, iniFile) == 1;
	NoStereoDisable = GetPrivateProfileInt(L"Device", L"force_stereo", 0, iniFile) == 1;
	ForceAutomaticStereo = GetPrivateProfileInt(L"Stereo", L"automatic_mode", 0, iniFile) == 1;
	gSurfaceCreateMode = GetPrivateProfileInt(L"Stereo", L"surface_createmode", -1, iniFile);
	UnlockSeparation = GetPrivateProfileInt(L"Stereo", L"unlock_separation", 0, iniFile) == 1;

	LogInfo("[Stereo]\n");
	LogInfo("  force_no_nvapi=%d \n", ForceNoNvAPI ? 1 : 0);
	LogInfo("  force_stereo=%d \n", NoStereoDisable ? 1 : 0);
	LogInfo("  automatic_mode=%d \n", ForceAutomaticStereo ? 1 : 0);
	LogInfo("  unlock_separation=%d \n", UnlockSeparation ? 1 : 0);
	LogInfo("  surface_createmode=%d \n", gSurfaceCreateMode);
}

static void loadDll()
{
	if (nvDLL)
		return;

	LoadConfigFile();

	// Make sure our d3d11.dll is loaded, so that we get the benefit of the DLLMainHook
	// In general this is not necessary, but if a game calls nvapi before d3d11 loads
	// we'll crash.  This makes sure that won't happen.  In the normal case, the
	// 3Dmigoto d3d11 is already loaded, and this does nothing.
	LoadLibrary(L"d3d11.dll");

	// We need to load the real version of nvapi, in order to get the addresses
	// of the original routines.  This will be fixed up in DLLMainHook to give us the
	// original library, while giving every other caller our library from the game folder.
	// We hook LoadLibraryExW, so we need to use that here.
#if (_WIN64)
#define REAL_NVAPI_DLL L"original_nvapi64.dll"
#else
#define REAL_NVAPI_DLL L"original_nvapi.dll"
#endif
	LogDebugW(L"Trying to load %s \n", REAL_NVAPI_DLL);
	nvDLL = LoadLibraryEx(REAL_NVAPI_DLL, NULL, 0);
	if (nvDLL == NULL)
	{
		LogInfoW(L"*** LoadLibrary of %s failed. *** \n", REAL_NVAPI_DLL);
		DoubleBeepExit();
	}

	DllCanUnloadNowPtr = (DllCanUnloadNowType)GetProcAddress(nvDLL, "DllCanUnloadNow");
	DllGetClassObjectPtr = (DllGetClassObjectType)GetProcAddress(nvDLL, "DllGetClassObject");
	DllRegisterServerPtr = (DllRegisterServerType)GetProcAddress(nvDLL, "DllRegisterServer");
	DllUnregisterServerPtr = (DllUnregisterServerType)GetProcAddress(nvDLL, "DllUnregisterServer");
	nvapi_QueryInterfacePtr = (nvapi_QueryInterfaceType)GetProcAddress(nvDLL, "nvapi_QueryInterface");
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
	return (*DllGetClassObjectPtr)(rclsid, riid, ppv);
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

// -----------------------------------------------------------------------------------------------

// When called to init NvAPI, we'll always allow the init, but if specified in the .ini file we
// will return an error message to fake out the calling game to think that NvAPI is not available.
// This will allow us to use NvAPI for stereo management, but force the game into Automatic Mode.
// This is the equivalent to the technique of editing the .exe to set nvapi->nvbpi.

static NvAPI_Status __cdecl NvAPI_Initialize(void)
{
	LogCall("%s - NvAPI_Initialize called. \n", LogTime().c_str());

	NvAPI_Status ret;

	if (gDirectXOverride || !ForceNoNvAPI)
	{
		ret = (*_NvAPI_Initialize)();
		gDirectXOverride = false;
	}
	else 
	{
		ret = NVAPI_NO_IMPLEMENTATION;
		LogCall("  NvAPI_Initialize force return err: %d \n", ret);
	}

	return ret;
}

static NvAPI_Status __cdecl NvAPI_Stereo_GetConvergence(StereoHandle stereoHandle, float *pConvergence)
{
	NvAPI_Status ret = (*_NvAPI_Stereo_GetConvergence)(stereoHandle, pConvergence);
	if (GetConvergence != *pConvergence)
	{
		LogConvergence("%s - GetConvergence value=%e, hex=%x\n", LogTime().c_str(), GetConvergence = *pConvergence, *reinterpret_cast<unsigned int *>(pConvergence));
	}
	return ret;
}

static NvAPI_Status __cdecl NvAPI_Stereo_SetConvergence(StereoHandle stereoHandle, float newConvergence)
{
	if (SetConvergence != newConvergence)
	{
		LogConvergence("%s - Request SetConvergence to %e, hex=%x\n", LogTime().c_str(), SetConvergence = newConvergence, *reinterpret_cast<unsigned int *>(&newConvergence));
	}

	if (gDirectXOverride)
	{
		LogDebug("%s - Stereo_SetConvergence called from DirectX wrapper: ignoring user overrides.\n", LogTime().c_str());
		gDirectXOverride = false;
		return (*_NvAPI_Stereo_SetConvergence)(stereoHandle, newConvergence);
	}

	// Save current user convergence value.
	float currentConvergence;
	_NvAPI_Stereo_GetConvergence = (tNvAPI_Stereo_GetConvergence)(*nvapi_QueryInterfacePtr)(0x4ab00934);
	(*_NvAPI_Stereo_GetConvergence)(stereoHandle, &currentConvergence);

	if (GameConvergenceMapInv.find(currentConvergence) == GameConvergenceMapInv.end())
		UserConvergence = currentConvergence;
	// Map special convergence value?
	map<float, float>::iterator i = GameConvergenceMap.find(newConvergence);
	if (i != GameConvergenceMap.end())
		newConvergence = i->second;
	else
		// Normal convergence value. Replace with user value.
		newConvergence = UserConvergence;

	// Update needed?
	if (currentConvergence == newConvergence)
		return NVAPI_OK;
	if (SetConvergence != newConvergence)
	{
		LogConvergence("%s - Remap SetConvergence to %e, hex=%x\n", LogTime().c_str(), SetConvergence = newConvergence, *reinterpret_cast<unsigned int *>(&newConvergence));
	}
	return (*_NvAPI_Stereo_SetConvergence)(stereoHandle, newConvergence);
}

static NvAPI_Status __cdecl NvAPI_Stereo_GetSeparation(StereoHandle stereoHandle, float *pSeparationPercentage)
{
	NvAPI_Status ret = (*_NvAPI_Stereo_GetSeparation)(stereoHandle, pSeparationPercentage);
	if (GetSeparation != *pSeparationPercentage)
	{
		LogSeparation("%s - GetSeparation value=%e, hex=%x\n", LogTime().c_str(), GetSeparation = *pSeparationPercentage, *reinterpret_cast<unsigned int *>(pSeparationPercentage));
	}
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_SetSeparation(StereoHandle stereoHandle, float newSeparationPercentage)
{
	if (gDirectXOverride)
	{
		if (gLogDebug) LogCall("%s - Stereo_SetSeparation called from DirectX wrapper: ignoring user overrides.\n", LogTime().c_str());
		gDirectXOverride = false;
		return (*_NvAPI_Stereo_SetSeparation)(stereoHandle, newSeparationPercentage);
	}

	if (UnlockSeparation)
		return NVAPI_OK;

	NvAPI_Status ret = (*_NvAPI_Stereo_SetSeparation)(stereoHandle, newSeparationPercentage);
	if (SetSeparation != newSeparationPercentage)
	{
		LogSeparation("%s - SetSeparation to %e, hex=%x\n", LogTime().c_str(), SetSeparation = newSeparationPercentage, *reinterpret_cast<unsigned int *>(&newSeparationPercentage));
	}
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_Disable()
{
	LogCall("%s - Stereo_Disable called.\n", LogTime().c_str());
	if (NoStereoDisable)
	{
		LogCall("  Stereo_Disable ignored.\n");
		return NVAPI_OK;
	}
	return (*_NvAPI_Stereo_Disable)();
}
//static NvAPI_Status __cdecl NvAPI_D3D9_VideoSetStereoInfo(IDirect3DDevice9 *pDev,
//	NV_DX_VIDEO_STEREO_INFO *pStereoInfo)
//{
//	LogCall("%s - D3D9_VideoSetStereoInfo called width\n", LogTime().c_str());
//	LogCall("  IDirect3DDevice9 = %p\n", pDev);
//	LogCall("  hSurface = %p\n", pStereoInfo->hSurface);
//	LogCall("  Format = %x\n", pStereoInfo->eFormat);
//	LogCall("  StereoEnable = %d\n", pStereoInfo->bStereoEnable);
//	return (*_NvAPI_D3D9_VideoSetStereoInfo)(pDev, pStereoInfo);
//}
static NvAPI_Status __cdecl NvAPI_Stereo_CreateConfigurationProfileRegistryKey(
	NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType)
{
	LogCall("%s - Stereo_CreateConfigurationProfileRegistryKey called width type = %d\n", LogTime().c_str(),
		registryProfileType);
	return (*_NvAPI_Stereo_CreateConfigurationProfileRegistryKey)(registryProfileType);
}
static NvAPI_Status __cdecl NvAPI_Stereo_DeleteConfigurationProfileRegistryKey(
	NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType)
{
	LogCall("%s - Stereo_DeleteConfigurationProfileRegistryKey called width type = %d\n", LogTime().c_str(),
		registryProfileType);
	return (*_NvAPI_Stereo_DeleteConfigurationProfileRegistryKey)(registryProfileType);
}
static NvAPI_Status __cdecl NvAPI_Stereo_SetConfigurationProfileValue(
	NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType,
	NV_STEREO_REGISTRY_ID valueRegistryID, void *pValue)
{
	LogCall("%s - Stereo_SetConfigurationProfileValue called width type = %d\n", LogTime().c_str(),
		registryProfileType);
	LogCall("  value ID = %x\n", valueRegistryID);
	LogCall("  value = %x\n", *(NvAPI_Status*)pValue);
	return (*_NvAPI_Stereo_DeleteConfigurationProfileRegistryKey)(registryProfileType);
}
static NvAPI_Status __cdecl NvAPI_Stereo_DeleteConfigurationProfileValue(
	NV_STEREO_REGISTRY_PROFILE_TYPE registryProfileType,
	NV_STEREO_REGISTRY_ID valueRegistryID)
{
	LogCall("%s - Stereo_SetConfigurationProfileValue called width type = %d\n", LogTime().c_str(),
		registryProfileType);
	LogCall("  value ID = %x\n", valueRegistryID);
	return (*_NvAPI_Stereo_DeleteConfigurationProfileValue)(registryProfileType, valueRegistryID);
}
static NvAPI_Status __cdecl NvAPI_Stereo_Enable()
{
	LogCall("%s - Stereo_Enable called\n", LogTime().c_str());
	return (*_NvAPI_Stereo_Enable)();
}

// Some games like pCars will call to see if stereo is enabled and change behavior.
// We allow the d3dx.ini to force automatic mode by always returning false if specified.

static NvAPI_Status __cdecl NvAPI_Stereo_IsEnabled(NvU8 *pIsStereoEnabled)
{
	LogCall("%s - NvAPI_Stereo_IsEnabled called. \n", LogTime().c_str());

	NvAPI_Status ret = (*_NvAPI_Stereo_IsEnabled)(pIsStereoEnabled);

	if (!gDirectXOverride && ForceAutomaticStereo)
	{
		*pIsStereoEnabled = false;
		gDirectXOverride = false;
		LogCall("  NvAPI_Stereo_IsEnabled force return false \n");
	}

	LogCall("  Returns IsStereoEnabled = %d, Result = %d \n", *pIsStereoEnabled, ret);

	return ret;
}

static NvAPI_Status __cdecl NvAPI_Stereo_GetStereoSupport(
	__in NvMonitorHandle hMonitor, __out NVAPI_STEREO_CAPS *pCaps)
{
	NvAPI_Status ret = (*_NvAPI_Stereo_GetStereoSupport)(hMonitor, pCaps);
	LogCall("%s - Stereo_GetStereoSupportStereo_Enable called with hMonitor = %p. Returns:\n", LogTime().c_str(),
		hMonitor);
	LogCall("  result = %d\n", ret);
	LogCall("  version = %d\n", pCaps->version);
	LogCall("  supportsWindowedModeOff = %d\n", pCaps->supportsWindowedModeOff);
	LogCall("  supportsWindowedModeAutomatic = %d\n", pCaps->supportsWindowedModeAutomatic);
	LogCall("  supportsWindowedModePersistent = %d\n", pCaps->supportsWindowedModePersistent);
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_CreateHandleFromIUnknown(
	IUnknown *pDevice, StereoHandle *pStereoHandle)
{
	NvAPI_Status ret = (*_NvAPI_Stereo_CreateHandleFromIUnknown)(pDevice, pStereoHandle);
	LogCall("%s - Stereo_CreateHandleFromIUnknown called with device = %p. Result = %d\n", LogTime().c_str(), pDevice, ret);
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_DestroyHandle(StereoHandle stereoHandle)
{
	NvAPI_Status ret = (*_NvAPI_Stereo_DestroyHandle)(stereoHandle);
	LogCall("%s - Stereo_DestroyHandle called. Result = %d\n", LogTime().c_str(), ret);
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_Activate(StereoHandle stereoHandle)
{
	NvAPI_Status ret = (*_NvAPI_Stereo_Activate)(stereoHandle);
	LogCall("%s - Stereo_Activate called. Result = %d\n", LogTime().c_str(), ret);
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_Deactivate(StereoHandle stereoHandle)
{
	LogCall("%s - Stereo_Deactivate called.\n", LogTime().c_str());
	if (NoStereoDisable)
	{
		LogCall("  Stereo_Deactivate ignored.\n");
		return NVAPI_OK;
	}
	NvAPI_Status ret = (*_NvAPI_Stereo_Deactivate)(stereoHandle);
	LogCall("  Result = %d\n", ret);
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_IsActivated(StereoHandle stereoHandle, NvU8 *pIsStereoOn)
{
	NvAPI_Status ret = (*_NvAPI_Stereo_IsActivated)(stereoHandle, pIsStereoOn);
	if (gLogDebug)
		LogCall("%s - Stereo_IsActivated called. Result = %d, IsStereoOn = %d\n", LogTime().c_str(), ret, *pIsStereoOn);
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_DecreaseSeparation(StereoHandle stereoHandle)
{
	LogCall("%s - Stereo_DecreaseSeparation called.\n", LogTime().c_str());
	return (*_NvAPI_Stereo_DecreaseSeparation)(stereoHandle);
}
static NvAPI_Status __cdecl NvAPI_Stereo_IncreaseSeparation(StereoHandle stereoHandle)
{
	LogCall("%s - Stereo_IncreaseSeparation called.\n", LogTime().c_str());
	return (*_NvAPI_Stereo_IncreaseSeparation)(stereoHandle);
}
static NvAPI_Status __cdecl NvAPI_Stereo_DecreaseConvergence(StereoHandle stereoHandle)
{
	LogCall("%s - Stereo_DecreaseConvergence called.\n", LogTime().c_str());
	return (*_NvAPI_Stereo_DecreaseConvergence)(stereoHandle);
}
static NvAPI_Status __cdecl NvAPI_Stereo_IncreaseConvergence(StereoHandle stereoHandle)
{
	LogCall("%s - Stereo_IncreaseConvergence called.\n", LogTime().c_str());
	return (*_NvAPI_Stereo_IncreaseConvergence)(stereoHandle);
}
static NvAPI_Status __cdecl NvAPI_Stereo_GetFrustumAdjustMode(StereoHandle stereoHandle,
	NV_FRUSTUM_ADJUST_MODE *pFrustumAdjustMode)
{
	NvAPI_Status ret = (*_NvAPI_Stereo_GetFrustumAdjustMode)(stereoHandle, pFrustumAdjustMode);
	LogCall("%s - Stereo_GetFrustumAdjustMode called. Result = %d, returns:\n", LogTime().c_str(), ret);
	LogCall("  FrustumAdjustMode = %d\n", *pFrustumAdjustMode);
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_SetFrustumAdjustMode(StereoHandle stereoHandle,
	NV_FRUSTUM_ADJUST_MODE newFrustumAdjustModeValue)
{
	LogCall("%s - Stereo_SetFrustumAdjustMode called with FrustumAdjustMode = %d\n", LogTime().c_str(), newFrustumAdjustModeValue);
	return (*_NvAPI_Stereo_SetFrustumAdjustMode)(stereoHandle, newFrustumAdjustModeValue);
}
static NvAPI_Status __cdecl NvAPI_Stereo_InitActivation(__in StereoHandle hStereoHandle,
	__in NVAPI_STEREO_INIT_ACTIVATION_FLAGS flags)
{
	NvAPI_Status ret = (*_NvAPI_Stereo_InitActivation)(hStereoHandle, flags);
	LogCall("%s - Stereo_InitActivation called with flags = %d. Result = %d\n", LogTime().c_str(),
		flags, ret);
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_Trigger_Activation(__in StereoHandle hStereoHandle)
{
	NvAPI_Status ret = (*_NvAPI_Stereo_Trigger_Activation)(hStereoHandle);
	LogCall("%s - Stereo_Trigger_Activation called. Result = %d\n", LogTime().c_str(), ret);
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_ReverseStereoBlitControl(StereoHandle hStereoHandle, NvU8 TurnOn)
{
	NvAPI_Status ret = (*_NvAPI_Stereo_ReverseStereoBlitControl)(hStereoHandle, TurnOn);
	LogCall("%s - Stereo_Trigger_Activation called with TurnOn = %d. Result = %d\n", LogTime().c_str(),
		TurnOn, ret);
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_SetActiveEye(StereoHandle hStereoHandle,
	NV_STEREO_ACTIVE_EYE StereoEye)
{
	NvAPI_Status ret = (*_NvAPI_Stereo_SetActiveEye)(hStereoHandle, StereoEye);
	LogCall("%s - Stereo_SetActiveEye called with StereoEye = %d. Result = %d\n", LogTime().c_str(),
		StereoEye, ret);
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_SetDriverMode(NV_STEREO_DRIVER_MODE mode)
{
	if (LogCalls)
	{
		LogInfo("%s - Stereo_SetDriverMode called with mode = %d.\n", LogTime().c_str(), mode);
		switch (mode)
		{
			case NVAPI_STEREO_DRIVER_MODE_AUTOMATIC:
				LogInfo("  mode %d means automatic mode\n", mode);
				break;
			case NVAPI_STEREO_DRIVER_MODE_DIRECT:
				LogInfo("  mode %d means direct mode\n", mode);
				break;
		}
	}
	if (ForceAutomaticStereo && mode != NVAPI_STEREO_DRIVER_MODE_AUTOMATIC)
	{
		LogCall("    mode forced to automatic mode\n");
		mode = NVAPI_STEREO_DRIVER_MODE_AUTOMATIC;
	}
	NvAPI_Status ret = (*_NvAPI_Stereo_SetDriverMode)(mode);
	LogCall("  Result = %d\n", ret);
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_GetEyeSeparation(StereoHandle hStereoHandle, float *pSeparation)
{
	NvAPI_Status ret = (*_NvAPI_Stereo_GetEyeSeparation)(hStereoHandle, pSeparation);
	if (gLogDebug)
	{
		LogSeparation("%s - Stereo_GetEyeSeparation called. Result = %d, Separation = %f\n", LogTime().c_str(),
			ret, *pSeparation);
	}
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_SetSurfaceCreationMode(__in StereoHandle hStereoHandle,
	__in NVAPI_STEREO_SURFACECREATEMODE creationMode)
{
	if (gDirectXOverride)
	{
		LogCall("%s - Stereo_SetSurfaceCreationMode called from DirectX wrapper: ignoring user overrides.\n", LogTime().c_str());
		gDirectXOverride = false;
	}
	else if (gSurfaceCreateMode >= 0)
	{
		creationMode = (NVAPI_STEREO_SURFACECREATEMODE)gSurfaceCreateMode;
	}

	NvAPI_Status ret = (*_NvAPI_Stereo_SetSurfaceCreationMode)(hStereoHandle, creationMode);
	LogCall("%s - Stereo_SetSurfaceCreationMode called with CreationMode = %d. Result = %d\n", LogTime().c_str(),
		creationMode, ret);
	return ret;
}
static NvAPI_Status __cdecl NvAPI_Stereo_GetSurfaceCreationMode(__in StereoHandle hStereoHandle,
	__in NVAPI_STEREO_SURFACECREATEMODE* pCreationMode)
{
	NvAPI_Status ret = (*_NvAPI_Stereo_GetSurfaceCreationMode)(hStereoHandle, pCreationMode);
	LogCall("%s - Stereo_GetSurfaceCreationMode called. Result = %d, CreationMode = %d\n", LogTime().c_str(),
		ret, *pCreationMode);
	return ret;
}
static NvAPI_Status __cdecl NvAPI_D3D1x_CreateSwapChain(StereoHandle hStereoHandle,
	DXGI_SWAP_CHAIN_DESC* pDesc,
	IDXGISwapChain** ppSwapChain,
	NV_STEREO_SWAPCHAIN_MODE mode)
{
	LogCall("%s - NVAPI::D3D1x_CreateSwapChain called with parameters\n", LogTime().c_str());
	LogCall("  Width = %d\n", pDesc->BufferDesc.Width);
	LogCall("  Height = %d\n", pDesc->BufferDesc.Height);
	LogCall("  Refresh rate = %f\n",
		(float)pDesc->BufferDesc.RefreshRate.Numerator / (float)pDesc->BufferDesc.RefreshRate.Denominator);
	LogCall("  Windowed = %d\n", pDesc->Windowed);

	if (SCREEN_REFRESH >= 0)
	{
		pDesc->BufferDesc.RefreshRate.Numerator = SCREEN_REFRESH;
		pDesc->BufferDesc.RefreshRate.Denominator = 1;
	}
	if (SCREEN_WIDTH >= 0) pDesc->BufferDesc.Width = SCREEN_WIDTH;
	if (SCREEN_HEIGHT >= 0) pDesc->BufferDesc.Height = SCREEN_HEIGHT;
	if (SCREEN_FULLSCREEN >= 0) pDesc->Windowed = !SCREEN_FULLSCREEN;

	NvAPI_Status ret = (*_NvAPI_D3D1x_CreateSwapChain)(hStereoHandle, pDesc, ppSwapChain, mode);
	LogCall("  returned %d\n", ret);
	return ret;
}
//static NvAPI_Status __cdecl NvAPI_D3D9_CreateSwapChain(StereoHandle hStereoHandle,
//	D3DPRESENT_PARAMETERS *pPresentationParameters,
//	IDirect3DSwapChain9 **ppSwapChain,
//	NV_STEREO_SWAPCHAIN_MODE mode)
//{
//	LogCall("%s - D3D9_CreateSwapChain called with parameters\n", LogTime().c_str());
//	LogCall("  Width = %d\n", pPresentationParameters->BackBufferWidth);
//	LogCall("  Height = %d\n", pPresentationParameters->BackBufferHeight);
//	LogCall("  Refresh rate = %d\n", pPresentationParameters->FullScreen_RefreshRateInHz);
//	LogCall("  Windowed = %d\n", pPresentationParameters->Windowed);
//	if (SCREEN_REFRESH >= 0)
//	{
//		LogCall("    overriding refresh rate = %d\n", SCREEN_REFRESH);
//		pPresentationParameters->FullScreen_RefreshRateInHz = SCREEN_REFRESH;
//	}
//	if (SCREEN_WIDTH >= 0)
//	{
//		LogCall("    overriding width = %d\n", SCREEN_WIDTH);
//		pPresentationParameters->BackBufferWidth = SCREEN_WIDTH;
//	}
//	if (SCREEN_HEIGHT >= 0)
//	{
//		LogCall("    overriding height = %d\n", SCREEN_HEIGHT);
//		pPresentationParameters->BackBufferHeight = SCREEN_HEIGHT;
//	}
//	if (SCREEN_FULLSCREEN >= 0)
//	{
//		LogCall("    overriding full screen = %d\n", SCREEN_FULLSCREEN);
//		pPresentationParameters->Windowed = !SCREEN_FULLSCREEN;
//	}
//	NvAPI_Status ret = (*_NvAPI_D3D9_CreateSwapChain)(hStereoHandle, pPresentationParameters, ppSwapChain, mode);
//	LogCall("  returned %d\n", ret);
//	return ret;
//}

static NvAPI_Status __cdecl NvAPI_D3D_GetCurrentSLIState(__in IUnknown *pDevice, __in NV_GET_CURRENT_SLI_STATE *pSliState)
{
	NvAPI_Status ret = (*_NvAPI_D3D_GetCurrentSLIState)(pDevice, pSliState);
	LogCall("%s - NvAPI_D3D_GetCurrentSLIState called with device = %p. Result = %d\n", LogTime().c_str(), pDevice, ret);
	return ret;
}

// This seems like it might have a reentrancy hole, where a given call sets up to not
// be overridden, but something else sneaks in and steals it.  
// In fact, I'm exploiting that for force_no_nvapi because I need the Initialize from
// the stereo driver to succeed, and call enable_stereo after an EnableOverride, so that
// the Initialize succeeds.

static NvAPI_Status __cdecl EnableOverride(void)
{
	LogCall("%s - NvAPI EnableOverride called. Next NvAPI call made will not be wrapped. \n", LogTime().c_str());

	gDirectXOverride = true;

	return (NvAPI_Status)0xeecc34ab;
}


// __declspec(dllexport)
// Removed this declare spec, because we are using the .def file for declarations.
// This fixes a warning from x64 compiles.

extern "C" NvAPI_Status * __cdecl nvapi_QueryInterface(unsigned int offset)
{
	loadDll();
	NvAPI_Status *ptr = (*nvapi_QueryInterfacePtr)(offset);
	switch (offset)
	{
		// Special signature for being called from d3d11 dll code.
		case 0xb03bb03b:
			ptr = (NvAPI_Status *)EnableOverride();
			break;
	
		case 0x0150E828:
			_NvAPI_Initialize = (tNvAPI_Initialize)ptr;
			ptr = (NvAPI_Status *)NvAPI_Initialize;
			break;
		case 0x4ab00934:
			_NvAPI_Stereo_GetConvergence = (tNvAPI_Stereo_GetConvergence)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_GetConvergence;
			break;
		case 0x3dd6b54b:
			_NvAPI_Stereo_SetConvergence = (tNvAPI_Stereo_SetConvergence)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_SetConvergence;
			break;
		case 0x451f2134:
			_NvAPI_Stereo_GetSeparation = (tNvAPI_Stereo_GetSeparation)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_GetSeparation;
			break;
		case 0x5c069fa3:
			_NvAPI_Stereo_SetSeparation = (tNvAPI_Stereo_SetSeparation)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_SetSeparation;
			break;
		case 0x2ec50c2b:
			_NvAPI_Stereo_Disable = (tNvAPI_Stereo_Disable)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_Disable;
			break;
		//case 0xB852F4DB:
		//	_NvAPI_D3D9_VideoSetStereoInfo = (tNvAPI_D3D9_VideoSetStereoInfo)ptr;
		//	ptr = (NvAPI_Status *)NvAPI_D3D9_VideoSetStereoInfo;
		//	break;
		case 0xBE7692EC:
			_NvAPI_Stereo_CreateConfigurationProfileRegistryKey = (tNvAPI_Stereo_CreateConfigurationProfileRegistryKey)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_CreateConfigurationProfileRegistryKey;
			break;
		case 0xF117B834:
			_NvAPI_Stereo_DeleteConfigurationProfileRegistryKey = (tNvAPI_Stereo_DeleteConfigurationProfileRegistryKey)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_DeleteConfigurationProfileRegistryKey;
			break;
		case 0x24409F48:
			_NvAPI_Stereo_SetConfigurationProfileValue = (tNvAPI_Stereo_SetConfigurationProfileValue)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_SetConfigurationProfileValue;
			break;
		case 0x49BCEECF:
			_NvAPI_Stereo_DeleteConfigurationProfileValue = (tNvAPI_Stereo_DeleteConfigurationProfileValue)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_DeleteConfigurationProfileValue;
			break;
		case 0x239C4545:
			_NvAPI_Stereo_Enable = (tNvAPI_Stereo_Enable)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_Enable;
			break;
		case 0x348FF8E1:
			_NvAPI_Stereo_IsEnabled = (tNvAPI_Stereo_IsEnabled)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_IsEnabled;
			break;
		case 0x296C434D:
			_NvAPI_Stereo_GetStereoSupport = (tNvAPI_Stereo_GetStereoSupport)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_GetStereoSupport;
			break;
		case 0xAC7E37F4:
			_NvAPI_Stereo_CreateHandleFromIUnknown = (tNvAPI_Stereo_CreateHandleFromIUnknown)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_CreateHandleFromIUnknown;
			break;
		case 0x3A153134:
			_NvAPI_Stereo_DestroyHandle = (tNvAPI_Stereo_DestroyHandle)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_DestroyHandle;
			break;
		case 0xF6A1AD68:
			_NvAPI_Stereo_Activate = (tNvAPI_Stereo_Activate)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_Activate;
			break;
		case 0x2D68DE96:
			_NvAPI_Stereo_Deactivate = (tNvAPI_Stereo_Deactivate)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_Deactivate;
			break;
		case 0x1FB0BC30:
			_NvAPI_Stereo_IsActivated = (tNvAPI_Stereo_IsActivated)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_IsActivated;
			break;
		case 0xDA044458:
			_NvAPI_Stereo_DecreaseSeparation = (tNvAPI_Stereo_DecreaseSeparation)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_DecreaseSeparation;
			break;
		case 0xC9A8ECEC:
			_NvAPI_Stereo_IncreaseSeparation = (tNvAPI_Stereo_IncreaseSeparation)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_IncreaseSeparation;
			break;
		case 0x4C87E317:
			_NvAPI_Stereo_DecreaseConvergence = (tNvAPI_Stereo_DecreaseConvergence)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_DecreaseConvergence;
			break;
		case 0xA17DAABE:
			_NvAPI_Stereo_IncreaseConvergence = (tNvAPI_Stereo_IncreaseConvergence)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_IncreaseConvergence;
			break;
		case 0xE6839B43:
			_NvAPI_Stereo_GetFrustumAdjustMode = (tNvAPI_Stereo_GetFrustumAdjustMode)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_GetFrustumAdjustMode;
			break;
		case 0x7BE27FA2:
			_NvAPI_Stereo_SetFrustumAdjustMode = (tNvAPI_Stereo_SetFrustumAdjustMode)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_SetFrustumAdjustMode;
			break;
		case 0xC7177702:
			_NvAPI_Stereo_InitActivation = (tNvAPI_Stereo_InitActivation)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_InitActivation;
			break;
		case 0x0D6C6CD2:
			_NvAPI_Stereo_Trigger_Activation = (tNvAPI_Stereo_Trigger_Activation)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_Trigger_Activation;
			break;
		case 0x3CD58F89:
			_NvAPI_Stereo_ReverseStereoBlitControl = (tNvAPI_Stereo_ReverseStereoBlitControl)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_ReverseStereoBlitControl;
			break;
		case 0x96EEA9F8:
			_NvAPI_Stereo_SetActiveEye = (tNvAPI_Stereo_SetActiveEye)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_SetActiveEye;
			break;
		case 0x5E8F0BEC:
			_NvAPI_Stereo_SetDriverMode = (tNvAPI_Stereo_SetDriverMode)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_SetDriverMode;
			break;
		case 0xCE653127:
			_NvAPI_Stereo_GetEyeSeparation = (tNvAPI_Stereo_GetEyeSeparation)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_GetEyeSeparation;
			break;
		case 0xF5DCFCBA:
			_NvAPI_Stereo_SetSurfaceCreationMode = (tNvAPI_Stereo_SetSurfaceCreationMode)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_SetSurfaceCreationMode;
			break;
		case 0x36F1C736:
			_NvAPI_Stereo_GetSurfaceCreationMode = (tNvAPI_Stereo_GetSurfaceCreationMode)ptr;
			ptr = (NvAPI_Status *)NvAPI_Stereo_GetSurfaceCreationMode;
			break;
		case 0x1BC21B66:
			_NvAPI_D3D1x_CreateSwapChain = (tNvAPI_D3D1x_CreateSwapChain)ptr;
			ptr = (NvAPI_Status *)NvAPI_D3D1x_CreateSwapChain;
			break;
		//case 0x1A131E09:
		//	_NvAPI_D3D9_CreateSwapChain = (tNvAPI_D3D9_CreateSwapChain)ptr;
		//	ptr = (NvAPI_Status *)NvAPI_D3D9_CreateSwapChain;
		//	break;

			// Informational logging
		case 0x4B708B54:
			_NvAPI_D3D_GetCurrentSLIState = (tNvAPI_D3D_GetCurrentSLIState)ptr;
			ptr = (NvAPI_Status *)NvAPI_D3D_GetCurrentSLIState;
			break;
	}
	// If it's not on our list of calls to wrap, just pass through.
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
NvAPI_GetNvAPI_StatuserfaceVersionString  -  01053FA5
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


_NvAPI_Initialize   150E828h
_NvAPI_Unload   0D22BDD7Eh
_NvAPI_GetErrorMessage  6C2D048Ch
_NvAPI_GetNvAPI_StatuserfaceVersionString    1053FA5h
_NvAPI_GetDisplayDriverVersion  0F951A4D1h
_NvAPI_SYS_GetDriverAndBranchVersion    2926AAADh
_NvAPI_EnumNvidiaDisplayHandle  9ABDD40Dh
_NvAPI_EnumNvidiaUnAttachedDisplayHandle    20DE9260h
_NvAPI_EnumPhysicalGPUs 0E5AC921Fh
_NvAPI_EnumLogicalGPUs  48B3EA59h
_NvAPI_GetPhysicalGPUsFromDisplay   34EF9506h
_NvAPI_GetPhysicalGPUFromUnAttachedDisplay  5018ED61h
_NvAPI_CreateDisplayFromUnAttachedDisplay   63F9799Eh
_NvAPI_GetLogicalGPUFromDisplay 0EE1370CFh
_NvAPI_GetLogicalGPUFromPhysicalGPU 0ADD604D1h
_NvAPI_GetPhysicalGPUsFromLogicalGPU    0AEA3FA32h
_NvAPI_GetAssociatedNvidiaDisplayHandle 35C29134h
_NvAPI_DISP_GetAssociatedUnAttachedNvidiaDisplayHandle  0A70503B2h
_NvAPI_GetAssociatedNvidiaDisplayName   22A78B05h
_NvAPI_GetUnAttachedAssociatedDisplayName   4888D790h
_NvAPI_EnableHWCursor   2863148Dh
_NvAPI_DisableHWCursor  0AB163097h
_NvAPI_GetVBlankCounter 67B5DB55h
_NvAPI_SetRefreshRateOverride   3092AC32h
_NvAPI_GetAssociatedDisplayOutputId 0D995937Eh
_NvAPI_GetDisplayPortInfo   0C64FF367h
_NvAPI_SetDisplayPort   0FA13E65Ah
_NvAPI_GetHDMISupportInfo   6AE16EC3h
_NvAPI_DISP_EnumHDMIStereoModes 0D2CCF5D6h
_NvAPI_GetInfoFrame 9734F1Dh
_NvAPI_SetInfoFrame 69C6F365h
_NvAPI_SetInfoFrameState    67EFD887h
_NvAPI_GetInfoFrameState    41511594h
_NvAPI_Disp_InfoFrameControl    6067AF3Fh
_NvAPI_Disp_ColorControl    92F9D80Dh
_NvAPI_DISP_GetVirtualModeData  3230D69Ah
_NvAPI_DISP_OverrideDisplayModeList 291BFF2h
_NvAPI_GetDisplayDriverMemoryInfo   774AA982h
_NvAPI_GetDriverMemoryInfo  2DC95125h
_NvAPI_GetDVCInfo   4085DE45h
_NvAPI_SetDVCLevel  172409B4h
_NvAPI_GetDVCInfoEx 0E45002Dh
_NvAPI_SetDVCLevelEx    4A82C2B1h
_NvAPI_GetHUEInfo   95B64341h
_NvAPI_SetHUEAngle  0F5A0F22Ch
_NvAPI_GetImageSharpeningInfo   9FB063DFh
_NvAPI_SetImageSharpeningLevel  3FC9A59Ch
_NvAPI_D3D_GetCurrentSLIState   4B708B54h
_NvAPI_D3D9_RegisterResource    0A064BDFCh
_NvAPI_D3D9_UnregisterResource  0BB2B17AAh
_NvAPI_D3D9_AliasSurfaceAsTexture   0E5CEAE41h
_NvAPI_D3D9_StretchRectEx   22DE03AAh
_NvAPI_D3D9_ClearRT 332D3942h
_NvAPI_D3D_CreateQuery  5D19BCA4h
_NvAPI_D3D_DestroyQuery 0C8FF7258h
_NvAPI_D3D_Query_Begin  0E5A9AAE0h
_NvAPI_D3D_Query_End    2AC084FAh
_NvAPI_D3D_Query_GetData    0F8B53C69h
_NvAPI_D3D_Query_GetDataSize    0F2A54796h
_NvAPI_D3D_Query_GetType    4ACEEAF7h
_NvAPI_D3D_RegisterApp  0D44D3C4Eh
_NvAPI_D3D9_CreatePathContextNV 0A342F682h
_NvAPI_D3D9_DestroyPathContextNV    667C2929h
_NvAPI_D3D9_CreatePathNV    71329DF3h
_NvAPI_D3D9_DeletePathNV    73E0019Ah
_NvAPI_D3D9_PathVerticesNV  0C23DF926h
_NvAPI_D3D9_PathParameterfNV    0F7FF00C1h
_NvAPI_D3D9_PathParameteriNV    0FC31236Ch
_NvAPI_D3D9_PathMatrixNV    0D2F6C499h
_NvAPI_D3D9_PathDepthNV 0FCB16330h
_NvAPI_D3D9_PathClearDepthNV    157E45C4h
_NvAPI_D3D9_PathEnableDepthTestNV   0E99BA7F3h
_NvAPI_D3D9_PathEnableColorWriteNV  3E2804A2h
_NvAPI_D3D9_DrawPathNV  13199B3Dh
_NvAPI_D3D9_GetSurfaceHandle    0F2DD3F2h
_NvAPI_D3D9_GetOverlaySurfaceHandles    6800F5FCh
_NvAPI_D3D9_GetTextureHandle    0C7985ED5h
_NvAPI_D3D9_GpuSyncGetHandleSize    80C9FD3Bh
_NvAPI_D3D9_GpuSyncInit 6D6FDAD4h
_NvAPI_D3D9_GpuSyncEnd  754033F0h
_NvAPI_D3D9_GpuSyncMapTexBuffer 0CDE4A28Ah
_NvAPI_D3D9_GpuSyncMapSurfaceBuffer 2AB714ABh
_NvAPI_D3D9_GpuSyncMapVertexBuffer  0DBC803ECh
_NvAPI_D3D9_GpuSyncMapIndexBuffer   12EE68F2h
_NvAPI_D3D9_SetPitchSurfaceCreation 18CDF365h
_NvAPI_D3D9_GpuSyncAcquire  0D00B8317h
_NvAPI_D3D9_GpuSyncRelease  3D7A86BBh
_NvAPI_D3D9_GetCurrentRenderTargetHandle    22CAD61h
_NvAPI_D3D9_GetCurrentZBufferHandle 0B380F218h
_NvAPI_D3D9_GetIndexBufferHandle    0FC5A155Bh
_NvAPI_D3D9_GetVertexBufferHandle   72B19155h
_NvAPI_D3D9_CreateTexture   0D5E13573h
_NvAPI_D3D9_AliasPrimaryAsTexture   13C7112Eh
_NvAPI_D3D9_PresentSurfaceToDesktop 0F7029C5h
_NvAPI_D3D9_CreateVideoBegin    84C9D553h
_NvAPI_D3D9_CreateVideoEnd  0B476BF61h
_NvAPI_D3D9_CreateVideo 89FFD9A3h
_NvAPI_D3D9_FreeVideo   3111BED1h
_NvAPI_D3D9_PresentVideo    5CF7F862h
_NvAPI_D3D9_VideoSetStereoInfo  0B852F4DBh
_NvAPI_D3D9_SetGamutData    2BBDA32Eh
_NvAPI_D3D9_SetSurfaceCreationLayout    5609B86Ah
_NvAPI_D3D9_GetVideoCapabilities    3D596B93h
_NvAPI_D3D9_QueryVideoInfo  1E6634B3h
_NvAPI_D3D9_AliasPrimaryFromDevice  7C20C5BEh
_NvAPI_D3D9_SetResourceHNvAPI_Status 905F5C27h
_NvAPI_D3D9_Lock    6317345Ch
_NvAPI_D3D9_Unlock  0C182027Eh
_NvAPI_D3D9_GetVideoState   0A4527BF8h
_NvAPI_D3D9_SetVideoState   0BD4BC56Fh
_NvAPI_D3D9_EnumVideoFeatures   1DB7C52Ch
_NvAPI_D3D9_GetSLIInfo  694BFF4Dh
_NvAPI_D3D9_SetSLIMode  0BFDC062Ch
_NvAPI_D3D9_QueryAAOverrideMode 0DDF5643Ch
_NvAPI_D3D9_VideoSurfaceEncryptionControl   9D2509EFh
_NvAPI_D3D9_DMA 962B8AF6h
_NvAPI_D3D9_EnableStereo    492A6954h
_NvAPI_D3D9_StretchRect 0AEAECD41h
_NvAPI_D3D9_CreateRenderTarget  0B3827C8h
_NvAPI_D3D9_NVFBC_GetStatus 0BD3EB475h
_NvAPI_D3D9_IFR_SetUpTargetBufferToSys  55255D05h
_NvAPI_D3D9_GPUBasedCPUSleep    0D504DDA7h
_NvAPI_D3D9_IFR_TransferRenderTarget    0AB7C2DCh
_NvAPI_D3D9_IFR_SetUpTargetBufferToNV12BLVideoSurface   0CFC92C15h
_NvAPI_D3D9_IFR_TransferRenderTargetToNV12BLVideoSurface    5FE72F64h
_NvAPI_D3D10_AliasPrimaryAsTexture  8AAC133Dh
_NvAPI_D3D10_SetPrimaryFlipChainCallbacks   73EB9329h
_NvAPI_D3D10_ProcessCallbacks   0AE9C2019h
_NvAPI_D3D10_GetRenderedCursorAsBitmap  0CAC3CE5Dh
_NvAPI_D3D10_BeginShareResource 35233210h
_NvAPI_D3D10_BeginShareResourceEx   0EF303A9Dh
_NvAPI_D3D10_EndShareResource   0E9C5853h
_NvAPI_D3D10_SetDepthBoundsTest 4EADF5D2h
_NvAPI_D3D10_CreateDevice   2DE11D61h
_NvAPI_D3D10_CreateDeviceAndSwapChain   5B803DAFh
_NvAPI_D3D11_CreateDevice   6A16D3A0h
_NvAPI_D3D11_CreateDeviceAndSwapChain   0BB939EE5h
_NvAPI_D3D11_BeginShareResource 121BDC6h
_NvAPI_D3D11_EndShareResource   8FFB8E26h
_NvAPI_D3D11_SetDepthBoundsTest 7AAF7A04h
_NvAPI_GPU_GetShaderPipeCount   63E2F56Fh
_NvAPI_GPU_GetShaderSubPipeCount    0BE17923h
_NvAPI_GPU_GetPartitionCount    86F05D7Ah
_NvAPI_GPU_GetMemPartitionMask  329D77CDh
_NvAPI_GPU_GetTPCMask   4A35DF54h
_NvAPI_GPU_GetSMMask    0EB7AF173h
_NvAPI_GPU_GetTotalTPCCount 4E2F76A8h
_NvAPI_GPU_GetTotalSMCount  0AE5FBCFEh
_NvAPI_GPU_GetTotalSPCount  0B6D62591h
_NvAPI_GPU_GetGpuCoreCount  0C7026A87h
_NvAPI_GPU_GetAllOutputs    7D554F8Eh
_NvAPI_GPU_GetConnectedOutputs  1730BFC9h
_NvAPI_GPU_GetConnectedSLIOutputs   680DE09h
_NvAPI_GPU_GetConnectedDisplayIds   78DBA2h
_NvAPI_GPU_GetAllDisplayIds 785210A2h
_NvAPI_GPU_GetConnectedOutputsWithLidState  0CF8CAF39h
_NvAPI_GPU_GetConnectedSLIOutputsWithLidState   96043CC7h
_NvAPI_GPU_GetSystemType    0BAAABFCCh
_NvAPI_GPU_GetActiveOutputs 0E3E89B6Fh
_NvAPI_GPU_GetEDID  37D32E69h
_NvAPI_GPU_SetEDID  0E83D6456h
_NvAPI_GPU_GetOutputType    40A505E4h
_NvAPI_GPU_GetDeviceDisplayMode 0D2277E3Ah
_NvAPI_GPU_GetFlatPanelInfo 36CFF969h
_NvAPI_GPU_ValidateOutputCombination    34C9C2D4h
_NvAPI_GPU_GetConnectorInfo 4ECA2C10h
_NvAPI_GPU_GetFullName  0CEEE8E9Fh
_NvAPI_GPU_GetPCIIdentifiers    2DDFB66Eh
_NvAPI_GPU_GetGPUType   0C33BAEB1h
_NvAPI_GPU_GetBusType   1BB18724h
_NvAPI_GPU_GetBusId 1BE0B8E5h
_NvAPI_GPU_GetBusSlotId 2A0A350Fh
_NvAPI_GPU_GetIRQ   0E4715417h
_NvAPI_GPU_GetVbiosRevision 0ACC3DA0Ah
_NvAPI_GPU_GetVbiosOEMRevision  2D43FB31h
_NvAPI_GPU_GetVbiosVersionString    0A561FD7Dh
_NvAPI_GPU_GetAGPAperture   6E042794h
_NvAPI_GPU_GetCurrentAGPRate    0C74925A0h
_NvAPI_GPU_GetCurrentPCIEDownstreamWidth    0D048C3B1h
_NvAPI_GPU_GetPhysicalFrameBufferSize   46FBEB03h
_NvAPI_GPU_GetVirtualFrameBufferSize    5A04B644h
_NvAPI_GPU_GetQuadroStatus  0E332FA47h
_NvAPI_GPU_GetBoardInfo 22D54523h
_NvAPI_GPU_GetRamType   57F7CAACh
_NvAPI_GPU_GetFBWidthAndLocation    11104158h
_NvAPI_GPU_GetAllClockFrequencies   0DCB616C3h
_NvAPI_GPU_GetPerfClocks    1EA54A3Bh
_NvAPI_GPU_SetPerfClocks    7BCF4ACh
_NvAPI_GPU_GetCoolerSettings    0DA141340h
_NvAPI_GPU_SetCoolerLevels  891FA0AEh
_NvAPI_GPU_RestoreCoolerSettings    8F6ED0FBh
_NvAPI_GPU_GetCoolerPolicyTable 518A32Ch
_NvAPI_GPU_SetCoolerPolicyTable 987947CDh
_NvAPI_GPU_RestoreCoolerPolicyTable 0D8C4FE63h
_NvAPI_GPU_GetPstatesInfo   0BA94C56Eh
_NvAPI_GPU_GetPstatesInfoEx 843C0256h
_NvAPI_GPU_SetPstatesInfo   0CDF27911h
_NvAPI_GPU_GetPstates20 6FF81213h
_NvAPI_GPU_SetPstates20 0F4DAE6Bh
_NvAPI_GPU_GetCurrentPstate 927DA4F6h
_NvAPI_GPU_GetPstateClientLimits    88C82104h
_NvAPI_GPU_SetPstateClientLimits    0FDFC7D49h
_NvAPI_GPU_EnableOverclockedPstates 0B23B70EEh
_NvAPI_GPU_EnableDynamicPstates 0FA579A0Fh
_NvAPI_GPU_GetDynamicPstatesInfoEx  60DED2EDh
_NvAPI_GPU_GetVoltages  7D656244h
_NvAPI_GPU_GetThermalSettings   0E3640A56h
_NvAPI_GPU_SetDitherControl 0DF0DFCDDh
_NvAPI_GPU_GetDitherControl 932AC8FBh
_NvAPI_GPU_GetColorSpaceConversion  8159E87Ah
_NvAPI_GPU_SetColorSpaceConversion  0FCABD23Ah
_NvAPI_GetTVOutputInfo  30C805D5h
_NvAPI_GetTVEncoderControls 5757474Ah
_NvAPI_SetTVEncoderControls 0CA36A3ABh
_NvAPI_GetTVOutputBorderColor   6DFD1C8Ch
_NvAPI_SetTVOutputBorderColor   0AED02700h
_NvAPI_GetDisplayPosition   6BB1EE5Dh
_NvAPI_SetDisplayPosition   57D9060Fh
_NvAPI_GetValidGpuTopologies    5DFAB48Ah
_NvAPI_GetInvalidGpuTopologies  15658BE6h
_NvAPI_SetGpuTopologies 25201F3Dh
_NvAPI_GPU_GetPerGpuTopologyStatus  0A81F8992h
_NvAPI_SYS_GetChipSetTopologyStatus 8A50F126h
_NvAPI_GPU_Get_DisplayPort_DongleInfo   76A70E8Dh
_NvAPI_I2CRead  2FDE12C5h
_NvAPI_I2CWrite 0E812EB07h
_NvAPI_I2CWriteEx   283AC65Ah
_NvAPI_I2CReadEx    4D7B0709h
_NvAPI_GPU_GetPowerMizerInfo    76BFA16Bh
_NvAPI_GPU_SetPowerMizerInfo    50016C78h
_NvAPI_GPU_GetVoltageDomainsStatus  0C16C7E2Ch
_NvAPI_GPU_ClientPowerTopologyGetInfo   0A4DFD3F2h
_NvAPI_GPU_ClientPowerTopologyGetStatus 0EDCF624Eh
_NvAPI_GPU_ClientPowerPoliciesGetInfo   34206D86h
_NvAPI_GPU_ClientPowerPoliciesGetStatus 70916171h
_NvAPI_GPU_ClientPowerPoliciesSetStatus 0AD95F5EDh
_NvAPI_GPU_WorkstationFeatureSetup  6C1F3FE4h
_NvAPI_SYS_GetChipSetInfo   53DABBCAh
_NvAPI_SYS_GetLidAndDockInfo    0CDA14D8Ah
_NvAPI_OGL_ExpertModeSet    3805EF7Ah
_NvAPI_OGL_ExpertModeGet    22ED9516h
_NvAPI_OGL_ExpertModeDefaultsSet    0B47A657Eh
_NvAPI_OGL_ExpertModeDefaultsGet    0AE921F12h
_NvAPI_SetDisplaySettings   0E04F3D86h
_NvAPI_GetDisplaySettings   0DC27D5D4h
_NvAPI_GetTiming    0AFC4833Eh
_NvAPI_DISP_GetMonitorCapabilities  3B05C7E1h
_NvAPI_EnumCustomDisplay    42892957h
_NvAPI_TryCustomDisplay 0BF6C1762h
_NvAPI_RevertCustomDisplayTrial 854BA405h
_NvAPI_DeleteCustomDisplay  0E7CB998Dh
_NvAPI_SaveCustomDisplay    0A9062C78h
_NvAPI_QueryUnderscanCap    61D7B624h
_NvAPI_EnumUnderscanConfig  4144111Ah
_NvAPI_DeleteUnderscanConfig    0F98854C8h
_NvAPI_SetUnderscanConfig   3EFADA1Dh
_NvAPI_GetDisplayFeatureConfig  8E985CCDh
_NvAPI_SetDisplayFeatureConfig  0F36A668Dh
_NvAPI_GetDisplayFeatureConfigDefaults  0F5F4D01h
_NvAPI_SetView  957D7B6h
_NvAPI_GetView  0D6B99D89h
_NvAPI_SetViewEx    6B89E68h
_NvAPI_GetViewEx    0DBBC0AF4h
_NvAPI_GetSupportedViews    66FB7FC0h
_NvAPI_GetHDCPLinkParameters    0B3BB0772h
_NvAPI_Disp_DpAuxChannelControl 8EB56969h
_NvAPI_SetHybridMode    0FB22D656h
_NvAPI_GetHybridMode    0E23B68C1h
_NvAPI_Coproc_GetCoprocStatus   1EFC3957h
_NvAPI_Coproc_SetCoprocInfoFlagsEx  0F4C863ACh
_NvAPI_Coproc_GetCoprocInfoFlagsEx  69A9874Dh
_NvAPI_Coproc_NotifyCoprocPowerState    0CADCB956h
_NvAPI_Coproc_GetApplicationCoprocInfo  79232685h
_NvAPI_GetVideoState    1C5659CDh
_NvAPI_SetVideoState    54FE75Ah
_NvAPI_SetFrameRateNotify   18919887h
_NvAPI_SetPVExtName 4FEEB498h
_NvAPI_GetPVExtName 2F5B08E0h
_NvAPI_SetPVExtProfile  8354A8F4h
_NvAPI_GetPVExtProfile  1B1B9A16h
_NvAPI_VideoSetStereoInfo   97063269h
_NvAPI_VideoGetStereoInfo   8E1F8CFEh
_NvAPI_Mosaic_GetSupportedTopoInfo  0FDB63C81h
_NvAPI_Mosaic_GetTopoGroup  0CB89381Dh
_NvAPI_Mosaic_GetOverlapLimits  989685F0h
_NvAPI_Mosaic_SetCurrentTopo    9B542831h
_NvAPI_Mosaic_GetCurrentTopo    0EC32944Eh
_NvAPI_Mosaic_EnableCurrentTopo 5F1AA66Ch
_NvAPI_Mosaic_SetGridTopology   3F113C77h
_NvAPI_Mosaic_GetMosaicCapabilities 0DA97071Eh
_NvAPI_Mosaic_GetDisplayCapabilities    0D58026B9h
_NvAPI_Mosaic_EnumGridTopologies    0A3C55220h
_NvAPI_Mosaic_GetDisplayViewportsByResolution   0DC6DC8D3h
_NvAPI_Mosaic_GetMosaicViewports    7EBA036h
_NvAPI_Mosaic_SetDisplayGrids   4D959A89h
_NvAPI_Mosaic_ValidateDisplayGridsWithSLI   1ECFD263h
_NvAPI_Mosaic_ValidateDisplayGrids  0CF43903Dh
_NvAPI_Mosaic_EnumDisplayModes  78DB97D7h
_NvAPI_Mosaic_ChooseGpuTopologies   0B033B140h
_NvAPI_Mosaic_EnumDisplayGrids  0DF2887AFh
_NvAPI_GetSupportedMosaicTopologies 410B5C25h
_NvAPI_GetCurrentMosaicTopology 0F60852BDh
_NvAPI_SetCurrentMosaicTopology 0D54B8989h
_NvAPI_EnableCurrentMosaicTopology  74073CC9h
_NvAPI_QueryNonMigratableApps   0BB9EF1C3h
_NvAPI_GPU_QueryActiveApps  65B1C5F5h
_NvAPI_Hybrid_QueryUnblockedNonMigratableApps   5F35BCB5h
_NvAPI_Hybrid_QueryBlockedMigratableApps    0F4C2F8CCh
_NvAPI_Hybrid_SetAppMigrationState  0FA0B9A59h
_NvAPI_Hybrid_IsAppMigrationStateChangeable 584CB0B6h
_NvAPI_GPU_GPIOQueryLegalPins   0FAB69565h
_NvAPI_GPU_GPIOReadFromPin  0F5E10439h
_NvAPI_GPU_GPIOWriteToPin   0F3B11E68h
_NvAPI_GPU_GetHDCPSupportStatus 0F089EEF5h
_NvAPI_SetTopologyFocusDisplayAndView   0A8064F9h
_NvAPI_Stereo_CreateConfigurationProfileRegistryKey 0BE7692ECh
_NvAPI_Stereo_DeleteConfigurationProfileRegistryKey 0F117B834h
_NvAPI_Stereo_SetConfigurationProfileValue  24409F48h
_NvAPI_Stereo_DeleteConfigurationProfileValue   49BCEECFh
_NvAPI_Stereo_Enable    239C4545h
_NvAPI_Stereo_Disable   2EC50C2Bh
_NvAPI_Stereo_IsEnabled 348FF8E1h
_NvAPI_Stereo_GetStereoCaps 0DFC063B7h
_NvAPI_Stereo_GetStereoSupport  296C434Dh
_NvAPI_Stereo_CreateHandleFromIUnknown  0AC7E37F4h
_NvAPI_Stereo_DestroyHandle 3A153134h
_NvAPI_Stereo_Activate  0F6A1AD68h
_NvAPI_Stereo_Deactivate    2D68DE96h
_NvAPI_Stereo_IsActivated   1FB0BC30h
_NvAPI_Stereo_GetSeparation 451F2134h
_NvAPI_Stereo_SetSeparation 5C069FA3h
_NvAPI_Stereo_DecreaseSeparation    0DA044458h
_NvAPI_Stereo_IncreaseSeparation    0C9A8ECECh
_NvAPI_Stereo_GetConvergence    4AB00934h
_NvAPI_Stereo_SetConvergence    3DD6B54Bh
_NvAPI_Stereo_DecreaseConvergence   4C87E317h
_NvAPI_Stereo_IncreaseConvergence   0A17DAABEh
_NvAPI_Stereo_GetFrustumAdjustMode  0E6839B43h
_NvAPI_Stereo_SetFrustumAdjustMode  7BE27FA2h
_NvAPI_Stereo_CaptureJpegImage  932CB140h
_NvAPI_Stereo_CapturePngImage   8B7E99B5h
_NvAPI_Stereo_ReverseStereoBlitControl  3CD58F89h
_NvAPI_Stereo_SetNotificationMessage    6B9B409Eh
_NvAPI_Stereo_SetActiveEye  96EEA9F8h
_NvAPI_Stereo_SetDriverMode 5E8F0BECh
_NvAPI_Stereo_GetEyeSeparation  0CE653127h
_NvAPI_Stereo_IsWindowedModeSupported   40C8ED5Eh
_NvAPI_Stereo_AppHandShake  8C610BDAh
_NvAPI_Stereo_HandShake_Trigger_Activation  0B30CD1A7h
_NvAPI_Stereo_HandShake_Message_Control 315E0EF0h
_NvAPI_Stereo_SetSurfaceCreationMode    0F5DCFCBAh
_NvAPI_Stereo_GetSurfaceCreationMode    36F1C736h
_NvAPI_Stereo_Debug_WasLastDrawStereoized   0ED4416C5h
_NvAPI_Stereo_ForceToScreenDepth    2D495758h
_NvAPI_Stereo_SetVertexShaderConstantF  416C07B3h
_NvAPI_Stereo_SetVertexShaderConstantB  5268716Fh
_NvAPI_Stereo_SetVertexShaderConstantI  7923BA0Eh
_NvAPI_Stereo_GetVertexShaderConstantF  622FDC87h
_NvAPI_Stereo_GetVertexShaderConstantB  712BAA5Bh
_NvAPI_Stereo_GetVertexShaderConstantI  5A60613Ah
_NvAPI_Stereo_SetPixelShaderConstantF   0A9657F32h
_NvAPI_Stereo_SetPixelShaderConstantB   0BA6109EEh
_NvAPI_Stereo_SetPixelShaderConstantI   912AC28Fh
_NvAPI_Stereo_GetPixelShaderConstantF   0D4974572h
_NvAPI_Stereo_GetPixelShaderConstantB   0C79333AEh
_NvAPI_Stereo_GetPixelShaderConstantI   0ECD8F8CFh
_NvAPI_Stereo_SetDefaultProfile 44F0ECD1h
_NvAPI_Stereo_GetDefaultProfile 624E21C2h
_NvAPI_Stereo_Is3DCursorSupported   0D7C9EC09h
_NvAPI_Stereo_GetCursorSeparation   72162B35h
_NvAPI_Stereo_SetCursorSeparation   0FBC08FC1h
_NvAPI_VIO_GetCapabilities  1DC91303h
_NvAPI_VIO_Open 44EE4841h
_NvAPI_VIO_Close    0D01BD237h
_NvAPI_VIO_Status   0E6CE4F1h
_NvAPI_VIO_SyncFormatDetect 118D48A3h
_NvAPI_VIO_GetConfig    0D34A789Bh
_NvAPI_VIO_SetConfig    0E4EEC07h
_NvAPI_VIO_SetCSC   0A1EC8D74h
_NvAPI_VIO_GetCSC   7B0D72A3h
_NvAPI_VIO_SetGamma 964BF452h
_NvAPI_VIO_GetGamma 51D53D06h
_NvAPI_VIO_SetSyncDelay 2697A8D1h
_NvAPI_VIO_GetSyncDelay 462214A9h
_NvAPI_VIO_GetPCIInfo   0B981D935h
_NvAPI_VIO_IsRunning    96BD040Eh
_NvAPI_VIO_Start    0CDE8E1A3h
_NvAPI_VIO_Stop 6BA2A5D6h
_NvAPI_VIO_IsFrameLockModeCompatible    7BF0A94Dh
_NvAPI_VIO_EnumDevices  0FD7C5557h
_NvAPI_VIO_QueryTopology    869534E2h
_NvAPI_VIO_EnumSignalFormats    0EAD72FE4h
_NvAPI_VIO_EnumDataFormats  221FA8E8h
_NvAPI_GPU_GetTachReading   5F608315h
_NvAPI_3D_GetProperty   8061A4B1h
_NvAPI_3D_SetProperty   0C9175E8Dh
_NvAPI_3D_GetPropertyRange  0B85DE27Ch
_NvAPI_GPS_GetPowerSteeringStatus   540EE82Eh
_NvAPI_GPS_SetPowerSteeringStatus   9723D3A2h
_NvAPI_GPS_SetVPStateCap    68888EB4h
_NvAPI_GPS_GetVPStateCap    71913023h
_NvAPI_GPS_GetThermalLimit  583113EDh
_NvAPI_GPS_SetThermalLimit  0C07E210Fh
_NvAPI_GPS_GetPerfSensors   271C1109h
_NvAPI_SYS_GetDisplayIdFromGpuAndOutputId   8F2BAB4h
_NvAPI_SYS_GetGpuAndOutputIdFromDisplayId   112BA1A5h
_NvAPI_DISP_GetDisplayIdByDisplayName   0AE457190h
_NvAPI_DISP_GetGDIPrimaryDisplayId  1E9D8A31h
_NvAPI_DISP_GetDisplayConfig    11ABCCF8h
_NvAPI_DISP_SetDisplayConfig    5D8CF8DEh
_NvAPI_GPU_GetPixelClockRange   66AF10B7h
_NvAPI_GPU_SetPixelClockRange   5AC7F8E5h
_NvAPI_GPU_GetECCStatusInfo 0CA1DDAF3h
_NvAPI_GPU_GetECCErrorInfo  0C71F85A6h
_NvAPI_GPU_ResetECCErrorInfo    0C02EEC20h
_NvAPI_GPU_GetECCConfigurationInfo  77A796F3h
_NvAPI_GPU_SetECCConfiguration  1CF639D9h
_NvAPI_D3D1x_CreateSwapChain    1BC21B66h
_NvAPI_D3D9_CreateSwapChain 1A131E09h
_NvAPI_D3D_SetFPSIndicatorState 0A776E8DBh
_NvAPI_D3D9_Present 5650BEBh
_NvAPI_D3D9_QueryFrameCount 9083E53Ah
_NvAPI_D3D9_ResetFrameCount 0FA6A0675h
_NvAPI_D3D9_QueryMaxSwapGroup   5995410Dh
_NvAPI_D3D9_QuerySwapGroup  0EBA4D232h
_NvAPI_D3D9_JoinSwapGroup   7D44BB54h
_NvAPI_D3D9_BindSwapBarrier 9C39C246h
_NvAPI_D3D1x_Present    3B845A1h
_NvAPI_D3D1x_QueryFrameCount    9152E055h
_NvAPI_D3D1x_ResetFrameCount    0FBBB031Ah
_NvAPI_D3D1x_QueryMaxSwapGroup  9BB9D68Fh
_NvAPI_D3D1x_QuerySwapGroup 407F67AAh
_NvAPI_D3D1x_JoinSwapGroup  14610CD7h
_NvAPI_D3D1x_BindSwapBarrier    9DE8C729h
_NvAPI_SYS_VenturaGetState  0CB7C208Dh
_NvAPI_SYS_VenturaSetState  0CE2E9D9h
_NvAPI_SYS_VenturaGetCoolingBudget  0C9D86E33h
_NvAPI_SYS_VenturaSetCoolingBudget  85FF5A15h
_NvAPI_SYS_VenturaGetPowerReading   63685979h
_NvAPI_DISP_GetDisplayBlankingState 63E5D8DBh
_NvAPI_DISP_SetDisplayBlankingState 1E17E29Bh
_NvAPI_DRS_CreateSession    694D52Eh
_NvAPI_DRS_DestroySession   0DAD9CFF8h
_NvAPI_DRS_LoadSettings 375DBD6Bh
_NvAPI_DRS_SaveSettings 0FCBC7E14h
_NvAPI_DRS_LoadSettingsFromFile 0D3EDE889h
_NvAPI_DRS_SaveSettingsToFile   2BE25DF8h
_NvAPI_DRS_CreateProfile    0CC176068h
_NvAPI_DRS_DeleteProfile    17093206h
_NvAPI_DRS_SetCurrentGlobalProfile  1C89C5DFh
_NvAPI_DRS_GetCurrentGlobalProfile  617BFF9Fh
_NvAPI_DRS_GetProfileInfo   61CD6FD6h
_NvAPI_DRS_SetProfileInfo   16ABD3A9h
_NvAPI_DRS_FindProfileByName    7E4A9A0Bh
_NvAPI_DRS_EnumProfiles 0BC371EE0h
_NvAPI_DRS_GetNumProfiles   1DAE4FBCh
_NvAPI_DRS_CreateApplication    4347A9DEh
_NvAPI_DRS_DeleteApplicationEx  0C5EA85A1h
_NvAPI_DRS_DeleteApplication    2C694BC6h
_NvAPI_DRS_GetApplicationInfo   0ED1F8C69h
_NvAPI_DRS_EnumApplications 7FA2173Ah
_NvAPI_DRS_FindApplicationByName    0EEE566B2h
_NvAPI_DRS_SetSetting   577DD202h
_NvAPI_DRS_GetSetting   73BF8338h
_NvAPI_DRS_EnumSettings 0AE3039DAh
_NvAPI_DRS_EnumAvailableSettingIds  0F020614Ah
_NvAPI_DRS_EnumAvailableSettingValues   2EC39F90h
_NvAPI_DRS_GetSettingIdFromName 0CB7309CDh
_NvAPI_DRS_GetSettingNameFromId 0D61CBE6Eh
_NvAPI_DRS_DeleteProfileSetting 0E4A26362h
_NvAPI_DRS_RestoreAllDefaults   5927B094h
_NvAPI_DRS_RestoreProfileDefault    0FA5F6134h
_NvAPI_DRS_RestoreProfileDefaultSetting 53F0381Eh
_NvAPI_DRS_GetBaseProfile   0DA8466A0h
_NvAPI_Event_RegisterCallback   0E6DBEA69h
_NvAPI_Event_UnregisterCallback 0DE1F9B45h
_NvAPI_GPU_GetCurrentThermalLevel   0D2488B79h
_NvAPI_GPU_GetCurrentFanSpeedLevel  0BD71F0C9h
_NvAPI_GPU_SetScanoutNvAPI_Statusensity  0A57457A4h
_NvAPI_GPU_SetScanoutWarping    0B34BAB4Fh
_NvAPI_GPU_GetScanoutConfiguration  6A9F5B63h
_NvAPI_DISP_SetHCloneTopology   61041C24h
_NvAPI_DISP_GetHCloneTopology   47BAD137h
_NvAPI_DISP_ValidateHCloneTopology  5F4C2664h
_NvAPI_GPU_GetPerfDecreaseInfo  7F7F4600h
_NvAPI_GPU_QueryIlluminationSupport 0A629DA31h
_NvAPI_GPU_GetIllumination  9A1B9365h
_NvAPI_GPU_SetIllumination  254A187h
_NvAPI_D3D1x_IFR_SetUpTargetBufferToSys 473F7828h
_NvAPI_D3D1x_IFR_TransferRenderTarget   9FBAE4EBh

*/
