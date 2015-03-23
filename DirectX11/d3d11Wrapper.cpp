#include "d3d11Wrapper.h"

#include <Shlobj.h>
#include <Winuser.h>
#include <map>
#include <vector>
#include <set>
#include <iterator>
#include <string>

#include "../HLSLDecompiler/DecompileHLSL.h"
#include "../util.h"
#include "../log.h"
#include "Override.h"
#include "globals.h"
#include "Direct3D11Device.h"
#include "Direct3D11Context.h"
#include "IniHandler.h"

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

	NvAPI_ShortString errorMessage;
	NvAPI_Status status;

	status = NvAPI_Initialize();
	if (status != NVAPI_OK)
	{
		NvAPI_GetErrorMessage(status, errorMessage);
		LogInfo("  NvAPI_Initialize failed: %s\n", errorMessage);
		return false;
	}
	status = CheckStereo();
	if (status != NVAPI_OK)
	{
		NvAPI_GetErrorMessage(status, errorMessage);
		LogInfo("  *** stereo is disabled: %s  ***\n", errorMessage);
		return false;
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


static void InitD311()
{
	if (hD3D11) return;

	G = new Globals();
	InitializeCriticalSection(&G->mCriticalSection);

	if (!InitializeDLL())
	{
		// Failed to init?  Best to exit now, we are sure to crash.
		BeepFailure2();
		Sleep(500);
		BeepFailure2();
		Sleep(200);
		ExitProcess(0xc0000135);
	}

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
			LogInfo("trying to load %s\n", path);
		}
		hD3D11 = LoadLibrary(sysDir);
		if (!hD3D11)
		{
			if (LogFile)
			{
				char path[MAX_PATH];
				wcstombs(path, G->CHAIN_DLL_PATH, MAX_PATH);
				LogInfo("load failed. Trying to load %s\n", path);
			}
			hD3D11 = LoadLibrary(G->CHAIN_DLL_PATH);
		}
	}
	else
	{
		wchar_t sysDir[MAX_PATH];
		SHGetFolderPath(0, CSIDL_SYSTEM, 0, SHGFP_TYPE_CURRENT, sysDir);
#if (_WIN64 && HOOK_SYSTEM32)
		// We'll look for this in MainHook to avoid callback to self.		
		// Must remain all lower case to be matched in MainHook.
		wcscat(sysDir, L"\\original_d3d11.dll");
#else
		wcscat(sysDir, L"\\d3d11.dll");
#endif

		if (LogFile)
		{
			char path[MAX_PATH];
			wcstombs(path, sysDir, MAX_PATH);
			LogInfo("trying to load %s\n", path);
		}
		hD3D11 = LoadLibrary(sysDir);
	}
	if (hD3D11 == NULL)
	{
		LogInfo("LoadLibrary on d3d11.dll failed\n");

		return;
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

// For creating the device, we need to call the original D3D11CreateDevice in order to initialize
// Direct3D, and collect the original Device and original Context.  Both of those will be handed
// off to the wrapped HackerDevice and HackerContext objects, so they can call out to the originals
// as needed.  Both Hacker objects need access to both Context and Device, so since both are 
// created here, it's easy enough to provide them upon instantiation.

HRESULT WINAPI D3D11CreateDevice(
	IDXGIAdapter *pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D_FEATURE_LEVEL *pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	HackerDevice **ppDevice,
	D3D_FEATURE_LEVEL *pFeatureLevel,
	HackerContext **ppImmediateContext)
{
	InitD311();
	LogInfo("D3D11CreateDevice called with adapter = %p\n", pAdapter);

#if defined(_DEBUG_LAYER)
	// If the project is in a debug build, enable the debug layer.
	Flags |= D3D11_CREATE_DEVICE_DEBUG;
	Flags |= D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;
#endif

	ID3D11Device *origDevice = 0;
	ID3D11DeviceContext *origContext = 0;

	HRESULT ret = (*_D3D11CreateDevice)(pAdapter, DriverType, Software, Flags, pFeatureLevels,
		FeatureLevels, SDKVersion, &origDevice, pFeatureLevel, &origContext);

	if (!origDevice || !origContext)
		return ret;

	// ret from D3D11CreateDevice has the same problem as CreateDeviceAndSwapChain, in that it can return
	// a value that S_FALSE, which is a positive number.  It's not an error exactly, but it's not S_OK.
	// The best check here is for FAILED instead, to allow benign errors to continue.
	if (FAILED(ret))
	{
		LogInfo("  failed with HRESULT=%x\n", ret);
		return ret;
	}

	// Create a wrapped version of the original device to return to the game.
	HackerDevice *deviceWrap = new HackerDevice(origDevice, origContext);
	if (deviceWrap == NULL)
	{
		LogInfo("  error allocating deviceWrap.\n");
		origDevice->Release();
		origContext->Release();
		return E_OUTOFMEMORY;
	}

	// Create a wrapped version of the original context to return to the game.
	HackerContext *contextWrap = new HackerContext(origDevice, origContext);
	if (contextWrap == NULL)
	{
		LogInfo("  error allocating contextWrap.\n");
		origDevice->Release();
		origContext->Release();
		return E_OUTOFMEMORY;
	}

	// Let each of the new Hacker objects know about the other, needed for unusual
	// calls that we want to return the Hacker versions.
	deviceWrap->SetHackerContext(contextWrap);
	contextWrap->SetHackerDevice(deviceWrap);

	if (ppDevice)
		*ppDevice = deviceWrap;
	if (ppImmediateContext)
		*ppImmediateContext = contextWrap;

	LogInfo("  returns result = %x, device handle = %p, device wrapper = %p, context handle = %p, context wrapper = %p\n", ret, origDevice, deviceWrap, origContext, contextWrap);
	//LogInfo("  return types: origDevice = %s, deviceWrap = %s, origContext = %s, contextWrap = %s\n", 
	//	typeid(*origDevice).name(), typeid(*deviceWrap).name(), typeid(*origContext).name(), typeid(*contextWrap).name());
	return ret;
}

HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
	IDXGIAdapter *pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D_FEATURE_LEVEL *pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
	IDXGISwapChain **ppSwapChain,
	HackerDevice **ppDevice,
	D3D_FEATURE_LEVEL *pFeatureLevel,
	HackerContext **ppImmediateContext)
{
	InitD311();
	LogInfo("D3D11CreateDeviceAndSwapChain called with adapter = %p\n", pAdapter);
	if (pSwapChainDesc) LogInfo("  Windowed = %d\n", pSwapChainDesc->Windowed);
	if (pSwapChainDesc) LogInfo("  Width = %d\n", pSwapChainDesc->BufferDesc.Width);
	if (pSwapChainDesc) LogInfo("  Height = %d\n", pSwapChainDesc->BufferDesc.Height);
	if (pSwapChainDesc) LogInfo("  Refresh rate = %f\n",
		(float)pSwapChainDesc->BufferDesc.RefreshRate.Numerator / (float)pSwapChainDesc->BufferDesc.RefreshRate.Denominator);

	if (G->SCREEN_FULLSCREEN >= 0 && pSwapChainDesc) pSwapChainDesc->Windowed = !G->SCREEN_FULLSCREEN;
	if (G->SCREEN_REFRESH >= 0 && pSwapChainDesc && !pSwapChainDesc->Windowed)
	{
		pSwapChainDesc->BufferDesc.RefreshRate.Numerator = G->SCREEN_REFRESH;
		pSwapChainDesc->BufferDesc.RefreshRate.Denominator = 1;
	}
	if (G->SCREEN_WIDTH >= 0 && pSwapChainDesc) pSwapChainDesc->BufferDesc.Width = G->SCREEN_WIDTH;
	if (G->SCREEN_HEIGHT >= 0 && pSwapChainDesc) pSwapChainDesc->BufferDesc.Height = G->SCREEN_HEIGHT;
	if (pSwapChainDesc) LogInfo("  calling with parameters width = %d, height = %d, refresh rate = %f, windowed = %d\n",
		pSwapChainDesc->BufferDesc.Width, pSwapChainDesc->BufferDesc.Height,
		(float)pSwapChainDesc->BufferDesc.RefreshRate.Numerator / (float)pSwapChainDesc->BufferDesc.RefreshRate.Denominator,
		pSwapChainDesc->Windowed);

#if defined(_DEBUG_LAYER)
	// If the project is in a debug build, enable the debug layer.
	Flags |= D3D11_CREATE_DEVICE_DEBUG;
	Flags |= D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;
#endif

	ID3D11Device *origDevice = 0;
	ID3D11DeviceContext *origContext = 0;
	HRESULT ret = (*_D3D11CreateDeviceAndSwapChain)(pAdapter, DriverType, Software, Flags, pFeatureLevels,
		FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, &origDevice, pFeatureLevel, &origContext);

	// Changed to recognize that >0 DXGISTATUS values are possible, not just S_OK.
	if (FAILED(ret))
	{
		LogInfo("  failed with HRESULT=%x\n", ret);
		return ret;
	}

	LogInfo("  CreateDeviceAndSwapChain returned device handle = %p, context handle = %p\n", origDevice, origContext);

	if (!origDevice || !origContext)
		return ret;

	HackerDevice *deviceWrap = new HackerDevice(origDevice, origContext);
	if (deviceWrap == NULL)
	{
		LogInfo("  error allocating deviceWrap.\n");
		origDevice->Release();
		origContext->Release();
		return E_OUTOFMEMORY;
	}

	HackerContext *contextWrap = new HackerContext(origDevice, origContext);
	if (contextWrap == NULL)
	{
		LogInfo("  error allocating contextWrap.\n");
		origDevice->Release();
		origContext->Release();
		return E_OUTOFMEMORY;
	}

	// Let each of the new Hacker objects know about the other, needed for unusual
	// calls that we want to return the Hacker versions.
	deviceWrap->SetHackerContext(contextWrap);
	contextWrap->SetHackerDevice(deviceWrap);

	if (ppDevice)
		*ppDevice = deviceWrap;
	if (ppImmediateContext)
		*ppImmediateContext = contextWrap;

	LogInfo("  returns result = %x, device handle = %p, device wrapper = %p, context handle = %p, context wrapper = %p\n", ret, origDevice, deviceWrap, origContext, contextWrap);
	//LogInfo("  return types: origDevice = %s, deviceWrap = %s, origContext = %s, contextWrap = %s\n",
	//	typeid(*origDevice).name(), typeid(*deviceWrap).name(), typeid(*origContext).name(), typeid(*contextWrap).name());
	return ret;
}

void NvAPIOverride()
{
	// Override custom settings.
	const StereoHandle id1 = (StereoHandle)0x77aa8ebc;
	float id2 = 1.23f;
	if (NvAPI_Stereo_GetConvergence(id1, &id2) != 0xeecc34ab)
	{
		LogDebug("  overriding NVAPI wrapper failed.\n");
	}
}
