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
#include "HackerDXGI.h"

// The Log file and the Globals are both used globally, and these are the actual
// definitions of the variables.  All other uses will be via the extern in the 
// globals.h and log.h files.

Globals *G;

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
	if (hD3D11) return;

	G = new Globals();
	InitializeCriticalSection(&G->mCriticalSection);

	if (!InitializeDLL())
	{
		// Failed to init?  Best to exit now, we are sure to crash.
		DoubleBeepExit();
	}

	

	// Chain through to the either the original DLL in the system, or to a proxy
	// DLL with the same interface, specified in the d3dx.ini file.

	if (G->CHAIN_DLL_PATH[0])
	{
		wchar_t sysDir[MAX_PATH];
		GetModuleFileName(0, sysDir, MAX_PATH);
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
		LogInfo("Trying to load original_d3d11.dll \n");
		hD3D11 = LoadLibraryEx(L"original_d3d11.dll", NULL, 0);
	}
	if (hD3D11 == NULL)
	{
		LogInfo("*** LoadLibrary on original_d3d11.dll failed. \n");
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

		D3D11BridgeData *data = (D3D11BridgeData *)unknown1;
		LogInfo("  Bytecode hash = %08lx%08lx\n", (UINT32)(data->BinaryHash >> 32), (UINT32)data->BinaryHash);
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
	LogInfo("  D3D11CreateDevice _DEBUG_LAYER flags set: %#x \n", flags);

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
	_In_opt_  const D3D_FEATURE_LEVEL   *pFeatureLevels,
					UINT                FeatureLevels,
					UINT                SDKVersion,
	_Out_opt_       ID3D11Device        **ppDevice,
	_Out_opt_       D3D_FEATURE_LEVEL   *pFeatureLevel,
	_Out_opt_       ID3D11DeviceContext **ppImmediateContext)
{
	InitD311();
	LogInfo("\n\n *** D3D11CreateDevice called with  \n");
	LogInfo("    pAdapter = %p \n", pAdapter);
	LogInfo("    Flags = %#x \n", Flags);
	LogInfo("    pFeatureLevels = %#x \n", pFeatureLevels ? *pFeatureLevels : 0);
	LogInfo("    ppDevice = %p \n", ppDevice);
	LogInfo("    pFeatureLevel = %#x \n", pFeatureLevel ? *pFeatureLevel : 0);
	LogInfo("    ppImmediateContext = %p \n", ppImmediateContext);

#if _DEBUG_LAYER
	Flags = EnableDebugFlags(Flags);
#endif

	HRESULT ret = (*_D3D11CreateDevice)(pAdapter, DriverType, Software, Flags, pFeatureLevels,
		FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);

	if (FAILED(ret))
	{
		LogInfo("  failed with HRESULT=%x\n", ret);
		return ret;
	}

	// Optional parameters means these might be null.
	ID3D11Device *origDevice = ppDevice ? *ppDevice : nullptr;
	ID3D11DeviceContext *origContext = ppImmediateContext ? *ppImmediateContext : nullptr;

	LogInfo("->D3D11CreateDevice returned device handle = %p, context handle = %p \n",
		origDevice, origContext);

#if _DEBUG_LAYER
	ShowDebugInfo(origDevice);
#endif

	// Create a wrapped version of the original device to return to the game.
	HackerDevice *deviceWrap = nullptr;
	if (ppDevice != nullptr)
	{
		deviceWrap = new HackerDevice(origDevice, origContext);
		*ppDevice = deviceWrap;
		LogInfo("  HackerDevice %p created to wrap %p \n", deviceWrap, origDevice);
	}

	// Create a wrapped version of the original context to return to the game.
	HackerContext *contextWrap = nullptr;
	if (ppImmediateContext != nullptr)
	{
		contextWrap = new HackerContext(origDevice, origContext);
		*ppImmediateContext = contextWrap;
		LogInfo("  HackerContext %p created to wrap %p \n", contextWrap, origContext);
	}

	// Let each of the new Hacker objects know about the other, needed for unusual
	// calls in the Hacker objects where we want to return the Hacker versions.
	if (deviceWrap != nullptr)
		deviceWrap->SetHackerContext(contextWrap);
	if (contextWrap != nullptr)
		contextWrap->SetHackerDevice(deviceWrap);

	// With all the interacting objects set up, we can now safely finish the HackerDevice init.
	if (deviceWrap != nullptr)
		deviceWrap->CreateStereoAndIniTextures();

	LogInfo("->returns result = %x, device handle = %p, device wrapper = %p, context handle = %p, context wrapper = %p \n\n",
		ret, origDevice, deviceWrap, origContext, contextWrap);

	return ret;
}


// Additional strategy here, after learning from games.  Several games like DragonAge
// and Watch Dogs pass nullptr for some of these parameters, including the returned
// ppSwapChain.  Why you would call CreateDeviceAndSwapChain, then pass null is anyone's
// guess.  Because of that sort of silliness, these routines are now trying to be fully
// null safe, and never access anything without checking first.

HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
	_In_opt_        IDXGIAdapter         *pAdapter,
					D3D_DRIVER_TYPE      DriverType,
					HMODULE              Software,
					UINT                 Flags,
	_In_opt_  const D3D_FEATURE_LEVEL    *pFeatureLevels,
					UINT                 FeatureLevels,
					UINT                 SDKVersion,
	_In_opt_		DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
	_Out_opt_       IDXGISwapChain		 **ppSwapChain,
	_Out_opt_       ID3D11Device         **ppDevice,
	_Out_opt_       D3D_FEATURE_LEVEL    *pFeatureLevel,
	_Out_opt_       ID3D11DeviceContext  **ppImmediateContext)
{
	InitD311();
	LogInfo("\n\n *** D3D11CreateDeviceAndSwapChain called with \n");
	LogInfo("    pAdapter = %p \n", pAdapter);
	LogInfo("    Flags = %#x \n", Flags);
	LogInfo("    pFeatureLevels = %#x \n", pFeatureLevels ?  *pFeatureLevels : 0);
	LogInfo("    pSwapChainDesc = %p \n", pSwapChainDesc);
	LogInfo("    ppSwapChain = %p \n", ppSwapChain);
	LogInfo("    ppDevice = %p \n", ppDevice);
	LogInfo("    pFeatureLevel = %#x \n", pFeatureLevel ? *pFeatureLevel: 0);
	LogInfo("    ppImmediateContext = %p \n", ppImmediateContext);

	// Workaround for UPlay (systemdetection64.dll) and Origin (IGO32.dll)
	// that create a DX10 device that in turn calls in here and crashes
	// during the original _D3D11CreateDeviceAndSwapChain():
	if (FeatureLevels == 1 && pFeatureLevels && pFeatureLevels[0] == D3D_FEATURE_LEVEL_10_0) {
		LogInfo("  WARNING: FAILING CREATION OF FEATURE LEVEL 10.0 DEVICE!\n");
		return E_INVALIDARG;
	}


	ForceDisplayParams(pSwapChainDesc);

#if _DEBUG_LAYER
	Flags = EnableDebugFlags(Flags);
#endif

	HRESULT ret = (*_D3D11CreateDeviceAndSwapChain)(pAdapter, DriverType, Software, Flags, pFeatureLevels,
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

	LogInfo("->D3D11CreateDeviceAndSwapChain returned device handle = %p, context handle = %p, swapchain handle = %p \n", 
		origDevice, origContext, origSwapChain);

#if _DEBUG_LAYER
	ShowDebugInfo(origDevice);
#endif

	HackerDevice *deviceWrap = nullptr;
	if (ppDevice != nullptr)
	{
		deviceWrap = new HackerDevice(origDevice, origContext);
		*ppDevice = deviceWrap;
		LogInfo("  HackerDevice %p created to wrap %p \n", deviceWrap, origDevice);
	}

	HackerContext *contextWrap = nullptr;
	if (ppImmediateContext != nullptr)
	{
		contextWrap = new HackerContext(origDevice, origContext);
		*ppImmediateContext = contextWrap;
		LogInfo("  HackerContext %p created to wrap %p \n", contextWrap, origContext);
	}

	HackerDXGISwapChain *swapchainWrap = nullptr;
	if (ppSwapChain != nullptr)
	{
		swapchainWrap = new HackerDXGISwapChain(origSwapChain, deviceWrap, contextWrap);
		*ppSwapChain = reinterpret_cast<IDXGISwapChain*>(swapchainWrap);
		LogInfo("  HackerDXGISwapChain %p created to wrap %p \n", swapchainWrap, origSwapChain);
	}

	// Let each of the new Hacker objects know about the other, needed for unusual
	// calls in the Hacker objects where we want to return the Hacker versions.
	if (deviceWrap != nullptr)
		deviceWrap->SetHackerContext(contextWrap);
	if (deviceWrap != nullptr)
		deviceWrap->SetHackerSwapChain(swapchainWrap);
	if (contextWrap != nullptr)
		contextWrap->SetHackerDevice(deviceWrap);

	// With all the interacting objects set up, we can now safely finish the HackerDevice init.
	if (deviceWrap != nullptr)
		deviceWrap->CreateStereoAndIniTextures();

	LogInfo("->returns result = %x, device handle = %p, device wrapper = %p, context handle = %p, " 
		"context wrapper = %p, swapchain handle = %p, swapchain wrapper = %p \n\n", 
		ret, origDevice, deviceWrap, origContext, contextWrap, origSwapChain, swapchainWrap);
	
	return ret;
}

extern "C" NvAPI_Status __cdecl nvapi_QueryInterface(unsigned int offset);

void NvAPIOverride()
{
	// One shot, override custom settings.
	NvAPI_Status ret = nvapi_QueryInterface(0xb03bb03b);
	if (ret != 0xeecc34ab)
		LogInfo("  overriding NVAPI wrapper failed. \n");

	//const StereoHandle id1 = (StereoHandle)0x77aa8ebc;
	//float id2 = 1.23f;
	//if (NvAPI_Stereo_GetConvergence(id1, &id2) != 0xeecc34ab)
	//{
	//	LogDebug("  overriding NVAPI wrapper failed.\n");
	//}
}
