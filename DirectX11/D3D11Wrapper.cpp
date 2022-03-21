#include "D3D11Wrapper.h"

#include "log.h"
#include "Globals.h"
#include "IniHandler.h"
#include "HookedDXGI.h"

#include "nvprofile.h"

//#include <Shlobj.h>
//#include <Winuser.h>
//#include <map>
//#include <vector>
//#include <set>
//#include <iterator>
//#include <string>
//
//#include "util.h"
//#include "Override.hpp"
//#include "HackerDevice.hpp"
//#include "HackerContext.hpp"

// The Log file and the Globals are both used globally, and these are the actual
// definitions of the variables.  All other uses will be via the extern in the
// globals.h and log.h files.

// Globals used to be allocated on the heap, which is pointless given that it
// is global, and fragile given that we now have a second entry point for the
// profile helper that does not use the same init paths as the regular dll.
// Statically allocate it as StaticG and point the old G pointer to it to avoid
// needing to change every reference.
globals  StaticG;
globals* G = &StaticG;

FILE* LogFile   = 0;  // off by default.
bool  gLogDebug = false;

// This critical section must be held to avoid race conditions when creating
// any resource. The nvapi functions used to set the resource creation mode
// affect global state, so if multiple threads are creating resources
// simultaneously it is possible for a StereoMode override or stereo/mono copy
// on one thread to affect another. This should be taken before setting the
// surface creation mode and released only after it has been restored. If the
// creation mode is not being set it should still be taken around the actual
// CreateXXX call.
CRITICAL_SECTION resource_creation_mode_lock;

// During the initialize, we will also Log every setting that is enabled, so that the log
// has a complete list of active settings.  This should make it more accurate and clear.

static bool initialize_dll()
{
    if (G->gInitialized)
        return true;

    LoadConfigFile();

    // Preload OUR nvapi before we call init because we need some of our calls.
#if (_WIN64)
    #define NVAPI_DLL L"nvapi64.dll"
#else
    #define NVAPI_DLL L"nvapi.dll"
#endif

    // Load our nvapi wrapper from the same directory as our DLL, for injection cases
    wchar_t nvapi_path[MAX_PATH];
    if (GetModuleFileName(migoto_handle, nvapi_path, MAX_PATH))
    {
        wcsrchr(nvapi_path, L'\\')[1] = '\0';
        wcscat(nvapi_path, NVAPI_DLL);
        LoadLibrary(nvapi_path);
    }

    NvAPI_ShortString error_message;
    NvAPI_Status      status;

    // Tell our nvapi.dll that it's us calling, and it's OK.
    nvapi_override();
    status = NvAPI_Initialize();
    if (status != NVAPI_OK)
    {
        NvAPI_GetErrorMessage(status, error_message);
        LOG_INFO("  NvAPI_Initialize failed: %s\n", error_message);
        return false;
    }

    log_nv_driver_version();
    log_check_and_update_nv_profiles();

    // This sequence is to make the force_no_nvapi work.  When the game pCars
    // starts it calls NvAPI_Initialize that we want to return an error for.
    // But, the NV stereo driver ALSO calls NvAPI_Initialize, and we need to let
    // that one go through.  So by calling Stereo_Enable early here, we force
    // the NV stereo to load and take advantage of the pending nvapi_override,
    // then all subsequent game calls to Initialize will return an error.
    if (G->gForceNoNvAPI)
    {
        nvapi_override();
        status = Profiling::NvAPI_Stereo_Enable();
        if (status != NVAPI_OK)
        {
            NvAPI_GetErrorMessage(status, error_message);
            LOG_INFO("  NvAPI_Stereo_Enable failed: %s\n", error_message);
            return false;
        }
    }
    //status = CheckStereo();
    //if (status != NVAPI_OK)
    //{
    //    NvAPI_GetErrorMessage(status, errorMessage);
    //    LOG_INFO("  *** stereo is disabled: %s  ***\n", errorMessage);
    //    return false;
    //}

    // If we are going to use 3D Vision Direct Mode, we need to set the driver
    // into that mode early, before any possible CreateDevice occurs.
    if (G->gForceStereo == 2)
    {
        status = NvAPI_Stereo_SetDriverMode(NVAPI_STEREO_DRIVER_MODE_DIRECT);
        if (status != NVAPI_OK)
        {
            NvAPI_GetErrorMessage(status, error_message);
            LOG_INFO("*** NvAPI_Stereo_SetDriverMode to Direct, failed: %s\n", error_message);
            return false;
        }
        LOG_INFO("  NvAPI_Stereo_SetDriverMode successfully set to Direct Mode\n");
    }

    LOG_INFO("\n***  D3D11 DLL successfully initialized.  ***\n\n");
    return true;
}

void destroy_dll()
{
    if (LogFile)
    {
        LOG_INFO("Destroying DLL...\n");
        SavePersistentSettings();
        fclose(LogFile);
    }
}

int WINAPI D3DKMTCloseAdapter()
{
    LOG_DEBUG("D3DKMTCloseAdapter called.\n");

    return 0;
}
int WINAPI D3DKMTDestroyAllocation()
{
    LOG_DEBUG("D3DKMTDestroyAllocation called.\n");

    return 0;
}
int WINAPI D3DKMTDestroyContext()
{
    LOG_DEBUG("D3DKMTDestroyContext called.\n");

    return 0;
}
int WINAPI D3DKMTDestroyDevice()
{
    LOG_DEBUG("D3DKMTDestroyDevice called.\n");

    return 0;
}
int WINAPI D3DKMTDestroySynchronizationObject()
{
    LOG_DEBUG("D3DKMTDestroySynchronizationObject called.\n");

    return 0;
}
int WINAPI D3DKMTSetDisplayPrivateDriverFormat()
{
    LOG_DEBUG("D3DKMTSetDisplayPrivateDriverFormat called.\n");

    return 0;
}
int WINAPI D3DKMTSignalSynchronizationObject()
{
    LOG_DEBUG("D3DKMTSignalSynchronizationObject called.\n");

    return 0;
}
int WINAPI D3DKMTUnlock()
{
    LOG_DEBUG("D3DKMTUnlock called.\n");

    return 0;
}
int WINAPI D3DKMTWaitForSynchronizationObject()
{
    LOG_DEBUG("D3DKMTWaitForSynchronizationObject called.\n");

    return 0;
}
int WINAPI D3DKMTCreateAllocation()
{
    LOG_DEBUG("D3DKMTCreateAllocation called.\n");

    return 0;
}
int WINAPI D3DKMTCreateContext()
{
    LOG_DEBUG("D3DKMTCreateContext called.\n");

    return 0;
}
int WINAPI D3DKMTCreateDevice()
{
    LOG_DEBUG("D3DKMTCreateDevice called.\n");

    return 0;
}
int WINAPI D3DKMTCreateSynchronizationObject()
{
    LOG_DEBUG("D3DKMTCreateSynchronizationObject called.\n");

    return 0;
}
int WINAPI D3DKMTEscape()
{
    LOG_DEBUG("D3DKMTEscape called.\n");

    return 0;
}
int WINAPI D3DKMTGetContextSchedulingPriority()
{
    LOG_DEBUG("D3DKMTGetContextSchedulingPriority called.\n");

    return 0;
}
int WINAPI D3DKMTGetDisplayModeList()
{
    LOG_DEBUG("D3DKMTGetDisplayModeList called.\n");

    return 0;
}
int WINAPI D3DKMTGetMultisampleMethodList()
{
    LOG_DEBUG("D3DKMTGetMultisampleMethodList called.\n");

    return 0;
}
int WINAPI D3DKMTGetRuntimeData()
{
    LOG_DEBUG("D3DKMTGetRuntimeData called.\n");

    return 0;
}
int WINAPI D3DKMTGetSharedPrimaryHandle()
{
    LOG_DEBUG("D3DKMTGetRuntimeData called.\n");

    return 0;
}
int WINAPI D3DKMTLock()
{
    LOG_DEBUG("D3DKMTLock called.\n");

    return 0;
}
int WINAPI D3DKMTPresent()
{
    LOG_DEBUG("D3DKMTPresent called.\n");

    return 0;
}
int WINAPI D3DKMTQueryAllocationResidency()
{
    LOG_DEBUG("D3DKMTQueryAllocationResidency called.\n");

    return 0;
}
int WINAPI D3DKMTRender()
{
    LOG_DEBUG("D3DKMTRender called.\n");

    return 0;
}
int WINAPI D3DKMTSetAllocationPriority()
{
    LOG_DEBUG("D3DKMTSetAllocationPriority called.\n");

    return 0;
}
int WINAPI D3DKMTSetContextSchedulingPriority()
{
    LOG_DEBUG("D3DKMTSetContextSchedulingPriority called.\n");

    return 0;
}
int WINAPI D3DKMTSetDisplayMode()
{
    LOG_DEBUG("D3DKMTSetDisplayMode called.\n");

    return 0;
}
int WINAPI D3DKMTSetGammaRamp()
{
    LOG_DEBUG("D3DKMTSetGammaRamp called.\n");

    return 0;
}
int WINAPI D3DKMTSetVidPnSourceOwner()
{
    LOG_DEBUG("D3DKMTSetVidPnSourceOwner called.\n");

    return 0;
}

typedef ULONG D3DKMT_HANDLE;
typedef int   KMTQUERYADAPTERINFOTYPE;

typedef struct _D3DKMT_QUERYADAPTERINFO
{
    D3DKMT_HANDLE           hAdapter;
    KMTQUERYADAPTERINFOTYPE Type;
    VOID*                   pPrivateDriverData;
    UINT                    PrivateDriverDataSize;
} D3DKMT_QUERYADAPTERINFO;

typedef void* D3D10DDI_HRTADAPTER;
typedef void* D3D10DDI_HADAPTER;
typedef void  D3DDDI_ADAPTERCALLBACKS;
typedef void  D3D10DDI_ADAPTERFUNCS;
typedef void  D3D10_2DDI_ADAPTERFUNCS;

typedef struct D3D10DDIARG_OPENADAPTER
{
    D3D10DDI_HRTADAPTER            hRTAdapter;
    D3D10DDI_HADAPTER              hAdapter;
    UINT                           Interface;
    UINT                           Version;
    const D3DDDI_ADAPTERCALLBACKS* pAdapterCallbacks;
    union
    {
        D3D10DDI_ADAPTERFUNCS*   pAdapterFuncs;
        D3D10_2DDI_ADAPTERFUNCS* pAdapterFuncs_2;
    };
} D3D10DDIARG_OPENADAPTER;

static HMODULE hD3D11 = 0;

typedef int(WINAPI* tD3DKMTQueryAdapterInfo)(_D3DKMT_QUERYADAPTERINFO*);
static tD3DKMTQueryAdapterInfo _D3DKMTQueryAdapterInfo;

typedef int(WINAPI* tOpenAdapter10)(D3D10DDIARG_OPENADAPTER* adapter);
static tOpenAdapter10 _OpenAdapter10;

typedef int(WINAPI* tOpenAdapter10_2)(D3D10DDIARG_OPENADAPTER* adapter);
static tOpenAdapter10_2 _OpenAdapter10_2;

typedef int(WINAPI* tD3D11CoreCreateDevice)(__int32, int, int, LPCSTR lpModuleName, int, int, int, int, int, int);
static tD3D11CoreCreateDevice _D3D11CoreCreateDevice;

typedef HRESULT(WINAPI* tD3D11CoreCreateLayeredDevice)(const void* unknown0, DWORD unknown1, const void* unknown2, REFIID riid, void** ppvObj);
static tD3D11CoreCreateLayeredDevice _D3D11CoreCreateLayeredDevice;

typedef SIZE_T(WINAPI* tD3D11CoreGetLayeredDeviceSize)(const void* unknown0, DWORD unknown1);
static tD3D11CoreGetLayeredDeviceSize _D3D11CoreGetLayeredDeviceSize;

typedef HRESULT(WINAPI* tD3D11CoreRegisterLayers)(const void* unknown0, DWORD unknown1);
static tD3D11CoreRegisterLayers _D3D11CoreRegisterLayers;

typedef int(WINAPI* tD3DKMTGetDeviceState)(int a);
static tD3DKMTGetDeviceState _D3DKMTGetDeviceState;

typedef int(WINAPI* tD3DKMTOpenAdapterFromHdc)(int a);
static tD3DKMTOpenAdapterFromHdc _D3DKMTOpenAdapterFromHdc;

typedef int(WINAPI* tD3DKMTOpenResource)(int a);
static tD3DKMTOpenResource _D3DKMTOpenResource;

typedef int(WINAPI* tD3DKMTQueryResourceInfo)(int a);
static tD3DKMTQueryResourceInfo _D3DKMTQueryResourceInfo;

PFN_D3D11_CREATE_DEVICE                _D3D11CreateDevice;
PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN _D3D11CreateDeviceAndSwapChain;

#ifdef NTDDI_WIN10
PFN_D3D11ON12_CREATE_DEVICE _D3D11On12CreateDevice;
#endif

void init_d3d11()
{
    UINT ret;

    if (hD3D11)
        return;

    INIT_CRITICAL_SECTION(&G->mCriticalSection);
    INIT_CRITICAL_SECTION(&G->mResourcesLock);
    INIT_CRITICAL_SECTION(&resource_creation_mode_lock);

    initialize_dll();

    // Chain through to the either the original DLL in the system, or to a proxy
    // DLL with the same interface, specified in the d3dx.ini file.
    // In the proxy load case, the load_library_redirect flag must be set to
    // zero, otherwise the proxy d3d11.dll will call back into us, and create
    // an infinite loop.

    if (G->CHAIN_DLL_PATH[0])
    {
        LOG_INFO("Proxy loading active, Forcing load_library_redirect=0\n");
        G->load_library_redirect = 0;

        wchar_t sys_dir[MAX_PATH] = { 0 };
        if (!GetModuleFileName(migoto_handle, sys_dir, MAX_PATH))
        {
            LOG_INFO("GetModuleFileName failed\n");
            double_beep_exit();
        }
        wcsrchr(sys_dir, L'\\')[1] = 0;
        wcscat(sys_dir, G->CHAIN_DLL_PATH);
        if (LogFile)
        {
            char path[MAX_PATH];
            wcstombs(path, sys_dir, MAX_PATH);
            LOG_INFO("trying to chain load %s\n", path);
        }
        hD3D11 = LoadLibrary(sys_dir);
        if (!hD3D11)
        {
            if (LogFile)
            {
                char path[MAX_PATH];
                wcstombs(path, G->CHAIN_DLL_PATH, MAX_PATH);
                LOG_INFO("load failed. Trying to chain load %s\n", path);
            }
            hD3D11 = LoadLibrary(G->CHAIN_DLL_PATH);
        }
        LOG_INFO("Proxy loading result: %p\n", hD3D11);
    }
    else
    {
        // We'll look for this in DLLMainHook to avoid callback to self.
        // Must remain all lower case to be matched in DLLMainHook.
        // We need the system d3d11 in order to find the original proc addresses.
        // We hook LoadLibraryExW, so we need to use that here.
        LOG_INFO("Trying to load original_d3d11.dll\n");
        hD3D11 = LoadLibraryEx(L"original_d3d11.dll", NULL, 0);
        if (hD3D11 == NULL)
        {
            wchar_t lib_path[MAX_PATH];
            LOG_INFO("*** LoadLibrary on original_d3d11.dll failed.\n");

            // Redirected load failed. Something (like Origin's IGO32.dll
            // hook in ntdll.dll LdrLoadDll) is interfering with our hook.
            // Fall back to using the full path after suppressing 3DMigoto's
            // redirect to make sure we don't get a reference to ourselves:

            LoadLibraryEx(L"SUPPRESS_3DMIGOTO_REDIRECT", NULL, 0);

            ret = GetSystemDirectoryW(lib_path, ARRAYSIZE(lib_path));
            if (ret != 0 && ret < ARRAYSIZE(lib_path))
            {
                wcscat_s(lib_path, MAX_PATH, L"\\d3d11.dll");
                LOG_INFO_W(L"Trying to load %ls\n", lib_path);
                hD3D11 = LoadLibraryEx(lib_path, NULL, 0);
            }
        }
    }
    if (hD3D11 == NULL)
    {
        LOG_INFO("*** LoadLibrary on original or chained d3d11.dll failed.\n");
        double_beep_exit();
    }

    _D3DKMTQueryAdapterInfo        = (tD3DKMTQueryAdapterInfo)GetProcAddress(hD3D11, "D3DKMTQueryAdapterInfo");
    _OpenAdapter10                 = (tOpenAdapter10)GetProcAddress(hD3D11, "OpenAdapter10");
    _OpenAdapter10_2               = (tOpenAdapter10_2)GetProcAddress(hD3D11, "OpenAdapter10_2");
    _D3D11CoreCreateDevice         = (tD3D11CoreCreateDevice)GetProcAddress(hD3D11, "D3D11CoreCreateDevice");
    _D3D11CoreCreateLayeredDevice  = (tD3D11CoreCreateLayeredDevice)GetProcAddress(hD3D11, "D3D11CoreCreateLayeredDevice");
    _D3D11CoreGetLayeredDeviceSize = (tD3D11CoreGetLayeredDeviceSize)GetProcAddress(hD3D11, "D3D11CoreGetLayeredDeviceSize");
    _D3D11CoreRegisterLayers       = (tD3D11CoreRegisterLayers)GetProcAddress(hD3D11, "D3D11CoreRegisterLayers");
    if (!_D3D11CreateDevice)
        _D3D11CreateDevice = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(hD3D11, "D3D11CreateDevice");
    if (!_D3D11CreateDeviceAndSwapChain)
        _D3D11CreateDeviceAndSwapChain = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain");
    _D3DKMTGetDeviceState     = (tD3DKMTGetDeviceState)GetProcAddress(hD3D11, "D3DKMTGetDeviceState");
    _D3DKMTOpenAdapterFromHdc = (tD3DKMTOpenAdapterFromHdc)GetProcAddress(hD3D11, "D3DKMTOpenAdapterFromHdc");
    _D3DKMTOpenResource       = (tD3DKMTOpenResource)GetProcAddress(hD3D11, "D3DKMTOpenResource");
    _D3DKMTQueryResourceInfo  = (tD3DKMTQueryResourceInfo)GetProcAddress(hD3D11, "D3DKMTQueryResourceInfo");

#ifdef NTDDI_WIN10
    _D3D11On12CreateDevice = (PFN_D3D11ON12_CREATE_DEVICE)GetProcAddress(hD3D11, "D3D11On12CreateDevice");
#endif
}

int WINAPI D3DKMTQueryAdapterInfo(
    _D3DKMT_QUERYADAPTERINFO* info)
{
    init_d3d11();
    LOG_INFO("D3DKMTQueryAdapterInfo called.\n");

    return (*_D3DKMTQueryAdapterInfo)(info);
}

int WINAPI OpenAdapter10(
    struct D3D10DDIARG_OPENADAPTER* adapter)
{
    init_d3d11();
    LOG_INFO("OpenAdapter10 called.\n");

    return (*_OpenAdapter10)(adapter);
}

int WINAPI OpenAdapter10_2(
    struct D3D10DDIARG_OPENADAPTER* adapter)
{
    init_d3d11();
    LOG_INFO("OpenAdapter10_2 called.\n");

    return (*_OpenAdapter10_2)(adapter);
}

int WINAPI D3D11CoreCreateDevice(
    __int32 a,
    int     b,
    int     c,
    LPCSTR  lpModuleName,
    int     e,
    int     f,
    int     g,
    int     h,
    int     i,
    int     j)
{
    init_d3d11();
    LOG_INFO("D3D11CoreCreateDevice called.\n");

    return (*_D3D11CoreCreateDevice)(a, b, c, lpModuleName, e, f, g, h, i, j);
}

HRESULT WINAPI D3D11CoreCreateLayeredDevice(
    const void* unknown0,
    DWORD       unknown1,
    const void* unknown2,
    REFIID      riid,
    void**      ppvObj)
{
    init_d3d11();
    LOG_INFO("D3D11CoreCreateLayeredDevice called.\n");

    return (*_D3D11CoreCreateLayeredDevice)(unknown0, unknown1, unknown2, riid, ppvObj);
}

SIZE_T WINAPI D3D11CoreGetLayeredDeviceSize(
    const void* unknown0,
    DWORD       unknown1)
{
    init_d3d11();
    LOG_INFO("D3D11CoreGetLayeredDeviceSize called.\n");

    return (*_D3D11CoreGetLayeredDeviceSize)(unknown0, unknown1);
}

HRESULT WINAPI D3D11CoreRegisterLayers(
    const void* unknown0,
    DWORD       unknown1)
{
    init_d3d11();
    LOG_INFO("D3D11CoreRegisterLayers called.\n");

    return (*_D3D11CoreRegisterLayers)(unknown0, unknown1);
}

int WINAPI D3DKMTGetDeviceState(
    int a)
{
    init_d3d11();
    LOG_INFO("D3DKMTGetDeviceState called.\n");

    return (*_D3DKMTGetDeviceState)(a);
}

int WINAPI D3DKMTOpenAdapterFromHdc(
    int a)
{
    init_d3d11();
    LOG_INFO("D3DKMTOpenAdapterFromHdc called.\n");

    return (*_D3DKMTOpenAdapterFromHdc)(a);
}

int WINAPI D3DKMTOpenResource(
    int a)
{
    init_d3d11();
    LOG_INFO("D3DKMTOpenResource called.\n");

    return (*_D3DKMTOpenResource)(a);
}

int WINAPI D3DKMTQueryResourceInfo(
    int a)
{
    init_d3d11();
    LOG_INFO("D3DKMTQueryResourceInfo called.\n");

    return (*_D3DKMTQueryResourceInfo)(a);
}

// -----------------------------------------------------------------------------------------------

// Only where _DEBUG_LAYER=1, typically debug builds.  Set the flags to
// enable GPU debugging.  This makes the GPU dramatically slower but can
// catch bogus setup or bad calls to the GPU environment.
// The prevent threading optimizations can help show if we have a multi-threading problem.

static UINT enable_debug_flags(
    UINT flags)
{
    flags |= D3D11_CREATE_DEVICE_DEBUG;
    flags |= D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;
    LOG_INFO("  D3D11CreateDevice _DEBUG_LAYER flags set: %#x\n", flags);

    return flags;
}

// Only where _DEBUG_LAYER=1.  This enables the debug layer for the GPU
// itself, which will show GPU errors, warnings, and leaks in the console output.
// This call shows that the layer is active, and the initial LiveObjectState.
// And enable debugger breaks for errors that we might be introducing.

static void show_debug_info(
    ID3D11Device* orig_device)
{
    ID3D11Debug* d3d_debug = nullptr;
    if (orig_device != nullptr)
    {
        if (SUCCEEDED(orig_device->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3d_debug)))
        {
            ID3D11InfoQueue* d3d_info_queue = nullptr;

            if (SUCCEEDED(d3d_debug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3d_info_queue)))
            {
                d3d_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
                d3d_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
            }
        }
        d3d_debug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL);
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

static bool force_dx11(
    D3D_FEATURE_LEVEL* feature_levels)
{
    if (!feature_levels)
    {
        LOG_INFO("->Feature level null, defaults to D3D_FEATURE_LEVEL_11_0.\n");
        return false;
    }

    if (G->enable_create_device == 1)
    {
        LOG_INFO("->Feature level allowed through unchanged: %#x\n", *feature_levels);
        return false;
    }
    if (G->enable_create_device == 2)
    {
        *feature_levels = D3D_FEATURE_LEVEL_11_0;

        LOG_INFO("->Feature level forced to 11.0: %#x\n", *feature_levels);
        return false;
    }

    // Error out if we aren't looking for D3D_FEATURE_LEVEL_11_0.
    if (*feature_levels != D3D_FEATURE_LEVEL_11_0)
    {
        LOG_INFO("->Feature level != 11.0: %#x, returning E_INVALIDARG\n", *feature_levels);
        return true;
    }

    return false;
}

static HackerDevice* wrap_d3d11_device_and_context(
    ID3D11Device**        ppDevice,
    ID3D11DeviceContext** ppImmediateContext)
{
    // Optional parameters means these might be null.
    ID3D11Device*        ret_device  = ppDevice ? *ppDevice : nullptr;
    ID3D11DeviceContext* ret_context = ppImmediateContext ? *ppImmediateContext : nullptr;

    // We now want to always upconvert to ID3D11Device1 and ID3D11DeviceContext1,
    // and will only use the downlevel objects if we get an error on QueryInterface.
    ID3D11Device1*        orig_device1  = nullptr;
    ID3D11DeviceContext1* orig_context1 = nullptr;
    HRESULT               res;
    if (ret_device != nullptr)
    {
        res = ret_device->QueryInterface(IID_PPV_ARGS(&orig_device1));
        LOG_INFO("  QueryInterface(ID3D11Device1) returned result = %x, device1 handle = %p\n", res, orig_device1);
        if (SUCCEEDED(res))
            ret_device->Release();
        else
            orig_device1 = static_cast<ID3D11Device1*>(ret_device);
    }
    if (ret_context != nullptr)
    {
        res = ret_context->QueryInterface(IID_PPV_ARGS(&orig_context1));
        LOG_INFO("  QueryInterface(ID3D11DeviceContext1) returned result = %x, context1 handle = %p\n", res, orig_context1);
        if (SUCCEEDED(res))
            ret_context->Release();
        else
            orig_context1 = static_cast<ID3D11DeviceContext1*>(ret_context);
    }

    // Create a wrapped version of the original device to return to the game.
    HackerDevice* device_wrap = nullptr;
    if (orig_device1 != nullptr)
    {
        device_wrap = new HackerDevice(orig_device1, orig_context1);

        if (G->enable_hooks & EnableHooks::DEVICE)
            device_wrap->HookDevice();
        else
            *ppDevice = device_wrap;
        LOG_INFO("  HackerDevice %p created to wrap %p\n", device_wrap, orig_device1);

        // Add the HackerDevice to the DirectX device's private data,
        // which we can use as a last resort to allow us to always find
        // our HackerDevice even if we are handed an unwrapped IUnknown
        // that violates the COM identity rule, as happens with ReShade
        // in certain games (e.g. Resident Evil 2 remake). Not using
        // SetPrivateDataInterface for now because I suspect that will
        // screw up refcounting (though it might be worthwhile using it
        // to ensure we always get notification of device release):
        orig_device1->SetPrivateData(IID_HackerDevice, sizeof(HackerDevice*), &device_wrap);
    }

    // Create a wrapped version of the original context to return to the game.
    HackerContext* context_wrap = nullptr;
    if (orig_context1 != nullptr)
    {
        context_wrap = HackerContextFactory(orig_device1, orig_context1);

        if (G->enable_hooks & EnableHooks::IMMEDIATE_CONTEXT)
            context_wrap->HookContext();
        else
            *ppImmediateContext = context_wrap;
        LOG_INFO("  HackerContext %p created to wrap %p\n", context_wrap, orig_context1);
    }

    // Let each of the new Hacker objects know about the other, needed for unusual
    // calls in the Hacker objects where we want to return the Hacker versions.
    if (device_wrap != nullptr)
        device_wrap->SetHackerContext(context_wrap);
    if (context_wrap != nullptr)
        context_wrap->SetHackerDevice(device_wrap);

    // With all the interacting objects set up, we can now safely finish the HackerDevice init.
    if (device_wrap != nullptr)
        device_wrap->Create3DMigotoResources();
    if (context_wrap != nullptr)
    {
        context_wrap->Bind3DMigotoResources();
        if (!G->constants_run)
            context_wrap->InitIniParams();
    }

    LOG_INFO("-> device handle = %p, device wrapper = %p, context handle = %p, context wrapper = %p\n", orig_device1, device_wrap, orig_context1, context_wrap);

    return device_wrap;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// For creating the device, we need to call the original D3D11CreateDevice in order to initialize
// Direct3D, and collect the original Device and original Context.  Both of those will be handed
// off to the wrapped HackerDevice and HackerContext objects, so they can call out to the originals
// as needed.  Both Hacker objects need access to both Context and Device, so since both are
// created here, it's easy enough to provide them upon instantiation.
//
// Now intended to be fully null safe- as games seem to have a lot of variance.
//
// 1-8-18: Switching tacks to always return ID3D11Device1 objects, which are the
// platform_update required type.  Since it's a superset, we can in general just
// the reference as a normal ID3D11Device.
// In the no platform_update case, the mOrigDevice1 will actually be an ID3D11Device.
//
// This is the actual routine that is wrapping the d3d11.dll function. Because
// it's a wrapper, and not a hook, we can have scenarios where another tool
// will hook this function. In the past we attempted to call this from
// CreateDeviceAndSwapChain to simplify our init paths, however doing so
// interacted badly with SpecialK, which has a similar init path simplification
// where they implement CreateDevice by calling CreateDeviceAndSwapChain,
// creating an infinite recursive loop between the two. Previously we resolved
// that via the use of internal-only "unhookable" routines, however our
// CreateDeviceAndSwapChain change had also inconsistently broken the Steam
// overlay so we have now reverted all of that and once again implement
// CreateDeviceAndSwapChain and CreateDevice separately, with utility routines
// to reduce duplicated code between the two.
//
// The takeaway lessons are:
// 1. Don't reimplement DirectX routines in terms of others as we may break
//    assumptions other tools rely on.
// 2. Each routine called from the game should call through to the same routine
//    in DirectX.
// 3. Simplify similar routines via utility functions or templates, not by
//    calling one from the other, if the routines may have been hooked by an
//    external tool (DLL exports or public COM methods).

HRESULT WINAPI D3D11CreateDevice(
    IDXGIAdapter*            pAdapter,
    D3D_DRIVER_TYPE          DriverType,
    HMODULE                  Software,
    UINT                     Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels,
    UINT                     FeatureLevels,
    UINT                     SDKVersion,
    ID3D11Device**           ppDevice,
    D3D_FEATURE_LEVEL*       pFeatureLevel,
    ID3D11DeviceContext**    ppImmediateContext)
{
    if (get_tls()->hooking_quirk_protection)
    {
        LOG_INFO("Hooking Quirk: Unexpected call back into D3D11CreateDevice, passing through\n");
        // No confirmed cases
        return _D3D11CreateDevice(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
    }

    init_d3d11();

    LOG_INFO("\n\n *** D3D11CreateDevice called with\n");
    LOG_INFO("    pAdapter = %p\n", pAdapter);
    LOG_INFO("    Flags = %#x\n", Flags);
    LOG_INFO("    pFeatureLevels = %#x\n", pFeatureLevels ? *pFeatureLevels : 0);
    LOG_INFO("    FeatureLevels = %d\n", FeatureLevels);
    LOG_INFO("    ppDevice = %p\n", ppDevice);
    LOG_INFO("    pFeatureLevel = %#x\n", pFeatureLevel ? *pFeatureLevel : 0);
    LOG_INFO("    ppImmediateContext = %p\n", ppImmediateContext);

    if (force_dx11(const_cast<D3D_FEATURE_LEVEL*>(pFeatureLevels)))
        return E_INVALIDARG;

#if _DEBUG_LAYER
    Flags = enable_debug_flags(Flags);
#endif

    get_tls()->hooking_quirk_protection = true;
    HRESULT ret                         = (*_D3D11CreateDevice)(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
    get_tls()->hooking_quirk_protection = false;

    if (FAILED(ret))
    {
        LOG_INFO("->failed with HRESULT=%x\n", ret);
        return ret;
    }

    // Optional parameters means these might be null.
    ID3D11Device*        ret_device  = ppDevice ? *ppDevice : nullptr;
    ID3D11DeviceContext* ret_context = ppImmediateContext ? *ppImmediateContext : nullptr;

    LOG_INFO("  D3D11CreateDevice returned device handle = %p, context handle = %p\n", ret_device, ret_context);
    analyse_iunknown(ret_device);
    analyse_iunknown(ret_context);

#if _DEBUG_LAYER
    show_debug_info(ret_device);
#endif

    wrap_d3d11_device_and_context(ppDevice, ppImmediateContext);

    LOG_INFO("->D3D11CreateDevice result = %x\n", ret);

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
//
// 2019-06-07:
// This procedure has now gone through several incarnations and obsolete
// comments have now been removed. The current code is actually more similar to
// much older versions of the code than other interim versions, except using
// utility routines to reduce duplicated code between this and CreateDevice /
// CreateSwapChain. It is important to understand why some of the interim
// versions are no longer in use so we don't repeat the same mistakes:
//
// At one point we had D3D11CreateDeviceAndSwapChain reimplemented by calling
// the above D3D11CreateDevice and IDXGIFactory::CreateSwapChain with the goal
// of reducing our init path complexity. This caused issues with SpecialK and
// the Steam overlay because it broke assumptions that they had made on how
// DirectX init works. In the case of SpecialK it reimplemented
// D3D11CreateDevice by calling D3D11CreateDeviceAndSwapChain, which created an
// infinite recursion loop between the two. In the case of Steam this
// reimplemetation was able to bypass their hook on CreateSwapChain (depending
// on hooking order) by calling the Deviare trampoline to go through to the
// original function rather than calling the hooked CreateSwapChain.
//
// At some point we had an incarnation of this routine that depended on DirectX
// internally calling CreateSwapChain and triggering our hook on that routine.
// This caused all sorts of trouble since it handed our hook a DirectX device
// while we expected a HackerDevice. It's worth noting that this code path now
// technically exists once again, but we have since introduced detection for
// this particular quirk and our CreateSwapChain hook will notice this and pass
// the call straight through to DirectX without the rest of the processing that
// call would usually do, and we instead wrap the swap chain from here.

HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
    IDXGIAdapter*            pAdapter,
    D3D_DRIVER_TYPE          DriverType,
    HMODULE                  Software,
    UINT                     Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels,
    UINT                     FeatureLevels,
    UINT                     SDKVersion,
    DXGI_SWAP_CHAIN_DESC*    pSwapChainDesc,
    IDXGISwapChain**         ppSwapChain,
    ID3D11Device**           ppDevice,
    D3D_FEATURE_LEVEL*       pFeatureLevel,
    ID3D11DeviceContext**    ppImmediateContext)
{
    if (get_tls()->hooking_quirk_protection)
    {
        LOG_INFO("Hooking Quirk: Unexpected call back into D3D11CreateDeviceAndSwapChain, passing through\n");
        // Known case: DirectX implements D3D11CreateDevice by calling
        //             D3D11CreateDeviceAndSwapChain, triggering this
        //             if we call the former and have hooked the later.
        return _D3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
    }

    DXGI_SWAP_CHAIN_DESC orig_swap_chain_desc;

    init_d3d11();

    LOG_INFO("\n\n *** D3D11CreateDeviceAndSwapChain called with\n");
    LOG_INFO("    pAdapter = %p\n", pAdapter);
    LOG_INFO("    Flags = %#x\n", Flags);
    LOG_INFO("    pFeatureLevels = %#x\n", pFeatureLevels ? *pFeatureLevels : 0);
    LOG_INFO("    FeatureLevels = %d\n", FeatureLevels);
    LOG_INFO("    pSwapChainDesc = %p\n", pSwapChainDesc);
    LOG_INFO("    ppSwapChain = %p\n", ppSwapChain);
    LOG_INFO("    ppDevice = %p\n", ppDevice);
    LOG_INFO("    pFeatureLevel = %#x\n", pFeatureLevel ? *pFeatureLevel : 0);
    LOG_INFO("    ppImmediateContext = %p\n", ppImmediateContext);

    if (force_dx11(const_cast<D3D_FEATURE_LEVEL*>(pFeatureLevels)))
        return E_INVALIDARG;

#if _DEBUG_LAYER
    Flags = enable_debug_flags(Flags);
#endif

    override_swap_chain(pSwapChainDesc, &orig_swap_chain_desc);

    get_tls()->hooking_quirk_protection = true;
    HRESULT ret                         = (*_D3D11CreateDeviceAndSwapChain)(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
    get_tls()->hooking_quirk_protection = false;

    if (FAILED(ret))
    {
        LOG_INFO("->failed with HRESULT=%x\n", ret);
        return ret;
    }

    // Optional parameters means these might be null.
    ID3D11Device*        ret_device     = ppDevice ? *ppDevice : nullptr;
    ID3D11DeviceContext* ret_context    = ppImmediateContext ? *ppImmediateContext : nullptr;
    IDXGISwapChain*      ret_swap_chain = ppSwapChain ? *ppSwapChain : nullptr;

    LOG_INFO("  D3D11CreateDeviceAndSwapChain returned device handle = %p, context handle = %p, swap chain = %p\n", ret_device, ret_context, ret_swap_chain);
    analyse_iunknown(ret_device);
    analyse_iunknown(ret_context);
    analyse_iunknown(ret_swap_chain);

#if _DEBUG_LAYER
    show_debug_info(ret_device);
#endif

    HackerDevice* device_wrap = wrap_d3d11_device_and_context(ppDevice, ppImmediateContext);
    wrap_swap_chain(device_wrap, ppSwapChain, pSwapChainDesc, &orig_swap_chain_desc);

    LOG_INFO("->D3D11CreateDeviceAndSwapChain result = %x\n", ret);

    return ret;
}

// -----------------------------------------------------------------------------------------------
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

static HMODULE nv_dll;
static bool    nvapi_failed = false;
typedef NvAPI_Status*(__cdecl* nvapi_QueryInterfaceType)(unsigned int offset);
static nvapi_QueryInterfaceType nvapi_QueryInterfacePtr;

void nvapi_override()
{
    static bool warned = false;

    if (nvapi_failed)
        return;

    if (!nv_dll)
    {
        // Use GetModuleHandleEx instead of GetModuleHandle to bump the
        // refcount on our nvapi wrapper since we are storing a
        // function pointer from the library. This is important, since
        // if we aren't running on an nvidia system the nvapi static
        // library will unload the dynamic library, which would lead to
        // a crash when we later try to call the NvAPI_QueryInterface
        // function pointer.
        GetModuleHandleEx(0, L"nvapi64.dll", &nv_dll);
        if (!nv_dll)
        {
            GetModuleHandleEx(0, L"nvapi.dll", &nv_dll);
        }
        if (!nv_dll)
        {
            LOG_INFO("Can't get nvapi handle\n");
            nvapi_failed = true;
            return;
        }
    }
    if (!nvapi_QueryInterfacePtr)
    {
        nvapi_QueryInterfacePtr = (nvapi_QueryInterfaceType)GetProcAddress(nv_dll, "nvapi_QueryInterface");
        LOG_DEBUG("nvapi_QueryInterfacePtr @ 0x%p\n", nvapi_QueryInterfacePtr);
        if (!nvapi_QueryInterfacePtr)
        {
            LOG_INFO("Unable to call NvAPI_QueryInterface\n");
            nvapi_failed = true;
            return;
        }
    }

    // One shot, override custom settings.
    intptr_t ret = (intptr_t)nvapi_QueryInterfacePtr(0xb03bb03b);
    if (!warned && (ret & 0xffffffff) != 0xeecc34ab)
    {
        LOG_INFO("  overriding NVAPI wrapper failed.\n");
        warned = true;
    }
}

// -----------------------------------------------------------------------------------------------
// This is our hook for LoadLibraryExW, handling the loading of our original_* libraries.
//
// The hooks are actually installed during load time in DLLMainHook, but called here
// whenever the game or system calls LoadLibraryExW.  These are here because at this
// point in the runtime, it is OK to do normal calls like LoadLibrary, and logging
// is available.  This is normal runtime.

static HMODULE replace_on_match(
    LPCWSTR lpLibFileName,
    HANDLE  hFile,
    DWORD   dwFlags,
    LPCWSTR our_name,
    LPCWSTR library)
{
    WCHAR full_path[MAX_PATH];
    UINT  ret;

    // We can use System32 for all cases, because it will be properly rerouted
    // to SysWow64 by LoadLibraryEx itself.

    ret = GetSystemDirectoryW(full_path, ARRAYSIZE(full_path));
    if (ret == 0 || ret >= ARRAYSIZE(full_path))
        return NULL;
    wcscat_s(full_path, MAX_PATH, L"\\");
    wcscat_s(full_path, MAX_PATH, library);

    // Bypass the known expected call from our wrapped d3d11 & nvapi, where it needs
    // to call to the system to get APIs. This is a bit of a hack, but if the string
    // comes in as original_d3d11/nvapi/nvapi64, that's from us, and needs to switch
    // to the real one. The test string should have no path attached.

    if (_wcsicmp(lpLibFileName, our_name) == 0)
    {
        LOG_INFO_W(L"hooked_LoadLibraryExW switching to original dll: %s to %s.\n", lpLibFileName, full_path);

        return fnOrigLoadLibraryExW(full_path, hFile, dwFlags);
    }

    // For this case, we want to see if it's the game loading d3d11 or nvapi directly
    // from the system directory, and redirect it to the game folder if so, by stripping
    // the system path. This is to be case insensitive as we don't know if NVidia will
    // change that and otherwise break it it with a driver upgrade.

    if (_wcsicmp(lpLibFileName, full_path) == 0)
    {
        // If we are loaded via injection we should load from directory
        // where the 3DMigoto DLL resides rather than the game directory.
        // However, if the request is for nvapi and that file is missing
        // attempting the LoadLibrary by the abolute path will fail. So,
        // try by the absolute path first, then fall back to just the
        // library name.
        if (GetModuleFileName(migoto_handle, full_path, MAX_PATH))
        {
            wcsrchr(full_path, L'\\')[1] = '\0';
            wcscat(full_path, library);

            LOG_INFO_W(L"Replaced hooked_LoadLibraryExW for: %s to %s.\n", lpLibFileName, full_path);

            HMODULE ret = fnOrigLoadLibraryExW(full_path, hFile, dwFlags);
            if (ret)
                return ret;
        }

        LOG_INFO_W(L"Replaced hooked_LoadLibraryExW fallback for: %s to %s.\n", lpLibFileName, library);

        return fnOrigLoadLibraryExW(library, hFile, dwFlags);
    }

    return NULL;
}

// Function called for every LoadLibraryExW call once we have hooked it.
// We want to look for overrides to System32 that we can circumvent.  This only happens
// in the current process, not system wide.
//
// We need to do two things here.  First, we need to bypass all calls that go
// directly to the System32 folder, because that will circumvent our wrapping
// of the d3d11 and nvapi APIs. The nvapi itself does this specifically as fake
// security to avoid proxy DLLs like us.
// Second, because we are now forcing all LoadLibraryExW calls back to the game
// folder, we need somehow to allow us access to the original dlls so that we can
// get the original proc addresses to call.  We do this with the original_* names
// passed in to this routine.
//
// There three use cases:
// x32 game on x32 OS
//     LoadLibraryExW("C:\Windows\system32\d3d11.dll", NULL, 0)
//     LoadLibraryExW("C:\Windows\system32\nvapi.dll", NULL, 0)
// x64 game on x64 OS
//     LoadLibraryExW("C:\Windows\system32\d3d11.dll", NULL, 0)
//     LoadLibraryExW("C:\Windows\system32\nvapi64.dll", NULL, 0)
// x32 game on x64 OS
//     LoadLibraryExW("C:\Windows\SysWOW64\d3d11.dll", NULL, 0)
//     LoadLibraryExW("C:\Windows\SysWOW64\nvapi.dll", NULL, 0)
//
// To be general and simplify the init, we are going to specifically do the bypass
// for all variants, even though we only know of this happening on x64 games.
//
// An important thing to remember here is that System32 is automatically rerouted
// to SysWow64 by the OS as necessary, so we can use System32 in all cases.
//
// It's not clear if we should also hook LoadLibraryW, but we don't have examples
// where we need that yet.

// The storage for the original routine so we can call through.

HMODULE(__stdcall* fnOrigLoadLibraryExW)
(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags) = LoadLibraryExW;

HMODULE __stdcall hooked_LoadLibraryExW(
    LPCWSTR lpLibFileName,
    HANDLE  hFile,
    DWORD   dwFlags)
{
    HMODULE     module;
    static bool hook_enabled = true;

    LOG_DEBUG_W(L"   hooked_LoadLibraryExW load: %s.\n", lpLibFileName);

    if (_wcsicmp(lpLibFileName, L"SUPPRESS_3DMIGOTO_REDIRECT") == 0)
    {
        // Something (like Origin's IGO32.dll hook in ntdll.dll
        // LdrLoadDll) is interfering with our hook and the caller is
        // about to attempt the load again using the full path. Disable
        // our redirect for the next call to make sure we don't give
        // them a reference to themselves. Subsequent calls will be
        // armed again in case we still need the redirect.
        hook_enabled = false;
        return NULL;
    }

    // Only do these overrides if they are specified in the d3dx.ini file.
    //  load_library_redirect=0 for off, allowing all through unchanged.
    //  load_library_redirect=1 for nvapi.dll override only, forced to game folder.
    //  load_library_redirect=2 for both d3d11.dll and nvapi.dll forced to game folder.
    // This flag can be set by the proxy loading, because it must be off in that case.
    if (hook_enabled)
    {
        if (G->load_library_redirect > 1)
        {
            module = replace_on_match(lpLibFileName, hFile, dwFlags, L"original_d3d11.dll", L"d3d11.dll");
            if (module)
                return module;
        }

        if (G->load_library_redirect > 0)
        {
            module = replace_on_match(lpLibFileName, hFile, dwFlags, L"original_nvapi64.dll", L"nvapi64.dll");
            if (module)
                return module;

            module = replace_on_match(lpLibFileName, hFile, dwFlags, L"original_nvapi.dll", L"nvapi.dll");
            if (module)
                return module;
        }
    }
    else
        hook_enabled = true;

    // Normal unchanged case.
    return fnOrigLoadLibraryExW(lpLibFileName, hFile, dwFlags);
}

// This callback proc allows us to hook into any application we need. It is
// installed from an external program (of matching bit size) with code like:
//
// HMODULE module = LoadLibraryA("d3d11.dll");
// HOOKPROC fn = (HOOKPROC)GetProcAddress(module, "CBTProc");
// HHOOK hook = SetWindowsHookEx(WH_CBT, fn, module, 0);
// ... launch the game ...
// UnhookWindowsHookEx(hook);
//
// If it loads us named d3d11.dll it seems we may actually be able to just work
// with no further heroics (confirmed in Unity games at least), but beware that
// we will be hooked into *every* application, including core OS components,
// which we may want to bail from in DllMain.
//
// This method can even get us into Windows Store apps, if the external program
// installing the hook has the permissions to do so (UIAccess=true, digitally
// signed by a trusted root certificate, run from Program Files), but beware
// that by itself is not enough for us to be functional inside a Windows Store
// app - we don't automatically intercept the real d3d11.dll, and any attempt
// to breach the container (say, by accessing one of our files outside of where
// the container allows) may result in us being mercilessly killed.
LRESULT CALLBACK CBTProc(
    int    nCode,
    WPARAM wParam,
    LPARAM lParam)
{
    return CallNextHookEx(0, nCode, wParam, lParam);
}
