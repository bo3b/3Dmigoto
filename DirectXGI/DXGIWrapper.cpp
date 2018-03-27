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
// 
// 11-7-15: Reviving this subproject because Dying Light uses an odd loading
//	technique that winds up loading dxgi.dll *before* we ever get our first
//	instruction in d3d11 hooking.  So we cannot hook dxgi before it gets the
//	wrong one, and leads to not being able to use the overlay or advanced features.
//
//	This will just be a loader, like the D3D_Compiler46, not required.
//
// 3-21-18: Added Win10 support by changing the .def file to include CreateDXGIFactory2,
//  as a direct export. This makes it work as a loader on both Win10 and Win7.
//  Removed the LoadLibrary function by adding the ID3D11CreateDevice external
//  load requirement as part of the Link in project file.  This makes the DLL loader
//  in Windows directly load our d3d11.dll without any code or other requirements.


#include <Windows.h>
#include <stdio.h>
#include <Shlobj.h>
#include <dxgi1_2.h>

#include "DXGIWrapper.h"
//#include "HackerDXGI.h"

#include "log.h"


bool gLogDebug = false;
FILE *LogFile = 0;

static bool gInitialized = false;


void InitializeDLL()
{
	if (!gInitialized)
	{
		gInitialized = true;

		wchar_t dir[MAX_PATH];
		GetModuleFileName(0, dir, MAX_PATH);
		wcsrchr(dir, L'\\')[1] = 0;
		wcscat(dir, L"d3dx.ini");

		// If specified in Logging section, wait for Attach to Debugger.
		bool waitfordebugger = false;
		int debugger = GetPrivateProfileInt(L"Logging", L"waitfordebugger", 0, dir);
		if (debugger > 0)
		{
			waitfordebugger = true;
			do
			{
				Sleep(250);
			} while (!IsDebuggerPresent());
			if (debugger > 1)
				__debugbreak();
		}

		// Switch to unbuffered logging to remove need for fflush calls, and r/w access to make it easy
		// to open active files.
		if (GetPrivateProfileInt(L"Logging", L"calls", 1, dir))
		{
			LogFile = _fsopen("dxgi_log.txt", "w", _SH_DENYNO);
			LogInfo("\n *** DXGI DLL starting init  -  %s\n\n", LogTime().c_str());
		}

		gLogDebug = GetPrivateProfileInt(L"Logging", L"debug", 0, dir) == 1;

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
			BOOL result = SetProcessAffinityMask(GetCurrentProcess(), one);
			LogInfo("  CPU Affinity forced to 1- no multithreading: %s\n", result ? "true" : "false");
		}

		if (LogFile)
		{
			LogInfo("[Logging]\n");
			LogInfo("  calls=1\n");
			LogDebug("  debug=1\n");
			if (waitfordebugger) LogInfo("  waitfordebugger=1\n");
		}

		/* Skipped for now.  Maybe revive if we want to make this the wrapper once again.
			Right now this override work is done inside of HackerDXGI, and loaded by IniParams.

		wchar_t val[MAX_PATH];
		int read = GetPrivateProfileString(L"Device", L"width", 0, val, MAX_PATH, dir);
		if (read) swscanf_s(val, L"%d", &SCREEN_WIDTH);
		read = GetPrivateProfileString(L"Device", L"height", 0, val, MAX_PATH, dir);
		if (read) swscanf_s(val, L"%d", &SCREEN_HEIGHT);
		read = GetPrivateProfileString(L"Device", L"refresh_rate", 0, val, MAX_PATH, dir);
		if (read) swscanf_s(val, L"%d", &SCREEN_REFRESH);
		SCREEN_FULLSCREEN = GetPrivateProfileInt(L"Device", L"full_screen", 0, dir) == 1;
		SCREEN_ALLOW_COMMANDS = GetPrivateProfileInt(L"Device", L"allow_windowcommands", 0, dir) == 1;
		read = GetPrivateProfileString(L"Device", L"filter_refresh_rate", 0, val, MAX_PATH, dir);
		if (read) swscanf_s(val, L"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
			FILTER_REFRESH+0, FILTER_REFRESH+1, FILTER_REFRESH+2, FILTER_REFRESH+3, &FILTER_REFRESH+4,
			FILTER_REFRESH+5, FILTER_REFRESH+6, FILTER_REFRESH+7, FILTER_REFRESH+8, &FILTER_REFRESH+9);

		if (LogFile)
		{
			LogInfo("[Device]\n");
			if (SCREEN_WIDTH != -1) LogInfo("  width=%d\n", SCREEN_WIDTH);
			if (SCREEN_HEIGHT != -1) LogInfo("  height=%d\n", SCREEN_HEIGHT);
			if (SCREEN_REFRESH != -1) LogInfo("  refresh_rate=%d\n", SCREEN_REFRESH);
			if (FILTER_REFRESH[0]) LogInfoW(L"  filter_refresh_rate=%s\n", FILTER_REFRESH[0]);
			if (SCREEN_FULLSCREEN) LogInfo("  full_screen=1\n");

			if (SCREEN_ALLOW_COMMANDS) LogInfo("  allow_windowcommands=1\n");
		}
		*/
	}

	LogInfo(" *** DXGI DLL successfully initialized. *** \n\n");
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
HRESULT WINAPI D3DKMTWaitForVerticalBlankEvent() { return 0; }

// All Win10 routines that are exported.  We only need to export
// CreateDXGIFactory2, as the others are never expected to be seen,
// and will give a clear unambiguous error if they ever are.
// * items are new.

//    *	1    0 00018F30 ApplyCompatResolutionQuirking
//    *	2    1 00005AD0 CompatString
//    *	3    2 00005A40 CompatValue
//      10    3 00006490 CreateDXGIFactory
//      11    4 00007030 CreateDXGIFactory1
//    *	12    5 000065F0 CreateDXGIFactory2
//      13    6 000207B0 DXGID3D10CreateDevice
//      14    7 000207B0 DXGID3D10CreateLayeredDevice
//    *	15    8 0001A120 DXGID3D10ETWRundown
//      16    9 00045230 DXGID3D10GetLayeredDeviceSize
//      17    A 000207B0 DXGID3D10RegisterLayers
//    *	4    B 000432C0 DXGIDumpJournal
//    *	18    C 00045240 DXGIGetDebugInterface1
//      19    D 000038C0 DXGIReportAdapterConfiguration
//    *	5    E 000434D0 DXGIRevertToSxS
//    *	6    F 000208D0 PIXBeginCapture
//    *	7   10 000208D0 PIXEndCapture
//    *	8   11 000208D0 PIXGetCaptureState
//    *	9   12 00044100 SetAppCompatStringPointer


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

static HMODULE hDXGI = 0;

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

typedef HRESULT(WINAPI *tDXGID3D10CreateDevice)(HMODULE d3d10core, IDXGIFactory *factory, 
	IDXGIAdapter *adapter, UINT flags, void *unknown0, void **device);
static tDXGID3D10CreateDevice _DXGID3D10CreateDevice;

typedef HRESULT (WINAPI *tDXGID3D10CreateLayeredDevice)(int a, int b, int c, int d, int e);
static tDXGID3D10CreateLayeredDevice _DXGID3D10CreateLayeredDevice;

typedef HRESULT(WINAPI *tDXGID3D10GetLayeredDeviceSize)(const void *pLayers, UINT NumLayers);
static tDXGID3D10GetLayeredDeviceSize _DXGID3D10GetLayeredDeviceSize;

typedef HRESULT(WINAPI *tDXGID3D10RegisterLayers)(const struct dxgi_device_layer *layers, UINT layer_count);
static tDXGID3D10RegisterLayers _DXGID3D10RegisterLayers;

typedef HRESULT (WINAPI *tDXGIReportAdapterConfiguration)(int a);
static tDXGIReportAdapterConfiguration _DXGIReportAdapterConfiguration;

// These are the two interesting API entries for the dxgi.dll that we are replacing.
typedef HRESULT (WINAPI *tCreateDXGIFactory)(REFIID riid, void **ppFactory);
static tCreateDXGIFactory _CreateDXGIFactory;
typedef HRESULT(WINAPI *tCreateDXGIFactory1)(REFIID riid, void **ppFactory);
static tCreateDXGIFactory1 _CreateDXGIFactory1;

// Doesn't exist as a dll export, except for under 8.1 or greater
// Signature now includes Flags parameter.
typedef HRESULT(WINAPI *tCreateDXGIFactory2)(UINT Flags, REFIID riid, void **ppFactory);
static tCreateDXGIFactory2 _CreateDXGIFactory2 = nullptr;


static void InitDXGI()
{
	if (hDXGI) return;
	InitializeDLL();

	// It's OK to get the system dxgi.dll here, as most games do not ship with
	// a dxgi.dll.  This might interfere with other modding tools though.
	wchar_t sysDir[MAX_PATH];
	SHGetFolderPath(0, CSIDL_SYSTEM, 0, SHGFP_TYPE_CURRENT, sysDir);
	wcscat(sysDir, L"\\dxgi.dll");
	hDXGI = LoadLibrary(sysDir);	
    if (hDXGI == NULL)
    {
        LogInfo("\n *** LoadLibrary on System32 dxgi.dll failed *** \n\n");
        
        return;
    }

	_D3DKMTQueryAdapterInfo = (tD3DKMTQueryAdapterInfo) GetProcAddress(hDXGI, "D3DKMTQueryAdapterInfo");
	_OpenAdapter10 = (tOpenAdapter10) GetProcAddress(hDXGI, "OpenAdapter10");
	_OpenAdapter10_2 = (tOpenAdapter10_2) GetProcAddress(hDXGI, "OpenAdapter10_2");
	_D3DKMTGetDeviceState = (tD3DKMTGetDeviceState) GetProcAddress(hDXGI, "D3DKMTGetDeviceState");
	_D3DKMTOpenAdapterFromHdc = (tD3DKMTOpenAdapterFromHdc) GetProcAddress(hDXGI, "D3DKMTOpenAdapterFromHdc");
	_D3DKMTOpenResource = (tD3DKMTOpenResource) GetProcAddress(hDXGI, "D3DKMTOpenResource");
	_D3DKMTQueryResourceInfo = (tD3DKMTQueryResourceInfo) GetProcAddress(hDXGI, "D3DKMTQueryResourceInfo");
	_DXGIDumpJournal = (tDXGIDumpJournal) GetProcAddress(hDXGI, "DXGIDumpJournal");
	_DXGID3D10CreateDevice = (tDXGID3D10CreateDevice) GetProcAddress(hDXGI, "DXGID3D10CreateDevice");
	_DXGID3D10CreateLayeredDevice = (tDXGID3D10CreateLayeredDevice) GetProcAddress(hDXGI, "DXGID3D10CreateLayeredDevice");
	_DXGID3D10RegisterLayers = (tDXGID3D10RegisterLayers) GetProcAddress(hDXGI, "DXGID3D10RegisterLayers");
	_DXGIReportAdapterConfiguration = (tDXGIReportAdapterConfiguration) GetProcAddress(hDXGI, "DXGIReportAdapterConfiguration");
	_CreateDXGIFactory = (tCreateDXGIFactory) GetProcAddress(hDXGI, "CreateDXGIFactory");
	_CreateDXGIFactory1 = (tCreateDXGIFactory1) GetProcAddress(hDXGI, "CreateDXGIFactory1");

	// 8.1 and above, implemented in Win10 version of dxgi.dll
	_CreateDXGIFactory2 = (tCreateDXGIFactory2) GetProcAddress(hDXGI, "CreateDXGIFactory2");

	LogInfo("DXGI GetProcAddress for CreateDXGIFactory2: %p \n\n", _CreateDXGIFactory2);
}


// -----------------------------------------------------------------------------

std::string UUIDtoString(REFIID id)
{
	wchar_t wiid[128];
	if (SUCCEEDED(StringFromGUID2(id, wiid, 128)))
	{
		std::wstring convert = std::wstring(wiid);
		return std::string(convert.begin(), convert.end());
	}
	else
	{
		return "unknown";
	}
}

// -----------------------------------------------------------------------------


HRESULT WINAPI D3DKMTQueryAdapterInfo(_D3DKMT_QUERYADAPTERINFO *info)
{
	InitDXGI();
	return (*_D3DKMTQueryAdapterInfo)(info);
}

HRESULT WINAPI OpenAdapter10(struct D3D10DDIARG_OPENADAPTER *adapter)
{
	InitDXGI();
	return (*_OpenAdapter10)(adapter);
}

HRESULT WINAPI OpenAdapter10_2(struct D3D10DDIARG_OPENADAPTER *adapter)
{
	InitDXGI();
	return (*_OpenAdapter10_2)(adapter);
}

void WINAPI DXGIDumpJournal(void (__stdcall *function)(const char *))
{
	InitDXGI();
	(*_DXGIDumpJournal)(function);
}

HRESULT WINAPI DXGID3D10CreateDevice(HMODULE d3d10core, IDXGIFactory *factory,
	IDXGIAdapter *adapter, UINT flags, void *unknown0, void **device)
{
	InitDXGI();
	return (*_DXGID3D10CreateDevice)(d3d10core, factory, adapter, flags, unknown0, device);
}

// A bunch of these interfaces were using unknown int for their prototypes, but on x64
// anything that was a pointer would be off.  All of the other ones are fixed as best the
// internet can say, but this one is missing any interface info, and probably will not work.
HRESULT WINAPI DXGID3D10CreateLayeredDevice(int a, int b, int c, int d, int e)
{
	InitDXGI();
	return (*_DXGID3D10CreateLayeredDevice)(a, b, c, d, e);
}

HRESULT WINAPI DXGID3D10GetLayeredDeviceSize(const void *pLayers, UINT NumLayers)
{
	InitDXGI();
	return (*_DXGID3D10GetLayeredDeviceSize)(pLayers, NumLayers);
}

HRESULT WINAPI DXGID3D10RegisterLayers(const struct dxgi_device_layer *layers, UINT layer_count)
{
	InitDXGI();
	return (*_DXGID3D10RegisterLayers)(layers, layer_count);
}

HRESULT WINAPI DXGIReportAdapterConfiguration(int a)
{
	InitDXGI();
	return (*_DXGIReportAdapterConfiguration)(a);
}


// -----------------------------------------------------------------------------

HRESULT WINAPI CreateDXGIFactory(REFIID riid, void **ppFactory)
{
	InitDXGI();
	LogInfo("DXGI CreateDXGIFactory(%s) called \n", UUIDtoString(riid).c_str());

	HRESULT hr = (*_CreateDXGIFactory)(riid, ppFactory);

	LogInfo("  return: %x, factory = %p \n", hr, *ppFactory);
	return hr;
}

HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **ppFactory)
{
	InitDXGI();
	LogInfo("DXGI CreateDXGIFactory1(%s) called \n", UUIDtoString(riid).c_str());

	HRESULT hr = (*_CreateDXGIFactory1)(riid, ppFactory);

	LogInfo("  return: %x, factory = %p \n", hr, *ppFactory);
	return hr;
}


// Signature is different than previous Creates. 

HRESULT WINAPI CreateDXGIFactory2(
	UINT   Flags,
	REFIID riid,
	_Out_ void   **ppFactory)
{
	InitDXGI();
	LogInfo("DXGI CreateDXGIFactory2(%s) called Flags = %d \n", UUIDtoString(riid).c_str(), Flags);

	// If the _CreateDXGIFactory2 function is still nullptr, that means we
	// are running on Win7 where the CreateDXGIFactory2 function is not exported.  
	// If the platform update happens to be installed, we will still get the 
	// proper object by passing the IDXGIFactory2 RIID to CreateDXGIFactory. 
	// The reason to do all this is to make it possible to have a single 
	// dxgi.dll loader that works on all Win7 and Win10.
	if (_CreateDXGIFactory2 == nullptr)
	{
		HRESULT hr = (*_CreateDXGIFactory)(riid, ppFactory);
		LogInfo("  _CreateDXGIFactory returns %x factory = %p \n", hr, *ppFactory);
		return hr;
	}

	HRESULT hr = (*_CreateDXGIFactory2)(Flags, riid, ppFactory);

	LogInfo("  return: %x, factory = %p \n", hr, *ppFactory);
	return hr;
}

// -----------------------------------------------------------------------------

// This is only ever expected to exist or be called on Win8.1 and above.


/* Commented out for now- we might want to go back to this wrapping strategy,
	but the hooking is already being done for DXGI from d3d11.

HRESULT WINAPI CreateDXGIFactory2(REFIID riid, void **ppFactory)
	{
	InitDXGI();
	LogInfo("CreateDXGIFactory2 called with riid: %s\n", NameFromIID(riid).c_str());
	
	LogInfo("  calling original CreateDXGIFactory2 API\n");

	IDXGIFactory2 *origFactory2;
	HRESULT hr = (*_CreateDXGIFactory2)(riid, (void **)&origFactory2);
	if (FAILED(hr))
	{
		LogInfo("  failed with HRESULT=%x\n", hr);
		return hr;
	}
	LogInfo("  CreateDXGIFactory2 returned factory = %p, result = %x\n", origFactory2, hr);

	HackerDXGIFactory2 *factory2Wrap = new HackerDXGIFactory2(origFactory2, NULL, NULL);
	if (factory2Wrap == NULL)
	{
		LogInfo("  error allocating factory2Wrap.\n");
		origFactory2->Release();
		return E_OUTOFMEMORY;
	}
	if (ppFactory)
		*ppFactory = factory2Wrap;

	return hr;

	//D3D11Base::IDXGIFactory2 *origFactory = 0;
	//if (ppFactory)
	//	*ppFactory = 0;
	//HRESULT ret;
	//if (_CreateDXGIFactory2)
	//{
	//	LogInfo("  calling original CreateDXGIFactory2 API\n");
	//	
	//	ret = (*_CreateDXGIFactory2)(riid, (void **) &origFactory);
	//}
	//else if (_CreateDXGIFactory1)
	//{
	//	LogInfo("  calling original CreateDXGIFactory1 API\n");
	//	
	//	ret = (*_CreateDXGIFactory1)(riid, (void **) &origFactory);
	//}
	//else
	//{
	//	LogInfo("  calling original CreateDXGIFactory API\n");
	//	
	//	ret = (*_CreateDXGIFactory)(riid, (void **) &origFactory);
	//}
	//if (ret != S_OK)
	//{
	//	LogInfo("  failed with HRESULT=%x\n", ret);
	//	
	//	return ret;
	//}
	//
	//D3D11Wrapper::IDXGIFactory2 *wrapper = D3D11Wrapper::IDXGIFactory2::GetDirectFactory(origFactory);
	//if(wrapper == NULL)
	//{
	//	LogInfo("  error allocating wrapper.\n");
	//	
	//	origFactory->Release();
	//	return E_OUTOFMEMORY;
	//}
	//
	//if (ppFactory)
	//	*ppFactory = wrapper;
	//LogInfo("  returns result = %x, handle = %p, wrapper = %p\n", ret, origFactory, wrapper);
	//
	//return ret;
}
*/

// In the usual insane way that Microsoft loves to design things, you can pass 
// in a riid=DXGIFactory2 here, in order to get a factory2.  
// That's right, call CreateDXGIFactory1 to create an IDXGIFactory2.
//
// Given that insanity, I'm not sure how far to take these. Should we also
// look for a DXGIFactory IID here and return just Factory?
// I'll defer to whether we ever see that in practice.  The logging is pretty
// comprehensive, so in a case like that it should be clear that we need to
// add it.  It's better to do that as-needed, rather than try to guess their madness.

/*
HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **ppFactory)
{
	InitDXGI();
	LogInfo("CreateDXGIFactory1 called with riid: %s\n", NameFromIID(riid).c_str());
	LogInfo("  calling original CreateDXGIFactory1 API\n");

	IDXGIFactory1 *origFactory1;
	HRESULT hr = (*_CreateDXGIFactory1)(riid, (void **)&origFactory1);
	if (FAILED(hr))
	{
		LogInfo("  failed with HRESULT=%x\n", hr);
		return hr;
	}
	LogInfo("  CreateDXGIFactory1 returned factory = %p, result = %x\n", origFactory1, hr);

	HackerDXGIFactory1 *factory1Wrap;
	if (riid == __uuidof(IDXGIFactory2))
	{
		// At present, there seems to be a problem creating Factory2 correctly, and
		// we crash. Rather than debug something is barely used today, let's force
		// the fallback by saying it's not available.
		return E_NOINTERFACE;

		//factory1Wrap = new HackerDXGIFactory2(static_cast<IDXGIFactory2*>(origFactory1), NULL, NULL);
	}
	else
	{
		factory1Wrap = new HackerDXGIFactory1(origFactory1, NULL, NULL);
	}

	if (factory1Wrap == NULL)
	{
		LogInfo("  error allocating factory1Wrap.\n");
		origFactory1->Release();
		return E_OUTOFMEMORY;
	}
	if (ppFactory)
		*ppFactory = factory1Wrap;

	return hr;
}

HRESULT WINAPI CreateDXGIFactory(REFIID riid, void **ppFactory)
{
	InitDXGI();
	LogInfo("CreateDXGIFactory called with riid: %s\n", NameFromIID(riid).c_str());
	LogInfo("  calling original CreateDXGIFactory API\n");

	IDXGIFactory *origFactory;
	HRESULT hr = (*_CreateDXGIFactory)(riid, (void **)&origFactory);
	if (FAILED(hr))
	{
		LogInfo("  failed with HRESULT=%x\n", hr);
		return hr;
	}
	LogInfo("  CreateDXGIFactory returned factory = %p, result = %x\n", origFactory, hr);

	// Must also handle the bizarre case of being passed the DXGIFactory2 riid, where we
	// need to return that Factory2.  This is not likely to be used much yet, as Factory2
	// requires the evil update under Win7, or 8.0 and above.  
	// The middle section for Factory1 is probably unnecessary.

	HackerDXGIFactory *factoryWrap;
	if (riid == __uuidof(IDXGIFactory2))
	{
		// At present, there seems to be a problem creating Factory2 correctly, and
		// we crash. Rather than debug something is barely used today, let's force
		// the fallback by saying it's not available.
		return E_NOINTERFACE;
		
		//factoryWrap = new HackerDXGIFactory2(static_cast<IDXGIFactory2*>(origFactory), NULL, NULL);
	}
	else if (riid == __uuidof(IDXGIFactory1))
	{
		factoryWrap = new HackerDXGIFactory1(static_cast<IDXGIFactory1*>(origFactory), NULL, NULL);
	}
	else
	{
		factoryWrap = new HackerDXGIFactory(origFactory, NULL, NULL);
	}

	if (factoryWrap == NULL)
	{
		LogInfo("  error allocating factoryWrap.\n");
		origFactory->Release();
		return E_OUTOFMEMORY;
	}
	if (ppFactory)
		*ppFactory = factoryWrap;
	
	return hr;
}
*/

// -----------------------------------------------------------------------------

int WINAPI D3DKMTGetDeviceState(int a)
{
	InitDXGI();
	return (*_D3DKMTGetDeviceState)(a);
}

int WINAPI D3DKMTOpenAdapterFromHdc(int a)
{
	InitDXGI();
	return (*_D3DKMTOpenAdapterFromHdc)(a);
}

int WINAPI D3DKMTOpenResource(int a)
{
	InitDXGI();
	return (*_D3DKMTOpenResource)(a);
}

int WINAPI D3DKMTQueryResourceInfo(int a)
{
	InitDXGI();
	return (*_D3DKMTQueryResourceInfo)(a);
}

//static void ReplaceInterface(void **ppvObj)
//{
//    D3D11Wrapper::IDXGIAdapter1 *p1 = (D3D11Wrapper::IDXGIAdapter1*) D3D11Wrapper::IDXGIAdapter::m_List.GetDataPtr(*ppvObj);
//    if (p1)
//	{
//		unsigned long cnt = ((IUnknown*)*ppvObj)->Release();
//		*ppvObj = p1;
//		unsigned long cnt2 = p1->AddRef();
//		LogInfo("  interface replaced with IDXGIAdapter1 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter=%d\n", cnt, p1->m_ulRef, cnt2);
//	}
//    D3D11Wrapper::IDXGIOutput *p2 = (D3D11Wrapper::IDXGIOutput*) D3D11Wrapper::IDXGIOutput::m_List.GetDataPtr(*ppvObj);
//    if (p2)
//	{
//		unsigned long cnt = ((IUnknown*)*ppvObj)->Release();
//		*ppvObj = p2;
//		unsigned long cnt2 = p2->AddRef();
//		LogInfo("  interface replaced with IDXGIOutput wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter=%d\n", cnt, p2->m_ulRef, cnt2);
//	}
//    D3D11Wrapper::IDXGIFactory2 *p3 = (D3D11Wrapper::IDXGIFactory2*) D3D11Wrapper::IDXGIFactory::m_List.GetDataPtr(*ppvObj);
//    if (p3)
//	{
//		unsigned long cnt = ((IUnknown*)*ppvObj)->Release();
//		*ppvObj = p3;
//		unsigned long cnt2 = p3->AddRef();
//		LogInfo("  interface replaced with IDXGIFactory2 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter=%d\n", cnt, p3->m_ulRef, cnt2);
//	}
//    D3D11Wrapper::IDXGISwapChain *p4 = (D3D11Wrapper::IDXGISwapChain*) D3D11Wrapper::IDXGISwapChain::m_List.GetDataPtr(*ppvObj);
//    if (p4)
//	{
//		unsigned long cnt = ((IUnknown*)*ppvObj)->Release();
//		*ppvObj = p4;
//		unsigned long cnt2 = p4->AddRef();
//		LogInfo("  interface replaced with IDXGISwapChain wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter=%d\n", cnt, p4->m_ulRef, cnt2);
//	}
//}


//STDMETHODIMP D3D11Wrapper::IDirect3DUnknown::QueryInterface(THIS_ REFIID riid, void** ppvObj)
//{
////	LogInfo("D3DXGI::IDirect3DUnknown::QueryInterface called at 'this': %s\n", type_name(this));
//
//	LogInfo("QueryInterface request for %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx on %p\n",
//		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7], this);
//	bool unknown1 = riid.Data1 == 0x7abb6563 && riid.Data2 == 0x02bc && riid.Data3 == 0x47c4 && riid.Data4[0] == 0x8e && 
//		riid.Data4[1] == 0xf9 && riid.Data4[2] == 0xac && riid.Data4[3] == 0xc4 && riid.Data4[4] == 0x79 && 
//		riid.Data4[5] == 0x5e && riid.Data4[6] == 0xdb && riid.Data4[7] == 0xcf;
//	/*
//	if (unknown1) LogInfo("  7abb6563-02bc-47c4-8ef9-acc4795edbcf = undocumented. Forcing fail.\n");
//	if (unknown1)
//	{
//		*ppvObj = 0;
//		return E_OUTOFMEMORY;
//	}
//	*/
//	HRESULT hr = m_pUnk->QueryInterface(riid, ppvObj);
//	LogInfo("  result = %x, handle = %p\n", hr, *ppvObj);
//	ReplaceInterface(ppvObj);
//	/*
//	if (!p1 && !p2 && !p3)
//	{
//		((IDirect3DUnknown*)*ppvObj)->Release();
//		hr = E_NOINTERFACE;
//		LogInfo("  removing unknown interface and returning error.\n");
//	}
//	*/
//	
//	return hr;
//}
//
//static IUnknown *ReplaceDevice(IUnknown *wrapper)
//{
//	if (!wrapper)
//		return wrapper;
//	LogInfo("  checking for device wrapper, handle = %p\n", wrapper);
//	IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x01 } };
//	IUnknown *realDevice = wrapper;
//	LogInfo("  dxgi.ReplaceDevice calling wrapper->QueryInterface, wrapper: %s\n", type_name(wrapper));
//
//	if (wrapper->QueryInterface(marker, (void **)&realDevice) == 0x13bc7e31)
//	{
//		LogInfo("    device found. replacing with original handle = %p\n", realDevice);
//		
//		return realDevice;
//	}
//	return wrapper;
//}
//
//struct SwapChainInfo
//{
//	int width, height;
//};
//static void SendScreenResolution(IUnknown *wrapper, int width, int height)
//{
//	if (!wrapper)
//		return;
//	LogInfo("  sending screen resolution to DX11\n");	
//	IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x03 } };
//	SwapChainInfo info;
//	info.width = width;
//	info.height = height;
//	SwapChainInfo *infoPtr = &info;
//	LogInfo("  dxgi.SendScreenResolution calling wrapper->QueryInterface, wrapper: %s\n", type_name(wrapper));
//
//	if (wrapper->QueryInterface(marker, (void **)&infoPtr) == 0x13bc7e31)
//	{
//		LogInfo("    notification successful.\n");
//	}
//}


// We need to get control as early as possible in order to pre-load our 
// d3d11 and nvapi64 dlls.  This will get called at static load time to
// get us hooked into place.  
//
// Be Careful of the restrictions on this entry point. No LogInfo, No LoadLibrary e.g.
//
// This project has a static link to the d3d11.dll call of D3D11CreateDevice, so that any
// load of this dll will also statically load our d3d11.dll. That avoids the need to
// do any LoadLibrary here.

BOOL WINAPI DllMain(
	_In_  HINSTANCE hinstDLL,
	_In_  DWORD fdwReason,
	_In_  LPVOID lpvReserved)
{
	bool result = true;

	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		break;

	case DLL_PROCESS_DETACH:
		break;

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;
	}

	return result;
}
