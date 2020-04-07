#include "HookedD9.h"
#include "d3d9Wrapper.h"

void D3D9Wrapper::IDirect3D9::HookD9()
{

	// This will install hooks in the original device (if they have not
	// already been installed from a prior device) which will call the
	// equivalent function in this HackerDevice. It returns a trampoline
	// interface which we use in place of mOrigDevice to call the real
	// original device, thereby side stepping the problem that calling the
	// old mOrigDevice would be hooked and call back into us endlessly:
	if (_ex)
		m_pUnk = hook_D9(GetDirect3D9Ex(), reinterpret_cast<::IDirect3D9Ex*>(this));
	else
		m_pUnk = hook_D9(GetDirect3D9(), reinterpret_cast<::IDirect3D9*>(this));

}

inline void D3D9Wrapper::IDirect3D9::Delete()
{
	if (m_pRealUnk) m_List.DeleteMember(m_pRealUnk);
	m_pUnk = 0;
	m_pRealUnk = 0;
	delete this;
}

D3D9Wrapper::IDirect3D9::IDirect3D9(::LPDIRECT3D9 pD3D, bool ex)
    : IDirect3DUnknown((IUnknown*)pD3D),
	_ex(ex)
{
	if (G->enable_hooks >= EnableHooksDX9::ALL) {
		this->HookD9();
	}
	if (G->gAutoDetectDepthBuffer) {
		D3DDISPLAYMODE currentDisplayMode;
		GetDirect3D9()->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &currentDisplayMode);
		// determine if RESZ is supported
		m_isRESZ = GetDirect3D9()->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
			currentDisplayMode.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, D3DFMT_RESZ) == D3D_OK;
	}
}
D3D9Wrapper::IDirect3D9* D3D9Wrapper::IDirect3D9::GetDirect3D(::LPDIRECT3D9 pD3D, bool ex)
{
	D3D9Wrapper::IDirect3D9* p = new D3D9Wrapper::IDirect3D9(pD3D, ex);
	if (pD3D) m_List.AddMember(pD3D, p);
	return p;
}
STDMETHODIMP D3D9Wrapper::IDirect3D9::QueryInterface(THIS_ REFIID riid, void ** ppvObj)
{
	LogDebug("D3D9Wrapper::IDirect3D9::QueryInterface called\n");// at 'this': %s\n", type_name_dx9((IUnknown*)this));
	HRESULT hr = NULL;
	if (QueryInterface_DXGI_Callback(riid, ppvObj, &hr))
		return hr;
	LogInfo("QueryInterface request for %s on %p\n", NameFromIID(riid), this);
	hr = m_pUnk->QueryInterface(riid, ppvObj);
	if (hr == S_OK) {
		if ((*ppvObj) == GetRealOrig()) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppvObj = this;
				++m_ulRef;
				LogInfo("  interface replaced with IDirect3D9 wrapper.\n");
				LogInfo("  result = %x, handle = %p\n", hr, *ppvObj);
				return hr;
			}
		}
		D3D9Wrapper::IDirect3DUnknown *unk = QueryInterface_Find_Wrapper(*ppvObj);
		if (unk)
			*ppvObj = unk;
	}
	LogInfo("  result = %x, handle = %p\n", hr, *ppvObj);
	return hr;
}
STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3D9::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3D9::Release(THIS)
{
	LogDebug("IDirect3D9::Release handle=%p, counter=%lu, this=%p\n", m_pUnk, m_ulRef, this);

	ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);

	--m_ulRef;

    if (ulRef == 0)
    {
		if (!gLogDebug) LogInfo("IDirect3D9::Release handle=%p, counter=%lu, internal counter = %lu\n", m_pUnk, m_ulRef, ulRef);
		LogInfo("  deleting self\n");

		Delete();
    }
    return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::RegisterSoftwareDevice(THIS_ void* pInitializeFunction)
{
	LogInfo("IDirect3D9::RegisterSoftwareDevice called\n");
	return GetDirect3D9()->RegisterSoftwareDevice(pInitializeFunction);
}
STDMETHODIMP_(UINT) D3D9Wrapper::IDirect3D9::GetAdapterCount(THIS)
{
	LogInfo("IDirect3D9::GetAdapterCount called\n");

	UINT ret = GetDirect3D9()->GetAdapterCount();
	LogInfo("  return value = %d\n", ret);

	return ret;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::GetAdapterIdentifier(THIS_ UINT Adapter,DWORD Flags,::D3DADAPTER_IDENTIFIER9* pIdentifier)
{
	LogInfo("IDirect3D9::GetAdapterIdentifier called\n");

	HRESULT ret = GetDirect3D9()->GetAdapterIdentifier(Adapter, Flags, pIdentifier);
	if (ret == S_OK && LogFile)
	{
		LogInfo("  returns driver=%s, description=%s, GDI=%s\n", pIdentifier->Driver, pIdentifier->Description, pIdentifier->DeviceName);
	}
	return ret;
}

STDMETHODIMP_(UINT) D3D9Wrapper::IDirect3D9::GetAdapterModeCount(THIS_ UINT Adapter,::D3DFORMAT Format)
{
	LogInfo("IDirect3D9::GetAdapterModeCount called\n");

	UINT ret = GetDirect3D9()->GetAdapterModeCount(Adapter, Format);
	LogInfo("  return value = %d\n", ret);

	return ret;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::EnumAdapterModes(THIS_ UINT Adapter,::D3DFORMAT Format,UINT Mode,::D3DDISPLAYMODE* pMode)
{
	LogInfo("IDirect3D9::EnumAdapterModes called: adapter #%d requested with mode #%d\n", Adapter, Mode);

	HRESULT hr = GetDirect3D9()->EnumAdapterModes(Adapter, Format, Mode, pMode);
	if (hr == S_OK)
	{
		LogInfo("  driver returned width=%d, height=%d, refresh rate=%d\n", pMode->Width, pMode->Height, pMode->RefreshRate);

		/*
		if (SCREEN_REFRESH >= 0 && pMode->RefreshRate != SCREEN_REFRESH)
		{
			LogInfo("  video mode ignored because of mismatched refresh rate.\n");

			pMode->Width = pMode->Height = 0;
		}
		else if (SCREEN_WIDTH >= 0 && pMode->Width != SCREEN_WIDTH)
		{
			LogInfo("  video mode ignored because of mismatched screen width.\n");

			pMode->Width = pMode->Height = 0;
		}
		else if (SCREEN_HEIGHT >= 0 && pMode->Height != SCREEN_HEIGHT)
		{
			LogInfo("  video mode ignored because of mismatched screen height.\n");

			pMode->Width = pMode->Height = 0;
		}
		*/
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::GetAdapterDisplayMode(THIS_ UINT Adapter,::D3DDISPLAYMODE* pMode)
{
	LogInfo("IDirect3D9::GetAdapterDisplayMode called: adapter #%d\n", Adapter);

	HRESULT hr = GetDirect3D9()->GetAdapterDisplayMode(Adapter, pMode);
	if (hr == S_OK && pMode)
	{
		//if (G->UPSCALE_MODE > 0) {
		//	if (G->GAME_INTERNAL_WIDTH() > 1)
		//		pMode->Width = G->GAME_INTERNAL_WIDTH();
		//	if (G->GAME_INTERNAL_HEIGHT() > 1)
		//		pMode->Height = G->GAME_INTERNAL_HEIGHT();
		//}
//		if (G->GAME_INTERNAL_WIDTH() > 1 && G->FORCE_REPORT_GAME_RES) {
//			pMode->Width = G->GAME_INTERNAL_WIDTH();
//		}
///*		else if (G->FORCE_REPORT_WIDTH > 0) {
//			pMode->Width = G->FORCE_REPORT_WIDTH;
//		}*/else if (pMode->Width != G->SCREEN_WIDTH && G->SCREEN_WIDTH != -1)
//		{
//			LogInfo("  overriding Width %d with %d\n", pMode->Width, G->SCREEN_WIDTH);
//			pMode->Width = G->SCREEN_WIDTH;
//		}
//		if (G->GAME_INTERNAL_HEIGHT() > 1 && G->FORCE_REPORT_GAME_RES) {
//			pMode->Height = G->GAME_INTERNAL_HEIGHT();
//		}
///*		else if (G->FORCE_REPORT_HEIGHT > 0) {
//			pMode->Height = G->FORCE_REPORT_HEIGHT;
//		}*/else if (pMode->Height != G->SCREEN_HEIGHT && G->SCREEN_HEIGHT != -1)
//		{
//			LogInfo("  overriding Height %d with %d\n", pMode->Height, G->SCREEN_HEIGHT);
//			pMode->Height = G->SCREEN_HEIGHT;
//		}
//		if (pMode->RefreshRate != G->SCREEN_REFRESH && G->SCREEN_REFRESH != -1)
//		{
//			LogInfo("  overriding RefreshRate %d with %d\n", pMode->RefreshRate, G->SCREEN_REFRESH);
//			pMode->RefreshRate = G->SCREEN_REFRESH;
//		}
		LogInfo("  returns result=%x, width=%d, height=%d, refresh rate=%d\n", hr, pMode->Width, pMode->Height, pMode->RefreshRate);
	}
	else if (LogFile)
	{
		LogInfo("  returns result=%x\n", hr);
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::CheckDeviceType(THIS_ UINT Adapter,::D3DDEVTYPE DevType,::D3DFORMAT AdapterFormat,::D3DFORMAT BackBufferFormat,BOOL bWindowed)
{
	LogInfo("IDirect3D9::CheckDeviceType called with adapter=%d\n", Adapter);

	HRESULT hr = GetDirect3D9()->CheckDeviceType(Adapter, DevType, AdapterFormat, BackBufferFormat, bWindowed);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::CheckDeviceFormat(THIS_ UINT Adapter,::D3DDEVTYPE DeviceType,::D3DFORMAT AdapterFormat,DWORD Usage,::D3DRESOURCETYPE RType,::D3DFORMAT CheckFormat)
{
	LogInfo("IDirect3D9::CheckDeviceFormat called with adapter=%d\n", Adapter);

	HRESULT hr = GetDirect3D9()->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::CheckDeviceMultiSampleType(THIS_ UINT Adapter,::D3DDEVTYPE DeviceType,::D3DFORMAT SurfaceFormat,BOOL Windowed,::D3DMULTISAMPLE_TYPE MultiSampleType,DWORD* pQualityLevels)
{
	LogInfo("IDirect3D9::CheckDeviceMultiSampleType called: adapter #%d\n", Adapter);

	return GetDirect3D9()->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels);
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::CheckDepthStencilMatch(THIS_ UINT Adapter,::D3DDEVTYPE DeviceType,::D3DFORMAT AdapterFormat,::D3DFORMAT RenderTargetFormat,::D3DFORMAT DepthStencilFormat)
{
	LogInfo("IDirect3D9::CheckDepthStencilMatch called: adapter #%d\n", Adapter);

	HRESULT hr = GetDirect3D9()->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::CheckDeviceFormatConversion(THIS_ UINT Adapter,::D3DDEVTYPE DeviceType,::D3DFORMAT SourceFormat,::D3DFORMAT TargetFormat)
{
	LogInfo("IDirect3D9::CheckDeviceFormatConversion called: adapter #%d\n", Adapter);

	HRESULT hr = GetDirect3D9()->CheckDeviceFormatConversion(Adapter, DeviceType, SourceFormat, TargetFormat);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::GetDeviceCaps(THIS_ UINT Adapter,::D3DDEVTYPE DeviceType,::D3DCAPS9* pCaps)
{
	LogInfo("IDirect3D9::GetDeviceCaps called: adapter #%d\n", Adapter);

	HRESULT hr = GetDirect3D9()->GetDeviceCaps(Adapter, DeviceType, pCaps);
	LogInfo("  returns result = %x\n", hr);

	return hr;
}

STDMETHODIMP_(HMONITOR) D3D9Wrapper::IDirect3D9::GetAdapterMonitor(THIS_ UINT Adapter)
{
	LogInfo("IDirect3D9::GetAdapterMonitor called: adapter #%d\n", Adapter);

	return GetDirect3D9()->GetAdapterMonitor(Adapter);
}

DWORD WINAPI DeviceCreateThread(LPVOID lpParam)
{
	D3D9Wrapper::IDirect3DDevice9* newDevice = (D3D9Wrapper::IDirect3DDevice9*)lpParam;
	CheckDevice(newDevice);
	return S_OK;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::CreateDeviceEx(THIS_ UINT Adapter,::D3DDEVTYPE DeviceType,HWND hFocusWindow,DWORD BehaviorFlags,::D3DPRESENT_PARAMETERS* pPresentationParameters,::D3DDISPLAYMODEEX* pFullscreenDisplayMode,D3D9Wrapper::IDirect3DDevice9** ppReturnedDeviceInterface)
{
	LogInfo("IDirect3D9::CreateDeviceEx called with parameters FocusWindow = %p\n", hFocusWindow);

	if (pPresentationParameters)
	{
		LogInfo("  BackBufferWidth = %d\n", pPresentationParameters->BackBufferWidth);
		LogInfo("  BackBufferHeight = %d\n", pPresentationParameters->BackBufferHeight);
		LogInfo("  BackBufferFormat = %d\n", pPresentationParameters->BackBufferFormat);
		LogInfo("  BackBufferCount = %d\n", pPresentationParameters->BackBufferCount);
		LogInfo("  SwapEffect = %x\n", pPresentationParameters->SwapEffect);
		LogInfo("  Flags = %x\n", pPresentationParameters->Flags);
		LogInfo("  FullScreen_RefreshRateInHz = %d\n", pPresentationParameters->FullScreen_RefreshRateInHz);
		LogInfo("  PresentationInterval = %d\n", pPresentationParameters->PresentationInterval);
		LogInfo("  Windowed = %d\n", pPresentationParameters->Windowed);
		LogInfo("  EnableAutoDepthStencil = %d\n", pPresentationParameters->EnableAutoDepthStencil);
		LogInfo("  AutoDepthStencilFormat = %d\n", pPresentationParameters->AutoDepthStencilFormat);
		LogInfo("  MultiSampleType %d\n", pPresentationParameters->MultiSampleType);
		LogInfo("  MultiSampleQuality %d\n", pPresentationParameters->MultiSampleQuality);
	}
	if (pFullscreenDisplayMode)
	{
		LogInfo("  FullscreenDisplayFormat = %d\n", pFullscreenDisplayMode->Format);
		LogInfo("  FullscreenDisplayWidth = %d\n", pFullscreenDisplayMode->Width);
		LogInfo("  FullscreenDisplayHeight = %d\n", pFullscreenDisplayMode->Height);
		LogInfo("  FullscreenRefreshRate = %d\n", pFullscreenDisplayMode->RefreshRate);
		LogInfo("  ScanLineOrdering = %d\n", pFullscreenDisplayMode->ScanLineOrdering);
	}

	::D3DPRESENT_PARAMETERS originalPresentParams;
	if (pPresentationParameters != NULL) {
		// Save off the window handle so we can translate mouse cursor
		// coordinates to the window:
		if (pPresentationParameters->hDeviceWindow != NULL)
			G->sethWnd(pPresentationParameters->hDeviceWindow);
		else
			G->sethWnd(hFocusWindow);

		memcpy(&originalPresentParams, pPresentationParameters, sizeof(::D3DPRESENT_PARAMETERS));
		// Require in case the software mouse and upscaling are on at the same time
		// TODO: Use a helper class to track *all* different resolutions
		G->SET_GAME_INTERNAL_WIDTH(pPresentationParameters->BackBufferWidth);
		G->SET_GAME_INTERNAL_HEIGHT(pPresentationParameters->BackBufferHeight);
		// TODO: Use a helper class to track *all* different resolutions
		G->mResolutionInfo.width = pPresentationParameters->BackBufferWidth;
		G->mResolutionInfo.height = pPresentationParameters->BackBufferHeight;
		LogInfo("Got resolution from device creation: %ix%i\n",
				G->mResolutionInfo.width, G->mResolutionInfo.height);

	}
	if (G->SCREEN_UPSCALING > 0)
	{
		SetWindowPos(G->hWnd(), NULL, NULL, NULL, G->SCREEN_WIDTH, G->SCREEN_HEIGHT, SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOZORDER);
	}
	ForceDisplayParams(pPresentationParameters, pFullscreenDisplayMode);

	::D3DDISPLAYMODEEX fullScreenDisplayMode;
	if (pPresentationParameters && !(pPresentationParameters->Windowed) && !pFullscreenDisplayMode)
	{
		LogInfo("    creating full screen parameter structure.\n");

		fullScreenDisplayMode.Size = sizeof(::D3DDISPLAYMODEEX);
		fullScreenDisplayMode.Format = pPresentationParameters->BackBufferFormat;
		fullScreenDisplayMode.Height = pPresentationParameters->BackBufferHeight;
		fullScreenDisplayMode.Width = pPresentationParameters->BackBufferWidth;
		fullScreenDisplayMode.RefreshRate = pPresentationParameters->FullScreen_RefreshRateInHz;
		fullScreenDisplayMode.ScanLineOrdering = ::D3DSCANLINEORDERING_PROGRESSIVE;
		pFullscreenDisplayMode = &fullScreenDisplayMode;
	}
	LogDebug("Overridden Presentation Params\n");
	if (pPresentationParameters)
	{
		LogDebug("  BackBufferWidth = %d\n", pPresentationParameters->BackBufferWidth);
		LogDebug("  BackBufferHeight = %d\n", pPresentationParameters->BackBufferHeight);
		LogDebug("  BackBufferFormat = %d\n", pPresentationParameters->BackBufferFormat);
		LogDebug("  BackBufferCount = %d\n", pPresentationParameters->BackBufferCount);
		LogDebug("  SwapEffect = %x\n", pPresentationParameters->SwapEffect);
		LogDebug("  Flags = %x\n", pPresentationParameters->Flags);
		LogDebug("  FullScreen_RefreshRateInHz = %d\n", pPresentationParameters->FullScreen_RefreshRateInHz);
		LogDebug("  PresentationInterval = %d\n", pPresentationParameters->PresentationInterval);
		LogDebug("  Windowed = %d\n", pPresentationParameters->Windowed);
		LogDebug("  EnableAutoDepthStencil = %d\n", pPresentationParameters->EnableAutoDepthStencil);
		LogDebug("  AutoDepthStencilFormat = %d\n", pPresentationParameters->AutoDepthStencilFormat);
		LogInfo("  MultiSampleType %d\n", pPresentationParameters->MultiSampleType);
		LogInfo("  MultiSampleQuality %d\n", pPresentationParameters->MultiSampleQuality);
	}
	if (pFullscreenDisplayMode)
	{
		LogDebug("  FullscreenDisplayFormat = %d\n", pFullscreenDisplayMode->Format);
		LogDebug("  FullscreenDisplayWidth = %d\n", pFullscreenDisplayMode->Width);
		LogDebug("  FullscreenDisplayHeight = %d\n", pFullscreenDisplayMode->Height);
		LogDebug("  FullscreenRefreshRate = %d\n", pFullscreenDisplayMode->RefreshRate);
		LogDebug("  ScanLineOrdering = %d\n", pFullscreenDisplayMode->ScanLineOrdering);
	}

	::LPDIRECT3DDEVICE9EX baseDevice = NULL;
	HRESULT hr = S_OK;
	if (!G->gDelayDeviceCreation)
	{
		HRESULT hr = GetDirect3D9Ex()->CreateDeviceEx(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, &baseDevice);
		if (FAILED(hr))
		{
			LogInfo("  returns result = %x\n", hr);

			if (ppReturnedDeviceInterface) *ppReturnedDeviceInterface = NULL;
			return hr;
		}
	}
	else
	{
		LogInfo("  postponing device creation and returning only wrapper\n");
	}

#if _DEBUG_LAYER
	ShowDebugInfo(ppReturnedDeviceInterface);
#endif
	D3D9Wrapper::IDirect3DDevice9* newDevice;
	if (G->hunting || gLogDebug) {
		newDevice = D3D9Wrapper::FrameAnalysisDevice::GetDirect3DDevice(baseDevice, this, true);
	}
	else {
		newDevice = D3D9Wrapper::IDirect3DDevice9::GetDirect3DDevice(baseDevice, this, true);
	}

	if (newDevice == NULL)
	{
		LogInfo("  error creating IDirect3DDevice9 wrapper.\n");

		if (baseDevice)
			baseDevice->Release();
		return E_OUTOFMEMORY;
	}
	else if (!baseDevice)
	{
	}
	newDevice->_Adapter = Adapter;
	newDevice->_DeviceType = DeviceType;
	newDevice->_BehaviorFlags = BehaviorFlags;
	newDevice->_pFullscreenDisplayMode = pFullscreenDisplayMode;
	newDevice->_hFocusWindow = hFocusWindow;
	newDevice->_pOrigPresentationParameters = originalPresentParams;
	newDevice->_pPresentationParameters = *pPresentationParameters;

	LogInfo("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseDevice, newDevice);
	if (ppReturnedDeviceInterface) {
		if ((!(G->enable_hooks & EnableHooksDX9::DEVICE) && newDevice) || !baseDevice) {
			*ppReturnedDeviceInterface = newDevice;
		}
		else {
			*ppReturnedDeviceInterface = reinterpret_cast<D3D9Wrapper::IDirect3DDevice9*>(baseDevice);
		}

	}
	newDevice->Create3DMigotoResources();
	newDevice->Bind3DMigotoResources();
	newDevice->InitIniParams();
	newDevice->OnCreateOrRestore(&originalPresentParams, pPresentationParameters);

	pPresentationParameters->BackBufferWidth = originalPresentParams.BackBufferWidth;
	pPresentationParameters->BackBufferHeight = originalPresentParams.BackBufferHeight;
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::CreateDevice(THIS_ UINT Adapter,::D3DDEVTYPE DeviceType,HWND hFocusWindow,DWORD BehaviorFlags,::D3DPRESENT_PARAMETERS* pPresentationParameters, D3D9Wrapper::IDirect3DDevice9** ppReturnedDeviceInterface)
{
	LogInfo("IDirect3D9::CreateDevice called: adapter #%d\n", Adapter);

	if (G->gForwardToEx) {
		LogInfo("  forwarding to CreateDeviceEx.\n");

		::D3DDISPLAYMODEEX fullScreenDisplayMode;
		fullScreenDisplayMode.Size = sizeof(::D3DDISPLAYMODEEX);
		if (pPresentationParameters)
		{
			fullScreenDisplayMode.Format = pPresentationParameters->BackBufferFormat;
			fullScreenDisplayMode.Height = pPresentationParameters->BackBufferHeight;
			fullScreenDisplayMode.Width = pPresentationParameters->BackBufferWidth;
			fullScreenDisplayMode.RefreshRate = pPresentationParameters->FullScreen_RefreshRateInHz;
			fullScreenDisplayMode.ScanLineOrdering = ::D3DSCANLINEORDERING_PROGRESSIVE;
		}
		::D3DDISPLAYMODEEX *pFullScreenDisplayMode = &fullScreenDisplayMode;
		if (pPresentationParameters && pPresentationParameters->Windowed)
			pFullScreenDisplayMode = 0;
		return CreateDeviceEx(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullScreenDisplayMode, ppReturnedDeviceInterface);
	}
	if (pPresentationParameters)
	{
		LogInfo("  BackBufferWidth = %d\n", pPresentationParameters->BackBufferWidth);
		LogInfo("  BackBufferHeight = %d\n", pPresentationParameters->BackBufferHeight);
		LogInfo("  BackBufferFormat = %d\n", pPresentationParameters->BackBufferFormat);
		LogInfo("  BackBufferCount = %d\n", pPresentationParameters->BackBufferCount);
		LogInfo("  SwapEffect = %x\n", pPresentationParameters->SwapEffect);
		LogInfo("  Flags = %x\n", pPresentationParameters->Flags);
		LogInfo("  FullScreen_RefreshRateInHz = %d\n", pPresentationParameters->FullScreen_RefreshRateInHz);
		LogInfo("  PresentationInterval = %d\n", pPresentationParameters->PresentationInterval);
		LogInfo("  Windowed = %d\n", pPresentationParameters->Windowed);
		LogInfo("  EnableAutoDepthStencil = %d\n", pPresentationParameters->EnableAutoDepthStencil);
		LogInfo("  AutoDepthStencilFormat = %d\n", pPresentationParameters->AutoDepthStencilFormat);
		LogInfo("  MultiSampleType %d\n", pPresentationParameters->MultiSampleType);
		LogInfo("  MultiSampleQuality %d\n", pPresentationParameters->MultiSampleQuality);
	}
	::D3DPRESENT_PARAMETERS originalPresentParams;
	if (pPresentationParameters != NULL) {
		// Save off the window handle so we can translate mouse cursor
		// coordinates to the window:
		if (pPresentationParameters->hDeviceWindow != NULL)
			G->sethWnd(pPresentationParameters->hDeviceWindow);
		else
			G->sethWnd(hFocusWindow);

		memcpy(&originalPresentParams, pPresentationParameters, sizeof(::D3DPRESENT_PARAMETERS));
		// Require in case the software mouse and upscaling are on at the same time
		// TODO: Use a helper class to track *all* different resolutions
		G->SET_GAME_INTERNAL_WIDTH(pPresentationParameters->BackBufferWidth);
		G->SET_GAME_INTERNAL_HEIGHT(pPresentationParameters->BackBufferHeight);
		// TODO: Use a helper class to track *all* different resolutions
		G->mResolutionInfo.width = pPresentationParameters->BackBufferWidth;
		G->mResolutionInfo.height = pPresentationParameters->BackBufferHeight;
		LogInfo("Got resolution from device creation: %ix%i\n",
			G->mResolutionInfo.width, G->mResolutionInfo.height);

	}
	if (G->SCREEN_UPSCALING > 0)
	{
		SetWindowPos(G->hWnd(), NULL, NULL, NULL, G->SCREEN_WIDTH, G->SCREEN_HEIGHT, SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOZORDER);
	}
	ForceDisplayParams(pPresentationParameters);
	LogDebug("Overridden Presentation Params\n");
	if (pPresentationParameters)
	{
		LogDebug("  BackBufferWidth = %d\n", pPresentationParameters->BackBufferWidth);
		LogDebug("  BackBufferHeight = %d\n", pPresentationParameters->BackBufferHeight);
		LogDebug("  BackBufferFormat = %d\n", pPresentationParameters->BackBufferFormat);
		LogDebug("  BackBufferCount = %d\n", pPresentationParameters->BackBufferCount);
		LogDebug("  SwapEffect = %x\n", pPresentationParameters->SwapEffect);
		LogDebug("  Flags = %x\n", pPresentationParameters->Flags);
		LogDebug("  FullScreen_RefreshRateInHz = %d\n", pPresentationParameters->FullScreen_RefreshRateInHz);
		LogDebug("  PresentationInterval = %d\n", pPresentationParameters->PresentationInterval);
		LogDebug("  Windowed = %d\n", pPresentationParameters->Windowed);
		LogDebug("  EnableAutoDepthStencil = %d\n", pPresentationParameters->EnableAutoDepthStencil);
		LogDebug("  AutoDepthStencilFormat = %d\n", pPresentationParameters->AutoDepthStencilFormat);
		LogInfo("  MultiSampleType %d\n", pPresentationParameters->MultiSampleType);
		LogInfo("  MultiSampleQuality %d\n", pPresentationParameters->MultiSampleQuality);
	}
	::LPDIRECT3DDEVICE9 baseDevice = NULL;
	HRESULT hr = S_OK;
	if (!G->gDelayDeviceCreation)
	{
		HRESULT hr = GetDirect3D9()->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, &baseDevice);
		if (FAILED(hr))
		{
			LogInfo("  returns result = %x\n", hr);

			if (ppReturnedDeviceInterface) *ppReturnedDeviceInterface = NULL;
			return hr;
		}
	}
	else
	{
		LogInfo("  postponing device creation and returning only wrapper\n");
	}

#if _DEBUG_LAYER
	ShowDebugInfo(ppReturnedDeviceInterface);
#endif
	D3D9Wrapper::IDirect3DDevice9* newDevice;
	if (G->hunting || gLogDebug) {
		newDevice = D3D9Wrapper::FrameAnalysisDevice::GetDirect3DDevice(baseDevice, this, false);
	}
	else {
		newDevice = D3D9Wrapper::IDirect3DDevice9::GetDirect3DDevice(baseDevice, this, false);
	}
	if (newDevice == NULL)
	{
		LogInfo("  error creating IDirect3DDevice9 wrapper.\n");

		if (baseDevice)
			baseDevice->Release();
		return E_OUTOFMEMORY;
	}
	else if (!baseDevice)
	{
	}
	newDevice->_Adapter = Adapter;
	newDevice->_DeviceType = DeviceType;
	newDevice->_BehaviorFlags = BehaviorFlags;
	newDevice->_hFocusWindow = hFocusWindow;
	newDevice->_pOrigPresentationParameters = originalPresentParams;
	newDevice->_pPresentationParameters = *pPresentationParameters;

	LogInfo("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseDevice, newDevice);
	if (ppReturnedDeviceInterface) {
		if ((!(G->enable_hooks & EnableHooksDX9::DEVICE) && newDevice) || !baseDevice) {
			*ppReturnedDeviceInterface = newDevice;
		}
		else {
			*ppReturnedDeviceInterface = reinterpret_cast<D3D9Wrapper::IDirect3DDevice9*>(baseDevice);
		}

	}
	newDevice->Create3DMigotoResources();
	newDevice->Bind3DMigotoResources();
	newDevice->InitIniParams();
	newDevice->OnCreateOrRestore(&originalPresentParams, pPresentationParameters);

	pPresentationParameters->BackBufferWidth = originalPresentParams.BackBufferWidth;
	pPresentationParameters->BackBufferHeight = originalPresentParams.BackBufferHeight;
	return hr;
}

STDMETHODIMP_(UINT) D3D9Wrapper::IDirect3D9::GetAdapterModeCountEx(THIS_ UINT Adapter,CONST ::D3DDISPLAYMODEFILTER* pFilter )
{
	LogInfo("IDirect3D9::GetAdapterModeCountEx called: adapter #%d\n", Adapter);

	return GetDirect3D9Ex()->GetAdapterModeCountEx(Adapter, pFilter);
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::EnumAdapterModesEx(THIS_ UINT Adapter,CONST ::D3DDISPLAYMODEFILTER* pFilter,UINT Mode,::D3DDISPLAYMODEEX* pMode)
{
	LogInfo("IDirect3D9::EnumAdapterModesEx called: adapter #%d\n", Adapter);

	return GetDirect3D9Ex()->EnumAdapterModesEx(Adapter, pFilter, Mode, pMode);
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::GetAdapterDisplayModeEx(THIS_ UINT Adapter,::D3DDISPLAYMODEEX* pMode,::D3DDISPLAYROTATION* pRotation)
{
	LogInfo("IDirect3D9::GetAdapterDisplayModeEx called: adapter #%d\n", Adapter);

	HRESULT hr = GetDirect3D9Ex()->GetAdapterDisplayModeEx(Adapter, pMode, pRotation);
	if (hr == S_OK && pMode)
	{
		//if (G->UPSCALE_MODE > 0) {
		//	if (G->GAME_INTERNAL_WIDTH() > 1)
		//		pMode->Width = G->GAME_INTERNAL_WIDTH();
		//	if (G->GAME_INTERNAL_HEIGHT() > 1)
		//		pMode->Height = G->GAME_INTERNAL_HEIGHT();
		//}
		//if (G->GAME_INTERNAL_WIDTH() > 1 && G->FORCE_REPORT_GAME_RES) {
		//	pMode->Width = G->GAME_INTERNAL_WIDTH();
		//}
		///*		else if (G->FORCE_REPORT_WIDTH > 0) {
		//pMode->Width = G->FORCE_REPORT_WIDTH;
		//}*/else if (pMode->Width != G->SCREEN_WIDTH && G->SCREEN_WIDTH != -1)
		//{
		//	LogInfo("  overriding Width %d with %d\n", pMode->Width, G->SCREEN_WIDTH);
		//	pMode->Width = G->SCREEN_WIDTH;
		//}
		//if (G->GAME_INTERNAL_HEIGHT() > 1 && G->FORCE_REPORT_GAME_RES) {
		//	pMode->Height = G->GAME_INTERNAL_HEIGHT();
		//}
		///*		else if (G->FORCE_REPORT_HEIGHT > 0) {
		//pMode->Height = G->FORCE_REPORT_HEIGHT;
		//}*/else if (pMode->Height != G->SCREEN_HEIGHT && G->SCREEN_HEIGHT != -1)
		//{
		//	LogInfo("  overriding Height %d with %d\n", pMode->Height, G->SCREEN_HEIGHT);
		//	pMode->Height = G->SCREEN_HEIGHT;
		//}
		//if (pMode->RefreshRate != G->SCREEN_REFRESH && G->SCREEN_REFRESH != -1)
		//{
		//	LogInfo("  overriding RefreshRate %d with %d\n", pMode->RefreshRate, G->SCREEN_REFRESH);
		//	pMode->RefreshRate = G->SCREEN_REFRESH;
		//}
		LogInfo("  returns result=%x, width=%d, height=%d, refresh rate=%d\n", hr, pMode->Width, pMode->Height, pMode->RefreshRate);
	}
	else if (LogFile)
	{
		LogInfo("  returns result=%x\n", hr);
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::GetAdapterLUID(THIS_ UINT Adapter,LUID * pLUID)
{
	LogInfo("IDirect3D9::GetAdapterLUID called: adapter #%d\n", Adapter);

	return GetDirect3D9Ex()->GetAdapterLUID(Adapter, pLUID);
}
