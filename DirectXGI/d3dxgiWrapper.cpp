// 4-8-15: Changing this subproject to be just a logging based project.
//	Previously is was actively used for hooking dxgi, but was disabled
//  a while back to make the tool work on 8.1.  This has been broken
//  since then.
//
//  The goal of making it a logger, is to inspect game use of dxgi in
//  order to understand what we need.
//
// While still part of the overal project, this is a subpiece not shipped
// with game fixes.

#include <stdio.h>
#include <Winuser.h>
#include <Shlobj.h>

#include "log.h"


FILE *LogFile = 0;

static bool gInitialized = false;
static bool gAllowWindowCommands = false;
static int SCREEN_WIDTH = -1;
static int SCREEN_HEIGHT = -1;
static int SCREEN_REFRESH = -1;
static int FILTER_REFRESH[11] = { 0,0,0,0,0,0,0,0,0,0,0 };
static int SCREEN_FULLSCREEN = -1;

static bool mBlockingMode = false;


void InitializeDLL()
{
	if (!gInitialized)
	{
		gInitialized = true;
		wchar_t dir[MAX_PATH];
		GetModuleFileName(0, dir, MAX_PATH);
		wcsrchr(dir, L'\\')[1] = 0;
		wcscat(dir, L"d3dx.ini");

		// Switch to unbuffered logging to remove need for fflush calls, and r/w access to make it easy
		// to open active files.
		if (GetPrivateProfileInt(L"Logging", L"calls", 1, dir))
		{
			LogFile = _fsopen("dxgi_log.txt", "w", _SH_DENYNO);
			LogInfo("\n *** DXGI DLL starting init  -  %s\n\n", LogTime());
		}

		// Unbuffered logging to remove need for fflush calls, and r/w access to make it easy
		// to open active files.
		int unbuffered = -1;
		if (GetPrivateProfileInt(L"Logging", L"unbuffered", 0, dir))
		{
			unbuffered = setvbuf(LogFile, NULL, _IONBF, 0);
			LogInfo("  unbuffered=1  return: %d\n", unbuffered);
		}

		// Set the CPU affinity based upon d3dx.ini setting.  Useful for debugging and shader hunting in AC3.
		if (GetPrivateProfileInt(L"Logging", L"force_cpu_affinity", 0, dir))
		{
			DWORD one = 0x01;
			bool result = SetProcessAffinityMask(GetCurrentProcess(), one);
			LogInfo("  CPU Affinity forced to 1- no multithreading: %s\n", result ? "true" : "false");
		}

		wchar_t val[MAX_PATH];
		int read = GetPrivateProfileString(L"Device", L"width", 0, val, MAX_PATH, dir);
		if (read) swscanf_s(val, L"%d", &SCREEN_WIDTH);
		read = GetPrivateProfileString(L"Device", L"height", 0, val, MAX_PATH, dir);
		if (read) swscanf_s(val, L"%d", &SCREEN_HEIGHT);
		read = GetPrivateProfileString(L"Device", L"refresh_rate", 0, val, MAX_PATH, dir);
		if (read) swscanf_s(val, L"%d", &SCREEN_REFRESH);
		read = GetPrivateProfileString(L"Device", L"full_screen", 0, val, MAX_PATH, dir);
		if (read) swscanf_s(val, L"%d", &SCREEN_FULLSCREEN);
		gAllowWindowCommands = GetPrivateProfileInt(L"Device", L"allow_windowcommands", 0, dir) == 1;
		read = GetPrivateProfileString(L"Device", L"filter_refresh_rate", 0, val, MAX_PATH, dir);
		if (read) swscanf_s(val, L"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
			FILTER_REFRESH+0, FILTER_REFRESH+1, FILTER_REFRESH+2, FILTER_REFRESH+3, &FILTER_REFRESH+4,
			FILTER_REFRESH+5, FILTER_REFRESH+6, FILTER_REFRESH+7, FILTER_REFRESH+8, &FILTER_REFRESH+9);
	}

	LogInfo(" *** DXGI DLL successfully initialized. *** \n");
}

void DestroyDLL()
{
	if (LogFile)
	{
		LogInfo("Destroying DLL...\n");
		fclose(LogFile);
	}
}

HRESULT WINAPI D3DKMTCloseAdapter() { return 0; }
HRESULT WINAPI D3DKMTDestroyAllocation() { return 0; }
HRESULT WINAPI D3DKMTDestroyContext() { return 0; }
HRESULT WINAPI D3DKMTDestroyDevice() { return 0; }
HRESULT WINAPI D3DKMTDestroySynchronizationObject() { return 0; }
HRESULT WINAPI D3DKMTSetDisplayPrivateDriverFormat() { return 0; }
HRESULT WINAPI D3DKMTSignalSynchronizationObject() { return 0; }
HRESULT WINAPI D3DKMTUnlock() { return 0; }
HRESULT WINAPI D3DKMTWaitForSynchronizationObject() { return 0; }
HRESULT WINAPI D3DKMTCreateAllocation() { return 0; }
HRESULT WINAPI D3DKMTCreateContext() { return 0; }
HRESULT WINAPI D3DKMTCreateDevice() { return 0; }
HRESULT WINAPI D3DKMTCreateSynchronizationObject() { return 0; }
HRESULT WINAPI D3DKMTEscape() { return 0; }
HRESULT WINAPI D3DKMTGetContextSchedulingPriority() { return 0; }
HRESULT WINAPI D3DKMTGetDisplayModeList() { return 0; }
HRESULT WINAPI D3DKMTGetMultisampleMethodList() { return 0; }
HRESULT WINAPI D3DKMTGetRuntimeData() { return 0; }
HRESULT WINAPI D3DKMTGetSharedPrimaryHandle() { return 0; }
HRESULT WINAPI D3DKMTLock() { return 0; }
HRESULT WINAPI D3DKMTPresent() { return 0; }
HRESULT WINAPI D3DKMTQueryAllocationResidency() { return 0; }
HRESULT WINAPI D3DKMTRender() { return 0; }
HRESULT WINAPI D3DKMTSetAllocationPriority() { return 0; }
HRESULT WINAPI D3DKMTSetContextSchedulingPriority() { return 0; }
HRESULT WINAPI D3DKMTSetDisplayMode() { return 0; }
HRESULT WINAPI D3DKMTSetGammaRamp() { return 0; }
HRESULT WINAPI D3DKMTSetVidPnSourceOwner() { return 0; }

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
typedef HRESULT (WINAPI *tD3DKMTQueryAdapterInfo)(_D3DKMT_QUERYADAPTERINFO *);
static tD3DKMTQueryAdapterInfo _D3DKMTQueryAdapterInfo;
typedef HRESULT (WINAPI *tOpenAdapter10)(D3D10DDIARG_OPENADAPTER *adapter);
static tOpenAdapter10 _OpenAdapter10;
typedef HRESULT (WINAPI *tOpenAdapter10_2)(D3D10DDIARG_OPENADAPTER *adapter);
static tOpenAdapter10_2 _OpenAdapter10_2;

typedef HRESULT (WINAPI *tD3DKMTGetDeviceState)(int a);
static tD3DKMTGetDeviceState _D3DKMTGetDeviceState;
typedef HRESULT (WINAPI *tD3DKMTOpenAdapterFromHdc)(int a);
static tD3DKMTOpenAdapterFromHdc _D3DKMTOpenAdapterFromHdc;
typedef HRESULT (WINAPI *tD3DKMTOpenResource)(int a);
static tD3DKMTOpenResource _D3DKMTOpenResource;
typedef HRESULT (WINAPI *tD3DKMTQueryResourceInfo)(int a);
static tD3DKMTQueryResourceInfo _D3DKMTQueryResourceInfo;

typedef void (WINAPI *tDXGIDumpJournal)(void (__stdcall *function)(const char *));
static tDXGIDumpJournal _DXGIDumpJournal;

typedef HRESULT(WINAPI *tDXGID3D10CreateDevice)(HMODULE d3d10core, D3D11Wrapper::IDXGIFactory *factory, 
	D3D11Wrapper::IDXGIAdapter *adapter, UINT flags, void *unknown0, void **device);
static tDXGID3D10CreateDevice _DXGID3D10CreateDevice;

typedef HRESULT (WINAPI *tDXGID3D10CreateLayeredDevice)(int a, int b, int c, int d, int e);
static tDXGID3D10CreateLayeredDevice _DXGID3D10CreateLayeredDevice;

typedef HRESULT(WINAPI *tDXGID3D10GetLayeredDeviceSize)(const void *pLayers, UINT NumLayers);
static tDXGID3D10GetLayeredDeviceSize _DXGID3D10GetLayeredDeviceSize;

typedef HRESULT(WINAPI *tDXGID3D10RegisterLayers)(const struct dxgi_device_layer *layers, UINT layer_count);
static tDXGID3D10RegisterLayers _DXGID3D10RegisterLayers;

typedef HRESULT (WINAPI *tDXGIReportAdapterConfiguration)(int a);
static tDXGIReportAdapterConfiguration _DXGIReportAdapterConfiguration;

typedef HRESULT (WINAPI *tCreateDXGIFactory)(const IID *const riid, void **ppFactory);
static tCreateDXGIFactory _CreateDXGIFactory;
typedef HRESULT (WINAPI *tCreateDXGIFactory1)(const IID *const riid, void **ppFactory);
static tCreateDXGIFactory1 _CreateDXGIFactory1;
typedef HRESULT (WINAPI *tCreateDXGIFactory2)(const IID *const riid, void **ppFactory);
static tCreateDXGIFactory2 _CreateDXGIFactory2;

static void InitD311()
{
	if (hD3D11) return;
	InitializeDLL();
	wchar_t sysDir[MAX_PATH];
	SHGetFolderPath(0, CSIDL_SYSTEM, 0, SHGFP_TYPE_CURRENT, sysDir);
	wcscat(sysDir, L"\\dxgi.dll");
	hD3D11 = LoadLibrary(sysDir);	
    if (hD3D11 == NULL)
    {
        LogInfo("LoadLibrary on dxgi.dll failed\n");
        
        return;
    }

	_D3DKMTQueryAdapterInfo = (tD3DKMTQueryAdapterInfo) GetProcAddress(hD3D11, "D3DKMTQueryAdapterInfo");
	_OpenAdapter10 = (tOpenAdapter10) GetProcAddress(hD3D11, "OpenAdapter10");
	_OpenAdapter10_2 = (tOpenAdapter10_2) GetProcAddress(hD3D11, "OpenAdapter10_2");
	_D3DKMTGetDeviceState = (tD3DKMTGetDeviceState) GetProcAddress(hD3D11, "D3DKMTGetDeviceState");
	_D3DKMTOpenAdapterFromHdc = (tD3DKMTOpenAdapterFromHdc) GetProcAddress(hD3D11, "D3DKMTOpenAdapterFromHdc");
	_D3DKMTOpenResource = (tD3DKMTOpenResource) GetProcAddress(hD3D11, "D3DKMTOpenResource");
	_D3DKMTQueryResourceInfo = (tD3DKMTQueryResourceInfo) GetProcAddress(hD3D11, "D3DKMTQueryResourceInfo");
	_DXGIDumpJournal = (tDXGIDumpJournal) GetProcAddress(hD3D11, "DXGIDumpJournal");
	_DXGID3D10CreateDevice = (tDXGID3D10CreateDevice) GetProcAddress(hD3D11, "DXGID3D10CreateDevice");
	_DXGID3D10CreateLayeredDevice = (tDXGID3D10CreateLayeredDevice) GetProcAddress(hD3D11, "DXGID3D10CreateLayeredDevice");
	_DXGID3D10RegisterLayers = (tDXGID3D10RegisterLayers) GetProcAddress(hD3D11, "DXGID3D10RegisterLayers");
	_DXGIReportAdapterConfiguration = (tDXGIReportAdapterConfiguration) GetProcAddress(hD3D11, "DXGIReportAdapterConfiguration");
	_CreateDXGIFactory = (tCreateDXGIFactory) GetProcAddress(hD3D11, "CreateDXGIFactory");
	_CreateDXGIFactory1 = (tCreateDXGIFactory1) GetProcAddress(hD3D11, "CreateDXGIFactory1");
	_CreateDXGIFactory2 = (tCreateDXGIFactory2) GetProcAddress(hD3D11, "CreateDXGIFactory2");
}

HRESULT WINAPI D3DKMTQueryAdapterInfo(_D3DKMT_QUERYADAPTERINFO *info)
{
	InitD311();
	return (*_D3DKMTQueryAdapterInfo)(info);
}

HRESULT WINAPI OpenAdapter10(struct D3D10DDIARG_OPENADAPTER *adapter)
{
	InitD311();
	return (*_OpenAdapter10)(adapter);
}

HRESULT WINAPI OpenAdapter10_2(struct D3D10DDIARG_OPENADAPTER *adapter)
{
	InitD311();
	return (*_OpenAdapter10_2)(adapter);
}

void WINAPI DXGIDumpJournal(void (__stdcall *function)(const char *))
{
	InitD311();
	(*_DXGIDumpJournal)(function);
}

HRESULT WINAPI DXGID3D10CreateDevice(HMODULE d3d10core, D3D11Wrapper::IDXGIFactory *factory,
	D3D11Wrapper::IDXGIAdapter *adapter, UINT flags, void *unknown0, void **device)
{
	InitD311();
	return (*_DXGID3D10CreateDevice)(d3d10core, factory, adapter, flags, unknown0, device);
}

// A bunch of these interfaces were using unknown int for their prototypes, but on x64
// anything that was a pointer would be off.  All of the other ones are fixed as best the
// internet can say, but this one is missing any interface info, and probably will not work.
HRESULT WINAPI DXGID3D10CreateLayeredDevice(int a, int b, int c, int d, int e)
{
	InitD311();
	return (*_DXGID3D10CreateLayeredDevice)(a, b, c, d, e);
}

HRESULT WINAPI DXGID3D10GetLayeredDeviceSize(const void *pLayers, UINT NumLayers)
{
	InitD311();
	return (*_DXGID3D10GetLayeredDeviceSize)(pLayers, NumLayers);
}

HRESULT WINAPI DXGID3D10RegisterLayers(const struct dxgi_device_layer *layers, UINT layer_count)
{
	InitD311();
	return (*_DXGID3D10RegisterLayers)(layers, layer_count);
}

HRESULT WINAPI DXGIReportAdapterConfiguration(int a)
{
	InitD311();
	return (*_DXGIReportAdapterConfiguration)(a);
}

HRESULT WINAPI CreateDXGIFactory2(const IID *const riid, void **ppFactory)
{
	InitD311();
	LogInfo("CreateDXGIFactory2 called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		riid->Data1, riid->Data2, riid->Data3, riid->Data4[0], riid->Data4[1], riid->Data4[2], riid->Data4[3], riid->Data4[4], riid->Data4[5], riid->Data4[6], riid->Data4[7]);
	
	D3D11Base::IDXGIFactory2 *origFactory = 0;
	if (ppFactory)
		*ppFactory = 0;
	HRESULT ret;
	if (_CreateDXGIFactory2)
	{
		LogInfo("  calling original CreateDXGIFactory2 API\n");
		
		ret = (*_CreateDXGIFactory2)(riid, (void **) &origFactory);
	}
	else if (_CreateDXGIFactory1)
	{
		LogInfo("  calling original CreateDXGIFactory1 API\n");
		
		ret = (*_CreateDXGIFactory1)(riid, (void **) &origFactory);
	}
	else
	{
		LogInfo("  calling original CreateDXGIFactory API\n");
		
		ret = (*_CreateDXGIFactory)(riid, (void **) &origFactory);
	}
	if (ret != S_OK)
	{
		LogInfo("  failed with HRESULT=%x\n", ret);
		
		return ret;
	}
	
	D3D11Wrapper::IDXGIFactory2 *wrapper = D3D11Wrapper::IDXGIFactory2::GetDirectFactory(origFactory);
	if(wrapper == NULL)
	{
		LogInfo("  error allocating wrapper.\n");
		
		origFactory->Release();
		return E_OUTOFMEMORY;
	}
	
	if (ppFactory)
		*ppFactory = wrapper;
	LogInfo("  returns result = %x, handle = %p, wrapper = %p\n", ret, origFactory, wrapper);
	
	return ret;
}

HRESULT WINAPI CreateDXGIFactory(const IID *const riid, void **ppFactory)
{
	InitD311();
	LogInfo("CreateDXGIFactory called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		riid->Data1, riid->Data2, riid->Data3, riid->Data4[0], riid->Data4[1], riid->Data4[2], riid->Data4[3], riid->Data4[4], riid->Data4[5], riid->Data4[6], riid->Data4[7]);
	if (_CreateDXGIFactory2)
	{
		LogInfo("  routing call to CreateDXGIFactory2 with riid=50c83a1c-e072-4c48-87b0-3630fa36a6d0\n");
		
		IID factory2 = { 0x50c83a1cul, 0xe072, 0x4c48, { 0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0 } };
		return CreateDXGIFactory2(&factory2, ppFactory);
	}
	if (_CreateDXGIFactory1)
	{
		LogInfo("  routing call to CreateDXGIFactory2 with riid=770aae78-f26f-4dba-a829-253c83d1b387\n");
		
		IID factory1 = { 0x770aae78ul, 0xf26f, 0x4dba, { 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87 } };
		return CreateDXGIFactory2(&factory1, ppFactory);
	}
	LogInfo("  routing call to CreateDXGIFactory2 with original riid\n");
	
	return CreateDXGIFactory2(riid, ppFactory);
}

HRESULT WINAPI CreateDXGIFactory1(const IID *const riid, void **ppFactory)
{
	InitD311();
	LogInfo("CreateDXGIFactory1 called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		riid->Data1, riid->Data2, riid->Data3, riid->Data4[0], riid->Data4[1], riid->Data4[2], riid->Data4[3], riid->Data4[4], riid->Data4[5], riid->Data4[6], riid->Data4[7]);
	if (_CreateDXGIFactory1)
	{
		LogInfo("  routing call to CreateDXGIFactory2 with riid=770aae78-f26f-4dba-a829-253c83d1b387\n");
		
		IID factory1 = { 0x770aae78ul, 0xf26f, 0x4dba, { 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87 } };
		return CreateDXGIFactory2(&factory1, ppFactory);
	}
	LogInfo("  routing call to CreateDXGIFactory2 with original riid\n");
	
	return CreateDXGIFactory2(riid, ppFactory);
}

int WINAPI D3DKMTGetDeviceState(int a)
{
	InitD311();
	return (*_D3DKMTGetDeviceState)(a);
}

int WINAPI D3DKMTOpenAdapterFromHdc(int a)
{
	InitD311();
	return (*_D3DKMTOpenAdapterFromHdc)(a);
}

int WINAPI D3DKMTOpenResource(int a)
{
	InitD311();
	return (*_D3DKMTOpenResource)(a);
}

int WINAPI D3DKMTQueryResourceInfo(int a)
{
	InitD311();
	return (*_D3DKMTQueryResourceInfo)(a);
}

static void ReplaceInterface(void **ppvObj)
{
    D3D11Wrapper::IDXGIAdapter1 *p1 = (D3D11Wrapper::IDXGIAdapter1*) D3D11Wrapper::IDXGIAdapter::m_List.GetDataPtr(*ppvObj);
    if (p1)
	{
		unsigned long cnt = ((IUnknown*)*ppvObj)->Release();
		*ppvObj = p1;
		unsigned long cnt2 = p1->AddRef();
		LogInfo("  interface replaced with IDXGIAdapter1 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter=%d\n", cnt, p1->m_ulRef, cnt2);
	}
    D3D11Wrapper::IDXGIOutput *p2 = (D3D11Wrapper::IDXGIOutput*) D3D11Wrapper::IDXGIOutput::m_List.GetDataPtr(*ppvObj);
    if (p2)
	{
		unsigned long cnt = ((IUnknown*)*ppvObj)->Release();
		*ppvObj = p2;
		unsigned long cnt2 = p2->AddRef();
		LogInfo("  interface replaced with IDXGIOutput wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter=%d\n", cnt, p2->m_ulRef, cnt2);
	}
    D3D11Wrapper::IDXGIFactory2 *p3 = (D3D11Wrapper::IDXGIFactory2*) D3D11Wrapper::IDXGIFactory::m_List.GetDataPtr(*ppvObj);
    if (p3)
	{
		unsigned long cnt = ((IUnknown*)*ppvObj)->Release();
		*ppvObj = p3;
		unsigned long cnt2 = p3->AddRef();
		LogInfo("  interface replaced with IDXGIFactory2 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter=%d\n", cnt, p3->m_ulRef, cnt2);
	}
    D3D11Wrapper::IDXGISwapChain *p4 = (D3D11Wrapper::IDXGISwapChain*) D3D11Wrapper::IDXGISwapChain::m_List.GetDataPtr(*ppvObj);
    if (p4)
	{
		unsigned long cnt = ((IUnknown*)*ppvObj)->Release();
		*ppvObj = p4;
		unsigned long cnt2 = p4->AddRef();
		LogInfo("  interface replaced with IDXGISwapChain wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter=%d\n", cnt, p4->m_ulRef, cnt2);
	}
}

// Todo: Why is this named D3D11Wrapper, but in the d3dxgiWrapper?

STDMETHODIMP D3D11Wrapper::IDirect3DUnknown::QueryInterface(THIS_ REFIID riid, void** ppvObj)
{
//	LogInfo("D3DXGI::IDirect3DUnknown::QueryInterface called at 'this': %s\n", typeid(*this).name());

	LogInfo("QueryInterface request for %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx on %p\n",
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7], this);
	bool unknown1 = riid.Data1 == 0x7abb6563 && riid.Data2 == 0x02bc && riid.Data3 == 0x47c4 && riid.Data4[0] == 0x8e && 
		riid.Data4[1] == 0xf9 && riid.Data4[2] == 0xac && riid.Data4[3] == 0xc4 && riid.Data4[4] == 0x79 && 
		riid.Data4[5] == 0x5e && riid.Data4[6] == 0xdb && riid.Data4[7] == 0xcf;
	/*
	if (unknown1) LogInfo("  7abb6563-02bc-47c4-8ef9-acc4795edbcf = undocumented. Forcing fail.\n");
	if (unknown1)
	{
		*ppvObj = 0;
		return E_OUTOFMEMORY;
	}
	*/
	HRESULT hr = m_pUnk->QueryInterface(riid, ppvObj);
	LogInfo("  result = %x, handle = %p\n", hr, *ppvObj);
	ReplaceInterface(ppvObj);
	/*
	if (!p1 && !p2 && !p3)
	{
		((IDirect3DUnknown*)*ppvObj)->Release();
		hr = E_NOINTERFACE;
		LogInfo("  removing unknown interface and returning error.\n");
	}
	*/
	
	return hr;
}

static IUnknown *ReplaceDevice(IUnknown *wrapper)
{
	if (!wrapper)
		return wrapper;
	LogInfo("  checking for device wrapper, handle = %p\n", wrapper);
	IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x01 } };
	IUnknown *realDevice = wrapper;
	LogInfo("  dxgi.ReplaceDevice calling wrapper->QueryInterface, wrapper: %s\n", typeid(*wrapper).name());

	if (wrapper->QueryInterface(marker, (void **)&realDevice) == 0x13bc7e31)
	{
		LogInfo("    device found. replacing with original handle = %p\n", realDevice);
		
		return realDevice;
	}
	return wrapper;
}

struct SwapChainInfo
{
	int width, height;
};
static void SendScreenResolution(IUnknown *wrapper, int width, int height)
{
	if (!wrapper)
		return;
	LogInfo("  sending screen resolution to DX11\n");	
	IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x03 } };
	SwapChainInfo info;
	info.width = width;
	info.height = height;
	SwapChainInfo *infoPtr = &info;
	LogInfo("  dxgi.SendScreenResolution calling wrapper->QueryInterface, wrapper: %s\n", typeid(*wrapper).name());

	if (wrapper->QueryInterface(marker, (void **)&infoPtr) == 0x13bc7e31)
	{
		LogInfo("    notification successful.\n");
	}
}
