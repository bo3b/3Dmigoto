#include "D3D11Wrapper.h"

#include <Shlobj.h>
#include <Winuser.h>
#include <map>
#include <vector>
#include <set>
#include <iterator>
#include <string>

#include "util.h"
#include "log.h"
#include "DecompileHLSL.h"
#include "Override.h"
#include "Globals.h"
#include "HackerDevice.h"
#include "HackerContext.h"
#include "IniHandler.h"

#include "nvprofile.h"

// The Log file and the Globals are both used globally, and these are the actual
// definitions of the variables.  All other uses will be via the extern in the 
// globals.h and log.h files.

// Globals used to be allocated on the heap, which is pointless given that it
// is global, and fragile given that we now have a second entry point for the
// profile helper that does not use the same init paths as the regular dll.
// Statically allocate it as StaticG and point the old G pointer to it to avoid
// needing to change every reference.
Globals StaticG;
Globals *G = &StaticG;

FILE *LogFile = 0;		// off by default.
bool gLogDebug = false;


// During the initialize, we will also Log every setting that is enabled, so that the log
// has a complete list of active settings.  This should make it more accurate and clear.

bool InitializeDLL()
{
	if (G->gInitialized)
		return true;

	LoadConfigFile();


	// Preload OUR nvapi before we call init because we need some of our calls.
#if(_WIN64)
#define NVAPI_DLL L"nvapi64.dll"
#else 
#define NVAPI_DLL L"nvapi.dll"
#endif

	LoadLibrary(NVAPI_DLL);

	NvAPI_ShortString errorMessage;
	NvAPI_Status status;

	// Tell our nvapi.dll that it's us calling, and it's OK.
	NvAPIOverride();
	status = NvAPI_Initialize();
	if (status != NVAPI_OK)
	{
		NvAPI_GetErrorMessage(status, errorMessage);
		LogInfo("  NvAPI_Initialize failed: %s\n", errorMessage);
		return false;
	}

	log_nv_driver_version();
	log_check_and_update_nv_profiles();

	// This sequence is to make the force_no_nvapi work.  When the game pCars
	// starts it calls NvAPI_Initialize that we want to return an error for.
	// But, the NV stereo driver ALSO calls NvAPI_Initialize, and we need to let
	// that one go through.  So by calling Stereo_Enable early here, we force
	// the NV stereo to load and take advantage of the pending NvAPIOverride,
	// then all subsequent game calls to Initialize will return an error.
	if (G->gForceNoNvAPI)
	{
		NvAPIOverride();
		status = NvAPI_Stereo_Enable();
		if (status != NVAPI_OK)
		{
			NvAPI_GetErrorMessage(status, errorMessage);
			LogInfo("  NvAPI_Stereo_Enable failed: %s\n", errorMessage);
			return false;
		}
	}
	//status = CheckStereo();
	//if (status != NVAPI_OK)
	//{
	//	NvAPI_GetErrorMessage(status, errorMessage);
	//	LogInfo("  *** stereo is disabled: %s  ***\n", errorMessage);
	//	return false;
	//}

	// If we are going to use 3D Vision Direct Mode, we need to set the driver 
	// into that mode early, before any possible CreateDevice occurs.
	if (G->gForceStereo == 2)
	{
		status = NvAPI_Stereo_SetDriverMode(NVAPI_STEREO_DRIVER_MODE_DIRECT);
		if (status != NVAPI_OK)
		{
			NvAPI_GetErrorMessage(status, errorMessage);
			LogInfo("*** NvAPI_Stereo_SetDriverMode to Direct, failed: %s\n", errorMessage);
			return false;
		}
		LogInfo("  NvAPI_Stereo_SetDriverMode successfully set to Direct Mode\n");
	}

	LogInfo("\n***  D3D11 DLL successfully initialized.  ***\n\n");
	return true;
}

void DestroyDLL()
{
	if (LogFile)
	{
		LogInfo("Destroying DLL...\n");
		fclose(LogFile);
	}
}

// D3DCompiler bridge
struct D3D11BridgeData
{
	UINT64 BinaryHash;
	char *HLSLFileName;
};

int WINAPI D3DKMTCloseAdapter()
{
	LogDebug("D3DKMTCloseAdapter called.\n");

	return 0;
}
int WINAPI D3DKMTDestroyAllocation()
{
	LogDebug("D3DKMTDestroyAllocation called.\n");

	return 0;
}
int WINAPI D3DKMTDestroyContext()
{
	LogDebug("D3DKMTDestroyContext called.\n");

	return 0;
}
int WINAPI D3DKMTDestroyDevice()
{
	LogDebug("D3DKMTDestroyDevice called.\n");

	return 0;
}
int WINAPI D3DKMTDestroySynchronizationObject()
{
	LogDebug("D3DKMTDestroySynchronizationObject called.\n");

	return 0;
}
int WINAPI D3DKMTSetDisplayPrivateDriverFormat()
{
	LogDebug("D3DKMTSetDisplayPrivateDriverFormat called.\n");

	return 0;
}
int WINAPI D3DKMTSignalSynchronizationObject()
{
	LogDebug("D3DKMTSignalSynchronizationObject called.\n");

	return 0;
}
int WINAPI D3DKMTUnlock()
{
	LogDebug("D3DKMTUnlock called.\n");

	return 0;
}
int WINAPI D3DKMTWaitForSynchronizationObject()
{
	LogDebug("D3DKMTWaitForSynchronizationObject called.\n");

	return 0;
}
int WINAPI D3DKMTCreateAllocation()
{
	LogDebug("D3DKMTCreateAllocation called.\n");

	return 0;
}
int WINAPI D3DKMTCreateContext()
{
	LogDebug("D3DKMTCreateContext called.\n");

	return 0;
}
int WINAPI D3DKMTCreateDevice()
{
	LogDebug("D3DKMTCreateDevice called.\n");

	return 0;
}
int WINAPI D3DKMTCreateSynchronizationObject()
{
	LogDebug("D3DKMTCreateSynchronizationObject called.\n");

	return 0;
}
int WINAPI D3DKMTEscape()
{
	LogDebug("D3DKMTEscape called.\n");

	return 0;
}
int WINAPI D3DKMTGetContextSchedulingPriority()
{
	LogDebug("D3DKMTGetContextSchedulingPriority called.\n");

	return 0;
}
int WINAPI D3DKMTGetDisplayModeList()
{
	LogDebug("D3DKMTGetDisplayModeList called.\n");

	return 0;
}
int WINAPI D3DKMTGetMultisampleMethodList()
{
	LogDebug("D3DKMTGetMultisampleMethodList called.\n");

	return 0;
}
int WINAPI D3DKMTGetRuntimeData()
{
	LogDebug("D3DKMTGetRuntimeData called.\n");

	return 0;
}
int WINAPI D3DKMTGetSharedPrimaryHandle()
{
	LogDebug("D3DKMTGetRuntimeData called.\n");

	return 0;
}
int WINAPI D3DKMTLock()
{
	LogDebug("D3DKMTLock called.\n");

	return 0;
}
int WINAPI D3DKMTPresent()
{
	LogDebug("D3DKMTPresent called.\n");

	return 0;
}
int WINAPI D3DKMTQueryAllocationResidency()
{
	LogDebug("D3DKMTQueryAllocationResidency called.\n");

	return 0;
}
int WINAPI D3DKMTRender()
{
	LogDebug("D3DKMTRender called.\n");

	return 0;
}
int WINAPI D3DKMTSetAllocationPriority()
{
	LogDebug("D3DKMTSetAllocationPriority called.\n");

	return 0;
}
int WINAPI D3DKMTSetContextSchedulingPriority()
{
	LogDebug("D3DKMTSetContextSchedulingPriority called.\n");

	return 0;
}
int WINAPI D3DKMTSetDisplayMode()
{
	LogDebug("D3DKMTSetDisplayMode called.\n");

	return 0;
}
int WINAPI D3DKMTSetGammaRamp()
{
	LogDebug("D3DKMTSetGammaRamp called.\n");

	return 0;
}
int WINAPI D3DKMTSetVidPnSourceOwner()
{
	LogDebug("D3DKMTSetVidPnSourceOwner called.\n");

	return 0;
}

typedef ULONG 	D3DKMT_HANDLE;
typedef int		KMTQUERYADAPTERINFOTYPE;

typedef struct _D3DKMT_QUERYADAPTERINFO
{
	D3DKMT_HANDLE           hAdapter;
	KMTQUERYADAPTERINFOTYPE Type;
	VOID                    *pPrivateDriverData;
	UINT                    PrivateDriverDataSize;
} D3DKMT_QUERYADAPTERINFO;

typedef void *D3D10DDI_HRTADAPTER;
typedef void *D3D10DDI_HADAPTER;
typedef void D3DDDI_ADAPTERCALLBACKS;
typedef void D3D10DDI_ADAPTERFUNCS;
typedef void D3D10_2DDI_ADAPTERFUNCS;

typedef struct D3D10DDIARG_OPENADAPTER
{
	D3D10DDI_HRTADAPTER           hRTAdapter;
	D3D10DDI_HADAPTER             hAdapter;
	UINT                          Interface;
	UINT                          Version;
	const D3DDDI_ADAPTERCALLBACKS *pAdapterCallbacks;
	union {
		D3D10DDI_ADAPTERFUNCS   *pAdapterFuncs;
		D3D10_2DDI_ADAPTERFUNCS *pAdapterFuncs_2;
	};
} D3D10DDIARG_OPENADAPTER;

static HMODULE hD3D11 = 0;

typedef int (WINAPI *tD3DKMTQueryAdapterInfo)(_D3DKMT_QUERYADAPTERINFO *);
static tD3DKMTQueryAdapterInfo _D3DKMTQueryAdapterInfo;

typedef int (WINAPI *tOpenAdapter10)(D3D10DDIARG_OPENADAPTER *adapter);
static tOpenAdapter10 _OpenAdapter10;

typedef int (WINAPI *tOpenAdapter10_2)(D3D10DDIARG_OPENADAPTER *adapter);
static tOpenAdapter10_2 _OpenAdapter10_2;

typedef int (WINAPI *tD3D11CoreCreateDevice)(__int32, int, int, LPCSTR lpModuleName, int, int, int, int, int, int);
static tD3D11CoreCreateDevice _D3D11CoreCreateDevice;

typedef HRESULT(WINAPI *tD3D11CoreCreateLayeredDevice)(const void *unknown0, DWORD unknown1, const void *unknown2, REFIID riid, void **ppvObj);
static tD3D11CoreCreateLayeredDevice _D3D11CoreCreateLayeredDevice;

typedef SIZE_T(WINAPI *tD3D11CoreGetLayeredDeviceSize)(const void *unknown0, DWORD unknown1);
static tD3D11CoreGetLayeredDeviceSize _D3D11CoreGetLayeredDeviceSize;

typedef HRESULT(WINAPI *tD3D11CoreRegisterLayers)(const void *unknown0, DWORD unknown1);
static tD3D11CoreRegisterLayers _D3D11CoreRegisterLayers;

typedef int (WINAPI *tD3DKMTGetDeviceState)(int a);
static tD3DKMTGetDeviceState _D3DKMTGetDeviceState;

typedef int (WINAPI *tD3DKMTOpenAdapterFromHdc)(int a);
static tD3DKMTOpenAdapterFromHdc _D3DKMTOpenAdapterFromHdc;

typedef int (WINAPI *tD3DKMTOpenResource)(int a);
static tD3DKMTOpenResource _D3DKMTOpenResource;

typedef int (WINAPI *tD3DKMTQueryResourceInfo)(int a);
static tD3DKMTQueryResourceInfo _D3DKMTQueryResourceInfo;

typedef HRESULT(WINAPI *tD3D11CreateDevice)(
	IDXGIAdapter *pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D_FEATURE_LEVEL *pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	ID3D11Device **ppDevice,
	D3D_FEATURE_LEVEL *pFeatureLevel,
	ID3D11DeviceContext **ppImmediateContext);
static tD3D11CreateDevice _D3D11CreateDevice;
typedef HRESULT(WINAPI *tD3D11CreateDeviceAndSwapChain)(
	IDXGIAdapter *pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D_FEATURE_LEVEL *pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
	IDXGISwapChain **ppSwapChain,
	ID3D11Device **ppDevice,
	D3D_FEATURE_LEVEL *pFeatureLevel,
	ID3D11DeviceContext **ppImmediateContext);
static tD3D11CreateDeviceAndSwapChain _D3D11CreateDeviceAndSwapChain;


void InitD311()
{
	UINT ret;

	if (hD3D11) return;

	InitializeCriticalSection(&G->mCriticalSection);

	InitializeDLL();
	

	// Chain through to the either the original DLL in the system, or to a proxy
	// DLL with the same interface, specified in the d3dx.ini file.

	if (G->CHAIN_DLL_PATH[0])
	{
		wchar_t sysDir[MAX_PATH] = {0};
		if (!GetModuleFileName(0, sysDir, MAX_PATH)) {
			LogInfo("GetModuleFileName failed\n");
			DoubleBeepExit();
		}
		wcsrchr(sysDir, L'\\')[1] = 0;
		wcscat(sysDir, G->CHAIN_DLL_PATH);
		if (LogFile)
		{
			char path[MAX_PATH];
			wcstombs(path, sysDir, MAX_PATH);
			LogInfo("trying to chain load %s\n", path);
		}
		hD3D11 = LoadLibrary(sysDir);
		if (!hD3D11)
		{
			if (LogFile)
			{
				char path[MAX_PATH];
				wcstombs(path, G->CHAIN_DLL_PATH, MAX_PATH);
				LogInfo("load failed. Trying to chain load %s\n", path);
			}
			hD3D11 = LoadLibrary(G->CHAIN_DLL_PATH);
		}
	}
	else
	{
		// We'll look for this in DLLMainHook to avoid callback to self.		
		// Must remain all lower case to be matched in DLLMainHook.
		// We need the system d3d11 in order to find the original proc addresses.
		// We hook LoadLibraryExW, so we need to use that here.
		LogInfo("Trying to load original_d3d11.dll\n");
		hD3D11 = LoadLibraryEx(L"original_d3d11.dll", NULL, 0);
		if (hD3D11 == NULL)
		{
			wchar_t libPath[MAX_PATH];
			LogInfo("*** LoadLibrary on original_d3d11.dll failed.\n");

			// Redirected load failed. Something (like Origin's IGO32.dll
			// hook in ntdll.dll LdrLoadDll) is interfering with our hook.
			// Fall back to using the full path after suppressing 3DMigoto's
			// redirect to make sure we don't get a reference to ourselves:

			LoadLibraryEx(L"SUPPRESS_3DMIGOTO_REDIRECT", NULL, 0);

			ret = GetSystemDirectoryW(libPath, ARRAYSIZE(libPath));
			if (ret != 0 && ret < ARRAYSIZE(libPath)) {
				wcscat_s(libPath, MAX_PATH, L"\\d3d11.dll");
				LogInfoW(L"Trying to load %ls\n", libPath);
				hD3D11 = LoadLibraryEx(libPath, NULL, 0);
			}
		}
	}
	if (hD3D11 == NULL)
	{
		LogInfo("*** LoadLibrary on original or chained d3d11.dll failed.\n");
		DoubleBeepExit();
	}

	_D3DKMTQueryAdapterInfo = (tD3DKMTQueryAdapterInfo)GetProcAddress(hD3D11, "D3DKMTQueryAdapterInfo");
	_OpenAdapter10 = (tOpenAdapter10)GetProcAddress(hD3D11, "OpenAdapter10");
	_OpenAdapter10_2 = (tOpenAdapter10_2)GetProcAddress(hD3D11, "OpenAdapter10_2");
	_D3D11CoreCreateDevice = (tD3D11CoreCreateDevice)GetProcAddress(hD3D11, "D3D11CoreCreateDevice");
	_D3D11CoreCreateLayeredDevice = (tD3D11CoreCreateLayeredDevice)GetProcAddress(hD3D11, "D3D11CoreCreateLayeredDevice");
	_D3D11CoreGetLayeredDeviceSize = (tD3D11CoreGetLayeredDeviceSize)GetProcAddress(hD3D11, "D3D11CoreGetLayeredDeviceSize");
	_D3D11CoreRegisterLayers = (tD3D11CoreRegisterLayers)GetProcAddress(hD3D11, "D3D11CoreRegisterLayers");
	_D3D11CreateDevice = (tD3D11CreateDevice)GetProcAddress(hD3D11, "D3D11CreateDevice");
	_D3D11CreateDeviceAndSwapChain = (tD3D11CreateDeviceAndSwapChain)GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain");
	_D3DKMTGetDeviceState = (tD3DKMTGetDeviceState)GetProcAddress(hD3D11, "D3DKMTGetDeviceState");
	_D3DKMTOpenAdapterFromHdc = (tD3DKMTOpenAdapterFromHdc)GetProcAddress(hD3D11, "D3DKMTOpenAdapterFromHdc");
	_D3DKMTOpenResource = (tD3DKMTOpenResource)GetProcAddress(hD3D11, "D3DKMTOpenResource");
	_D3DKMTQueryResourceInfo = (tD3DKMTQueryResourceInfo)GetProcAddress(hD3D11, "D3DKMTQueryResourceInfo");
}

int WINAPI D3DKMTQueryAdapterInfo(_D3DKMT_QUERYADAPTERINFO *info)
{
	InitD311();
	LogInfo("D3DKMTQueryAdapterInfo called.\n");

	return (*_D3DKMTQueryAdapterInfo)(info);
}

int WINAPI OpenAdapter10(struct D3D10DDIARG_OPENADAPTER *adapter)
{
	InitD311();
	LogInfo("OpenAdapter10 called.\n");

	return (*_OpenAdapter10)(adapter);
}

int WINAPI OpenAdapter10_2(struct D3D10DDIARG_OPENADAPTER *adapter)
{
	InitD311();
	LogInfo("OpenAdapter10_2 called.\n");

	return (*_OpenAdapter10_2)(adapter);
}

int WINAPI D3D11CoreCreateDevice(__int32 a, int b, int c, LPCSTR lpModuleName, int e, int f, int g, int h, int i, int j)
{
	InitD311();
	LogInfo("D3D11CoreCreateDevice called.\n");

	return (*_D3D11CoreCreateDevice)(a, b, c, lpModuleName, e, f, g, h, i, j);
}


HRESULT WINAPI D3D11CoreCreateLayeredDevice(const void *unknown0, DWORD unknown1, const void *unknown2, REFIID riid, void **ppvObj)
{
	InitD311();
	LogInfo("D3D11CoreCreateLayeredDevice called.\n");

	return (*_D3D11CoreCreateLayeredDevice)(unknown0, unknown1, unknown2, riid, ppvObj);
}

SIZE_T WINAPI D3D11CoreGetLayeredDeviceSize(const void *unknown0, DWORD unknown1)
{
	InitD311();
	// Call from D3DCompiler (magic number from there) ?
	if ((intptr_t)unknown0 == 0x77aa128b)
	{
		LogInfo("Shader code info from D3DCompiler_xx.dll wrapper received:\n");

		// FIXME: Is this an unsigned 32bit integer or a pointer?
		// It can't be both because they are not the same size on x64.
		// We might be corrupting a pointer by using the wrong function signature
		D3D11BridgeData *data = (D3D11BridgeData *)unknown1;
		LogInfo("  Bytecode hash = %016llx\n", data->BinaryHash);
		LogInfo("  Filename = %s\n", data->HLSLFileName);

		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			G->mCompiledShaderMap[data->BinaryHash] = data->HLSLFileName;
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
		return 0xaa77125b;
	}
	LogInfo("D3D11CoreGetLayeredDeviceSize called.\n");

	return (*_D3D11CoreGetLayeredDeviceSize)(unknown0, unknown1);
}

HRESULT WINAPI D3D11CoreRegisterLayers(const void *unknown0, DWORD unknown1)
{
	InitD311();
	LogInfo("D3D11CoreRegisterLayers called.\n");

	return (*_D3D11CoreRegisterLayers)(unknown0, unknown1);
}

int WINAPI D3DKMTGetDeviceState(int a)
{
	InitD311();
	LogInfo("D3DKMTGetDeviceState called.\n");

	return (*_D3DKMTGetDeviceState)(a);
}

int WINAPI D3DKMTOpenAdapterFromHdc(int a)
{
	InitD311();
	LogInfo("D3DKMTOpenAdapterFromHdc called.\n");

	return (*_D3DKMTOpenAdapterFromHdc)(a);
}

int WINAPI D3DKMTOpenResource(int a)
{
	InitD311();
	LogInfo("D3DKMTOpenResource called.\n");

	return (*_D3DKMTOpenResource)(a);
}

int WINAPI D3DKMTQueryResourceInfo(int a)
{
	InitD311();
	LogInfo("D3DKMTQueryResourceInfo called.\n");

	return (*_D3DKMTQueryResourceInfo)(a);
}


// -----------------------------------------------------------------------------------------------

// Only where _DEBUG_LAYER=1, typically debug builds.  Set the flags to
// enable GPU debugging.  This makes the GPU dramatically slower but can
// catch bogus setup or bad calls to the GPU environment.
// The prevent threading optimizations can help show if we have a multi-threading problem.

UINT EnableDebugFlags(UINT flags)
{
	flags |= D3D11_CREATE_DEVICE_DEBUG;
	flags |= D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;
	LogInfo("  D3D11CreateDevice _DEBUG_LAYER flags set: %#x\n", flags);

	return flags;
}


// Only where _DEBUG_LAYER=1.  This enables the debug layer for the GPU
// itself, which will show GPU errors, warnings, and leaks in the console output.
// This call shows that the layer is active, and the initial LiveObjectState.
// And enable debugger breaks for errors that we might be introducing.

void ShowDebugInfo(ID3D11Device *origDevice)
{
	ID3D11Debug *d3dDebug = nullptr;
	if (origDevice != nullptr)
	{
		if (SUCCEEDED(origDevice->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug)))
		{
			ID3D11InfoQueue *d3dInfoQueue = nullptr;

			if (SUCCEEDED(d3dDebug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3dInfoQueue)))
			{
				d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
				d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
			}
		}
		d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL);
	}
}


// Any request for greater than 11.0 DX needs an E_INVALIDARG return, to match the
// documented behavior. We want to return an error for any higher level requests,
// at least for the time being, to avoid having to implement all the possible
// variants past 11.0, with nothing to show for it.  There's no performance or
// graphic advantage at present, and game devs need to support Win7, no evil-update
// until the market shrinks, or there is some compelling advance.
// Feature level D3D_FEATURE_LEVEL_11_1 requires Win8 (WDDM 1.2)
// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476875(v=vs.85).aspx
//
// Also, when we are called with down-level requests, we need to not wrap the objects
// and not generate iniParams or StereoTextures, because a bunch of features are 
// missing there, and can cause problems.  FarCry4 hangs on Win10 because they
// call with pFeatureLevels=9.2.  As seen in the link, CreateTexture1D does not
// exist in 9.x, so it's not legal to call that with a 9.x Device.  We will 
// assume that they don't plan to use that device for the game, and will look
// only for a DX11 device. No point in supporting DX10 here, too few games to matter.
// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476150(v=vs.85).aspx#ID3D11Device_CreateTexture1D
//
// So current approach is to just force an error for any request that is not 11.0.
// That seems pretty heavy handed, but testing this technique worked on 15 games,
// half x32, half x64.  And fixes a hang in FC4 on Win10, and makes Watch_Dogs work.
// We'd expect this to make games error out, but it's actually working very well
// during testing.  If this proves problematic later, it's probably worth keeping
// it as a .ini option to force this mode.
// (Can be an array. We are looking only at first element. Seems OK.)
//
// 7-21-16: Now adding the ability to disable this forcing function, because Marlow 
// Briggs and Narco Terror do not launch when we do this.  
// This will now make it a option in the d3dx.ini, default to force DX11, but can be
// disabled, or forced to always use DX11.
//
// pFeatureLevels can be modified here because we have changed the signature from const.
// If pFeatureLevels comes in null, that is OK, because the default behavior for
// CreateDevice is to create a DX11 Device.
// 
// Returns true if we need to error out with E_INVALIDARG, which is default in d3dx.ini.

bool ForceDX11(D3D_FEATURE_LEVEL *featureLevels)
{
	if (!featureLevels)
	{
		LogInfo("->Feature level null, defaults to D3D_FEATURE_LEVEL_11_0.\n");
		return false;
	}

	if (G->enable_create_device == 1)
	{
		LogInfo("->Feature level allowed through unchanged: %#x\n", *featureLevels);
		return false;
	}
	if (G->enable_create_device == 2)
	{
		*featureLevels = D3D_FEATURE_LEVEL_11_0;

		LogInfo("->Feature level forced to 11.0: %#x\n", *featureLevels);
		return false;
	}

	// Error out if we aren't looking for D3D_FEATURE_LEVEL_11_0.
	if (*featureLevels != D3D_FEATURE_LEVEL_11_0)
	{
		LogInfo("->Feature level != 11.0: %#x, returning E_INVALIDARG\n", *featureLevels);
		return true;
	}

	return false;
}


// For creating the device, we need to call the original D3D11CreateDevice in order to initialize
// Direct3D, and collect the original Device and original Context.  Both of those will be handed
// off to the wrapped HackerDevice and HackerContext objects, so they can call out to the originals
// as needed.  Both Hacker objects need access to both Context and Device, so since both are 
// created here, it's easy enough to provide them upon instantiation.
//
// Now intended to be fully null safe- as games seem to have a lot of variance.

HRESULT WINAPI D3D11CreateDevice(
	_In_opt_        IDXGIAdapter        *pAdapter,
					D3D_DRIVER_TYPE     DriverType,
					HMODULE             Software,
					UINT                Flags,
	_In_reads_opt_(FeatureLevels) const D3D_FEATURE_LEVEL   *pFeatureLevels,
					UINT                FeatureLevels,
					UINT                SDKVersion,
	_Out_opt_       ID3D11Device        **ppDevice,
	_Out_opt_       D3D_FEATURE_LEVEL   *pFeatureLevel,
	_Out_opt_       ID3D11DeviceContext **ppImmediateContext)
{
	InitD311();
	LogInfo("\n\n *** D3D11CreateDevice called with\n");
	LogInfo("    pAdapter = %p\n", pAdapter);
	LogInfo("    Flags = %#x\n", Flags);
	LogInfo("    pFeatureLevels = %#x\n", pFeatureLevels ? *pFeatureLevels : 0);
	LogInfo("    FeatureLevels = %d\n", FeatureLevels);
	LogInfo("    ppDevice = %p\n", ppDevice);
	LogInfo("    pFeatureLevel = %#x\n", pFeatureLevel ? *pFeatureLevel : 0);
	LogInfo("    ppImmediateContext = %p\n", ppImmediateContext);

	if (ForceDX11(const_cast<D3D_FEATURE_LEVEL*>(pFeatureLevels)))
		return E_INVALIDARG;

#if _DEBUG_LAYER
	Flags = EnableDebugFlags(Flags);
#endif

	HRESULT ret = (*_D3D11CreateDevice)(pAdapter, DriverType, Software, Flags, pFeatureLevels,
		FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);

	if (FAILED(ret))
	{
		LogInfo("->failed with HRESULT=%x\n", ret);
		return ret;
	}

	// Optional parameters means these might be null.
	ID3D11Device *origDevice = ppDevice ? *ppDevice : nullptr;
	ID3D11DeviceContext *origContext = ppImmediateContext ? *ppImmediateContext : nullptr;

	LogInfo("  D3D11CreateDevice returned device handle = %p, context handle = %p\n",
		origDevice, origContext);

#if _DEBUG_LAYER
	ShowDebugInfo(origDevice);
#endif

	// When platform update is desired, we want to create the HackerDevice1 and
	// HackerContext1 objects instead.  We'll store these and return them as
	// non1 objects so other code can just use the objects.
	ID3D11Device1 *origDevice1 = nullptr;
	ID3D11DeviceContext1 *origContext1 = nullptr;
	if (G->enable_platform_update)
	{
		origDevice->QueryInterface(IID_PPV_ARGS(&origDevice1));
		origContext->QueryInterface(IID_PPV_ARGS(&origContext1));
	}

	// Create a wrapped version of the original device to return to the game.
	HackerDevice *deviceWrap = nullptr;
	if (origDevice != nullptr)
	{
		if (G->enable_platform_update)
			deviceWrap = new HackerDevice1(origDevice1, origContext1);
		else
			deviceWrap = new HackerDevice(origDevice, origContext);

		if (G->enable_hooks & EnableHooks::DEVICE)
			deviceWrap->HookDevice();
		else
			*ppDevice = deviceWrap;
		LogInfo("  HackerDevice %p created to wrap %p\n", deviceWrap, origDevice);
	}

	// Create a wrapped version of the original context to return to the game.
	HackerContext *contextWrap = nullptr;
	if (origContext != nullptr)
	{
		if (G->enable_platform_update)
			contextWrap = new HackerContext1(origDevice1, origContext1);
		else
			contextWrap = new HackerContext(origDevice, origContext);

		if (G->enable_hooks & EnableHooks::IMMEDIATE_CONTEXT)
			contextWrap->HookContext();
		else
			*ppImmediateContext = contextWrap;
		LogInfo("  HackerContext %p created to wrap %p\n", contextWrap, origContext);
	}

	// Let each of the new Hacker objects know about the other, needed for unusual
	// calls in the Hacker objects where we want to return the Hacker versions.
	if (deviceWrap != nullptr)
		deviceWrap->SetHackerContext(contextWrap);
	if (contextWrap != nullptr)
		contextWrap->SetHackerDevice(deviceWrap);

	// With all the interacting objects set up, we can now safely finish the HackerDevice init.
	if (deviceWrap != nullptr)
		deviceWrap->Create3DMigotoResources();

	LogInfo("->D3D11CreateDevice result = %x, device handle = %p, device wrapper = %p, context handle = %p, context wrapper = %p\n\n",
		ret, origDevice, deviceWrap, origContext, contextWrap);

	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Additional strategy here, after learning from games.  Several games like DragonAge
// and Watch Dogs pass nullptr for some of these parameters, including the returned
// ppSwapChain.  Why you would call CreateDeviceAndSwapChain, then pass null is anyone's
// guess.  Because of that sort of silliness, these routines are now trying to be fully
// null safe, and never access anything without checking first.
// 
// See notes in CreateDevice.

HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
	_In_opt_			IDXGIAdapter         *pAdapter,
						D3D_DRIVER_TYPE      DriverType,
						HMODULE              Software,
						UINT                 Flags,
	_In_opt_ const		D3D_FEATURE_LEVEL    *pFeatureLevels,
						UINT                 FeatureLevels,
						UINT                 SDKVersion,
	_In_opt_			DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
	_Out_opt_			IDXGISwapChain		 **ppSwapChain,
	_Out_opt_			ID3D11Device         **ppDevice,
	_Out_opt_			D3D_FEATURE_LEVEL    *pFeatureLevel,
	_Out_opt_			ID3D11DeviceContext  **ppImmediateContext)
{
	HRESULT ret;
	DXGI_SWAP_CHAIN_DESC originalSwapChainDesc;

	InitD311();

	LogInfo("\n\n *** D3D11CreateDeviceAndSwapChain called with\n");
	LogInfo("    pAdapter = %p\n", pAdapter);
	LogInfo("    Flags = %#x\n", Flags);
	LogInfo("    pFeatureLevels = %#x\n", pFeatureLevels ?  *pFeatureLevels : 0);
	LogInfo("    FeatureLevels = %d\n", FeatureLevels);
	LogInfo("    pSwapChainDesc = %p\n", pSwapChainDesc);
	LogInfo("    ppSwapChain = %p\n", ppSwapChain);
	LogInfo("    ppDevice = %p\n", ppDevice);
	LogInfo("    pFeatureLevel = %#x\n", pFeatureLevel ? *pFeatureLevel: 0);
	LogInfo("    ppImmediateContext = %p\n", ppImmediateContext);

	if (ForceDX11(const_cast<D3D_FEATURE_LEVEL*>(pFeatureLevels)))
		return E_INVALIDARG;

#if _DEBUG_LAYER
	Flags = EnableDebugFlags(Flags);
#endif

	if (pSwapChainDesc != nullptr) {
		// Save off the window handle so we can translate mouse cursor
		// coordinates to the window:
		G->hWnd = pSwapChainDesc->OutputWindow;

		if (G->SCREEN_UPSCALING > 0)
		{		
			// Copy input swap chain desc for case the upscaling is on
			memcpy(&originalSwapChainDesc, pSwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));
		}

		// Require in case the software mouse and upscaling are on at the same time
		G->GAME_INTERNAL_WIDTH = pSwapChainDesc->BufferDesc.Width;
		G->GAME_INTERNAL_HEIGHT = pSwapChainDesc->BufferDesc.Height;

		if (G->mResolutionInfo.from == GetResolutionFrom::SWAP_CHAIN) {
			// TODO: Use a helper class to track *all* different resolutions
			G->mResolutionInfo.width = pSwapChainDesc->BufferDesc.Width;
			G->mResolutionInfo.height = pSwapChainDesc->BufferDesc.Height;
			LogInfo("Got resolution from swap chain: %ix%i\n",
				G->mResolutionInfo.width, G->mResolutionInfo.height);
		}
	}

	ForceDisplayParams(pSwapChainDesc);

	ret = (*_D3D11CreateDeviceAndSwapChain)(pAdapter, DriverType, Software, Flags, pFeatureLevels,
		FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);

	if (FAILED(ret))
	{
		LogInfo("  failed with HRESULT=%x\n", ret);
		return ret;
	}

	// Optional parameters means these might be null.
	ID3D11Device *origDevice = ppDevice ? *ppDevice : nullptr;
	ID3D11DeviceContext *origContext = ppImmediateContext ? *ppImmediateContext : nullptr;
	IDXGISwapChain *origSwapChain = ppSwapChain ? *ppSwapChain : nullptr;

	LogInfo("  D3D11CreateDeviceAndSwapChain returned device handle = %p, context handle = %p, swapchain handle = %p\n",
		origDevice, origContext, origSwapChain);

#if _DEBUG_LAYER
	ShowDebugInfo(origDevice);
#endif

	// When platform update is desired, we want to create the HackerDevice1 and
	// HackerContext1 objects instead.
	ID3D11Device1 *origDevice1 = nullptr;
	ID3D11DeviceContext1 *origContext1 = nullptr;
	if (G->enable_platform_update)
	{
		origDevice->QueryInterface(IID_PPV_ARGS(&origDevice1));
		origContext->QueryInterface(IID_PPV_ARGS(&origContext1));
	}

	HackerDevice *deviceWrap = nullptr;
	if (ppDevice != nullptr)
	{
		if (G->enable_platform_update)
			deviceWrap = new HackerDevice1(origDevice1, origContext1);
		else
			deviceWrap = new HackerDevice(origDevice, origContext);

		if (G->enable_hooks & EnableHooks::DEVICE)
			deviceWrap->HookDevice();
		else
			*ppDevice = deviceWrap;
		LogInfo("  HackerDevice %p created to wrap %p\n", deviceWrap, origDevice);
	}

	HackerContext *contextWrap = nullptr;
	if (ppImmediateContext != nullptr)
	{
		if (G->enable_platform_update)
			contextWrap = new HackerContext1(origDevice1, origContext1);
		else
			contextWrap = new HackerContext(origDevice, origContext);

		if (G->enable_hooks & EnableHooks::IMMEDIATE_CONTEXT)
			contextWrap->HookContext();
		else
			*ppImmediateContext = contextWrap;
		LogInfo("  HackerContext %p created to wrap %p\n", contextWrap, origContext);
	}

	HackerDXGISwapChain *swapchainWrap = nullptr;

	if (ppSwapChain != nullptr) {
		if (G->SCREEN_UPSCALING == 0)
		{
			swapchainWrap = new HackerDXGISwapChain(origSwapChain, deviceWrap, contextWrap);
			LogInfo("  HackerDXGISwapChain %p created to wrap %p\n", swapchainWrap, origSwapChain);
		}
		else
		{
			if (G->UPSCALE_MODE == 1)
			{
				//TODO: find a way to allow this!
				BeepFailure();
				LogInfo("The game uses D3D11CreateDeviceAndSwapChain to create the swap chain! For this function only upscale_mode = 0 is supported!\n");
				LogInfo("Trying to switch to this mode!\n");
				G->UPSCALE_MODE = 0;
			}

			try
			{
				swapchainWrap = new HackerUpscalingDXGISwapChain(origSwapChain, deviceWrap, contextWrap, &originalSwapChainDesc, G->SCREEN_WIDTH, G->SCREEN_HEIGHT,nullptr);
				LogInfo("  HackerUpscalingDXGISwapChain %p created to wrap %p.\n", swapchainWrap, origSwapChain);
			}
			catch (const Exception3DMigoto& e)
			{
				LogInfo("HackerDXGIFactory::CreateSwapChain(): Creation of Upscaling Swapchain failed. Error: %s\n", e.what().c_str());
				// Something went wrong inform the user with double beep and end!;
				DoubleBeepExit();
			}
		}

		if (swapchainWrap != nullptr)
			*ppSwapChain = reinterpret_cast<IDXGISwapChain*>(swapchainWrap);
	}

	// Let each of the new Hacker objects know about the other, needed for unusual
	// calls in the Hacker objects where we want to return the Hacker versions.
	if (deviceWrap != nullptr)
		deviceWrap->SetHackerContext(contextWrap);
	if (deviceWrap != nullptr) // Is it not already done in the hackerDXGISwapChain class?
		deviceWrap->SetHackerSwapChain(swapchainWrap);
	if (contextWrap != nullptr)
		contextWrap->SetHackerDevice(deviceWrap);

	// With all the interacting objects set up, we can now safely finish the HackerDevice init.
	if (deviceWrap != nullptr)
		deviceWrap->Create3DMigotoResources();

	LogInfo("->D3D11CreateDeviceAndSwapChain result = %x, device handle = %p, device wrapper = %p, context handle = %p, "
		"context wrapper = %p, swapchain handle = %p, swapchain wrapper = %p\n\n",
		ret, origDevice, deviceWrap, origContext, contextWrap, origSwapChain, swapchainWrap);

	return ret;
}

extern "C" NvAPI_Status __cdecl nvapi_QueryInterface(unsigned int offset);

void NvAPIOverride()
{
	// One shot, override custom settings.
	NvAPI_Status ret = nvapi_QueryInterface(0xb03bb03b);
	if (ret != 0xeecc34ab)
		LogInfo("  overriding NVAPI wrapper failed.\n");

	//const StereoHandle id1 = (StereoHandle)0x77aa8ebc;
	//float id2 = 1.23f;
	//if (NvAPI_Stereo_GetConvergence(id1, &id2) != 0xeecc34ab)
	//{
	//	LogDebug("  overriding NVAPI wrapper failed.\n");
	//}
}
