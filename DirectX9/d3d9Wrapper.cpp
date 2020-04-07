#include "Main.h"
#include <Shlobj.h>
#include <ctime>
#include "log.h"
#include "Hunting.h"
#include "Override.h"
#include "IniHandler.h"
#include "nvprofile.h"
#include "cursor.h" // For InstallSetWindowPosHook

ThreadSafePointerSet D3D9Wrapper::IDirect3DSwapChain9::m_List;
ThreadSafePointerSet D3D9Wrapper::IDirect3DDevice9::m_List;
ThreadSafePointerSet D3D9Wrapper::IDirect3D9::m_List;
ThreadSafePointerSet D3D9Wrapper::IDirect3DSurface9::m_List;
ThreadSafePointerSet D3D9Wrapper::IDirect3DVertexDeclaration9::m_List;
ThreadSafePointerSet D3D9Wrapper::IDirect3DTexture9::m_List;
ThreadSafePointerSet D3D9Wrapper::IDirect3DVertexBuffer9::m_List;
ThreadSafePointerSet D3D9Wrapper::IDirect3DIndexBuffer9::m_List;
ThreadSafePointerSet D3D9Wrapper::IDirect3DQuery9::m_List;
ThreadSafePointerSet D3D9Wrapper::IDirect3DVertexShader9::m_List;
ThreadSafePointerSet D3D9Wrapper::IDirect3DPixelShader9::m_List;

ThreadSafePointerSet D3D9Wrapper::IDirect3DCubeTexture9::m_List;
ThreadSafePointerSet D3D9Wrapper::IDirect3DVolumeTexture9::m_List;
ThreadSafePointerSet D3D9Wrapper::IDirect3DVolume9::m_List;

ThreadSafePointerSet D3D9Wrapper::IDirect3DStateBlock9::m_List;

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
static HMODULE hD3D;
FILE *LogFile = 0;
bool gLogDebug = false;

#pragma data_seg("multi_process_shared_globals")
__declspec (align(8)) volatile LONG process_count = 0;
__declspec (align(8)) volatile LONG shared_game_internal_width = 1;
__declspec (align(8)) volatile LONG shared_game_internal_height = 1;
__declspec (align(8)) volatile LONG shared_cursor_update_required = 1;
HWND shared_hWnd = NULL;

#pragma data_seg()

struct SwapChainInfo
{
	int width, height;
};
static bool InitializeDLL()
{
	if (G->gInitialized)
		return true;

		LONG count = InterlockedIncrement(&process_count);
		G->process_index = count - 1;
		LoadConfigFile();
		// Preload OUR nvapi before we call init because we need some of our calls.
#if(_WIN64)
#define NVAPI_DLL L"nvapi64.dll"
#else
#define NVAPI_DLL L"nvapi.dll"
#endif

		LoadLibrary(NVAPI_DLL);
//
		NvAPI_ShortString errorMessage;
		NvAPI_Status status;
//
//		// Tell our nvapi.dll that it's us calling, and it's OK.
		NvAPIOverride();
		status = NvAPI_Initialize();
		if (status != NVAPI_OK)
		{
			NvAPI_GetErrorMessage(status, errorMessage);
			LogInfo("  NvAPI_Initialize failed: %s\n", errorMessage);
			return false;
		}
		if (G->gTrackNvAPIStereoActive)
			NvAPIEnableStereoActiveTracking();
		if (G->gTrackNvAPIConvergence)
			NvAPIEnableConvergenceTracking();
		if (G->gTrackNvAPISeparation)
			NvAPIEnableSeparationTracking();
		if (G->gTrackNvAPIEyeSeparation)
			NvAPIEnableEyeSeparationTracking();

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

	LogInfo("\n***  D3D9 DLL successfully initialized.  ***\n\n");
	return true;
}

void InitD39()
{
	UINT ret;

	if (hD3D) return;
	InitializeCriticalSection(&G->mCriticalSection);
	InitializeDLL();

	// Chain through to the either the original DLL in the system, or to a proxy
	// DLL with the same interface, specified in the d3dx.ini file.
	// In the proxy load case, the load_library_redirect flag must be set to
	// zero, otherwise the proxy d3d11.dll will call back into us, and create
	// an infinite loop.

	if (G->CHAIN_DLL_PATH[0])
	{
		LogInfo("Proxy loading active, Forcing load_library_redirect=0\n");
		G->load_library_redirect = 0;

		wchar_t sysDir[MAX_PATH] = { 0 };
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
		hD3D = LoadLibrary(sysDir);
		if (!hD3D)
		{
			if (LogFile)
			{
				char path[MAX_PATH];
				wcstombs(path, G->CHAIN_DLL_PATH, MAX_PATH);
				LogInfo("load failed. Trying to chain load %s\n", path);
			}
			hD3D = LoadLibrary(G->CHAIN_DLL_PATH);
		}
		LogInfo("Proxy loading result: %p\n", hD3D);
	}
	else
	{
		// We'll look for this in DLLMainHook to avoid callback to self.
		// Must remain all lower case to be matched in DLLMainHook.
		// We need the system d3d11 in order to find the original proc addresses.
		// We hook LoadLibraryExW, so we need to use that here.
		LogInfo("Trying to load original_d3d9.dll\n");
		hD3D = LoadLibraryEx(L"original_d3d9.dll", NULL, 0);
		if (hD3D == NULL)
		{
			wchar_t libPath[MAX_PATH];
			LogInfo("*** LoadLibrary on original_d3d9.dll failed.\n");

			// Redirected load failed. Something (like Origin's IGO32.dll
			// hook in ntdll.dll LdrLoadDll) is interfering with our hook.
			// Fall back to using the full path after suppressing 3DMigoto's
			// redirect to make sure we don't get a reference to ourselves:

			LoadLibraryEx(L"SUPPRESS_3DMIGOTO_REDIRECT", NULL, 0);

			ret = GetSystemDirectoryW(libPath, ARRAYSIZE(libPath));
			if (ret != 0 && ret < ARRAYSIZE(libPath)) {
				wcscat_s(libPath, MAX_PATH, L"\\d3d9.dll");
				LogInfoW(L"Trying to load %ls\n", libPath);
				hD3D = LoadLibraryEx(libPath, NULL, 0);
			}
		}
	}
	if (hD3D == NULL)
	{
		LogInfo("*** LoadLibrary on original or chained d3d9.dll failed.\n");
		DoubleBeepExit();
	}

}
void DestroyDLL()
{
	if (LogFile)
	{
		LogInfo("Destroying DLL...\n");
		fclose(LogFile);
	}
}

int WINAPI D3DPERF_BeginEvent(DWORD col, LPCWSTR wszName)
{
	LogInfo("D3DPERF_BeginEvent called\n");

	D3D9Wrapper::D3DPERF_BeginEvent call = (D3D9Wrapper::D3DPERF_BeginEvent)GetProcAddress(hD3D, "D3DPERF_BeginEvent");
	return (*call)(col, wszName);
}

int WINAPI D3DPERF_EndEvent()
{
	LogInfo("D3DPERF_EndEvent called\n");

	D3D9Wrapper::D3DPERF_EndEvent call = (D3D9Wrapper::D3DPERF_EndEvent)GetProcAddress(hD3D, "D3DPERF_EndEvent");
	return (*call)();
}

DWORD WINAPI D3DPERF_GetStatus()
{
	LogInfo("D3DPERF_GetStatus called\n");

	D3D9Wrapper::D3DPERF_GetStatus call = (D3D9Wrapper::D3DPERF_GetStatus)GetProcAddress(hD3D, "D3DPERF_GetStatus");
	return (*call)();
}

BOOL WINAPI D3DPERF_QueryRepeatFrame()
{
	LogInfo("D3DPERF_QueryRepeatFrame called\n");

	D3D9Wrapper::D3DPERF_QueryRepeatFrame call = (D3D9Wrapper::D3DPERF_QueryRepeatFrame)GetProcAddress(hD3D, "D3DPERF_QueryRepeatFrame");
	return (*call)();
}

void WINAPI D3DPERF_SetMarker(::D3DCOLOR color, LPCWSTR name)
{
	LogInfo("D3DPERF_SetMarker called\n");

	D3D9Wrapper::D3DPERF_SetMarker call = (D3D9Wrapper::D3DPERF_SetMarker)GetProcAddress(hD3D, "D3DPERF_SetMarker");
	(*call)(color, name);
}

void WINAPI D3DPERF_SetOptions(DWORD options)
{
	LogInfo("D3DPERF_SetOptions called\n");

	D3D9Wrapper::D3DPERF_SetOptions call = (D3D9Wrapper::D3DPERF_SetOptions)GetProcAddress(hD3D, "D3DPERF_SetOptions");
	(*call)(options);
}

void WINAPI D3DPERF_SetRegion(::D3DCOLOR color, LPCWSTR name)
{
	LogInfo("D3DPERF_SetRegion called\n");

	D3D9Wrapper::D3DPERF_SetRegion call = (D3D9Wrapper::D3DPERF_SetRegion)GetProcAddress(hD3D, "D3DPERF_SetRegion");
	(*call)(color, name);
}

void WINAPI DebugSetLevel(int a1, int a2)
{
	LogInfo("DebugSetLevel called\n");

	D3D9Wrapper::DebugSetLevel call = (D3D9Wrapper::DebugSetLevel)GetProcAddress(hD3D, "DebugSetLevel");
	(*call)(a1, a2);
}

//#ifdef __cplusplus
//extern "C" {
//#endif
	//void WINAPI DebugSetMute(int a)
	//{
	//	LogInfo("DebugSetMute called\n");

	//	D3D9Wrapper::DebugSetMute call = (D3D9Wrapper::DebugSetMute)GetProcAddress(hD3D, "DebugSetMute");
	//	(*call)(a);
	//}

//#ifdef __cplusplus
//}
//#endif
void *WINAPI Direct3DShaderValidatorCreate9()
{
	LogInfo("Direct3DShaderValidatorCreate9 called\n");

	D3D9Wrapper::Direct3DShaderValidatorCreate9 call = (D3D9Wrapper::Direct3DShaderValidatorCreate9)GetProcAddress(hD3D, "Direct3DShaderValidatorCreate9");
	return (*call)();
}

void WINAPI PSGPError(void *D3DFE_PROCESSVERTICES, int PSGPERRORID, unsigned int a)
{
	LogInfo("PSGPError called\n");

	D3D9Wrapper::PSGPError call = (D3D9Wrapper::PSGPError)GetProcAddress(hD3D, "PSGPError");
	(*call)(D3DFE_PROCESSVERTICES, PSGPERRORID, a);
}

void WINAPI PSGPSampleTexture(void *D3DFE_PROCESSVERTICES, unsigned int a, float (* const b)[4], unsigned int c, float (* const d)[4])
{
	LogInfo("PSGPSampleTexture called\n");

	D3D9Wrapper::PSGPSampleTexture call = (D3D9Wrapper::PSGPSampleTexture)GetProcAddress(hD3D, "PSGPSampleTexture");
	(*call)(D3DFE_PROCESSVERTICES, a, b, c, d);
}
bool D3D9Wrapper::IDirect3DUnknown::QueryInterface_DXGI_Callback(REFIID riid, void ** ppvObj, HRESULT * result)
{
	// FIXME: These should be defined in a header somewhere
	IID m1 = { 0x017b2e72ul, 0xbcde, 0x9f15,{ 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x01 } };
	IID m2 = { 0x017b2e72ul, 0xbcde, 0x9f15,{ 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x02 } };
	IID m3 = { 0x017b2e72ul, 0xbcde, 0x9f15,{ 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x03 } };
	if (IsEqualIID(riid, m1))
	{
		LogInfo("Callback from dxgi.dll wrapper: requesting real IDirect3DDevice9 handle from %p\n", *ppvObj);

		D3D9Wrapper::IDirect3DDevice9 *p = (D3D9Wrapper::IDirect3DDevice9*) D3D9Wrapper::IDirect3DDevice9::m_List.GetDataPtr(*ppvObj);
		if (p)
		{
			LogInfo("  given pointer was already the real device.\n");
		}
		else
		{
			*ppvObj = ((D3D9Wrapper::IDirect3DDevice9 *)*ppvObj)->GetRealOrig();
		}
		LogInfo("  returning handle = %p\n", *ppvObj);

		*result = 0x13bc7e31;
		return true;
	}
	else if (IsEqualIID(riid, m2))
	{
		LogDebug("Callback from dxgi.dll wrapper: notification #%d received\n", (int)*ppvObj);

		switch ((int)*ppvObj)
		{
		case 0:
		{
			// Present received.
			// Todo: Not sure if this is actually used, but the 'this' here is going to be the wrong object and not work.
			IDirect3DDevice9 *device = (IDirect3DDevice9 *)this;
			// RunFrameActions(device);
			break;
		}
		}
		*result = 0x13bc7e31;
		return true;
	}
	else if (IsEqualIID(riid, m3))
	{
		SwapChainInfo *info = (SwapChainInfo *)*ppvObj;
		LogInfo("Callback from dxgi.dll wrapper: screen resolution width=%d, height=%d received\n",
			info->width, info->height);

		//G->mSwapChainInfo = *info;
		*result = 0x13bc7e31;
		return true;
	}

	return false;
}

D3D9Wrapper::IDirect3DUnknown * D3D9Wrapper::IDirect3DUnknown::QueryInterface_Find_Wrapper(void * ppvObj)
{
	D3D9Wrapper::IDirect3DDevice9 *p1 = (D3D9Wrapper::IDirect3DDevice9*) D3D9Wrapper::IDirect3DDevice9::m_List.GetDataPtr(ppvObj);
	if (p1)
	{
		if (G->enable_hooks & EnableHooksDX9::DEVICE)
			return nullptr;
		else {
			++p1->m_ulRef;
			LogInfo("  interface replaced with IDirect3DDevice9 wrapper.\n");
			return p1;
		}
	}
	if (G->enable_hooks >= EnableHooksDX9::ALL)
		return nullptr;
	D3D9Wrapper::IDirect3DSwapChain9 *p2 = (D3D9Wrapper::IDirect3DSwapChain9*) D3D9Wrapper::IDirect3DSwapChain9::m_List.GetDataPtr(ppvObj);
	if (p2)
	{
		++p2->m_ulRef;
		++p2->shared_ref_count;
		p2->zero_d3d_ref_count = false;
		LogInfo("  interface replaced with IDirect3DSwapChain9 wrapper.\n");
		return p2;
	}
	D3D9Wrapper::IDirect3D9 *p3 = (D3D9Wrapper::IDirect3D9*) D3D9Wrapper::IDirect3D9::m_List.GetDataPtr(ppvObj);
	if (p3)
	{
		++p3->m_ulRef;
		LogInfo("  interface replaced with IDirect3D9 wrapper.\n");
		return p3;
	}
	D3D9Wrapper::IDirect3DSurface9 *p4 = (D3D9Wrapper::IDirect3DSurface9*) D3D9Wrapper::IDirect3DSurface9::m_List.GetDataPtr(ppvObj);
	if (p4)
	{
		++p4->m_ulRef;
		p4->zero_d3d_ref_count = false;
		switch (p4->m_OwningContainerType)
		{
		case SurfaceContainerOwnerType::SwapChain:
			++p4->m_OwningSwapChain->shared_ref_count;
			break;
		case SurfaceContainerOwnerType::Texture:
			++p4->m_OwningTexture->shared_ref_count;
			break;
		case SurfaceContainerOwnerType::CubeTexture:
			++p4->m_OwningCubeTexture->shared_ref_count;
			break;
		}
		LogInfo("  interface replaced with IDirect3DSurface9 wrapper.\n");
		return p4;
	}
	D3D9Wrapper::IDirect3DVertexDeclaration9 *p5 = (D3D9Wrapper::IDirect3DVertexDeclaration9*) D3D9Wrapper::IDirect3DVertexDeclaration9::m_List.GetDataPtr(ppvObj);
	if (p5)
	{
		++p5->m_ulRef;
		p5->zero_d3d_ref_count = false;
		LogInfo("  interface replaced with IDirect3DVertexDeclaration9 wrapper.\n");
		return p5;
	}
	D3D9Wrapper::IDirect3DTexture9 *p6 = (D3D9Wrapper::IDirect3DTexture9*) D3D9Wrapper::IDirect3DTexture9::m_List.GetDataPtr(ppvObj);
	if (p6)
	{
		++p6->m_ulRef;
		++p6->shared_ref_count;
		p6->zero_d3d_ref_count = false;
		LogInfo("  interface replaced with IDirect3DTexture9 wrapper.\n");
		return p6;
	}
	D3D9Wrapper::IDirect3DVertexBuffer9 *p7 = (D3D9Wrapper::IDirect3DVertexBuffer9*) D3D9Wrapper::IDirect3DVertexBuffer9::m_List.GetDataPtr(ppvObj);
	if (p7)
	{
		++p7->m_ulRef;
		p7->zero_d3d_ref_count = false;
		LogInfo("  interface replaced with IDirect3DVertexBuffer9 wrapper.\n");
		return p7;
	}
	D3D9Wrapper::IDirect3DIndexBuffer9 *p8 = (D3D9Wrapper::IDirect3DIndexBuffer9*) D3D9Wrapper::IDirect3DIndexBuffer9::m_List.GetDataPtr(ppvObj);
	if (p8)
	{
		++p8->m_ulRef;
		p8->zero_d3d_ref_count = false;
		LogInfo("  interface replaced with IDirect3DIndexBuffer9 wrapper.\n");
		return p8;
	}
	D3D9Wrapper::IDirect3DQuery9 *p9 = (D3D9Wrapper::IDirect3DQuery9*) D3D9Wrapper::IDirect3DQuery9::m_List.GetDataPtr(ppvObj);
	if (p9)
	{
		++p9->m_ulRef;
		LogInfo("  interface replaced with IDirect3DQuery9 wrapper.\n");
		return p9;
	}
	D3D9Wrapper::IDirect3DVertexShader9 *p10 = (D3D9Wrapper::IDirect3DVertexShader9*) D3D9Wrapper::IDirect3DVertexShader9::m_List.GetDataPtr(ppvObj);
	if (p10)
	{
		++p10->m_ulRef;
		p10->zero_d3d_ref_count = false;
		LogInfo("  interface replaced with IDirect3DVertexShader9 wrapper.\n");
		return p10;
	}
	D3D9Wrapper::IDirect3DPixelShader9 *p11 = (D3D9Wrapper::IDirect3DPixelShader9*) D3D9Wrapper::IDirect3DPixelShader9::m_List.GetDataPtr(ppvObj);
	if (p11)
	{
		++p11->m_ulRef;
		p11->zero_d3d_ref_count = false;
		LogInfo("  interface replaced with IDirect3DPixelShader9 wrapper.\n");
		return p11;
	}
	D3D9Wrapper::IDirect3DCubeTexture9 *p12 = (D3D9Wrapper::IDirect3DCubeTexture9*) D3D9Wrapper::IDirect3DCubeTexture9::m_List.GetDataPtr(ppvObj);
	if (p12)
	{
		++p12->m_ulRef;
		++p12->shared_ref_count;
		p12->zero_d3d_ref_count = false;
		LogInfo("  interface replaced with IDirect3DCubeTexture9 wrapper.\n");
		return p12;
	}
	D3D9Wrapper::IDirect3DVolume9 *p13 = (D3D9Wrapper::IDirect3DVolume9*) D3D9Wrapper::IDirect3DVolume9::m_List.GetDataPtr(ppvObj);
	if (p13)
	{
		++p13->m_ulRef;
		++p13->m_OwningContainer->shared_ref_count;
		p13->zero_d3d_ref_count = false;
		LogInfo("  interface replaced with IDirect3DVolume9 wrapper.\n");
		return p13;
	}
	D3D9Wrapper::IDirect3DVolumeTexture9 *p14 = (D3D9Wrapper::IDirect3DVolumeTexture9*) D3D9Wrapper::IDirect3DVolumeTexture9::m_List.GetDataPtr(ppvObj);
	if (p14)
	{
		++p14->m_ulRef;
		++p14->shared_ref_count;
		p14->zero_d3d_ref_count = false;
		LogInfo("  interface replaced with IDirect3DVolumeTexture9 wrapper.\n");
		return p14;
	}
	D3D9Wrapper::IDirect3DStateBlock9 *p15 = (D3D9Wrapper::IDirect3DStateBlock9*) D3D9Wrapper::IDirect3DStateBlock9::m_List.GetDataPtr(ppvObj);
	if (p15)
	{
		++p15->m_ulRef;
		LogInfo("  interface replaced with IDirect3DStateBlock9 wrapper.\n");
		return p15;
	}
	return nullptr;
}

HRESULT _Direct3DCreate9Ex(UINT Version, ::IDirect3D9Ex **ppD3D) {
	LogInfo("Direct3DCreate9Ex called with Version=%d\n", Version);
	D3D9Wrapper::D3DCREATEEX pCreate = (D3D9Wrapper::D3DCREATEEX)GetProcAddress(hD3D, "Direct3DCreate9Ex");
	if (!pCreate)
	{
		LogInfo("  could not find Direct3DCreate9Ex in d3d9.dll\n");

		return NULL;
	}
	::LPDIRECT3D9EX pD3D = NULL;
	HRESULT hr = pCreate(Version, &pD3D);
	if (FAILED(hr) || pD3D == NULL)
	{
		LogInfo("  failed with hr=%x\n", hr);

		return NULL;
	}

	D3D9Wrapper::IDirect3D9 *wrapper = D3D9Wrapper::IDirect3D9::GetDirect3D(pD3D, true);
	LogInfo("  returns handle=%p, wrapper=%p\n", pD3D, wrapper);

	if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
		*ppD3D = (::IDirect3D9Ex*)wrapper;
	}
	else {
		*ppD3D = pD3D;
	}

	return hr;
}
::IDirect3D9* WINAPI Direct3DCreate9(UINT Version)
{
	InitD39();
	LogInfo("Direct3DCreate9 called with Version=%d\n", Version);

	if (G->gForwardToEx) {
		LogInfo("forwarding to Direct3DCreate9Ex\n");
		::IDirect3D9Ex *pD3D;
		_Direct3DCreate9Ex(Version, &pD3D);
		return pD3D;
	}
	D3D9Wrapper::D3DCREATE pCreate = (D3D9Wrapper::D3DCREATE)GetProcAddress(hD3D, "Direct3DCreate9");
    if (!pCreate)
    {
        LogInfo("  could not find Direct3DCreate9 in d3d9.dll\n");

        return NULL;
    }
	::LPDIRECT3D9 pD3D = NULL;
	pD3D = pCreate(Version);
    if (pD3D == NULL)
    {
		LogInfo("  failed to create D3D9\n");

        return NULL;
    }

    D3D9Wrapper::IDirect3D9 *wrapper = D3D9Wrapper::IDirect3D9::GetDirect3D(pD3D, false);
    LogInfo("  returns handle=%p, wrapper=%p\n", pD3D, wrapper);

	if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
		return (::IDirect3D9*)wrapper;
	}
	else {
		return pD3D;
	}
}

HRESULT WINAPI Direct3DCreate9Ex(UINT Version, ::IDirect3D9Ex **ppD3D)
{
	InitD39();
	HRESULT hr = _Direct3DCreate9Ex(Version, ppD3D);
	return hr;
}
static ::LPDIRECT3DSURFACE9 baseSurface9(D3D9Wrapper::IDirect3DSurface9 *pSurface);

static HRESULT createFakeSwapChain(D3D9Wrapper::IDirect3DDevice9 *HackerDevice, D3D9Wrapper::FakeSwapChain *FakeSwapChain, ::D3DPRESENT_PARAMETERS *PresentParams, UINT NewWidth, UINT NewHeight) {

	HRESULT hr;
	if (PresentParams->Windowed && (PresentParams->BackBufferHeight == 0 || PresentParams->BackBufferWidth == 0)) {
		if (PresentParams->hDeviceWindow != NULL) {
			RECT winDimensions;
			CursorUpscalingBypass_GetClientRect(PresentParams->hDeviceWindow, &winDimensions);
			LogInfo(" Get Upscaling Window with left = %d, right = %d, top = %d, bottom = %d\n", winDimensions.left, winDimensions.right, winDimensions.top, winDimensions.bottom);
			PresentParams->BackBufferWidth = winDimensions.right - winDimensions.left;
			PresentParams->BackBufferHeight = winDimensions.bottom - winDimensions.top;
		}
		else {
			RECT winDimensions;
			CursorUpscalingBypass_GetClientRect(HackerDevice->_hFocusWindow, &winDimensions);
			LogInfo(" Get Upscaling Window with left = %d, right = %d, top = %d, bottom = %d\n", winDimensions.left, winDimensions.right, winDimensions.top, winDimensions.bottom);
			PresentParams->BackBufferWidth = winDimensions.right - winDimensions.left;
			PresentParams->BackBufferHeight = winDimensions.bottom - winDimensions.top;
		}
	}
	for (UINT x = 0; x < PresentParams->BackBufferCount; x++) {
		D3D9Wrapper::IDirect3DSurface9 *wrappedFakeBackBuffer;
		if (G->gForceStereo == 2) {
			::IDirect3DSurface9* pLeftRenderTargetFake = NULL;
			::IDirect3DSurface9* pRightRenderTargetFake = NULL;
			// create left/mono
			hr = HackerDevice->GetD3D9Device()->CreateRenderTarget(PresentParams->BackBufferWidth, PresentParams->BackBufferHeight, PresentParams->BackBufferFormat, PresentParams->MultiSampleType, PresentParams->MultiSampleQuality, (PresentParams->Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER), &pLeftRenderTargetFake, NULL);
			++HackerDevice->migotoResourceCount;
			if (FAILED(hr)) {
				LogInfo("  Direct Mode Fake Render Target, failed to create left surface : %d \n", hr);
				return hr;
			}
			++HackerDevice->migotoResourceCount;
			hr = HackerDevice->GetD3D9Device()->CreateRenderTarget(PresentParams->BackBufferWidth, PresentParams->BackBufferHeight, PresentParams->BackBufferFormat, PresentParams->MultiSampleType, PresentParams->MultiSampleQuality, (PresentParams->Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER), &pRightRenderTargetFake, NULL);
			if (FAILED(hr)) {
				LogInfo("  Direct Mode Fake Render Target, failed to create right surface : %d \n", hr);
				return hr;
			}
			wrappedFakeBackBuffer = D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(pLeftRenderTargetFake, HackerDevice, pRightRenderTargetFake, HackerDevice->mWrappedSwapChains[FakeSwapChain->swapChainIndex]);
			if (G->SCREEN_UPSCALING > 0) {
				::IDirect3DSurface9* pLeftRenderTargetUpscaling = NULL;
				::IDirect3DSurface9* pRightRenderTargetUpscaling = NULL;
				// create left/mono
				hr = HackerDevice->GetD3D9Device()->CreateRenderTarget(NewWidth, NewHeight, PresentParams->BackBufferFormat, PresentParams->MultiSampleType, PresentParams->MultiSampleQuality, (PresentParams->Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER), &pLeftRenderTargetUpscaling, NULL);
				if (FAILED(hr)) {
					LogInfo("  Direct Mode Real Render Target, failed to create left surface : %d \n", hr);
					return hr;
				}
				hr = HackerDevice->GetD3D9Device()->CreateRenderTarget(NewWidth, NewHeight, PresentParams->BackBufferFormat, PresentParams->MultiSampleType, PresentParams->MultiSampleQuality, (PresentParams->Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER), &pRightRenderTargetUpscaling, NULL);
				if (FAILED(hr)) {
					LogInfo("  Direct Mode Real Render Target, failed to create right surface : %d \n", hr);
					return hr;
				}
				D3D9Wrapper::IDirect3DSurface9 *wrappedDirectModeUpscalingBackBuffer = D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(pLeftRenderTargetUpscaling, HackerDevice, pRightRenderTargetUpscaling, HackerDevice->mWrappedSwapChains[FakeSwapChain->swapChainIndex]);
				FakeSwapChain->mDirectModeUpscalingBackBuffers.emplace(FakeSwapChain->mFakeBackBuffers.begin() + x, wrappedDirectModeUpscalingBackBuffer);
			}
		}
		else {
			::IDirect3DSurface9 *pFakeBackBuffer;
			hr = HackerDevice->GetD3D9Device()->CreateRenderTarget(PresentParams->BackBufferWidth, PresentParams->BackBufferHeight, PresentParams->BackBufferFormat, PresentParams->MultiSampleType, PresentParams->MultiSampleQuality, (PresentParams->Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER), &pFakeBackBuffer, NULL);
			if (FAILED(hr)) {
				LogInfo("  Fake Render Target, failed to create surface : %d \n", hr);
				return hr;
			}
			++HackerDevice->migotoResourceCount;
			wrappedFakeBackBuffer = D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(pFakeBackBuffer, HackerDevice, NULL, HackerDevice->mWrappedSwapChains[FakeSwapChain->swapChainIndex]);
		}
		FakeSwapChain->mFakeBackBuffers.emplace(FakeSwapChain->mFakeBackBuffers.begin() + x, wrappedFakeBackBuffer);
		LogInfo("  Fake Render Target is lockable : %d \n", (PresentParams->Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER));
	}
	FakeSwapChain->mUpscalingWidth = NewWidth;
	FakeSwapChain->mUpscalingHeight = NewHeight;
	FakeSwapChain->mOrignalWidth = PresentParams->BackBufferWidth;
	FakeSwapChain->mOrignalHeight = PresentParams->BackBufferHeight;
	LogInfo("  Fake Back Buffer Created With Height = %d and Width = %d \n", PresentParams->BackBufferHeight, PresentParams->BackBufferWidth);
	return hr;

}
HRESULT CreateFakeSwapChain(D3D9Wrapper::IDirect3DDevice9 *me, D3D9Wrapper::FakeSwapChain **upSwapChain, ::D3DPRESENT_PARAMETERS *pOrigParams, ::D3DPRESENT_PARAMETERS *pParams, string *error) {
	HRESULT hr;
	if (pOrigParams == NULL || pParams == NULL) {
		return E_FAIL;
	}
	if (me->getOverlay())
		me->getOverlay()->Resize(pParams->BackBufferWidth, pParams->BackBufferHeight);
	D3D9Wrapper::FakeSwapChain newChain;
	newChain.swapChainIndex = (UINT)me->mFakeSwapChains.size();
	hr = createFakeSwapChain(me, &newChain, pOrigParams, pParams->BackBufferWidth, pParams->BackBufferHeight);
	if (FAILED(hr)) {
		LogOverlay(LOG_DIRE, "  HackerFakeSwapChain failed to create fake swap chain.\n");
		return hr;
	}
	me->mFakeSwapChains.push_back(newChain);
	LogInfo("  HackerFakeSwapChain for device wrap %p created for device %p.\n", me, me->GetD3D9Device());
	if (newChain.swapChainIndex == 0) {
		if (newChain.mFakeBackBuffers.size() > 0) {
			hr = me->GetD3D9Device()->SetRenderTarget(0, newChain.mFakeBackBuffers.at(0)->GetD3DSurface9());
			if (FAILED(hr)) {
				LogOverlay(LOG_DIRE, "  HackerFakeSwapChain for device failed to set default render target for fake back buffer.\n");
				return hr;
			}
			else {
				me->m_activeRenderTargets[0] = newChain.mFakeBackBuffers.at(0);
				LogInfo("  HackerFakeSwapChain for device set default render target for fake back buffer.\n");
			}
		}
		else {
			LogOverlay(LOG_DIRE, "  HackerFakeSwapChain for device failed to set default render target for fake back buffer, as couldn't find fake back buffer.\n");
		}
		if (pOrigParams->EnableAutoDepthStencil && (G->SCREEN_UPSCALING > 0 || G->gForceStereo == 2)) {
			if (me->mFakeDepthSurface) {
				me->mFakeDepthSurface->Release();
				me->mFakeDepthSurface = NULL;
			}
			::IDirect3DSurface9 *pRealFakeDepthStencil;
			hr = me->GetD3D9Device()->CreateDepthStencilSurface(pOrigParams->BackBufferWidth, pOrigParams->BackBufferHeight, pOrigParams->AutoDepthStencilFormat, pOrigParams->MultiSampleType, pOrigParams->MultiSampleQuality, (pOrigParams->Flags & D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL), &pRealFakeDepthStencil, NULL);
			if (FAILED(hr)) {
				LogOverlay(LOG_DIRE, " HackerFakeSwapChain failed to create fake depth surface.\n");
				return hr;
			}
			else {
				++me->migotoResourceCount;
				if (G->gForceStereo == 2) {
					::IDirect3DSurface9* pRightDepthStencil = NULL;
					hr = me->GetD3D9Device()->CreateDepthStencilSurface(pOrigParams->BackBufferWidth, pOrigParams->BackBufferHeight, pOrigParams->AutoDepthStencilFormat, pOrigParams->MultiSampleType, pOrigParams->MultiSampleQuality, (pOrigParams->Flags & D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL), &pRightDepthStencil, NULL);
					if (FAILED(hr)) {
						LogOverlay(LOG_DIRE, " HackerFakeSwapChain Direct Mode failed to create right fake depth surface.\n");
						return hr;
						me->mFakeDepthSurface = D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(pRealFakeDepthStencil, me, NULL);
					}
					else {
						++me->migotoResourceCount;
						me->mFakeDepthSurface = D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(pRealFakeDepthStencil, me, pRightDepthStencil);
					}
				}
				else {
					me->mFakeDepthSurface = D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(pRealFakeDepthStencil, me, NULL);
				}
				me->mFakeDepthSurface->Bound();
				me->m_pActiveDepthStencil = me->mFakeDepthSurface;
				if (G->gAutoDetectDepthBuffer) {
					me->depth_sources.emplace(me->m_pActiveDepthStencil);
					me->m_pActiveDepthStencil->possibleDepthBuffer = true;
					// Begin tracking
					const D3D9Wrapper::DepthSourceInfo info = { pOrigParams->BackBufferWidth, pOrigParams->BackBufferHeight};
					me->m_pActiveDepthStencil->depthSourceInfo = info;
				}
				LogInfo(" HackerFakeSwapChain created fake depth surface with Width = %d, Height = %d.\n", pOrigParams->BackBufferWidth, pOrigParams->BackBufferHeight);
				hr = me->GetD3D9Device()->SetDepthStencilSurface(me->mFakeDepthSurface->GetD3DSurface9());
				if (FAILED(hr)) {
					LogOverlay(LOG_DIRE, " HackerFakeSwapChain failed to set fake depth surface.\n");
					me->mFakeDepthSurface->Release();
					return hr;
				}
				else {
					LogInfo(" HackerFakeSwapChain set fake depth surface.\n");
				}
			}

		}
	}
	*upSwapChain = &me->mFakeSwapChains[newChain.swapChainIndex];
	return hr;
}

static void CheckDevice(D3D9Wrapper::IDirect3DDevice9 *me)
{
	if (!me->GetD3D9Device())
	{
		HRESULT hr;
		if (me->_ex) {
			LogInfo("  calling postponed CreateDevice.\n");
			hr = me->m_pD3D->GetDirect3D9Ex()->CreateDeviceEx(
				me->_Adapter,
				me->_DeviceType,
				me->_hFocusWindow,
				me->_BehaviorFlags,
				&me->_pPresentationParameters,
				me->_pFullscreenDisplayMode,
				(::IDirect3DDevice9Ex**) &me->m_pRealUnk);
		}
		else {
			hr = me->m_pD3D->GetDirect3D9()->CreateDevice(
				me->_Adapter,
				me->_DeviceType,
				me->_hFocusWindow,
				me->_BehaviorFlags,
				&me->_pPresentationParameters,
				(::IDirect3DDevice9**) &me->m_pRealUnk);
		}

		if (FAILED(hr))
		{
			LogInfo("    failed creating device with result = %x\n", hr);

			return;
		}
		me->m_pUnk = me->m_pRealUnk;
		if ((G->enable_hooks & EnableHooksDX9::DEVICE))
			me->HookDevice();
		me->createOverlay();
		me->Create3DMigotoResources();
		me->Bind3DMigotoResources();
		me->InitIniParams();
		me->OnCreateOrRestore(&me->_pOrigPresentationParameters, &me->_pPresentationParameters);

		if (me->pendingCreateDepthStencilSurface)
		{
			LogInfo("  calling postponed CreateDepthStencilSurface.\n");

			hr = me->GetD3D9Device()->CreateDepthStencilSurface(
				me->pendingCreateDepthStencilSurface->_Width,
				me->pendingCreateDepthStencilSurface->_Height,
				me->pendingCreateDepthStencilSurface->_Format,
				me->pendingCreateDepthStencilSurface->_MultiSample,
				me->pendingCreateDepthStencilSurface->_MultisampleQuality,
				me->pendingCreateDepthStencilSurface->_Discard,
				(::IDirect3DSurface9**) &me->pendingCreateDepthStencilSurface->m_pRealUnk,
				0);
			if (FAILED(hr))
			{
				LogInfo("    failed creating depth stencil surface with result=%x\n", hr);

				return;
			}
			me->pendingCreateDepthStencilSurface->m_pUnk = me->pendingCreateDepthStencilSurface->m_pRealUnk;
			if (G->enable_hooks >= EnableHooksDX9::ALL)
				me->pendingCreateDepthStencilSurface->HookSurface();
			me->pendingCreateDepthStencilSurface = 0;
		}
		if (me->pendingSetDepthStencilSurface)
		{
			LogInfo("  calling postponed SetDepthStencilSurface.\n");

			::LPDIRECT3DSURFACE9 baseStencil = baseSurface9(me->pendingSetDepthStencilSurface);
			hr = me->GetD3D9Device()->SetDepthStencilSurface(baseStencil);
			if (FAILED(hr))
			{
				LogInfo("    failed calling SetDepthStencilSurface with result = %x\n", hr);

				return;
			}
			me->pendingSetDepthStencilSurface = 0;
		}
		if (me->pendingCreateDepthStencilSurfaceEx)
		{
			LogInfo("  calling postponed CreateDepthStencilSurfaceEx.\n");

			hr = me->GetD3D9DeviceEx()->CreateDepthStencilSurfaceEx(
				me->pendingCreateDepthStencilSurfaceEx->_Width,
				me->pendingCreateDepthStencilSurfaceEx->_Height,
				me->pendingCreateDepthStencilSurfaceEx->_Format,
				me->pendingCreateDepthStencilSurfaceEx->_MultiSample,
				me->pendingCreateDepthStencilSurfaceEx->_MultisampleQuality,
				me->pendingCreateDepthStencilSurfaceEx->_Discard,
				(::IDirect3DSurface9**) &me->pendingCreateDepthStencilSurface->m_pRealUnk,
				0,
				me->pendingCreateDepthStencilSurfaceEx->_Usage);
			if (FAILED(hr))
			{
				LogInfo("    failed creating depth stencil surface ex with result=%x\n", hr);

				return;
			}
			me->pendingCreateDepthStencilSurfaceEx->m_pUnk = me->pendingCreateDepthStencilSurfaceEx->m_pRealUnk;
			if (G->enable_hooks >= EnableHooksDX9::ALL)
				me->pendingCreateDepthStencilSurfaceEx->HookSurface();
			me->pendingCreateDepthStencilSurfaceEx = 0;
		}
	}
}

static void CheckVertexDeclaration9(D3D9Wrapper::IDirect3DVertexDeclaration9 *me)
{
	if (!me->pendingCreateVertexDeclaration)
		return;
	me->pendingCreateVertexDeclaration = false;
	CheckDevice(me->pendingDevice);

	LogInfo("  calling postponed CreateVertexDeclaration.\n");
	HRESULT hr = me->pendingDevice->GetD3D9Device()->CreateVertexDeclaration(&me->_VertexElements, (::IDirect3DVertexDeclaration9**) &me->m_pRealUnk);
	if (FAILED(hr))
	{
		LogInfo("    failed creating vertex declaration with result = %x\n", hr);

		return;
	}

	me->m_pUnk = me->m_pRealUnk;
	if (G->enable_hooks >= EnableHooksDX9::ALL)
		me->HookVertexDeclaration();
}

static void CheckTexture9(D3D9Wrapper::IDirect3DTexture9 *me)
{
	if (me->pendingCreateTexture)
	{
		me->pendingCreateTexture = false;
		CheckDevice(me->_Device);
		LogInfo("  calling postponed CreateTexture.\n");
		HRESULT hr = me->_Device->GetD3D9Device()->CreateTexture(me->_Width, me->_Height, me->_Levels, me->_Usage, me->_Format, me->_Pool, (::IDirect3DTexture9**) &me->m_pRealUnk, 0);
		if (FAILED(hr))
		{
			LogInfo("    failed creating texture with result = %x\n", hr);

			return;
		}
		me->m_pUnk = me->m_pRealUnk;
		if (G->enable_hooks >= EnableHooksDX9::ALL)
			me->HookTexture();
	}
	if (me->pendingLockUnlock)
	{
		me->pendingLockUnlock = false;
		LogInfo("  calling postponed Lock.\n");

		::D3DLOCKED_RECT rect;
		HRESULT hr = me->LockRect(me->_Level, &rect, 0, me->_Flags);
		if (FAILED(hr))
		{
			LogInfo("    failed locking texture with result = %x\n", hr);

			return;
		}
		for (UINT y = 0; y < me->_Height; ++y)
			memcpy(((char*)rect.pBits) + y*rect.Pitch, me->_Buffer + y*me->_Width*4, rect.Pitch);
		hr = me->UnlockRect(me->_Level);
		if (FAILED(hr))
		{
			LogInfo("    failed unlocking texture with result = %x\n", hr);

			return;
		}
		delete me->_Buffer; me->_Buffer = 0;
	}

}
static void CheckVolumeTexture9(D3D9Wrapper::IDirect3DVolumeTexture9 *me)
{
	if (me->pendingCreateTexture)
	{
		me->pendingCreateTexture = false;
		CheckDevice(me->_Device);
		LogInfo("  calling postponed CreateVolumeTexture.\n");
		HRESULT hr = me->_Device->GetD3D9Device()->CreateVolumeTexture(me->_Width, me->_Height, me->_Depth, me->_Levels, me->_Usage, me->_Format, me->_Pool, (::IDirect3DVolumeTexture9**) &me->m_pRealUnk, 0);
		if (FAILED(hr))
		{
			LogInfo("    failed creating volume texture with result = %x\n", hr);

			return;
		}

		me->m_pUnk = me->m_pRealUnk;
		if (G->enable_hooks >= EnableHooksDX9::ALL)
			me->HookVolumeTexture();
	}
	if (me->pendingLockUnlock)
	{
		me->pendingLockUnlock = false;
		LogInfo("  calling postponed Lock.\n");

		::D3DLOCKED_BOX box;
		HRESULT hr = me->LockBox(me->_Level, &box, 0, me->_Flags);
		if (FAILED(hr))
		{
			LogInfo("    failed locking volume texture with result = %x\n", hr);

			return;
		}
		for (UINT z = 0; z < me->_Depth; ++z)
			memcpy(((char*)box.pBits) + z * box.SlicePitch, me->_Buffer + z * me->_Height * me->_Width * 4, box.SlicePitch);
		hr = me->UnlockBox(me->_Level);
		if (FAILED(hr))
		{
			LogInfo("    failed unlocking volume texture with result = %x\n", hr);

			return;
		}
		delete me->_Buffer; me->_Buffer = 0;
	}

}
static void CheckCubeTexture9(D3D9Wrapper::IDirect3DCubeTexture9 *me)
{
	if (me->pendingCreateTexture)
	{
		me->pendingCreateTexture = false;
		CheckDevice(me->_Device);
		LogInfo("  calling postponed CreateCubeTexture.\n");
		HRESULT hr = me->_Device->GetD3D9Device()->CreateCubeTexture(me->_EdgeLength, me->_Levels, me->_Usage, me->_Format, me->_Pool, (::IDirect3DCubeTexture9**) &me->m_pRealUnk, 0);
		if (FAILED(hr))
		{
			LogInfo("    failed creating cube texture with result = %x\n", hr);

			return;
		}
		me->m_pUnk = me->m_pRealUnk;
		if (G->enable_hooks >= EnableHooksDX9::ALL)
			me->HookCubeTexture();
	}
	if (me->pendingLockUnlock)
	{
		me->pendingLockUnlock = false;
		LogInfo("  calling postponed Lock.\n");

		::D3DLOCKED_RECT rect;
		HRESULT hr = me->LockRect(me->_FaceType, me->_Level, &rect, 0, me->_Flags);
		if (FAILED(hr))
		{
			LogInfo("    failed locking cube texture with result = %x\n", hr);

			return;
		}
		for (UINT y = 0; y < me->_EdgeLength; ++y)
			memcpy(((char*)rect.pBits) + y * rect.Pitch, me->_Buffer + y * me->_EdgeLength * 4, rect.Pitch);
		hr = me->UnlockRect(me->_FaceType, me->_Level);
		if (FAILED(hr))
		{
			LogInfo("    failed unlocking cube texture with result = %x\n", hr);

			return;
		}
		delete me->_Buffer; me->_Buffer = 0;
	}

}
static void CheckTexture9(D3D9Wrapper::IDirect3DBaseTexture9 *me)
{
	::D3DRESOURCETYPE type = me->GetD3DBaseTexture9()->GetType();
	switch (type) {
	case ::D3DRTYPE_TEXTURE:
		CheckTexture9(((D3D9Wrapper::IDirect3DTexture9*)me));
	case ::D3DRTYPE_CUBETEXTURE:
		CheckCubeTexture9(((D3D9Wrapper::IDirect3DCubeTexture9*)me));
	case ::D3DRTYPE_VOLUMETEXTURE:
		CheckVolumeTexture9(((D3D9Wrapper::IDirect3DVolumeTexture9*)me));
	}
}
static void CheckSurface9(D3D9Wrapper::IDirect3DSurface9 *me)
{
	if (!me->pendingGetSurfaceLevel)
		return;
	me->pendingGetSurfaceLevel = false;

	if (!me->_Texture) {
		CheckTexture9(me->_Texture);

		LogInfo("  calling postponed GetSurfaceLevel.\n");
		HRESULT hr = me->_Texture->GetD3DTexture9()->GetSurfaceLevel(me->_Level, (::IDirect3DSurface9**) &me->m_pRealUnk);
		if (FAILED(hr))
		{
			LogInfo("    failed getting surface with result = %x\n", hr);

			return;
		}
		me->m_pUnk = me->m_pRealUnk;
		if (G->enable_hooks >= EnableHooksDX9::ALL)
			me->HookSurface();

	}
	else if (!me->_CubeTexture) {

		CheckCubeTexture9(me->_CubeTexture);

		LogInfo("  calling postponed GetCubeMapSurface.\n");
		HRESULT hr = me->_CubeTexture->GetD3DCubeTexture9()->GetCubeMapSurface(me->_FaceType, me->_Level, (::IDirect3DSurface9**) &me->m_pRealUnk);
		if (FAILED(hr))
		{
			LogInfo("    failed getting surface with result = %x\n", hr);

			return;
		}
		me->m_pUnk = me->m_pRealUnk;
	}
}

static void CheckVolume9(D3D9Wrapper::IDirect3DVolume9 *me)
{
	if (!me->pendingGetVolumeLevel)
		return;
	me->pendingGetVolumeLevel = false;
	CheckVolumeTexture9(me->_VolumeTexture);

	LogInfo("  calling postponed GetVolumeLevel.\n");
	HRESULT hr = me->_VolumeTexture->GetD3DVolumeTexture9()->GetVolumeLevel(me->_Level, (::IDirect3DVolume9**) &me->m_pRealUnk);
	if (FAILED(hr))
	{
		LogInfo("    failed getting volume with result = %x\n", hr);

		return;
	}

	me->m_pUnk = me->m_pRealUnk;
	if (G->enable_hooks >= EnableHooksDX9::ALL)
		me->HookVolume();
}

static void CheckVertexBuffer9(D3D9Wrapper::IDirect3DVertexBuffer9 *me)
{
	if (me->pendingCreateVertexBuffer)
	{
		me->pendingCreateVertexBuffer = false;
		CheckDevice(me->_Device);
		LogInfo("  calling postponed CreateVertexBuffer.\n");
		HRESULT hr = me->_Device->GetD3D9Device()->CreateVertexBuffer(me->_Length, me->_Usage, me->_FVF, me->_Pool, (::IDirect3DVertexBuffer9**) &me->m_pRealUnk, 0);
		if (FAILED(hr))
		{
			LogInfo("    failed creating vertex buffer with result = %x\n", hr);

			return;
		}

		me->m_pUnk = me->m_pRealUnk;
		if (G->enable_hooks >= EnableHooksDX9::ALL)
			me->HookVertexBuffer();
	}
	if (me->pendingLockUnlock)
	{
		me->pendingLockUnlock = false;
		LogInfo("  calling postponed Lock.\n");
		void *ppbData;
		HRESULT hr = me->Lock(0, me->_Length, &ppbData, me->_Flags);
		if (FAILED(hr))
		{
			LogInfo("    failed locking vertex buffer with result = %x\n", hr);

			return;
		}
		memcpy(ppbData, me->_Buffer, me->_Length);
		hr = me->Unlock();
		if (FAILED(hr))
		{
			LogInfo("    failed unlocking vertex buffer with result = %x\n", hr);

			return;
		}
		delete me->_Buffer; me->_Buffer = 0;
	}
}

static void CheckIndexBuffer9(D3D9Wrapper::IDirect3DIndexBuffer9 *me)
{
	if (me->pendingCreateIndexBuffer)
	{
		me->pendingCreateIndexBuffer = false;
		CheckDevice(me->_Device);
		LogInfo("  calling postponed CreateIndexBuffer.\n");
		HRESULT hr = me->_Device->GetD3D9Device()->CreateIndexBuffer(me->_Length, me->_Usage, me->_Format, me->_Pool, (::IDirect3DIndexBuffer9**) &me->m_pRealUnk, 0);
		if (FAILED(hr))
		{
			LogInfo("    failed creating index buffer with result = %x\n", hr);

			return;
		}

		me->m_pUnk = me->m_pRealUnk;
		if (G->enable_hooks >= EnableHooksDX9::ALL)
			me->HookIndexBuffer();
	}
	if (me->pendingLockUnlock)
	{
		me->pendingLockUnlock = false;
		LogInfo("  calling postponed Lock.\n");
		void *ppbData;
		HRESULT hr = me->Lock(0, me->_Length, &ppbData, me->_Flags);
		if (FAILED(hr))
		{
			LogInfo("    failed locking index buffer with result = %x\n", hr);

			return;
		}
		memcpy(ppbData, me->_Buffer, me->_Length);
		hr = me->Unlock();
		if (FAILED(hr))
		{
			LogInfo("    failed unlocking index buffer with result = %x\n", hr);

			return;
		}
		delete me->_Buffer; me->_Buffer = 0;
	}
}

bool _simulateTextureUpdate(D3D9Wrapper::IDirect3DTexture9 * sourceTexture, D3D9Wrapper::IDirect3DTexture9 * destinationTexture)
{
	if (sourceTexture->pendingCreateTexture && destinationTexture->pendingCreateTexture && sourceTexture->pendingLockUnlock &&
		sourceTexture->_Width == destinationTexture->_Width && sourceTexture->_Height == destinationTexture->_Height)
	{
		LogInfo("  simulating texture update because both textures are not created yet.\n");

		if (!destinationTexture->pendingLockUnlock)
		{
			::D3DLOCKED_RECT rect;
			destinationTexture->LockRect(0, &rect, 0, 0);
		}
		memcpy(destinationTexture->_Buffer, sourceTexture->_Buffer, sourceTexture->_Width*sourceTexture->_Height * 4);
		return true;
	}
	return false;
}
bool _simulateTextureUpdate(D3D9Wrapper::IDirect3DCubeTexture9 *sourceTexture, D3D9Wrapper::IDirect3DCubeTexture9 *destinationTexture)
{
	if (sourceTexture->pendingCreateTexture && destinationTexture->pendingCreateTexture && sourceTexture->pendingLockUnlock &&
		sourceTexture->_EdgeLength == destinationTexture->_EdgeLength)
	{
		LogInfo("  simulating cube texture update because both textures are not created yet.\n");

		if (!destinationTexture->pendingLockUnlock)
		{
			::D3DLOCKED_RECT rect;
			destinationTexture->LockRect(::D3DCUBEMAP_FACE_POSITIVE_X, 0, &rect, 0, 0);
		}
		memcpy(destinationTexture->_Buffer, sourceTexture->_Buffer, sourceTexture->_EdgeLength*sourceTexture->_EdgeLength * 4);
		return true;
	}

	return false;

}
bool _simulateTextureUpdate(D3D9Wrapper::IDirect3DVolumeTexture9 *sourceTexture, D3D9Wrapper::IDirect3DVolumeTexture9 *destinationTexture) {

	if (sourceTexture->pendingCreateTexture && destinationTexture->pendingCreateTexture && sourceTexture->pendingLockUnlock &&
			sourceTexture->_Width == destinationTexture->_Width && sourceTexture->_Height == destinationTexture->_Height && sourceTexture->_Depth == destinationTexture->_Depth)
	{
		LogInfo("  simulating volume texture update because both textures are not created yet.\n");

		if (!destinationTexture->pendingLockUnlock)
		{
			::D3DLOCKED_BOX box;
			destinationTexture->LockBox(0, &box, 0, 0);
		}
		memcpy(destinationTexture->_Buffer, sourceTexture->_Buffer, sourceTexture->_Height*sourceTexture->_Width*sourceTexture->_Depth * 4);
		return true;
	}
	return false;
}

bool simulateTextureUpdate(D3D9Wrapper::IDirect3DBaseTexture9 * pSourceTexture, D3D9Wrapper::IDirect3DBaseTexture9 * pDestinationTexture)
{
	if (!G->gDelayDeviceCreation || G->enable_hooks >= EnableHooksDX9::ALL || !pSourceTexture || !pDestinationTexture)
		return false;
	if (pSourceTexture->texType == D3D9Wrapper::TextureType::Texture2D && pDestinationTexture->texType == D3D9Wrapper::TextureType::Texture2D)
		return _simulateTextureUpdate((D3D9Wrapper::IDirect3DTexture9*)pSourceTexture, (D3D9Wrapper::IDirect3DTexture9*)pDestinationTexture);
	if (pSourceTexture->texType == D3D9Wrapper::TextureType::Cube && pDestinationTexture->texType == D3D9Wrapper::TextureType::Cube)
		return _simulateTextureUpdate((D3D9Wrapper::IDirect3DCubeTexture9*)pSourceTexture, (D3D9Wrapper::IDirect3DCubeTexture9*)pDestinationTexture);
	if (pSourceTexture->texType == D3D9Wrapper::TextureType::Volume && pDestinationTexture->texType == D3D9Wrapper::TextureType::Volume)
		return _simulateTextureUpdate((D3D9Wrapper::IDirect3DVolumeTexture9*)pSourceTexture, (D3D9Wrapper::IDirect3DVolumeTexture9*)pDestinationTexture);

	return false;
}

static ::IDirect3DSurface9* baseSurface9(D3D9Wrapper::IDirect3DSurface9 *pSurface) {

	if (!pSurface) return NULL;
	if (G->gDelayDeviceCreation && pSurface->pendingGetSurfaceLevel && !pSurface->GetD3DSurface9()) {
		CheckSurface9(pSurface);
		return pSurface->GetD3DSurface9();
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pSurface->GetD3DSurface9();
	}
	else {
		return (::IDirect3DSurface9*)pSurface;
	}
}
static D3D9Wrapper::IDirect3DSurface9* wrappedSurface9(D3D9Wrapper::IDirect3DSurface9 *pSurface) {

	if (!pSurface) return NULL;
	if (G->gDelayDeviceCreation && pSurface->pendingGetSurfaceLevel && !pSurface->GetD3DSurface9()) {
		CheckSurface9(pSurface);
		return pSurface;
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pSurface;
	}
	else {
		D3D9Wrapper::IDirect3DSurface9* pWrapper = (D3D9Wrapper::IDirect3DSurface9*) D3D9Wrapper::IDirect3DSurface9::m_List.GetDataPtr(pSurface);
		return pWrapper;
	}
}
static ::IDirect3DVolume9* baseVolume9(D3D9Wrapper::IDirect3DVolume9 *pVolume) {

	if (!pVolume) return NULL;
	if (G->gDelayDeviceCreation && pVolume->pendingGetVolumeLevel && !pVolume->GetD3DVolume9()) {
		CheckVolume9(pVolume);
		return pVolume->GetD3DVolume9();
	}
	else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pVolume->GetD3DVolume9();
	}
	else {
		return reinterpret_cast<::IDirect3DVolume9*>(pVolume);
	}
}
static D3D9Wrapper::IDirect3DVolume9* wrappedVolume9(D3D9Wrapper::IDirect3DVolume9 *pVolume) {

	if (!pVolume) return NULL;
	if (G->gDelayDeviceCreation && pVolume->pendingGetVolumeLevel && !pVolume->GetD3DVolume9()) {
		CheckVolume9(pVolume);
		return pVolume;
	}
	else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pVolume;
	}
	else {
		D3D9Wrapper::IDirect3DVolume9* pWrapper =  reinterpret_cast<D3D9Wrapper::IDirect3DVolume9*>(D3D9Wrapper::IDirect3DVolume9::m_List.GetDataPtr(pVolume));
		return pWrapper;
	}
}
static ::IDirect3DVertexDeclaration9* baseVertexDeclaration9(D3D9Wrapper::IDirect3DVertexDeclaration9 *pVertexDeclaration) {

	if (!pVertexDeclaration) return NULL;
	if (G->gDelayDeviceCreation && pVertexDeclaration->pendingCreateVertexDeclaration && !pVertexDeclaration->GetD3DVertexDeclaration9()) {
		CheckVertexDeclaration9(pVertexDeclaration);
		return pVertexDeclaration->GetD3DVertexDeclaration9();
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pVertexDeclaration->GetD3DVertexDeclaration9();
	}
	else {
		return (::IDirect3DVertexDeclaration9*)pVertexDeclaration;
	}
}
static D3D9Wrapper::IDirect3DVertexDeclaration9* wrappedVertexDeclaration9(D3D9Wrapper::IDirect3DVertexDeclaration9 *pVertexDeclaration) {

	if (!pVertexDeclaration) return NULL;
	if (G->gDelayDeviceCreation && pVertexDeclaration->pendingCreateVertexDeclaration && !pVertexDeclaration->GetD3DVertexDeclaration9()) {
		CheckVertexDeclaration9(pVertexDeclaration);
		return pVertexDeclaration;
	} else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pVertexDeclaration;
	}
	else {
		D3D9Wrapper::IDirect3DVertexDeclaration9* pWrapper = (D3D9Wrapper::IDirect3DVertexDeclaration9*) D3D9Wrapper::IDirect3DVertexDeclaration9::m_List.GetDataPtr(pVertexDeclaration);
		return pWrapper;

	}
}
static ::IDirect3DVertexBuffer9* baseVertexBuffer9(D3D9Wrapper::IDirect3DVertexBuffer9 *pVertexBuffer) {

	if (!pVertexBuffer) return NULL;
	if (G->gDelayDeviceCreation && pVertexBuffer->pendingCreateVertexBuffer && !pVertexBuffer->GetD3DVertexBuffer9()) {
		CheckVertexBuffer9(pVertexBuffer);
		return pVertexBuffer->GetD3DVertexBuffer9();
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pVertexBuffer->GetD3DVertexBuffer9();
	}
	else {
		return (::IDirect3DVertexBuffer9*)pVertexBuffer;
	}
}
static D3D9Wrapper::IDirect3DVertexBuffer9* wrappedVertexBuffer9(D3D9Wrapper::IDirect3DVertexBuffer9 *pVertexBuffer) {

	if (!pVertexBuffer) return NULL;
	if (G->gDelayDeviceCreation && pVertexBuffer->pendingCreateVertexBuffer && !pVertexBuffer->GetD3DVertexBuffer9()) {
		CheckVertexBuffer9(pVertexBuffer);
		return pVertexBuffer;

	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pVertexBuffer;
	}
	else{
		D3D9Wrapper::IDirect3DVertexBuffer9* pWrapper = (D3D9Wrapper::IDirect3DVertexBuffer9*) D3D9Wrapper::IDirect3DVertexBuffer9::m_List.GetDataPtr(pVertexBuffer);
		return pWrapper;
	}
}
static ::IDirect3DIndexBuffer9* baseIndexBuffer9(D3D9Wrapper::IDirect3DIndexBuffer9 *pIndexBuffer) {

	if (!pIndexBuffer) return NULL;
	if (G->gDelayDeviceCreation && pIndexBuffer->pendingCreateIndexBuffer && !pIndexBuffer->GetD3DIndexBuffer9()) {
		CheckIndexBuffer9(pIndexBuffer);
		return pIndexBuffer->GetD3DIndexBuffer9();
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pIndexBuffer->GetD3DIndexBuffer9();
	}
	else {
		return (::IDirect3DIndexBuffer9*)pIndexBuffer;
	}
}
static D3D9Wrapper::IDirect3DIndexBuffer9* wrappedIndexBuffer9(D3D9Wrapper::IDirect3DIndexBuffer9 *pIndexBuffer) {

	if (!pIndexBuffer) return NULL;
	if (G->gDelayDeviceCreation && pIndexBuffer->pendingCreateIndexBuffer && !pIndexBuffer->GetD3DIndexBuffer9()) {
		CheckIndexBuffer9(pIndexBuffer);
		return pIndexBuffer;
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pIndexBuffer;
	}
	else{
		D3D9Wrapper::IDirect3DIndexBuffer9* pWrapper = (D3D9Wrapper::IDirect3DIndexBuffer9*) D3D9Wrapper::IDirect3DIndexBuffer9::m_List.GetDataPtr(pIndexBuffer);
		return pWrapper;

	}
}
static ::IDirect3DTexture9* baseTexture9(D3D9Wrapper::IDirect3DTexture9 *pTexture) {

	if (!pTexture) return NULL;
	if (G->gDelayDeviceCreation && pTexture->pendingCreateTexture && !pTexture->GetD3DTexture9()) {
		CheckTexture9(pTexture);
		return pTexture->GetD3DTexture9();
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pTexture->GetD3DTexture9();
	}
	else {
		return (::IDirect3DTexture9*)pTexture;
	}
}
static D3D9Wrapper::IDirect3DTexture9* wrappedTexture9(D3D9Wrapper::IDirect3DTexture9 *pTexture) {

	if (!pTexture) return NULL;
	if (G->gDelayDeviceCreation && pTexture->pendingCreateTexture && !pTexture->GetD3DTexture9()) {
		CheckTexture9(pTexture);
		return pTexture;
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pTexture;
	}
	else {
		D3D9Wrapper::IDirect3DTexture9* pWrapper = (D3D9Wrapper::IDirect3DTexture9*) D3D9Wrapper::IDirect3DTexture9::m_List.GetDataPtr(pTexture);
		return pWrapper;

	}
}

static ::IDirect3DCubeTexture9* baseTexture9(D3D9Wrapper::IDirect3DCubeTexture9 *pCubeTexture) {

	if (!pCubeTexture) return NULL;
	if (G->gDelayDeviceCreation && pCubeTexture->pendingCreateTexture && !pCubeTexture->GetD3DCubeTexture9()) {
		CheckCubeTexture9(pCubeTexture);
		return pCubeTexture->GetD3DCubeTexture9();
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pCubeTexture->GetD3DCubeTexture9();
	}
	else {
		return (::IDirect3DCubeTexture9*)pCubeTexture;
	}
}
static D3D9Wrapper::IDirect3DCubeTexture9* wrappedTexture9(D3D9Wrapper::IDirect3DCubeTexture9 *pCubeTexture) {

	if (!pCubeTexture) return NULL;
	if (G->gDelayDeviceCreation && pCubeTexture->pendingCreateTexture && !pCubeTexture->GetD3DCubeTexture9()) {
		CheckCubeTexture9(pCubeTexture);
		return pCubeTexture;
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pCubeTexture;
	}
	else {
		D3D9Wrapper::IDirect3DCubeTexture9* pWrapper = (D3D9Wrapper::IDirect3DCubeTexture9*) D3D9Wrapper::IDirect3DCubeTexture9::m_List.GetDataPtr(pCubeTexture);
		return pWrapper;

	}
}
static ::IDirect3DVolumeTexture9* baseTexture9(D3D9Wrapper::IDirect3DVolumeTexture9 *pVolumeTexture) {

	if (!pVolumeTexture) return NULL;
	if (G->gDelayDeviceCreation && pVolumeTexture->pendingCreateTexture && !pVolumeTexture->GetD3DVolumeTexture9()) {
		CheckVolumeTexture9(pVolumeTexture);
		return pVolumeTexture->GetD3DVolumeTexture9();
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pVolumeTexture->GetD3DVolumeTexture9();
	}
	else {
		return (::IDirect3DVolumeTexture9*)pVolumeTexture;
	}
}
static D3D9Wrapper::IDirect3DVolumeTexture9* wrappedTexture9(D3D9Wrapper::IDirect3DVolumeTexture9 *pVolumeTexture) {

	if (!pVolumeTexture) return NULL;
	if (G->gDelayDeviceCreation && pVolumeTexture->pendingCreateTexture && !pVolumeTexture->GetD3DVolumeTexture9()) {
		CheckVolumeTexture9(pVolumeTexture);
		return pVolumeTexture;
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pVolumeTexture;
	}
	else {
		D3D9Wrapper::IDirect3DVolumeTexture9* pWrapper = (D3D9Wrapper::IDirect3DVolumeTexture9*) D3D9Wrapper::IDirect3DVolumeTexture9::m_List.GetDataPtr(pVolumeTexture);
		return pWrapper;
	}
}
static ::LPDIRECT3DBASETEXTURE9 baseTexture9(D3D9Wrapper::IDirect3DBaseTexture9 *pTexture)
{
	if (!pTexture) return 0;
	if (G->gDelayDeviceCreation && pTexture->pendingCreateTexture && !pTexture->GetD3DBaseTexture9()) {
		CheckTexture9(pTexture);
		return pTexture->GetD3DBaseTexture9();
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pTexture->GetD3DBaseTexture9();
	}
	else{
		return (::IDirect3DBaseTexture9*)pTexture;
	}
	return NULL;
}

static D3D9Wrapper::IDirect3DBaseTexture9* wrappedTexture9(D3D9Wrapper::IDirect3DBaseTexture9 *pTexture) {

	if (!pTexture) return NULL;
	if (G->gDelayDeviceCreation && pTexture->pendingCreateTexture && !pTexture->GetD3DBaseTexture9()) {
		CheckTexture9(pTexture);
		return pTexture;
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pTexture;
	}
	else {
		::D3DRESOURCETYPE type = ((::IDirect3DBaseTexture9*)pTexture)->GetType();
		D3D9Wrapper::IDirect3DBaseTexture9 *p = NULL;
		switch (type) {
		case ::D3DRTYPE_TEXTURE:
			p = (D3D9Wrapper::IDirect3DTexture9*) D3D9Wrapper::IDirect3DTexture9::m_List.GetDataPtr(pTexture);
			break;
		case ::D3DRTYPE_CUBETEXTURE:
			p = (D3D9Wrapper::IDirect3DCubeTexture9*) D3D9Wrapper::IDirect3DCubeTexture9::m_List.GetDataPtr(pTexture);
			break;
		case ::D3DRTYPE_VOLUMETEXTURE:
			p = (D3D9Wrapper::IDirect3DVolumeTexture9*) D3D9Wrapper::IDirect3DVolumeTexture9::m_List.GetDataPtr(pTexture);
			break;
		}
		return p;
	}
}
static ::IDirect3DVertexShader9* baseVertexShader9(D3D9Wrapper::IDirect3DVertexShader9 *pVertexShader) {

	if (!pVertexShader) return NULL;
	if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pVertexShader->GetD3DVertexShader9();

	}
	else {
		return (::IDirect3DVertexShader9*)pVertexShader;
	}
}
static D3D9Wrapper::IDirect3DVertexShader9* wrappedVertexShader9(D3D9Wrapper::IDirect3DVertexShader9 *pVertexShader) {

	if (!pVertexShader) return NULL;
	if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pVertexShader;

	}
	else{
		D3D9Wrapper::IDirect3DVertexShader9* pWrapper = (D3D9Wrapper::IDirect3DVertexShader9*) D3D9Wrapper::IDirect3DVertexShader9::m_List.GetDataPtr(pVertexShader);
		return pWrapper;

	}
}
static ::IDirect3DPixelShader9* basePixelShader9(D3D9Wrapper::IDirect3DPixelShader9 *pPixelShader) {

	if (!pPixelShader) return NULL;
	if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pPixelShader->GetD3DPixelShader9();

	}
	else {
		return (::IDirect3DPixelShader9*)pPixelShader;
	}
}
static D3D9Wrapper::IDirect3DPixelShader9* wrappedPixelShader9(D3D9Wrapper::IDirect3DPixelShader9 *pPixelShader) {

	if (!pPixelShader) return NULL;
	if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pPixelShader;

	}
	else {
		D3D9Wrapper::IDirect3DPixelShader9* pWrapper = (D3D9Wrapper::IDirect3DPixelShader9*) D3D9Wrapper::IDirect3DPixelShader9::m_List.GetDataPtr(pPixelShader);
		return pWrapper;

	}
}
static D3D9Wrapper::IDirect3DDevice9* wrappedDevice9(D3D9Wrapper::IDirect3DDevice9 *pDevice) {

	if (!pDevice) return NULL;
	if (G->gDelayDeviceCreation && !pDevice->GetD3D9Device()) {
		CheckDevice(pDevice);
		return pDevice;
	}else if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pDevice;
	}
	else {
		D3D9Wrapper::IDirect3DDevice9* pWrapper = (D3D9Wrapper::IDirect3DDevice9*) D3D9Wrapper::IDirect3DDevice9::m_List.GetDataPtr(pDevice);
		return pWrapper;

	}
}
static ::IDirect3DStateBlock9* baseStateBlock9(D3D9Wrapper::IDirect3DStateBlock9 *pStateBlock) {

	if (!pStateBlock) return NULL;
	if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pStateBlock->GetD3DStateBlock9();
	}
	else {
		return (::IDirect3DStateBlock9*)pStateBlock;
	}
}
static D3D9Wrapper::IDirect3DStateBlock9* wrappedStateBlock9(D3D9Wrapper::IDirect3DStateBlock9 *pStateBlock) {

	if (!pStateBlock) return NULL;
	if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
		return pStateBlock;
	}
	else {
		D3D9Wrapper::IDirect3DStateBlock9* pWrapper = (D3D9Wrapper::IDirect3DStateBlock9*) D3D9Wrapper::IDirect3DStateBlock9::m_List.GetDataPtr(pStateBlock);
		return pWrapper;

	}
}
static void ForceDisplayMode(::D3DPRESENT_PARAMETERS *PresentParams, ::D3DDISPLAYMODEEX* FullscreenDisplayMode = NULL)
{
	// Historically we have only forced the refresh rate when full-screen.
	// Not positive if it would hurt to do otherwise, but for now assuming
	// we might have had a good reason and keeping that behaviour. See also
	// the note in ResizeTarget().
	if (G->SCREEN_REFRESH >= 0 && !PresentParams->Windowed)
	{
		// FIXME: This may disable flipping (and use blitting instead)
		// if the forced numerator and denominator does not exactly
		// match a mode enumerated on the output. e.g. We would force
		// 60Hz as 60/1, but the display might actually use 60000/1001
		// for 60Hz and we would lose flipping and degrade performance.
		//BufferDesc->RefreshRate.Numerator = G->SCREEN_REFRESH;
		//BufferDesc->RefreshRate.Denominator = 1;
		PresentParams->FullScreen_RefreshRateInHz = G->SCREEN_REFRESH;
		LogInfo("->Forcing refresh rate to = %f\n",
			(float)PresentParams->FullScreen_RefreshRateInHz);
		if (FullscreenDisplayMode != NULL)
			FullscreenDisplayMode->RefreshRate = G->SCREEN_REFRESH;
	}
	if (G->SCREEN_WIDTH >= 0)
	{
		PresentParams->BackBufferWidth = G->SCREEN_WIDTH;
		LogInfo("->Forcing Width to = %d\n", PresentParams->BackBufferWidth);

		if (FullscreenDisplayMode != NULL)
			FullscreenDisplayMode->Width = G->SCREEN_WIDTH;
	}
	if (G->SCREEN_HEIGHT >= 0)
	{
		PresentParams->BackBufferHeight = G->SCREEN_HEIGHT;
		LogInfo("->Forcing Height to = %d\n", PresentParams->BackBufferHeight);

		if (FullscreenDisplayMode != NULL)
			FullscreenDisplayMode->Height = G->SCREEN_HEIGHT;
	}

	// To support 3D Vision Direct Mode, we need to force the backbuffer from the
	// swapchain to be 2x its normal width.
	//
	// I don't particularly like that we've lumped this in with direct mode
	// - direct mode does *not* require a 2x width back buffer - it has two
	// completely separate back buffers, switched via nvapi. This is a hack
	// for one specific tool that has yet to see the light of day, and
	// ideally this would have been done in that tool, not here. For
	// quickly getting up and running, I don't see why the ordinary
	// resolution overrides would not have been sufficient, or adding a
	// multiplier to those if necessary.
	//   - DarkStarSword
	//if (G->gForceStereo == 2)
	//{
	//	PresentParams->BackBufferWidth *= 2;
	//	LogInfo("->Direct Mode: Forcing Width to = %d\n", PresentParams->BackBufferWidth);

	//}
}
void ForceDisplayParams(::D3DPRESENT_PARAMETERS *PresentParams, ::D3DDISPLAYMODEEX* FullscreenDisplayMode = NULL)
{
	if (PresentParams == NULL)
		return;

	LogInfo("     Windowed = %d\n", PresentParams->Windowed);
	LogInfo("     Width = %d\n", PresentParams->BackBufferWidth);
	LogInfo("     Height = %d\n", PresentParams->BackBufferHeight);
	LogInfo("     Refresh rate = %f\n",
		(float)PresentParams->FullScreen_RefreshRateInHz);

	if (G->SCREEN_UPSCALING == 2 || G->SCREEN_FULLSCREEN > 0)
	{
		PresentParams->Windowed = false;
		LogInfo("->Forcing Windowed to = %d\n", PresentParams->Windowed);
	}

	if (G->SCREEN_FULLSCREEN == 2 || G->SCREEN_UPSCALING > 0)
	{
		// We install this hook on demand to avoid any possible
		// issues with hooking the call when we don't need it:
		// Unconfirmed, but possibly related to:
		// https://forums.geforce.com/default/topic/685657/3d-vision/3dmigoto-now-open-source-/post/4801159/#4801159
		//
		// This hook is very important in case of upscaling
		InstallSetWindowPosHook();
	}

	ForceDisplayMode(PresentParams, FullscreenDisplayMode);
}
void DirectModePreCommandList(D3D9Wrapper::IDirect3DDevice9 *hackerDevice)//, D3D9Wrapper::FakeSwapChain *fakeSwapChain, D3D9Wrapper::IDirect3DSwapChain9 *swapChain = NULL)
{
	if (G->gForceStereo != 2)
		return;
}
struct CUSTOMVERTEX_DRAWCOMMAND
{
	float vertexID;
};
void DirectModePostCommandList(D3D9Wrapper::IDirect3DDevice9 *hackerDevice, D3D9Wrapper::IDirect3DSwapChain9 *swapChain = NULL)
{
	if (G->gForceStereo != 2)
		return;
	D3D9Wrapper::FakeSwapChain *fakeSwapChain;
	::IDirect3DSurface9 *pRealBackBuffer;
	::IDirect3DSurface9 *pLeftSurface;
	::IDirect3DSurface9 *pRightSurface;
	if (swapChain == NULL) {
		hackerDevice->GetD3D9Device()->GetBackBuffer(0, 0, ::D3DBACKBUFFER_TYPE_MONO, &pRealBackBuffer);
		fakeSwapChain = &hackerDevice->mFakeSwapChains[0];
	}
	else {
		swapChain->GetSwapChain9()->GetBackBuffer(0, ::D3DBACKBUFFER_TYPE_MONO, &pRealBackBuffer);
		fakeSwapChain = swapChain->mFakeSwapChain;
	}
	::D3DSURFACE_DESC desc;
	pRealBackBuffer->GetDesc(&desc);
	if (G->SCREEN_UPSCALING > 0) {
		pLeftSurface = fakeSwapChain->mDirectModeUpscalingBackBuffers.at(0)->DirectModeGetLeft();
		pRightSurface = fakeSwapChain->mDirectModeUpscalingBackBuffers.at(0)->DirectModeGetRight();
	}
	else {
		pLeftSurface = fakeSwapChain->mFakeBackBuffers.at(0)->DirectModeGetLeft();
		pRightSurface = fakeSwapChain->mFakeBackBuffers.at(0)->DirectModeGetRight();
	}
	hackerDevice->GetD3D9Device()->StretchRect(pLeftSurface, NULL, pRealBackBuffer, NULL, ::D3DTEXF_POINT);
	pRealBackBuffer->Release();
}
void RunFrameActions(D3D9Wrapper::IDirect3DDevice9 *hackerDevice, CachedStereoValues *cachedStereoValues = NULL)
{
	LogDebug("Running frame actions.  Device: %p\n", hackerDevice);

	//if (G->frame_no == 0) {
	//	G->fps_start_time = chrono::high_resolution_clock::now();
	//	G->fps_frame_no = 0;
	//}
	//else {
	//	G->fps_frame_no++;
	//	std::chrono::duration<double, std::milli> fps_time_passed = chrono::high_resolution_clock::now() - G->fps_start_time;
	//	if (fps_time_passed > chrono::milliseconds(2000) && G->fps_frame_no > 10)
	//	{
	//		G->fps = (unsigned int)round((double)G->fps_frame_no / (fps_time_passed.count() / 1000.0));
	//		//LogOverlay(LOG_WARNING, "FPS = %i\n", G->fps);
	//		if (G->fps <= 55) {
	//			std::chrono::duration<double, std::milli> fps_time_since_last_flush = chrono::high_resolution_clock::now() - G->fps_vram_last_flushed;
	//			if (fps_time_since_last_flush > chrono::milliseconds(5000))
	//			{
	//				INPUT ip;
	//				// Set up a generic keyboard event.
	//				WORD vkey = VK_F5;
	//				ip.type = INPUT_KEYBOARD;
	//				ip.ki.wScan = MapVirtualKey(vkey, MAPVK_VK_TO_VSC);
	//				ip.ki.time = 0;
	//				ip.ki.dwExtraInfo = 0;
	//				ip.ki.dwFlags = 0;
	//				// Press the "A" key
	//				ip.ki.wVk = vkey; // virtual-key code for the "a" key
	//				ip.ki.dwFlags = 0; // 0 for key press
	//				SendInput(1, &ip, sizeof(INPUT));

	//				// Release the "A" key
	//				ip.ki.dwFlags = KEYEVENTF_KEYUP; // KEYEVENTF_KEYUP for key release
	//				SendInput(1, &ip, sizeof(INPUT));
	//				G->fps_vram_last_flushed = chrono::high_resolution_clock::now();
	//			}
	//		}
	//		G->fps_start_time = chrono::high_resolution_clock::now();
	//		G->fps_frame_no = 0;
	//	}
	//}

	// Regardless of log settings, since this runs every frame, let's flush the log
	// so that the most lost will be one frame worth.  Tradeoff of performance to accuracy
	if (LogFile) fflush(LogFile);

	hackerDevice->GetD3D9Device()->BeginScene();
	// Run the command list here, before drawing the overlay so that a
	// custom shader on the present call won't remove the overlay. Also,
	// run this before most frame actions so that this can be considered as
	// a pre-present command list. We have a separate post-present command
	// list after the present call in case we need to restore state or
	// affect something at the start of the frame.
	DirectModePreCommandList(hackerDevice);
	RunCommandList(hackerDevice, &G->present_command_list, NULL, false, cachedStereoValues);
	DirectModePostCommandList(hackerDevice);
	if (G->analyse_frame) {
		// We don't allow hold to be changed mid-frame due to potential
		// for filename conflicts, so use def_analyse_options:
		if (G->def_analyse_options & FrameAnalysisOptions::HOLD) {
			// If using analyse_options=hold we don't stop the
			// analysis at the frame boundary (it will be stopped
			// at the key up event instead), but we do increment
			// the frame count and reset the draw count:
			G->analyse_frame_no++;
		}
		else {
			G->analyse_frame = false;
			if (G->DumpUsage)
				DumpUsage(G->ANALYSIS_PATH);
			LogOverlay(LOG_INFO, "Frame analysis saved to %S\n", G->ANALYSIS_PATH);
		}
	}

	// NOTE: Now that key overrides can check an ini param, the ordering of
	// this and the present_command_list is significant. We might set an
	// ini param during a frame for scene detection, which is checked on
	// override activation, then cleared from the command list run on
	// present. If we ever needed to run the command list before this
	// point, we should consider making an explicit "pre" command list for
	// that purpose rather than breaking the existing behaviour.
	bool newEvent = DispatchInputEvents(hackerDevice);

	CurrentTransition.UpdatePresets(hackerDevice, cachedStereoValues);
	CurrentTransition.UpdateTransitions(hackerDevice, cachedStereoValues);

	// The config file is not safe to reload from within the input handler
	// since it needs to change the key bindings, so it sets this flag
	// instead and we handle it now.
	if (G->gReloadConfigPending)
		ReloadConfig(hackerDevice);

	// Draw the on-screen overlay text with hunting and informational
	// messages, before final Present. We now do this after the shader and
	// config reloads, so if they have any notices we will see them this
	// frame (just in case we crash next frame or something).
	if (hackerDevice->getOverlay() && !G->suppress_overlay)
		hackerDevice->getOverlay()->DrawOverlay(cachedStereoValues);
	G->suppress_overlay = false;

	hackerDevice->GetD3D9Device()->EndScene();

	// This must happen on the same side of the config and shader reloads
	// to ensure the config reload can't clear messages from the shader
	// reload. It doesn't really matter which side we do it on at the
	// moment, but let's do it last, because logically it makes sense to be
	// incremented when we call the original present call:
	G->frame_no++;

	// When not hunting most keybindings won't have been registered, but
	// still skip the below logic that only applies while hunting.
	if (G->hunting != HUNTING_MODE_ENABLED)
		return;

	// Update the huntTime whenever we get fresh user input.
	if (newEvent)
		G->huntTime = time(NULL);

	// Clear buffers after some user idle time.  This allows the buffers to be
	// stable during a hunt, and cleared after one minute of idle time.  The idea
	// is to make the arrays of shaders stable so that hunting up and down the arrays
	// is consistent, while the user is engaged.  After 1 minute, they are likely onto
	// some other spot, and we should start with a fresh set, to keep the arrays and
	// active shader list small for easier hunting.  Until the first keypress, the arrays
	// are cleared at each thread wake, just like before.
	// The arrays will be continually filled by the SetShader sections, but should
	// rapidly converge upon all active shaders.

	if (difftime(time(NULL), G->huntTime) > 60) {
		EnterCriticalSection(&G->mCriticalSection);
		TimeoutHuntingBuffers();
		LeaveCriticalSection(&G->mCriticalSection);
	}
}
// Only Texture2D surfaces can be square. Use template specialisation to skip
// the check on other resource types:
template <typename DescType>
static bool is_square_surface(DescType *desc) {
	return false;
}

static bool is_square_surface(const D3D2DTEXTURE_DESC *desc)
{
	return (desc->Width == desc->Height);
}

static bool ShouldDuplicate(D3D2DTEXTURE_DESC * pDesc)
{
	if (pDesc->Usage & D3DUSAGE_DEPTHSTENCIL || pDesc->Usage & D3DUSAGE_RENDERTARGET) {
		if (!is_square_surface(pDesc) || G->gSurfaceSquareCreateMode != 2) {
			if (G->gDirectModeStereoLargeSurfacesOnly == false || (pDesc->Width >= (UINT)G->GAME_INTERNAL_WIDTH() && pDesc->Height >= (UINT)G->GAME_INTERNAL_HEIGHT())) {
				if (G->gDirectModeStereoSmallerThanBackBuffer == false || (pDesc->Width <= (UINT)G->GAME_INTERNAL_WIDTH() && pDesc->Height <= (UINT)G->GAME_INTERNAL_HEIGHT()))
					if (G->gDirectModeStereoMinSurfaceArea <= 0 || ((pDesc->Width * pDesc->Height) >= (UINT)G->gDirectModeStereoMinSurfaceArea))
						return true;
			}
		}
	}
	return false;
}
static void UnWrapTexture(D3D9Wrapper::IDirect3DBaseTexture9* pWrappedTexture, ::IDirect3DBaseTexture9** ppActualLeftTexture, ::IDirect3DBaseTexture9** ppActualRightTexture) {
	if (!pWrappedTexture)
		return;

	::D3DRESOURCETYPE type = pWrappedTexture->GetD3DBaseTexture9()->GetType();

	*ppActualLeftTexture = NULL;
	*ppActualRightTexture = NULL;

	switch (type)
	{
	case ::D3DRTYPE_TEXTURE:
	{
		D3D9Wrapper::IDirect3DTexture9* pDerivedTexture = reinterpret_cast<D3D9Wrapper::IDirect3DTexture9*> (pWrappedTexture);
		*ppActualLeftTexture = pDerivedTexture->DirectModeGetLeft();
		*ppActualRightTexture = pDerivedTexture->DirectModeGetRight();

		break;
	}
	case ::D3DRTYPE_CUBETEXTURE:
	{
		D3D9Wrapper::IDirect3DCubeTexture9* pDerivedTexture = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*> (pWrappedTexture);
		*ppActualLeftTexture = pDerivedTexture->DirectModeGetLeft();
		*ppActualRightTexture = pDerivedTexture->DirectModeGetRight();
		break;
	}
	case ::D3DRTYPE_VOLUMETEXTURE:
	{
		D3D9Wrapper::IDirect3DVolumeTexture9* pDerivedTexture = reinterpret_cast<D3D9Wrapper::IDirect3DVolumeTexture9*> (pWrappedTexture);
		*ppActualLeftTexture = pDerivedTexture->GetD3DVolumeTexture9();
		break;
	}
	default:
		LogDebug("Direct Mode, Unhandled texture type in SetTexture\n");
		break;
	}

	if ((*ppActualLeftTexture) == NULL) {
		LogDebug("Direct Mode, No left texture\n");
	}
}
#include "Direct3D9Functions.h"
#include "Direct3DDevice9Functions.h"

#include "FrameAnalysisFunctions.h"

#include "Direct3DSwapChain9Functions.h"

#include "Direct3DResource9Functions.h"

#include "Direct3DSurface9Functions.h"
#include "Direct3DVertexDeclaration9Functions.h"
#include "Direct3DBaseTexture9Functions.h"

#include "Direct3DTexture9Functions.h"
#include "Direct3DVertexBuffer9Functions.h"
#include "Direct3DIndexBuffer9Functions.h"
#include "Direct3DQuery9Functions.h"
#include "Direct3DVertexShader9Functions.h"
#include "Direct3DPixelShader9Functions.h"
#include "Direct3DShader9Functions.h"

#include "Direct3DCubeTexture9Functions.h"
#include "Direct3DVolume9Functions.h"
#include "Direct3DVolumeTexture9Functions.h"

#include "Direct3DStateBlock9Functions.h"

#include "HookedD3DXFunctions.h"
