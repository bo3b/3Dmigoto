#include "Main.h"
#include <Shlobj.h>

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

static HMODULE hD3D;
static FILE *LogFile = 0;
static bool LogInput = false, LogDebug = false;
static bool gInitialized = false;
static int SCREEN_WIDTH = -1;
static int SCREEN_WIDTH_DELAY = -1;
static int SCREEN_HEIGHT = -1;
static int SCREEN_HEIGHT_DELAY = -1;
static int SCREEN_REFRESH = -1;
static int SCREEN_REFRESH_DELAY = -1;
static int SCREEN_FULLSCREEN = -1;
static wchar_t DLL_PATH[MAX_PATH] = { 0 };
static bool gDelayDeviceCreation = false;

struct SwapChainInfo
{
	int width, height;
};

void InitializeDLL()
{
	if (!gInitialized)
	{
		gInitialized = true;
		wchar_t dir[MAX_PATH];
		GetModuleFileName(0, dir, MAX_PATH);
		wcsrchr(dir, L'\\')[1] = 0;
		wcscat(dir, L"d3dx.ini");
		LogFile = GetPrivateProfileInt(L"Logging", L"calls", 0, dir) ? (FILE *)-1 : 0;
		if (LogFile)
			LogFile = fopen("d3d9_log.txt", "w");
		LogInput = GetPrivateProfileInt(L"Logging", L"input", 0, dir);
		LogDebug = GetPrivateProfileInt(L"Logging", L"debug", 0, dir);
		wchar_t val[MAX_PATH];
		SCREEN_WIDTH = GetPrivateProfileInt(L"Device", L"width", -1, dir);
		SCREEN_HEIGHT = GetPrivateProfileInt(L"Device", L"height", -1, dir);
		SCREEN_REFRESH = GetPrivateProfileInt(L"Device", L"refresh_rate", -1, dir);
		SCREEN_FULLSCREEN = GetPrivateProfileInt(L"Device", L"full_screen", -1, dir);
		GetPrivateProfileString(L"System", L"proxy_d3d9", 0, DLL_PATH, MAX_PATH, dir);
		gDelayDeviceCreation = GetPrivateProfileInt(L"Device", L"delay_devicecreation", 0, dir) ? true : false;
		if (SCREEN_FULLSCREEN == 2)
		{
			SCREEN_WIDTH_DELAY = SCREEN_WIDTH; SCREEN_WIDTH = -1;
			SCREEN_HEIGHT_DELAY = SCREEN_HEIGHT; SCREEN_HEIGHT = -1;
			SCREEN_REFRESH_DELAY = SCREEN_REFRESH; SCREEN_REFRESH = -1;
		}

		if (LogFile) fprintf(LogFile, "DLL initialized.\n");
	}
}

void DestroyDLL()
{
	if (LogFile)
	{
		if (LogFile) fprintf(LogFile, "Destroying DLL...\n");
		fclose(LogFile);
	}
}

int WINAPI D3DPERF_BeginEvent(DWORD col, LPCWSTR wszName)
{
	if (LogFile) fprintf(LogFile, "D3DPERF_BeginEvent called\n");

	D3D9Wrapper::D3DPERF_BeginEvent call = (D3D9Wrapper::D3DPERF_BeginEvent)GetProcAddress(hD3D, "D3DPERF_BeginEvent");
	return (*call)(col, wszName);
}

int WINAPI D3DPERF_EndEvent()
{
	if (LogFile) fprintf(LogFile, "D3DPERF_EndEvent called\n");

	D3D9Wrapper::D3DPERF_EndEvent call = (D3D9Wrapper::D3DPERF_EndEvent)GetProcAddress(hD3D, "D3DPERF_EndEvent");
	return (*call)();
}

DWORD WINAPI D3DPERF_GetStatus()
{
	if (LogFile) fprintf(LogFile, "D3DPERF_GetStatus called\n");

	D3D9Wrapper::D3DPERF_GetStatus call = (D3D9Wrapper::D3DPERF_GetStatus)GetProcAddress(hD3D, "D3DPERF_GetStatus");
	return (*call)();
}

BOOL WINAPI D3DPERF_QueryRepeatFrame()
{
	if (LogFile) fprintf(LogFile, "D3DPERF_QueryRepeatFrame called\n");

	D3D9Wrapper::D3DPERF_QueryRepeatFrame call = (D3D9Wrapper::D3DPERF_QueryRepeatFrame)GetProcAddress(hD3D, "D3DPERF_QueryRepeatFrame");
	return (*call)();
}

void WINAPI D3DPERF_SetMarker(D3D9Base::D3DCOLOR color, LPCWSTR name)
{
	if (LogFile) fprintf(LogFile, "D3DPERF_SetMarker called\n");

	D3D9Wrapper::D3DPERF_SetMarker call = (D3D9Wrapper::D3DPERF_SetMarker)GetProcAddress(hD3D, "D3DPERF_SetMarker");
	(*call)(color, name);
}

void WINAPI D3DPERF_SetOptions(DWORD options)
{
	if (LogFile) fprintf(LogFile, "D3DPERF_SetOptions called\n");

	D3D9Wrapper::D3DPERF_SetOptions call = (D3D9Wrapper::D3DPERF_SetOptions)GetProcAddress(hD3D, "D3DPERF_SetOptions");
	(*call)(options);
}

void WINAPI D3DPERF_SetRegion(D3D9Base::D3DCOLOR color, LPCWSTR name)
{
	if (LogFile) fprintf(LogFile, "D3DPERF_SetRegion called\n");

	D3D9Wrapper::D3DPERF_SetRegion call = (D3D9Wrapper::D3DPERF_SetRegion)GetProcAddress(hD3D, "D3DPERF_SetRegion");
	(*call)(color, name);
}

void WINAPI DebugSetLevel(int a1, int a2)
{
	if (LogFile) fprintf(LogFile, "DebugSetLevel called\n");

	D3D9Wrapper::DebugSetLevel call = (D3D9Wrapper::DebugSetLevel)GetProcAddress(hD3D, "DebugSetLevel");
	(*call)(a1, a2);
}

void WINAPI DebugSetMute(int a)
{
	if (LogFile) fprintf(LogFile, "DebugSetMute called\n");

	D3D9Wrapper::DebugSetMute call = (D3D9Wrapper::DebugSetMute)GetProcAddress(hD3D, "DebugSetMute");
	(*call)(a);
}

void *WINAPI Direct3DShaderValidatorCreate9()
{
	if (LogFile) fprintf(LogFile, "Direct3DShaderValidatorCreate9 called\n");

	D3D9Wrapper::Direct3DShaderValidatorCreate9 call = (D3D9Wrapper::Direct3DShaderValidatorCreate9)GetProcAddress(hD3D, "Direct3DShaderValidatorCreate9");
	return (*call)();
}

void WINAPI PSGPError(void *D3DFE_PROCESSVERTICES, int PSGPERRORID, unsigned int a)
{
	if (LogFile) fprintf(LogFile, "PSGPError called\n");

	D3D9Wrapper::PSGPError call = (D3D9Wrapper::PSGPError)GetProcAddress(hD3D, "PSGPError");
	(*call)(D3DFE_PROCESSVERTICES, PSGPERRORID, a);
}

void WINAPI PSGPSampleTexture(void *D3DFE_PROCESSVERTICES, unsigned int a, float (* const b)[4], unsigned int c, float (* const d)[4])
{
	if (LogFile) fprintf(LogFile, "PSGPSampleTexture called\n");

	D3D9Wrapper::PSGPSampleTexture call = (D3D9Wrapper::PSGPSampleTexture)GetProcAddress(hD3D, "PSGPSampleTexture");
	(*call)(D3DFE_PROCESSVERTICES, a, b, c, d);
}

STDMETHODIMP D3D9Wrapper::IDirect3DUnknown::QueryInterface(THIS_ REFIID riid, void** ppvObj)
{
	IID m1 = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x01 } };
	IID m2 = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x02 } };
	IID m3 = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x03 } };
	if (riid.Data1 == m1.Data1 && riid.Data2 == m1.Data2 && riid.Data3 == m1.Data3 && 
		riid.Data4[0] == m1.Data4[0] && riid.Data4[1] == m1.Data4[1] && riid.Data4[2] == m1.Data4[2] && riid.Data4[3] == m1.Data4[3] && 
		riid.Data4[4] == m1.Data4[4] && riid.Data4[5] == m1.Data4[5] && riid.Data4[6] == m1.Data4[6] && riid.Data4[7] == m1.Data4[7])
	{
		if (LogFile) fprintf(LogFile, "Callback from dxgi.dll wrapper: requesting real ID3D9Device handle from %x\n", *ppvObj);
	
	    D3D9Wrapper::IDirect3DDevice9 *p = (D3D9Wrapper::IDirect3DDevice9*) D3D9Wrapper::IDirect3DDevice9::m_List.GetDataPtr(*ppvObj);
		if (p)
		{
			if (LogFile) fprintf(LogFile, "  given pointer was already the real device.\n");
		}
		else
		{
			*ppvObj = ((D3D9Wrapper::IDirect3DDevice9 *)*ppvObj)->GetD3D9Device();
		}
		if (LogFile) fprintf(LogFile, "  returning handle = %x\n", *ppvObj);
	
		return 0x13bc7e31;
	}
	else if (riid.Data1 == m2.Data1 && riid.Data2 == m2.Data2 && riid.Data3 == m2.Data3 && 
		riid.Data4[0] == m2.Data4[0] && riid.Data4[1] == m2.Data4[1] && riid.Data4[2] == m2.Data4[2] && riid.Data4[3] == m2.Data4[3] && 
		riid.Data4[4] == m2.Data4[4] && riid.Data4[5] == m2.Data4[5] && riid.Data4[6] == m2.Data4[6] && riid.Data4[7] == m2.Data4[7])
	{
		if (LogFile && LogDebug) fprintf(LogFile, "Callback from dxgi.dll wrapper: notification #%d received\n", (int) *ppvObj);

		switch ((int) *ppvObj)
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
		return 0x13bc7e31;
	}
	else if (riid.Data1 == m3.Data1 && riid.Data2 == m3.Data2 && riid.Data3 == m3.Data3 && 
		riid.Data4[0] == m3.Data4[0] && riid.Data4[1] == m3.Data4[1] && riid.Data4[2] == m3.Data4[2] && riid.Data4[3] == m3.Data4[3] && 
		riid.Data4[4] == m3.Data4[4] && riid.Data4[5] == m3.Data4[5] && riid.Data4[6] == m3.Data4[6] && riid.Data4[7] == m3.Data4[7])
	{
		SwapChainInfo *info = (SwapChainInfo *)*ppvObj;
		if (LogFile) fprintf(LogFile, "Callback from dxgi.dll wrapper: screen resolution width=%d, height=%d received\n", 
			info->width, info->height);
	
		//G->mSwapChainInfo = *info;
		return 0x13bc7e31;
	}

	if (LogFile) fprintf(LogFile, "QueryInterface request for %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx on %x\n", 
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7], this);

	HRESULT hr = m_pUnk->QueryInterface(riid, ppvObj);
	if (hr == S_OK)
	{
		D3D9Wrapper::IDirect3DDevice9 *p1 = (D3D9Wrapper::IDirect3DDevice9*) D3D9Wrapper::IDirect3DDevice9::m_List.GetDataPtr(*ppvObj);
		if (p1)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p1;
			unsigned long cnt2 = p1->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with IDirect3DDevice9 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p1->m_ulRef, cnt2);
		}
		D3D9Wrapper::IDirect3DSwapChain9 *p2 = (D3D9Wrapper::IDirect3DSwapChain9*) D3D9Wrapper::IDirect3DSwapChain9::m_List.GetDataPtr(*ppvObj);
		if (p2)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p2;
			unsigned long cnt2 = p2->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with IDirect3DSwapChain9 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p2->m_ulRef, cnt2);
		}
		D3D9Wrapper::IDirect3D9 *p3 = (D3D9Wrapper::IDirect3D9*) D3D9Wrapper::IDirect3D9::m_List.GetDataPtr(*ppvObj);
		if (p3)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p3;
			unsigned long cnt2 = p3->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with IDirect3D9 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p3->m_ulRef, cnt2);
		}
		D3D9Wrapper::IDirect3DSurface9 *p4 = (D3D9Wrapper::IDirect3DSurface9*) D3D9Wrapper::IDirect3DSurface9::m_List.GetDataPtr(*ppvObj);
		if (p4)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p4;
			unsigned long cnt2 = p4->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with IDirect3DSurface9 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p4->m_ulRef, cnt2);
		}
		D3D9Wrapper::IDirect3DVertexDeclaration9 *p5 = (D3D9Wrapper::IDirect3DVertexDeclaration9*) D3D9Wrapper::IDirect3DVertexDeclaration9::m_List.GetDataPtr(*ppvObj);
		if (p5)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p5;
			unsigned long cnt2 = p5->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with IDirect3DVertexDeclaration9 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p5->m_ulRef, cnt2);
		}
		D3D9Wrapper::IDirect3DTexture9 *p6 = (D3D9Wrapper::IDirect3DTexture9*) D3D9Wrapper::IDirect3DTexture9::m_List.GetDataPtr(*ppvObj);
		if (p6)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p6;
			unsigned long cnt2 = p6->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with IDirect3DTexture9 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p6->m_ulRef, cnt2);
		}
		D3D9Wrapper::IDirect3DVertexBuffer9 *p7 = (D3D9Wrapper::IDirect3DVertexBuffer9*) D3D9Wrapper::IDirect3DVertexBuffer9::m_List.GetDataPtr(*ppvObj);
		if (p7)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p7;
			unsigned long cnt2 = p7->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with IDirect3DVertexBuffer9 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p7->m_ulRef, cnt2);
		}
		D3D9Wrapper::IDirect3DIndexBuffer9 *p8 = (D3D9Wrapper::IDirect3DIndexBuffer9*) D3D9Wrapper::IDirect3DIndexBuffer9::m_List.GetDataPtr(*ppvObj);
		if (p8)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p8;
			unsigned long cnt2 = p8->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with IDirect3DIndexBuffer9 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p8->m_ulRef, cnt2);
		}
		D3D9Wrapper::IDirect3DQuery9 *p9 = (D3D9Wrapper::IDirect3DQuery9*) D3D9Wrapper::IDirect3DQuery9::m_List.GetDataPtr(*ppvObj);
		if (p9)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p9;
			unsigned long cnt2 = p9->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with IDirect3DQuery9 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p9->m_ulRef, cnt2);
		}
		D3D9Wrapper::IDirect3DVertexShader9 *p10 = (D3D9Wrapper::IDirect3DVertexShader9*) D3D9Wrapper::IDirect3DVertexShader9::m_List.GetDataPtr(*ppvObj);
		if (p10)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p10;
			unsigned long cnt2 = p10->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with IDirect3DVertexShader9 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p10->m_ulRef, cnt2);
		}
		D3D9Wrapper::IDirect3DPixelShader9 *p11 = (D3D9Wrapper::IDirect3DPixelShader9*) D3D9Wrapper::IDirect3DPixelShader9::m_List.GetDataPtr(*ppvObj);
		if (p11)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p11;
			unsigned long cnt2 = p11->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with IDirect3DPixelShader9 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p11->m_ulRef, cnt2);
		}
	}
	if (LogFile) fprintf(LogFile, "  result = %x, handle = %x\n", hr, *ppvObj);

	return hr;
}

D3D9Wrapper::IDirect3D9* WINAPI Direct3DCreate9(UINT Version)
{
	InitializeDLL();
	if (DLL_PATH[0])
	{
		wchar_t sysDir[MAX_PATH];
		GetModuleFileName(0, sysDir, MAX_PATH);
		wcsrchr(sysDir, L'\\')[1] = 0;
		wcscat(sysDir, DLL_PATH);
		if (LogFile)
		{
			char path[MAX_PATH];
			wcstombs(path, sysDir, MAX_PATH);
			fprintf(LogFile, "trying to load %s\n", path);
		}
		hD3D = LoadLibrary(sysDir);	
		if (!hD3D)
		{
			if (LogFile)
			{
				char path[MAX_PATH];
				wcstombs(path, DLL_PATH, MAX_PATH);
				fprintf(LogFile, "load failed. Trying to load %s\n", path);
			}
			hD3D = LoadLibrary(DLL_PATH);
		}
	}
	else
	{
		wchar_t sysDir[MAX_PATH];
		SHGetFolderPath(0, CSIDL_SYSTEM, 0, SHGFP_TYPE_CURRENT, sysDir);
		wcscat(sysDir, L"\\d3d9.dll");
		if (LogFile)
		{
			char path[MAX_PATH];
			wcstombs(path, sysDir, MAX_PATH);
			fprintf(LogFile, "trying to load %s\n", path);
		}
		hD3D = LoadLibrary(sysDir);
	}
	
    if (!hD3D)
    {
        if (LogFile) fprintf(LogFile, "LoadLibrary on d3d9.dll failed\n");
	
        return NULL;
    }
    if (LogFile) fprintf(LogFile, "Direct3DCreate9 called with Version=%d\n", Version);


	D3D9Wrapper::D3DCREATE pCreate = (D3D9Wrapper::D3DCREATE)GetProcAddress(hD3D, "Direct3DCreate9Ex");
    if (!pCreate)
    {
        if (LogFile) fprintf(LogFile, "  could not find Direct3DCreate9Ex in d3d9.dll\n");
	
        return NULL;
    }
	D3D9Base::LPDIRECT3D9EX pD3D = NULL;
	HRESULT hr = pCreate(Version, &pD3D);
    if (FAILED(hr) || pD3D == NULL)
    {
		if (LogFile) fprintf(LogFile, "  failed with hr=%x\n", hr);
	
        return NULL;
    }
    
    D3D9Wrapper::IDirect3D9 *wrapper = D3D9Wrapper::IDirect3D9::GetDirect3D(pD3D);
    if (LogFile) fprintf(LogFile, "  returns handle=%x, wrapper=%x\n", pD3D, wrapper);

	return wrapper;
}

HRESULT WINAPI Direct3DCreate9Ex(UINT Version, D3D9Wrapper::IDirect3D9 **ppD3D)
{
	if (ppD3D)
	{
		*ppD3D = Direct3DCreate9(Version);
		return S_OK;
	}
	return E_OUTOFMEMORY;
}

static D3D9Base::LPDIRECT3DSURFACE9 replaceSurface9(D3D9Wrapper::IDirect3DSurface9 *pSurface);
static void CheckDevice(D3D9Wrapper::IDirect3DDevice9 *me)
{
	if (!me->GetD3D9Device())
	{
		if (LogFile) fprintf(LogFile, "  calling postponed CreateDevice.\n");
		HRESULT hr = me->_pD3D->CreateDeviceEx(
			me->_Adapter,
			me->_DeviceType, 
			me->_hFocusWindow, 
			me->_BehaviorFlags, 
			&me->_pPresentationParameters,
			&me->_pFullscreenDisplayMode,
			(D3D9Base::IDirect3DDevice9Ex**) &me->m_pUnk);
		if (FAILED(hr))
		{
			if (LogFile) fprintf(LogFile, "    failed creating device with result = %x\n", hr);
		
			return;
		}
		if (me->pendingCreateDepthStencilSurface)
		{
			if (LogFile) fprintf(LogFile, "  calling postponed CreateDepthStencilSurface.\n");
		
			hr = me->GetD3D9Device()->CreateDepthStencilSurface(
				me->pendingCreateDepthStencilSurface->_Width,
				me->pendingCreateDepthStencilSurface->_Height,
				me->pendingCreateDepthStencilSurface->_Format,
				me->pendingCreateDepthStencilSurface->_MultiSample,
				me->pendingCreateDepthStencilSurface->_MultisampleQuality,
				me->pendingCreateDepthStencilSurface->_Discard,
				(D3D9Base::IDirect3DSurface9**) &me->pendingCreateDepthStencilSurface->m_pUnk,
				0);
			if (FAILED(hr))
			{
				if (LogFile) fprintf(LogFile, "    failed creating depth stencil surface with result=%x\n", hr);
			
				return;
			}
			me->pendingCreateDepthStencilSurface = 0;
		}
		if (me->pendingSetDepthStencilSurface)
		{
			if (LogFile) fprintf(LogFile, "  calling postponed SetDepthStencilSurface.\n");
		
			D3D9Base::LPDIRECT3DSURFACE9 baseStencil = replaceSurface9(me->pendingSetDepthStencilSurface);
			hr = me->GetD3D9Device()->SetDepthStencilSurface(baseStencil);
			if (FAILED(hr))
			{
				if (LogFile) fprintf(LogFile, "    failed calling SetDepthStencilSurface with result = %x\n", hr);
			
				return;
			}
			me->pendingSetDepthStencilSurface = 0;
		}
	}
}

static void CheckVertexDeclaration9(D3D9Wrapper::IDirect3DVertexDeclaration9 *me)
{
	if (!me->pendingCreateVertexDeclaration)
		return;
	me->pendingCreateVertexDeclaration = false;
	CheckDevice(me->pendingDevice);

	if (LogFile) fprintf(LogFile, "  calling postponed CreateVertexDeclaration.\n");
	HRESULT hr = me->pendingDevice->GetD3D9Device()->CreateVertexDeclaration(&me->_VertexElements, (D3D9Base::IDirect3DVertexDeclaration9**) &me->m_pUnk);
	if (FAILED(hr))
	{
		if (LogFile) fprintf(LogFile, "    failed creating vertex declaration with result = %x\n", hr);
	
		return;
	}
}

static void CheckTexture9(D3D9Wrapper::IDirect3DTexture9 *me)
{
	if (me->pendingCreateTexture)
	{
		me->pendingCreateTexture = false;
		CheckDevice(me->_Device);	
		if (LogFile) fprintf(LogFile, "  calling postponed CreateTexture.\n");
		HRESULT hr = me->_Device->GetD3D9Device()->CreateTexture(me->_Width, me->_Height, me->_Levels, me->_Usage, me->_Format, me->_Pool, (D3D9Base::IDirect3DTexture9**) &me->m_pUnk, 0);
		if (FAILED(hr))
		{
			if (LogFile) fprintf(LogFile, "    failed creating texture with result = %x\n", hr);
		
			return;
		}
	}
	if (me->pendingLockUnlock)
	{
		me->pendingLockUnlock = false;
		if (LogFile) fprintf(LogFile, "  calling postponed Lock.\n");
		void *ppbData;
		D3D9Base::D3DLOCKED_RECT rect;
		HRESULT hr = me->LockRect(me->_Level, &rect, 0, me->_Flags);
		if (FAILED(hr))
		{
			if (LogFile) fprintf(LogFile, "    failed locking texture with result = %x\n", hr);
		
			return;
		}
		for (int y = 0; y < me->_Height; ++y)
			memcpy(((char*)rect.pBits) + y*rect.Pitch, me->_Buffer + y*me->_Width*4, rect.Pitch);
		hr = me->UnlockRect(me->_Level);
		if (FAILED(hr))
		{
			if (LogFile) fprintf(LogFile, "    failed unlocking texture with result = %x\n", hr);
		
			return;
		}
		delete me->_Buffer; me->_Buffer = 0;
	}

}

static void CheckSurface9(D3D9Wrapper::IDirect3DSurface9 *me)
{
	if (!me->pendingGetSurfaceLevel)
		return;
	me->pendingGetSurfaceLevel = false;
	CheckTexture9(me->_Texture);

	if (LogFile) fprintf(LogFile, "  calling postponed GetSurfaceLevel.\n");
	HRESULT hr = me->_Texture->GetD3DTexture9()->GetSurfaceLevel(me->_Level, (D3D9Base::IDirect3DSurface9**) &me->m_pUnk);
	if (FAILED(hr))
	{
		if (LogFile) fprintf(LogFile, "    failed getting surface with result = %x\n", hr);
	
		return;
	}
}

static void CheckVertexBuffer9(D3D9Wrapper::IDirect3DVertexBuffer9 *me)
{
	if (me->pendingCreateVertexBuffer)
	{
		me->pendingCreateVertexBuffer = false;
		CheckDevice(me->_Device);
		if (LogFile) fprintf(LogFile, "  calling postponed CreateVertexBuffer.\n");
		HRESULT hr = me->_Device->GetD3D9Device()->CreateVertexBuffer(me->_Length, me->_Usage, me->_FVF, me->_Pool, (D3D9Base::IDirect3DVertexBuffer9**) &me->m_pUnk, 0);
		if (FAILED(hr))
		{
			if (LogFile) fprintf(LogFile, "    failed creating vertex buffer with result = %x\n", hr);
		
			return;
		}
	}
	if (me->pendingLockUnlock)
	{
		me->pendingLockUnlock = false;
		if (LogFile) fprintf(LogFile, "  calling postponed Lock.\n");
		void *ppbData;
		HRESULT hr = me->Lock(0, me->_Length, &ppbData, me->_Flags);
		if (FAILED(hr))
		{
			if (LogFile) fprintf(LogFile, "    failed locking vertex buffer with result = %x\n", hr);
		
			return;
		}
		memcpy(ppbData, me->_Buffer, me->_Length);
		hr = me->Unlock();
		if (FAILED(hr))
		{
			if (LogFile) fprintf(LogFile, "    failed unlocking vertex buffer with result = %x\n", hr);
		
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
		if (LogFile) fprintf(LogFile, "  calling postponed CreateIndexBuffer.\n");
		HRESULT hr = me->_Device->GetD3D9Device()->CreateIndexBuffer(me->_Length, me->_Usage, me->_Format, me->_Pool, (D3D9Base::IDirect3DIndexBuffer9**) &me->m_pUnk, 0);
		if (FAILED(hr))
		{
			if (LogFile) fprintf(LogFile, "    failed creating index buffer with result = %x\n", hr);
		
			return;
		}
	}
	if (me->pendingLockUnlock)
	{
		me->pendingLockUnlock = false;
		if (LogFile) fprintf(LogFile, "  calling postponed Lock.\n");
		void *ppbData;
		HRESULT hr = me->Lock(0, me->_Length, &ppbData, me->_Flags);
		if (FAILED(hr))
		{
			if (LogFile) fprintf(LogFile, "    failed locking index buffer with result = %x\n", hr);
		
			return;
		}
		memcpy(ppbData, me->_Buffer, me->_Length);
		hr = me->Unlock();
		if (FAILED(hr))
		{
			if (LogFile) fprintf(LogFile, "    failed unlocking index buffer with result = %x\n", hr);
		
			return;
		}
		delete me->_Buffer; me->_Buffer = 0;
	}
}

static D3D9Base::LPDIRECT3DSURFACE9 replaceSurface9(D3D9Wrapper::IDirect3DSurface9 *pSurface)
{
	if (!pSurface) return 0;
    D3D9Wrapper::IDirect3DSurface9 *p = (D3D9Wrapper::IDirect3DSurface9*) D3D9Wrapper::IDirect3DSurface9::m_List.GetDataPtr(pSurface);
	if (!p && pSurface->magic == 0x7da43feb)
	{
		CheckSurface9(pSurface);
		if (LogFile && LogDebug) fprintf(LogFile, "    using Direct3DSurface9 %x from wrapper %x\n", pSurface->GetD3DSurface9(), pSurface);
		return pSurface->GetD3DSurface9();
	}
	if (LogFile && LogDebug) fprintf(LogFile, "    warning: original Direct3DSurface9 %x used\n", pSurface);
	return (D3D9Base::LPDIRECT3DSURFACE9) pSurface;
}

static D3D9Base::LPDIRECT3DVERTEXDECLARATION9 replaceVertexDeclaration9(D3D9Wrapper::IDirect3DVertexDeclaration9 *pVertexDeclaration)
{
	if (!pVertexDeclaration) return 0;
    D3D9Wrapper::IDirect3DVertexDeclaration9 *p = (D3D9Wrapper::IDirect3DVertexDeclaration9*) D3D9Wrapper::IDirect3DVertexDeclaration9::m_List.GetDataPtr(pVertexDeclaration);
	if (!p && pVertexDeclaration->magic == 0x7da43feb)
	{
		CheckVertexDeclaration9(pVertexDeclaration);
		if (LogFile && LogDebug) fprintf(LogFile, "    using Direct3DVertexDeclaration9 %x from wrapper %x\n", pVertexDeclaration->GetD3DVertexDeclaration9(), pVertexDeclaration);
		return pVertexDeclaration->GetD3DVertexDeclaration9();
	}
	if (LogFile && LogDebug) fprintf(LogFile, "    warning: original Direct3DVertexDeclaration9 %x used\n", pVertexDeclaration);
	return (D3D9Base::LPDIRECT3DVERTEXDECLARATION9) pVertexDeclaration;
}

static D3D9Base::LPDIRECT3DVERTEXBUFFER9 replaceVertexBuffer9(D3D9Wrapper::IDirect3DVertexBuffer9 *pVertexBuffer)
{
	if (!pVertexBuffer) return 0;
    D3D9Wrapper::IDirect3DVertexBuffer9 *p = (D3D9Wrapper::IDirect3DVertexBuffer9*) D3D9Wrapper::IDirect3DVertexBuffer9::m_List.GetDataPtr(pVertexBuffer);
	if (!p && pVertexBuffer->magic == 0x7da43feb)
	{
		CheckVertexBuffer9(pVertexBuffer);
		if (LogFile && LogDebug) fprintf(LogFile, "    using Direct3DVertexBuffer9 %x from wrapper %x\n", pVertexBuffer->GetD3DVertexBuffer9(), pVertexBuffer);
		return pVertexBuffer->GetD3DVertexBuffer9();
	}
	if (LogFile && LogDebug) fprintf(LogFile, "    warning: original Direct3DVertexBuffer9 %x used\n", pVertexBuffer);
	return (D3D9Base::LPDIRECT3DVERTEXBUFFER9) pVertexBuffer;
}

static D3D9Base::LPDIRECT3DINDEXBUFFER9 replaceIndexBuffer9(D3D9Wrapper::IDirect3DIndexBuffer9 *pIndexBuffer)
{
	if (!pIndexBuffer) return 0;
    D3D9Wrapper::IDirect3DIndexBuffer9 *p = (D3D9Wrapper::IDirect3DIndexBuffer9*) D3D9Wrapper::IDirect3DIndexBuffer9::m_List.GetDataPtr(pIndexBuffer);
	if (!p && pIndexBuffer->magic == 0x7da43feb)
	{
		CheckIndexBuffer9(pIndexBuffer);
		if (LogFile && LogDebug) fprintf(LogFile, "    using Direct3DIndexBuffer9 %x from wrapper %x\n", pIndexBuffer->GetD3DIndexBuffer9(), pIndexBuffer);
		return pIndexBuffer->GetD3DIndexBuffer9();
	}
	if (LogFile && LogDebug) fprintf(LogFile, "    warning: original Direct3DIndexBuffer9 %x used\n", pIndexBuffer);
	return (D3D9Base::LPDIRECT3DINDEXBUFFER9) pIndexBuffer;
}

static D3D9Base::LPDIRECT3DTEXTURE9 replaceTexture9(D3D9Wrapper::IDirect3DTexture9 *pTexture)
{
	if (!pTexture) return 0;
    D3D9Wrapper::IDirect3DTexture9 *p = (D3D9Wrapper::IDirect3DTexture9*) D3D9Wrapper::IDirect3DTexture9::m_List.GetDataPtr(pTexture);
	if (!p && pTexture->magic == 0x7da43feb)
	{
		CheckTexture9(pTexture);
		if (LogFile && LogDebug) fprintf(LogFile, "    using Direct3DTexture9 %x from wrapper %x\n", pTexture->GetD3DTexture9(), pTexture);
		return pTexture->GetD3DTexture9();
	}
	if (LogFile && LogDebug) fprintf(LogFile, "    warning: original Direct3DTexture9 %x used\n", pTexture);
	return (D3D9Base::LPDIRECT3DTEXTURE9) pTexture;
}

static D3D9Base::LPDIRECT3DVERTEXSHADER9 replaceVertexShader9(D3D9Wrapper::IDirect3DVertexShader9 *pVertexShader)
{
	if (!pVertexShader) return 0;
    D3D9Wrapper::IDirect3DVertexShader9 *p = (D3D9Wrapper::IDirect3DVertexShader9*) D3D9Wrapper::IDirect3DVertexShader9::m_List.GetDataPtr(pVertexShader);
	if (!p && pVertexShader->magic == 0x7da43feb)
	{
		if (LogFile && LogDebug) fprintf(LogFile, "    using Direct3DVertexShader9 %x from wrapper %x\n", pVertexShader->GetD3DVertexShader9(), pVertexShader);
		return pVertexShader->GetD3DVertexShader9();
	}
	if (LogFile && LogDebug) fprintf(LogFile, "    warning: original Direct3DVertexShader9 %x used\n", pVertexShader);
	return (D3D9Base::LPDIRECT3DVERTEXSHADER9) pVertexShader;
}

static D3D9Base::LPDIRECT3DPIXELSHADER9 replacePixelShader9(D3D9Wrapper::IDirect3DPixelShader9 *pPixelShader)
{
	if (!pPixelShader) return 0;
    D3D9Wrapper::IDirect3DPixelShader9 *p = (D3D9Wrapper::IDirect3DPixelShader9*) D3D9Wrapper::IDirect3DPixelShader9::m_List.GetDataPtr(pPixelShader);
	if (!p && pPixelShader->magic == 0x7da43feb)
	{
		if (LogFile && LogDebug) fprintf(LogFile, "    using Direct3DPixelShader9 %x from wrapper %x\n", pPixelShader->GetD3DPixelShader9(), pPixelShader);
		return pPixelShader->GetD3DPixelShader9();
	}
	if (LogFile && LogDebug) fprintf(LogFile, "    warning: original Direct3DPixelShader9 %x used\n", pPixelShader);
	return (D3D9Base::LPDIRECT3DPIXELSHADER9) pPixelShader;
}

#include "Direct3D9Functions.h"
#include "Direct3DDevice9Functions.h"
#include "Direct3DSwapChain9Functions.h"
#include "Direct3DSurface9Functions.h"
#include "Direct3DVertexDeclaration9Functions.h"
#include "Direct3DTexture9Functions.h"
#include "Direct3DVertexBuffer9Functions.h"
#include "Direct3DIndexBuffer9Functions.h"
#include "Direct3DQuery9Functions.h"
#include "Direct3DVertexShader9Functions.h"
#include "Direct3DPixelShader9Functions.h"
