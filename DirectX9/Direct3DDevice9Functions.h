
D3D9Wrapper::IDirect3DDevice9::IDirect3DDevice9(D3D9Base::LPDIRECT3DDEVICE9EX pDevice)
    : IDirect3DUnknown((IUnknown*) pDevice),
	pendingCreateDepthStencilSurface(0),
	pendingSetDepthStencilSurface(0)
{
}

D3D9Wrapper::IDirect3DDevice9* D3D9Wrapper::IDirect3DDevice9::GetDirect3DDevice(D3D9Base::LPDIRECT3DDEVICE9EX pDevice)
{
    D3D9Wrapper::IDirect3DDevice9* p = (D3D9Wrapper::IDirect3DDevice9*) m_List.GetDataPtr(pDevice);
    if (!p)
    {
        p = new D3D9Wrapper::IDirect3DDevice9(pDevice);
		if (pDevice) m_List.AddMember(pDevice, p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DDevice9::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DDevice9::Release(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	if (LogFile && LogDebug) fflush(LogFile);
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	if (LogFile && LogDebug) fprintf(LogFile, "  internal counter = %d\n", ulRef);
	if (LogFile && LogDebug) fflush(LogFile);
	--m_ulRef;

    if (ulRef == 0)
    {
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DDevice9::Release handle=%x, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		if (LogFile) fprintf(LogFile, "  deleting self\n");
		if (LogFile) fflush(LogFile);
        if (m_pUnk) m_List.DeleteMember(m_pUnk); 
		m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::TestCooperativeLevel(THIS)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::TestCooperativeLevel called\n");
	if (LogFile) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->TestCooperativeLevel();
}

STDMETHODIMP_(UINT) D3D9Wrapper::IDirect3DDevice9::GetAvailableTextureMem(THIS)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::GetAvailableTextureMem called\n");
	if (LogFile) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->GetAvailableTextureMem();
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::EvictManagedResources(THIS)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::EvictManagedResources called\n");
	if (LogFile) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->EvictManagedResources();
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetDirect3D(THIS_ D3D9Wrapper::IDirect3D9** ppD3D9)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::GetDirect3D called\n");
	if (LogFile) fflush(LogFile);
	CheckDevice(this);
    D3D9Base::LPDIRECT3D9 baseDirect3D = NULL;
    HRESULT hr = GetD3D9Device()->GetDirect3D(&baseDirect3D);
    if (FAILED(hr))
    {
		if (LogFile) fprintf(LogFile, "  call failed with hr=%x\n", hr);
		if (LogFile) fflush(LogFile);
        if (ppD3D9) *ppD3D9 = NULL;
        return hr;
    }
	// Cast to LPDIRECT3D9EX
	D3D9Base::LPDIRECT3D9EX baseDirect3DEx = NULL;
	const IID IID_IDirect3D9Ex = { 0x02177241, 0x69FC, 0x400C, { 0x8F, 0xF1, 0x93, 0xA4, 0x4D, 0xF6, 0x86, 0x1D }};
    hr = baseDirect3D->QueryInterface(IID_IDirect3D9Ex, (void **)&baseDirect3DEx);
	baseDirect3D->Release();
    if (FAILED(hr))
    {
		if (LogFile) fprintf(LogFile, "  cast to IDirect3D9Ex failed with hr=%x\n", hr);
		if (LogFile) fflush(LogFile);
        if (ppD3D9) *ppD3D9 = NULL;
        return hr;
    }

	D3D9Wrapper::IDirect3D9* pD3D = D3D9Wrapper::IDirect3D9::GetDirect3D(baseDirect3DEx);
    if (ppD3D9) *ppD3D9 = pD3D;
	if (LogFile) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseDirect3DEx, pD3D);
	if (LogFile) fflush(LogFile);
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetDeviceCaps(THIS_ D3D9Base::D3DCAPS9* pCaps)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::GetDeviceCaps called\n");
	if (LogFile) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetDeviceCaps(pCaps);
	if (LogFile) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetDisplayMode(THIS_ UINT iSwapChain,D3D9Base::D3DDISPLAYMODE* pMode)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::GetDisplayMode called\n");
	if (LogFile) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetDisplayMode(iSwapChain, pMode);
	if (hr == S_OK && pMode)
	{
		if (SCREEN_REFRESH != -1 && pMode->RefreshRate != SCREEN_REFRESH)
		{
			if (LogFile) fprintf(LogFile, "  overriding refresh rate %d with %d\n", pMode->RefreshRate, SCREEN_REFRESH);
			if (LogFile) fflush(LogFile);
			pMode->RefreshRate = SCREEN_REFRESH;
		}
	}
	if (LogFile && !pMode) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && pMode) fprintf(LogFile, "  returns result=%x, Width=%d, Height=%d, RefreshRate=%d, Format=%d\n", hr,
		pMode->Width, pMode->Height, pMode->RefreshRate, pMode->Format);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetCreationParameters(THIS_ D3D9Base::D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::GetCreationParameters called\n");
	if (LogFile) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetCreationParameters(pParameters);
	if (LogFile && !pParameters) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && pParameters) fprintf(LogFile, "  returns result=%x, AdapterOrdinal=%d, DeviceType=%d, FocusWindow=%x, BehaviorFlags=%x\n", hr,
		pParameters->AdapterOrdinal, pParameters->DeviceType, pParameters->hFocusWindow, pParameters->BehaviorFlags);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetCursorProperties(THIS_ UINT XHotSpot,UINT YHotSpot, D3D9Wrapper::IDirect3DSurface9 *pCursorBitmap)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::SetCursorProperties called\n");
	if (LogFile) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetCursorProperties(XHotSpot, YHotSpot, replaceSurface9(pCursorBitmap));
	if (LogFile) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DDevice9::SetCursorPosition(THIS_ int X,int Y,DWORD Flags)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::SetCursorPosition called\n");
	if (LogFile) fflush(LogFile);
	CheckDevice(this);
	GetD3D9Device()->SetCursorPosition(X, Y, Flags);
}

STDMETHODIMP_(BOOL) D3D9Wrapper::IDirect3DDevice9::ShowCursor(THIS_ BOOL bShow)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::ShowCursor called\n");
	if (LogFile) fflush(LogFile);
	CheckDevice(this);
	BOOL hr = GetD3D9Device()->ShowCursor(bShow);
	if (LogFile) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateAdditionalSwapChain(THIS_ D3D9Base::D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DSwapChain9** pSwapChain)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::CreateAdditionalSwapChain called\n");
	if (LogFile) fflush(LogFile);
	CheckDevice(this);

	D3D9Base::IDirect3DSwapChain9 *baseSwapChain;
	HRESULT hr = GetD3D9Device()->CreateAdditionalSwapChain(pPresentationParameters, &baseSwapChain);
    if (FAILED(hr) || baseSwapChain == NULL)
    {
		if (LogFile) fprintf(LogFile, "  failed with hr=%x\n", hr);
		if (LogFile) fflush(LogFile);
        return hr;
    }
	D3D9Base::LPDIRECT3DSWAPCHAIN9EX baseSwapChainEx = NULL;
	const IID IID_IDirect3DSwapChain9Ex = { 0x91886caf, 0x1c3d, 0x4d2e, { 0xa0, 0xab, 0x3e, 0x4c, 0x7d, 0x8d, 0x33, 0x3 }};
	hr = baseSwapChain->QueryInterface(IID_IDirect3DSwapChain9Ex, (void **)&baseSwapChainEx);
	baseSwapChain->Release();
	if (FAILED(hr) || baseSwapChainEx == NULL)
	{
		if (LogFile) fprintf(LogFile, "  cast to IDirect3DSwapChain9Ex failed with hr=%x\n", hr);
		if (LogFile) fflush(LogFile);
		return hr;
	}
    
    D3D9Wrapper::IDirect3DSwapChain9* NewSwapChain = D3D9Wrapper::IDirect3DSwapChain9::GetSwapChain(baseSwapChainEx, this);
    if (NewSwapChain == NULL)
    {
		if (LogFile) fprintf(LogFile, "  error creating wrapper\n", hr);
		if (LogFile) fflush(LogFile);
        baseSwapChainEx->Release();
        return E_OUTOFMEMORY;
    }
	if (pSwapChain) *pSwapChain = NewSwapChain;
	if (LogFile) fprintf(LogFile, "  returns result=%x, handle=%x\n", hr, NewSwapChain);
	if (LogFile) fflush(LogFile);
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetSwapChain(THIS_ UINT iSwapChain,IDirect3DSwapChain9** pSwapChain)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::GetSwapChain called with SwapChain=%d\n", iSwapChain);
	if (LogFile) fflush(LogFile);

	if (!GetD3D9Device())
	{
		if (LogFile) fprintf(LogFile, "  postponing call because device was not created yet.\n");
		if (LogFile) fflush(LogFile);
		D3D9Wrapper::IDirect3DSwapChain9 *wrapper = D3D9Wrapper::IDirect3DSwapChain9::GetSwapChain((D3D9Base::LPDIRECT3DSWAPCHAIN9EX) 0, this);
		wrapper->_SwapChain = iSwapChain;
		wrapper->pendingGetSwapChain = true;
		wrapper->pendingDevice = this;
		*pSwapChain = wrapper;
		if (LogFile) fprintf(LogFile, "  returns handle=%x\n", wrapper);
		if (LogFile) fflush(LogFile);
		return S_OK;
	}

    *pSwapChain = NULL;
	D3D9Base::LPDIRECT3DSWAPCHAIN9 baseSwapChain = NULL;
    HRESULT hr = GetD3D9Device()->GetSwapChain(iSwapChain, &baseSwapChain);
    if (FAILED(hr) || baseSwapChain == NULL)
    {
		if (LogFile) fprintf(LogFile, "  failed with hr=%x\n", hr);
		if (LogFile) fflush(LogFile);
        return hr;
    }
	D3D9Base::LPDIRECT3DSWAPCHAIN9EX baseSwapChainEx = NULL;
	const IID IID_IDirect3DSwapChain9Ex = { 0x91886caf, 0x1c3d, 0x4d2e, { 0xa0, 0xab, 0x3e, 0x4c, 0x7d, 0x8d, 0x33, 0x3 }};
	hr = baseSwapChain->QueryInterface(IID_IDirect3DSwapChain9Ex, (void **)&baseSwapChainEx);
	baseSwapChain->Release();
	if (FAILED(hr) || baseSwapChainEx == NULL)
	{
		if (LogFile) fprintf(LogFile, "  cast to IDirect3DSwapChain9Ex failed with hr=%x\n", hr);
		if (LogFile) fflush(LogFile);
		return hr;
	}
    
    D3D9Wrapper::IDirect3DSwapChain9* newSwapChain = D3D9Wrapper::IDirect3DSwapChain9::GetSwapChain(baseSwapChainEx, this);
    if (newSwapChain == NULL)
    {
		if (LogFile) fprintf(LogFile, "  error creating wrapper\n", hr);
		if (LogFile) fflush(LogFile);
        baseSwapChainEx->Release();
        return E_OUTOFMEMORY;
    }
	if (pSwapChain) *pSwapChain = newSwapChain;
	if (LogFile) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseSwapChainEx, newSwapChain);
	if (LogFile) fflush(LogFile);
    return hr;
}

STDMETHODIMP_(UINT) D3D9Wrapper::IDirect3DDevice9::GetNumberOfSwapChains(THIS)
{
	if (LogFile) fprintf(LogFile, "GetNumberOfSwapChains\n");
	if (LogFile) fflush(LogFile);
	CheckDevice(this);
	UINT hr = GetD3D9Device()->GetNumberOfSwapChains();
	if (LogFile) fprintf(LogFile, "  returns NumberOfSwapChains=%d\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::Reset(THIS_ D3D9Base::D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::Reset called on handle=%x\n", GetD3D9Device());
	if (LogFile) fflush(LogFile);
	CheckDevice(this);
	if (LogFile) fprintf(LogFile, "  BackBufferWidth %d\n", pPresentationParameters->BackBufferWidth);
	if (LogFile) fprintf(LogFile, "  BackBufferHeight %d\n", pPresentationParameters->BackBufferHeight);
	if (LogFile) fprintf(LogFile, "  BackBufferFormat %x\n", pPresentationParameters->BackBufferFormat);
	if (LogFile) fprintf(LogFile, "  BackBufferCount %d\n", pPresentationParameters->BackBufferCount);
	if (LogFile) fprintf(LogFile, "  SwapEffect %x\n", pPresentationParameters->SwapEffect);
	if (LogFile) fprintf(LogFile, "  Flags %x\n", pPresentationParameters->Flags);
	if (LogFile) fprintf(LogFile, "  FullScreen_RefreshRateInHz %d\n", pPresentationParameters->FullScreen_RefreshRateInHz);
	if (LogFile) fprintf(LogFile, "  PresentationInterval %d\n", pPresentationParameters->PresentationInterval);
	if (LogFile) fprintf(LogFile, "  Windowed %d\n", pPresentationParameters->Windowed);
	if (LogFile) fprintf(LogFile, "  EnableAutoDepthStencil %d\n", pPresentationParameters->EnableAutoDepthStencil);
	if (LogFile) fprintf(LogFile, "  AutoDepthStencilFormat %d\n", pPresentationParameters->AutoDepthStencilFormat);
	if (LogFile) fflush(LogFile);

	if (SCREEN_REFRESH >= 0) 
	{
		if (LogFile) fprintf(LogFile, "    overriding refresh rate = %d\n", SCREEN_REFRESH);
		pPresentationParameters->FullScreen_RefreshRateInHz = SCREEN_REFRESH;
	}
	if (SCREEN_WIDTH >= 0) 
	{
		if (LogFile) fprintf(LogFile, "    overriding width = %d\n", SCREEN_WIDTH);
		pPresentationParameters->BackBufferWidth = SCREEN_WIDTH;
	}
	if (SCREEN_HEIGHT >= 0) 
	{
		if (LogFile) fprintf(LogFile, "    overriding height = %d\n", SCREEN_HEIGHT);
		pPresentationParameters->BackBufferHeight = SCREEN_HEIGHT;
	}
	if (SCREEN_FULLSCREEN >= 0 && SCREEN_FULLSCREEN < 2)
	{
		if (LogFile) fprintf(LogFile, "    overriding full screen = %d\n", SCREEN_FULLSCREEN);
		pPresentationParameters->Windowed = !SCREEN_FULLSCREEN;
	}

//	pPresentationParameters->SwapEffect = D3D9Base::D3DSWAPEFFECT_DISCARD;
//	pPresentationParameters->PresentationInterval = 0; // D3DPRESENT_INTERVAL_DEFAULT
//	pPresentationParameters->EnableAutoDepthStencil = TRUE;
//	pPresentationParameters->AutoDepthStencilFormat = D3D9Base::D3DFMT_D24X8; // 77
//	pPresentationParameters->BackBufferFormat = D3D9Base::D3DFMT_A8R8G8B8; // 21

	HRESULT hr = GetD3D9Device()->Reset(pPresentationParameters);
	if (LogFile) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

UINT FrameIndex = 0;
STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::Present(THIS_ CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::Present called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);

	if (SCREEN_FULLSCREEN == 2)
	{
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DDevice9::Present called.\n");
		if (LogFile) fprintf(LogFile, "  initiating reset to switch to full screen.\n");
		if (LogFile) fflush(LogFile);
		SCREEN_FULLSCREEN = 1;
		SCREEN_WIDTH = SCREEN_WIDTH_DELAY;
		SCREEN_HEIGHT = SCREEN_HEIGHT_DELAY;
		SCREEN_REFRESH = SCREEN_REFRESH_DELAY;
		return D3DERR_DEVICELOST;
	}

	HRESULT hr = GetD3D9Device()->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	if (hr == D3DERR_DEVICELOST)
	{
		if (LogFile) fprintf(LogFile, "  returns D3DERR_DEVICELOST\n");
		if (LogFile) fflush(LogFile);
	}
	else
	{
		if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
		if (LogFile && LogDebug) fflush(LogFile);
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetBackBuffer(THIS_ UINT iSwapChain,UINT iBackBuffer,D3D9Base::D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::GetBackBuffer called\n");
	if (LogFile) fflush(LogFile);
	CheckDevice(this);

	D3D9Base::LPDIRECT3DSURFACE9 baseSurface = 0;
    HRESULT hr = GetD3D9Device()->GetBackBuffer(iSwapChain, iBackBuffer, Type, &baseSurface);
	if (FAILED(hr) || !baseSurface)
    {
		if (LogFile) fprintf(LogFile, "  failed with hr=%x\n", hr);
		if (LogFile) fflush(LogFile);
		if (ppBackBuffer) *ppBackBuffer = 0;
        return hr;
    }
	if (ppBackBuffer && baseSurface) *ppBackBuffer = IDirect3DSurface9::GetDirect3DSurface9(baseSurface);
	if (LogFile && ppBackBuffer) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseSurface, *ppBackBuffer);
	if (LogFile) fflush(LogFile);
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetRasterStatus(THIS_ UINT iSwapChain,D3D9Base::D3DRASTER_STATUS* pRasterStatus)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetRasterStatus called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetRasterStatus(iSwapChain, pRasterStatus);
	if (LogFile) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetDialogBoxMode(THIS_ BOOL bEnableDialogs)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetDialogBoxMode called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetDialogBoxMode(bEnableDialogs);
	if (LogFile) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DDevice9::SetGammaRamp(THIS_ UINT iSwapChain,DWORD Flags,CONST D3D9Base::D3DGAMMARAMP* pRamp)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetGammaRamp called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	GetD3D9Device()->SetGammaRamp(iSwapChain, Flags, pRamp);
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DDevice9::GetGammaRamp(THIS_ UINT iSwapChain,D3D9Base::D3DGAMMARAMP* pRamp)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetGammaRamp called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	GetD3D9Device()->GetGammaRamp(iSwapChain, pRamp);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateTexture(THIS_ UINT Width,UINT Height,UINT Levels,DWORD Usage,D3D9Base::D3DFORMAT Format,D3D9Base::D3DPOOL Pool, IDirect3DTexture9 **ppTexture,HANDLE* pSharedHandle)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::CreateTexture called with Width=%d, Height=%d, Levels=%d, Usage=%x, Format=%d, Pool=%d, SharedHandle=%x\n", 
		Width, Height, Levels, Usage, Format, Pool, pSharedHandle);
	if (LogFile && LogDebug) fflush(LogFile);

	if (!GetD3D9Device())
	{
		if (LogFile) fprintf(LogFile, "  postponing call because device was not created yet.\n");
		if (LogFile) fflush(LogFile);
		D3D9Wrapper::IDirect3DTexture9 *wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DVertexDeclaration9((D3D9Base::LPDIRECT3DTEXTURE9) 0);
		wrapper->_Width = Width;
		wrapper->_Height = Height;
		wrapper->_Levels = Levels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = this;
		wrapper->pendingCreateTexture = true;
		*ppTexture = wrapper;
		if (LogFile) fprintf(LogFile, "  returns handle=%x\n", wrapper);
		if (LogFile) fflush(LogFile);
		return S_OK;
	}

	D3D9Base::LPDIRECT3DTEXTURE9 baseTexture = 0;
	if (Pool == D3D9Base::D3DPOOL_MANAGED)
	{
		if (LogFile && LogDebug) fprintf(LogFile, "  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");
		if (LogFile && LogDebug) fflush(LogFile);
		Pool = D3D9Base::D3DPOOL_DEFAULT;
	}
    HRESULT hr = GetD3D9Device()->CreateTexture(Width, Height, Levels, Usage, Format, Pool, &baseTexture, pSharedHandle);
	if (ppTexture && baseTexture) *ppTexture = IDirect3DTexture9::GetDirect3DVertexDeclaration9(baseTexture);	
	if (LogFile && LogDebug && ppTexture) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseTexture, *ppTexture);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateVolumeTexture(THIS_ UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3D9Base::D3DFORMAT Format,D3D9Base::D3DPOOL Pool,D3D9Base::IDirect3DVolumeTexture9** ppVolumeTexture,HANDLE* pSharedHandle)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::CreateVolumeTexture Width=%d Height=%d Format=%d\n", Width, Height, Format);
	if (LogFile) fflush(LogFile);
	CheckDevice(this);
	if (Pool == D3D9Base::D3DPOOL_MANAGED)
	{
		if (LogFile && LogDebug) fprintf(LogFile, "  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");
		if (LogFile) fflush(LogFile);
		Pool = D3D9Base::D3DPOOL_DEFAULT;
	}
	HRESULT hr = GetD3D9Device()->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);
	if (LogFile) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateCubeTexture(THIS_ UINT EdgeLength,UINT Levels,DWORD Usage,D3D9Base::D3DFORMAT Format,D3D9Base::D3DPOOL Pool,D3D9Base::IDirect3DCubeTexture9** ppCubeTexture,HANDLE* pSharedHandle)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::CreateCubeTexture EdgeLength=%d Format=%d\n", EdgeLength, Format);
	if (LogFile) fflush(LogFile);
	CheckDevice(this);
	if (Pool == D3D9Base::D3DPOOL_MANAGED)
	{
		if (LogFile && LogDebug) fprintf(LogFile, "  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");
		if (LogFile) fflush(LogFile);
		Pool = D3D9Base::D3DPOOL_DEFAULT;
	}
	HRESULT hr = GetD3D9Device()->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);
	if (LogFile) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateVertexBuffer(THIS_ UINT Length,DWORD Usage,DWORD FVF,D3D9Base::D3DPOOL Pool, IDirect3DVertexBuffer9 **ppVertexBuffer,HANDLE* pSharedHandle)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::CreateVertexBuffer called with Length=%d\n", Length);
	if (LogFile && LogDebug) fflush(LogFile);

	if (!GetD3D9Device())
	{
		if (LogFile) fprintf(LogFile, "  postponing call because device was not created yet.\n");
		if (LogFile) fflush(LogFile);
		D3D9Wrapper::IDirect3DVertexBuffer9 *wrapper = D3D9Wrapper::IDirect3DVertexBuffer9::GetDirect3DVertexBuffer9((D3D9Base::LPDIRECT3DVERTEXBUFFER9) 0);
		wrapper->_Length = Length;
		wrapper->_Usage = Usage;
		wrapper->_FVF = FVF;
		wrapper->_Pool = Pool;
		wrapper->_Device = this;
		wrapper->pendingCreateVertexBuffer = true;
		if (ppVertexBuffer) *ppVertexBuffer = wrapper;
		if (LogFile) fprintf(LogFile, "  returns handle=%x\n", wrapper);
		if (LogFile) fflush(LogFile);
		return S_OK;
	}

	D3D9Base::LPDIRECT3DVERTEXBUFFER9 baseVB = 0;
	if (Pool == D3D9Base::D3DPOOL_MANAGED)
	{
		if (LogFile && LogDebug) fprintf(LogFile, "  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");
		if (LogFile) fflush(LogFile);
		Pool = D3D9Base::D3DPOOL_DEFAULT;
	}
    HRESULT hr = GetD3D9Device()->CreateVertexBuffer(Length, Usage, FVF, Pool, &baseVB, pSharedHandle);
	if (ppVertexBuffer && baseVB) *ppVertexBuffer = IDirect3DVertexBuffer9::GetDirect3DVertexBuffer9(baseVB);
	if (LogFile && LogDebug && ppVertexBuffer) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseVB, *ppVertexBuffer);
	if (LogFile && LogDebug) fflush(LogFile);    
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateIndexBuffer(THIS_ UINT Length,DWORD Usage,D3D9Base::D3DFORMAT Format,D3D9Base::D3DPOOL Pool,
	IDirect3DIndexBuffer9 **ppIndexBuffer,HANDLE* pSharedHandle)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::CreateIndexBuffer called with Length=%d, Usage=%x, Format=%d, Pool=%d, SharedHandle=%x, IndexBufferPtr=%x\n",
		Length, Usage, Format, Pool, pSharedHandle, ppIndexBuffer);
	if (LogFile && LogDebug) fflush(LogFile);

	if (!GetD3D9Device())
	{
		if (LogFile) fprintf(LogFile, "  postponing call because device was not created yet.\n");
		if (LogFile) fflush(LogFile);
		D3D9Wrapper::IDirect3DIndexBuffer9 *wrapper = D3D9Wrapper::IDirect3DIndexBuffer9::GetDirect3DIndexBuffer9((D3D9Base::LPDIRECT3DINDEXBUFFER9) 0);
		wrapper->_Length = Length;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = this;
		wrapper->pendingCreateIndexBuffer = true;
		if (ppIndexBuffer) *ppIndexBuffer = wrapper;
		if (LogFile) fprintf(LogFile, "  returns handle=%x\n", wrapper);
		if (LogFile) fflush(LogFile);
		return S_OK;
	}

	D3D9Base::LPDIRECT3DINDEXBUFFER9 baseIB = 0;
	if (Pool == D3D9Base::D3DPOOL_MANAGED)
	{
		if (LogFile && LogDebug) fprintf(LogFile, "  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");
		if (LogFile) fflush(LogFile);
		Pool = D3D9Base::D3DPOOL_DEFAULT;
	}
    HRESULT hr = GetD3D9Device()->CreateIndexBuffer(Length, Usage, Format, Pool, &baseIB, pSharedHandle);
	if (ppIndexBuffer && baseIB) *ppIndexBuffer = IDirect3DIndexBuffer9::GetDirect3DIndexBuffer9(baseIB);
	if (LogFile && LogDebug && ppIndexBuffer) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseIB, *ppIndexBuffer);
	if (LogFile && LogDebug) fflush(LogFile);
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateRenderTarget(THIS_ UINT Width,UINT Height,D3D9Base::D3DFORMAT Format,D3D9Base::D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable, IDirect3DSurface9 **ppSurface,HANDLE* pSharedHandle)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::CreateRenderTarget called with Width=%d Height=%d Format=%d\n", Width, Height, Format);
	if (LogFile) fflush(LogFile);
	CheckDevice(this);

	D3D9Base::LPDIRECT3DSURFACE9 baseSurface = NULL;
    HRESULT hr = GetD3D9Device()->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, &baseSurface, pSharedHandle);
    if (ppSurface && baseSurface) *ppSurface = IDirect3DSurface9::GetDirect3DSurface9(baseSurface);
	if (LogFile && ppSurface) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseSurface, *ppSurface);
	if (LogFile) fflush(LogFile);    
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateDepthStencilSurface(THIS_ UINT Width,UINT Height,D3D9Base::D3DFORMAT Format,D3D9Base::D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Discard,
																	  D3D9Wrapper::IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::CreateDepthStencilSurface Width=%d Height=%d Format=%d\n", Width, Height, Format);
	if (LogFile) fflush(LogFile);

	if (!GetD3D9Device())
	{
		if (LogFile) fprintf(LogFile, "  postponing call because device was not created yet.\n");
		if (LogFile) fflush(LogFile);
		D3D9Wrapper::IDirect3DSurface9 *wrapper = D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9((D3D9Base::LPDIRECT3DSURFACE9) 0);
		wrapper->_Width = Width;
		wrapper->_Height = Height;
		wrapper->_Format = Format;
		wrapper->_MultiSample = MultiSample;
		wrapper->_MultisampleQuality = MultisampleQuality;
		wrapper->_Discard = Discard;
		wrapper->_Device = this;
		pendingCreateDepthStencilSurface = wrapper;
		*ppSurface = wrapper;
		if (LogFile) fprintf(LogFile, "  returns handle=%x\n", wrapper);
		if (LogFile) fflush(LogFile);
		return S_OK;
	}

	D3D9Base::LPDIRECT3DSURFACE9 baseSurface = NULL;
    HRESULT hr = GetD3D9Device()->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, &baseSurface, pSharedHandle);
	if (ppSurface && baseSurface) *ppSurface = IDirect3DSurface9::GetDirect3DSurface9(baseSurface);
	if (LogFile && ppSurface) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseSurface, *ppSurface);
	if (LogFile) fflush(LogFile);    
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::UpdateSurface(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSourceSurface,CONST RECT* pSourceRect,
														  D3D9Wrapper::IDirect3DSurface9 *pDestinationSurface,CONST POINT* pDestPoint)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::UpdateSurface called with SourceSurface=%x, DestinationSurface=%x\n", pSourceSurface, pDestinationSurface);
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	D3D9Base::LPDIRECT3DSURFACE9 baseSourceSurface = replaceSurface9(pSourceSurface);
	D3D9Base::LPDIRECT3DSURFACE9 baseDestinationSurface = replaceSurface9(pDestinationSurface);
    HRESULT hr = GetD3D9Device()->UpdateSurface(baseSourceSurface, pSourceRect, baseDestinationSurface, pDestPoint);
	if (LogFile) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::UpdateTexture(THIS_ D3D9Base::LPDIRECT3DBASETEXTURE9 pSourceTexture,D3D9Base::LPDIRECT3DBASETEXTURE9 pDestinationTexture)
{
    if (LogFile) fprintf(LogFile, "IDirect3DDevice9::UpdateTexture called with SourceTexture=%x, DestinationTexture=%x\n", pSourceTexture, pDestinationTexture);
	if (LogFile) fflush(LogFile);

	IDirect3DTexture9 *sourceTexture = (IDirect3DTexture9*) IDirect3DTexture9::m_List.GetDataPtr(pSourceTexture);
	IDirect3DTexture9 *destinationTexture = (IDirect3DTexture9*) IDirect3DTexture9::m_List.GetDataPtr(pDestinationTexture);
	if (!sourceTexture && !destinationTexture)
	{
		sourceTexture = (IDirect3DTexture9*)pSourceTexture;
		destinationTexture = (IDirect3DTexture9*)pDestinationTexture;
		if (sourceTexture->pendingCreateTexture && destinationTexture->pendingCreateTexture && sourceTexture->pendingLockUnlock &&
			sourceTexture->_Width == destinationTexture->_Width && sourceTexture->_Height == destinationTexture->_Height)
		{
			if (LogFile) fprintf(LogFile, "  simulating texture update because both textures are not created yet.\n");
			if (LogFile) fflush(LogFile);
			if (!destinationTexture->pendingLockUnlock)
			{
				D3D9Base::D3DLOCKED_RECT rect;
				destinationTexture->LockRect(0, &rect, 0, 0);
			}
			memcpy(destinationTexture->_Buffer, sourceTexture->_Buffer, sourceTexture->_Width*sourceTexture->_Height*4);
			return S_OK;
		}
	}

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->UpdateTexture(replaceTexture9((IDirect3DTexture9 *) pSourceTexture), replaceTexture9((IDirect3DTexture9 *) pDestinationTexture));
	if (LogFile) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetRenderTargetData(THIS_ D3D9Wrapper::IDirect3DSurface9 *pRenderTarget,
																D3D9Wrapper::IDirect3DSurface9 *pDestSurface)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::GetRenderTargetData called with RenderTarget=%x, DestSurface=%x\n", pRenderTarget, pDestSurface);
	if (LogFile) fflush(LogFile);
	CheckDevice(this);

	D3D9Base::LPDIRECT3DSURFACE9 baseRenderTarget = replaceSurface9(pRenderTarget);
	D3D9Base::LPDIRECT3DSURFACE9 baseDestSurface = replaceSurface9(pDestSurface);
	HRESULT hr = GetD3D9Device()->GetRenderTargetData(baseRenderTarget, baseDestSurface);
	if (LogFile) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetFrontBufferData(THIS_ UINT iSwapChain, D3D9Wrapper::IDirect3DSurface9 *pDestSurface)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::GetFrontBufferData called with SwapChain=%d, DestSurface=%x\n", iSwapChain, pDestSurface);
	if (LogFile) fflush(LogFile);
	CheckDevice(this);

	D3D9Base::LPDIRECT3DSURFACE9 baseDestSurface = replaceSurface9(pDestSurface);
	HRESULT hr = GetD3D9Device()->GetFrontBufferData(iSwapChain, baseDestSurface);
	if (LogFile) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::StretchRect(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSourceSurface,CONST RECT* pSourceRect,
														D3D9Wrapper::IDirect3DSurface9 *pDestSurface,CONST RECT* pDestRect,D3D9Base::D3DTEXTUREFILTERTYPE Filter)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::StretchRect called using SourceSurface=%x, DestSurface=%x\n", pSourceRect, pDestSurface);
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	D3D9Base::LPDIRECT3DSURFACE9 baseSourceSurface = replaceSurface9(pSourceSurface);
	D3D9Base::LPDIRECT3DSURFACE9 baseDestSurface = replaceSurface9(pDestSurface);
	HRESULT hr = GetD3D9Device()->StretchRect(baseSourceSurface, pSourceRect, baseDestSurface, pDestRect, Filter);
	if (LogFile) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::ColorFill(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSurface,CONST RECT* pRect,D3D9Base::D3DCOLOR color)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::ColorFill called with Surface=%x\n", pSurface);
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	D3D9Base::LPDIRECT3DSURFACE9 baseSurface = replaceSurface9(pSurface);
	HRESULT hr = GetD3D9Device()->ColorFill(baseSurface, pRect, color);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateOffscreenPlainSurface(THIS_ UINT Width,UINT Height,D3D9Base::D3DFORMAT Format,D3D9Base::D3DPOOL Pool, IDirect3DSurface9 **ppSurface,HANDLE* pSharedHandle)
{
	if (LogFile) fprintf(LogFile, "IDirect3DDevice9::CreateOffscreenPlainSurface called with Width=%d Height=%d\n", Width, Height);
	if (LogFile) fflush(LogFile);
	CheckDevice(this);

	D3D9Base::LPDIRECT3DSURFACE9 baseSurface = NULL;
	if (Pool == D3D9Base::D3DPOOL_MANAGED)
	{
		if (LogFile && LogDebug) fprintf(LogFile, "  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");
		if (LogFile) fflush(LogFile);
		Pool = D3D9Base::D3DPOOL_DEFAULT;
	}
    HRESULT hr = GetD3D9Device()->CreateOffscreenPlainSurface(Width, Height, Format, Pool, &baseSurface, pSharedHandle);
	if (ppSurface && baseSurface) *ppSurface = IDirect3DSurface9::GetDirect3DSurface9(baseSurface);    
	if (LogFile && ppSurface) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseSurface, *ppSurface);
	if (LogFile) fflush(LogFile);
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetRenderTarget(THIS_ DWORD RenderTargetIndex, D3D9Wrapper::IDirect3DSurface9 *pRenderTarget)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetRenderTarget called with RenderTargetIndex=%d, pRenderTarget=%x.\n", RenderTargetIndex, pRenderTarget);
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	D3D9Base::LPDIRECT3DSURFACE9 baseRenderTarget = replaceSurface9(pRenderTarget);
	HRESULT hr = GetD3D9Device()->SetRenderTarget(RenderTargetIndex, baseRenderTarget);
	if (LogFile) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetRenderTarget(THIS_ DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetRenderTarget called with RenderTargetIndex=%d\n", RenderTargetIndex);
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);

	D3D9Base::LPDIRECT3DSURFACE9 baseSurface = 0;
    HRESULT hr = GetD3D9Device()->GetRenderTarget(RenderTargetIndex, &baseSurface);
    if (FAILED(hr) || !baseSurface)
    {
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetRenderTarget called.\n");	
		if (LogFile) fprintf(LogFile, "  failed with hr=%x\n", hr);
		if (LogFile) fflush(LogFile);
		if (ppRenderTarget) *ppRenderTarget = 0;
        return hr;
    }
	if (ppRenderTarget && baseSurface) *ppRenderTarget = IDirect3DSurface9::GetDirect3DSurface9(baseSurface);
	if (LogFile && LogDebug && ppRenderTarget) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseSurface, *ppRenderTarget);
	if (LogFile && LogDebug) fflush(LogFile);
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetDepthStencilSurface(THIS_ D3D9Wrapper::IDirect3DSurface9 *pNewZStencil)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetDepthStencilSurface called with NewZStencil=%x\n", pNewZStencil);
	if (LogFile && LogDebug) fflush(LogFile);

	if (!GetD3D9Device())
	{
		if (LogFile) fprintf(LogFile, "  postponing call because device was not created yet.\n");
		if (LogFile) fflush(LogFile);
		pendingSetDepthStencilSurface = pNewZStencil;
		return S_OK;
	}

	D3D9Base::LPDIRECT3DSURFACE9 baseStencil = replaceSurface9(pNewZStencil);
	HRESULT hr = GetD3D9Device()->SetDepthStencilSurface(baseStencil);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetDepthStencilSurface(THIS_ IDirect3DSurface9 **ppZStencilSurface)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetDepthStencilSurface called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);

	D3D9Base::LPDIRECT3DSURFACE9 baseSurface = 0;
    HRESULT hr = GetD3D9Device()->GetDepthStencilSurface(&baseSurface);
    if (FAILED(hr) || !baseSurface)
    {
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetDepthStencilSurface called.\n");	
		if (LogFile) fprintf(LogFile, "  failed with hr=%x\n", hr);
		if (LogFile) fflush(LogFile);
		if (ppZStencilSurface) *ppZStencilSurface = 0;
        return hr;
    }
	if (ppZStencilSurface) *ppZStencilSurface = IDirect3DSurface9::GetDirect3DSurface9(baseSurface);
	if (LogFile && LogDebug && ppZStencilSurface) fprintf(LogFile, "  returns hr=%x, handle=%x, wrapper=%x\n", hr, baseSurface, *ppZStencilSurface);
	if (LogFile && LogDebug) fflush(LogFile);
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::BeginScene(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::BeginScene called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
    HRESULT hr = GetD3D9Device()->BeginScene();
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::EndScene(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::EndScene called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
    HRESULT hr = GetD3D9Device()->EndScene();
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::Clear(THIS_ DWORD Count,CONST D3D9Base::D3DRECT* pRects,DWORD Flags,D3D9Base::D3DCOLOR Color,float Z,DWORD Stencil)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::Clear called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->Clear(Count, pRects, Flags, Color, Z, Stencil);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetTransform(THIS_ D3D9Base::D3DTRANSFORMSTATETYPE State,CONST D3D9Base::D3DMATRIX* pMatrix)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetTransform called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetTransform(State, pMatrix);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetTransform(THIS_ D3D9Base::D3DTRANSFORMSTATETYPE State,D3D9Base::D3DMATRIX* pMatrix)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetTransform called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetTransform(State, pMatrix);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::MultiplyTransform(THIS_ D3D9Base::D3DTRANSFORMSTATETYPE a,CONST D3D9Base::D3DMATRIX *b)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::MultiplyTransform called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->MultiplyTransform(a, b);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetViewport(THIS_ CONST D3D9Base::D3DVIEWPORT9* pViewport)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetViewport called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetViewport(pViewport);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetViewport(THIS_ D3D9Base::D3DVIEWPORT9* pViewport)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetViewport called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetViewport(pViewport);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetMaterial(THIS_ CONST D3D9Base::D3DMATERIAL9* pMaterial)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetMaterial called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetMaterial(pMaterial);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetMaterial(THIS_ D3D9Base::D3DMATERIAL9* pMaterial)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetMaterial called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetMaterial(pMaterial);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetLight(THIS_ DWORD Index,CONST D3D9Base::D3DLIGHT9 *Light)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetLight called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetLight(Index, Light);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetLight(THIS_ DWORD Index,D3D9Base::D3DLIGHT9* Light)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetLight called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetLight(Index, Light);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::LightEnable(THIS_ DWORD Index,BOOL Enable)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::LightEnable called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->LightEnable(Index, Enable);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetLightEnable(THIS_ DWORD Index,BOOL* pEnable)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetLightEnable called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetLightEnable(Index, pEnable);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetClipPlane(THIS_ DWORD Index,CONST float* pPlane)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetClipPlane called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetClipPlane(Index, pPlane);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetClipPlane(THIS_ DWORD Index,float* pPlane)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetClipPlane called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetClipPlane(Index, pPlane);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetRenderState(THIS_ D3D9Base::D3DRENDERSTATETYPE State,DWORD Value)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetRenderState called with State=%d, Value=%d\n", State, Value);
	if (LogFile && LogDebug) fflush(LogFile);

	if (!GetD3D9Device())
	{
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetRenderState called.\n");
		if (LogFile) fprintf(LogFile, "  ignoring call because device was not created yet.\n");
		if (LogFile) fflush(LogFile);
		return S_OK;
	}

	HRESULT hr = GetD3D9Device()->SetRenderState(State, Value);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetRenderState(THIS_ D3D9Base::D3DRENDERSTATETYPE State,DWORD* pValue)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetRenderState called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetRenderState(State, pValue);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateStateBlock(THIS_ D3D9Base::D3DSTATEBLOCKTYPE Type,D3D9Base::IDirect3DStateBlock9** ppSB)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::CreateStateBlock called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->CreateStateBlock(Type, ppSB);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::BeginStateBlock(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::BeginStateBlock called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->BeginStateBlock();
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::EndStateBlock(THIS_ D3D9Base::IDirect3DStateBlock9** ppSB)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::EndStateBlock called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->EndStateBlock(ppSB);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetClipStatus(THIS_ CONST D3D9Base::D3DCLIPSTATUS9* pClipStatus)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetClipStatus called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetClipStatus(pClipStatus);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetClipStatus(THIS_ D3D9Base::D3DCLIPSTATUS9* pClipStatus)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetClipStatus called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetClipStatus(pClipStatus);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetTexture(THIS_ DWORD Stage,D3D9Base::LPDIRECT3DBASETEXTURE9* ppTexture)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetTexture called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);

	D3D9Base::LPDIRECT3DBASETEXTURE9 baseTexture = 0;
    HRESULT hr = GetD3D9Device()->GetTexture(Stage, &baseTexture);
    if (FAILED(hr) || !baseTexture)
    {
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetTexture called.\n");
		if (LogFile) fprintf(LogFile, "  failed with hr=%x\n", hr);
		if (LogFile) fflush(LogFile);
		if (ppTexture) *ppTexture = 0;
        return hr;
    } 
	if (ppTexture) *ppTexture = baseTexture;
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetTexture(THIS_ DWORD Stage,D3D9Base::LPDIRECT3DBASETEXTURE9 pTexture)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetTexture called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetTexture(Stage, replaceTexture9((IDirect3DTexture9 *) pTexture));
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetTextureStageState(THIS_ DWORD Stage,D3D9Base::D3DTEXTURESTAGESTATETYPE Type,DWORD* pValue)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetTextureStageState called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetTextureStageState(Stage, Type, pValue);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetTextureStageState(THIS_ DWORD Stage,D3D9Base::D3DTEXTURESTAGESTATETYPE Type,DWORD Value)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetTextureStageState called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetTextureStageState(Stage, Type, Value);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetSamplerState(THIS_ DWORD Sampler,D3D9Base::D3DSAMPLERSTATETYPE Type,DWORD* pValue)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetSamplerState called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetSamplerState(Sampler, Type, pValue);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetSamplerState(THIS_ DWORD Sampler,D3D9Base::D3DSAMPLERSTATETYPE Type,DWORD Value)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetSamplerState called with Sampler=%d, StateType=%d, Value=%d\n", Sampler, Type, Value);
	if (LogFile && LogDebug) fflush(LogFile);

	if (!GetD3D9Device())
	{
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetSamplerState called.\n");
		if (LogFile) fprintf(LogFile, "  ignoring call because device was not created yet.\n");
		if (LogFile) fflush(LogFile);
		return S_OK;
	}

	HRESULT hr = GetD3D9Device()->SetSamplerState(Sampler, Type, Value);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::ValidateDevice(THIS_ DWORD* pNumPasses)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::ValidateDevice called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->ValidateDevice(pNumPasses);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetPaletteEntries(THIS_ UINT PaletteNumber,CONST PALETTEENTRY* pEntries)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetPaletteEntries called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetPaletteEntries(PaletteNumber, pEntries);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetPaletteEntries(THIS_ UINT PaletteNumber,PALETTEENTRY* pEntries)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetPaletteEntries called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetPaletteEntries(PaletteNumber, pEntries);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetCurrentTexturePalette(THIS_ UINT PaletteNumber)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetCurrentTexturePalette called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->SetCurrentTexturePalette(PaletteNumber);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetCurrentTexturePalette(THIS_ UINT *PaletteNumber)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetCurrentTexturePalette called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->GetCurrentTexturePalette( PaletteNumber);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetScissorRect(THIS_ CONST RECT* pRect)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetScissorRect called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->SetScissorRect(pRect);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetScissorRect(THIS_ RECT* pRect)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetScissorRect called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->GetScissorRect(pRect);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetSoftwareVertexProcessing(THIS_ BOOL bSoftware)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetSoftwareVertexProcessing called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->SetSoftwareVertexProcessing(bSoftware);
}

STDMETHODIMP_(BOOL) D3D9Wrapper::IDirect3DDevice9::GetSoftwareVertexProcessing(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetSoftwareVertexProcessing called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->GetSoftwareVertexProcessing();
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetNPatchMode(THIS_ float nSegments)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetNPatchMode called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->SetNPatchMode(nSegments);
}

STDMETHODIMP_(float) D3D9Wrapper::IDirect3DDevice9::GetNPatchMode(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetNPatchMode called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->GetNPatchMode();
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::DrawPrimitive(THIS_ D3D9Base::D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::DrawPrimitive called with PrimitiveType=%d, StartVertex=%d, PrimitiveCount=%d\n",
		PrimitiveType, StartVertex, PrimitiveCount);
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::DrawIndexedPrimitive(THIS_ D3D9Base::D3DPRIMITIVETYPE Type,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::DrawIndexedPrimitive called with Type=%d, BaseVertexIndex=%d, MinVertexIndex=%d, NumVertices=%d, startIndex=%d, primCount=%d\n",
		Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->DrawIndexedPrimitive(Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::DrawPrimitiveUP(THIS_ D3D9Base::D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::DrawPrimitiveUP called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::DrawIndexedPrimitiveUP(THIS_ D3D9Base::D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,CONST void* pIndexData,D3D9Base::D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::DrawIndexedPrimitiveUP called with PrimitiveType=%d, MinVertexIndex=%d, NumVertices=%d, PrimitiveCount=%d, IndexDataFormat=%d\n",
		PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, IndexDataFormat);
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::ProcessVertices(THIS_ UINT SrcStartIndex,UINT DestIndex,UINT VertexCount, IDirect3DVertexBuffer9 *pDestBuffer,IDirect3DVertexDeclaration9* pVertexDecl,DWORD Flags)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::ProcessVertices called with SrcStartIndex=%d, DestIndex=%d, VertexCount=%d, Flags=%x\n",
		SrcStartIndex, DestIndex, VertexCount, Flags);
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	CheckVertexBuffer9(pDestBuffer);
	HRESULT hr = GetD3D9Device()->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, replaceVertexBuffer9(pDestBuffer), replaceVertexDeclaration9(pVertexDecl), Flags);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateVertexDeclaration(THIS_ CONST D3D9Base::D3DVERTEXELEMENT9* pVertexElements,IDirect3DVertexDeclaration9** ppDecl)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::CreateVertexDeclaration called.\n");
	if (LogFile && LogDebug) fflush(LogFile);

	if (!GetD3D9Device())
	{
		if (LogFile) fprintf(LogFile, "  postponing call because device was not created yet.\n");
		if (LogFile) fflush(LogFile);
		D3D9Wrapper::IDirect3DVertexDeclaration9 *wrapper = D3D9Wrapper::IDirect3DVertexDeclaration9::GetDirect3DVertexDeclaration9((D3D9Base::LPDIRECT3DVERTEXDECLARATION9) 0);
		if (pVertexElements)
			wrapper->_VertexElements = *pVertexElements;
		wrapper->pendingDevice = this;
		if (LogFile) fprintf(LogFile, "  returns handle=%x\n", wrapper);
		if (LogFile) fflush(LogFile);
		return S_OK;
	}

	D3D9Base::LPDIRECT3DVERTEXDECLARATION9 baseVertexDeclaration = 0;
	HRESULT hr = GetD3D9Device()->CreateVertexDeclaration(pVertexElements, &baseVertexDeclaration);
	if (ppDecl && baseVertexDeclaration) *ppDecl = IDirect3DVertexDeclaration9::GetDirect3DVertexDeclaration9(baseVertexDeclaration);
	if (LogFile && LogDebug && ppDecl) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseVertexDeclaration, *ppDecl);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetVertexDeclaration(THIS_ IDirect3DVertexDeclaration9* pDecl)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetVertexDeclaration called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->SetVertexDeclaration(replaceVertexDeclaration9(pDecl));
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetVertexDeclaration(THIS_ IDirect3DVertexDeclaration9** ppDecl)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetVertexDeclaration called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);

	D3D9Base::LPDIRECT3DVERTEXDECLARATION9 baseVertexDeclaration = 0;
	HRESULT hr = GetD3D9Device()->GetVertexDeclaration(&baseVertexDeclaration);
	if (ppDecl && baseVertexDeclaration) *ppDecl = IDirect3DVertexDeclaration9::GetDirect3DVertexDeclaration9(baseVertexDeclaration);
	if (LogFile && LogDebug && ppDecl) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseVertexDeclaration, *ppDecl);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetFVF(THIS_ DWORD FVF)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetFVF called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->SetFVF(FVF);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetFVF(THIS_ DWORD* pFVF)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetFVF called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->GetFVF(pFVF);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateVertexShader(THIS_ CONST DWORD* pFunction, IDirect3DVertexShader9 **ppShader)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::CreateVertexShader called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);

	D3D9Base::IDirect3DVertexShader9 *baseShader = 0;
	HRESULT hr = GetD3D9Device()->CreateVertexShader(pFunction, &baseShader);
	if (ppShader && baseShader) *ppShader = IDirect3DVertexShader9::GetDirect3DVertexShader9(baseShader);
	if (LogFile && LogDebug && ppShader) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseShader, *ppShader);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetVertexShader(THIS_ IDirect3DVertexShader9 *pShader)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetVertexShader called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetVertexShader(replaceVertexShader9(pShader));
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetVertexShader(THIS_ IDirect3DVertexShader9** ppShader)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetVertexShader called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	D3D9Base::IDirect3DVertexShader9 *baseShader = 0;
	HRESULT hr = GetD3D9Device()->GetVertexShader(&baseShader);
	if (ppShader && baseShader) *ppShader = IDirect3DVertexShader9::GetDirect3DVertexShader9(baseShader);
	if (LogFile && LogDebug && ppShader) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseShader, *ppShader);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;	
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetVertexShaderConstantF(THIS_ UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetVertexShaderConstantF called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetVertexShaderConstantF(THIS_ UINT StartRegister,float* pConstantData,UINT Vector4fCount)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetVertexShaderConstantF called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetVertexShaderConstantI(THIS_ UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetVertexShaderConstantI called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetVertexShaderConstantI(THIS_ UINT StartRegister,int* pConstantData,UINT Vector4iCount)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetVertexShaderConstantI called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetVertexShaderConstantB(THIS_ UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetVertexShaderConstantB called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetVertexShaderConstantB(THIS_ UINT StartRegister,BOOL* pConstantData,UINT BoolCount)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetVertexShaderConstantB called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetStreamSource(THIS_ UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes,UINT Stride)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetStreamSource called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->SetStreamSource(StreamNumber, replaceVertexBuffer9(pStreamData), OffsetInBytes, Stride);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetStreamSource(THIS_ UINT StreamNumber, IDirect3DVertexBuffer9 **ppStreamData,UINT* pOffsetInBytes,UINT* pStride)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetStreamSource called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	D3D9Base::LPDIRECT3DVERTEXBUFFER9 baseVB = 0;
    HRESULT hr = GetD3D9Device()->GetStreamSource(StreamNumber, &baseVB, pOffsetInBytes, pStride);
	if (ppStreamData && baseVB) *ppStreamData = IDirect3DVertexBuffer9::GetDirect3DVertexBuffer9(baseVB);
	if (LogFile && LogDebug && ppStreamData) fprintf(LogFile, "  returns hr=%x, handle=%x, wrapper=%x\n", hr, baseVB, *ppStreamData);
	if (LogFile && LogDebug) fflush(LogFile);
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetStreamSourceFreq(THIS_ UINT StreamNumber, UINT FrequencyParameter)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetStreamSourceFreq called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->SetStreamSourceFreq(StreamNumber, FrequencyParameter);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetStreamSourceFreq(THIS_ UINT StreamNumber,UINT* pSetting)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetStreamSourceFreq called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->GetStreamSourceFreq(StreamNumber, pSetting);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetIndices(THIS_ IDirect3DIndexBuffer9 *pIndexData)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetIndices called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);

	HRESULT hr = GetD3D9Device()->SetIndices(replaceIndexBuffer9(pIndexData));
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetIndices(THIS_ IDirect3DIndexBuffer9 **ppIndexData)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetIndices called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);

	D3D9Base::LPDIRECT3DINDEXBUFFER9 baseIB = 0;
    HRESULT hr = GetD3D9Device()->GetIndices(&baseIB);
    if (FAILED(hr) || !baseIB)
    {
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetIndices called.\n");
		if (LogFile) fprintf(LogFile, "  failed with hr=%x\n", hr);
		if (LogFile) fflush(LogFile);
		if (ppIndexData) *ppIndexData = 0;
        return hr;
    }
	if (ppIndexData && baseIB) *ppIndexData = IDirect3DIndexBuffer9::GetDirect3DIndexBuffer9(baseIB);
	if (LogFile && LogDebug && ppIndexData) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseIB, *ppIndexData);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreatePixelShader(THIS_ CONST DWORD* pFunction, IDirect3DPixelShader9 **ppShader)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::CreatePixelShader called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);

	D3D9Base::IDirect3DPixelShader9 *baseShader = 0;
	HRESULT hr = GetD3D9Device()->CreatePixelShader(pFunction, &baseShader);
	if (ppShader && baseShader) *ppShader = IDirect3DPixelShader9::GetDirect3DPixelShader9(baseShader);
	if (LogFile && LogDebug && ppShader) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseShader, *ppShader);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;

}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetPixelShader(THIS_ IDirect3DPixelShader9 *pShader)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetPixelShader called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetPixelShader(replacePixelShader9(pShader));
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetPixelShader(THIS_ IDirect3DPixelShader9 **ppShader)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetPixelShader called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	D3D9Base::IDirect3DPixelShader9 *baseShader = 0;
	HRESULT hr = GetD3D9Device()->GetPixelShader(&baseShader);
	if (ppShader && baseShader) *ppShader = IDirect3DPixelShader9::GetDirect3DPixelShader9(baseShader);
	if (LogFile && LogDebug && ppShader) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseShader, *ppShader);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;	
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetPixelShaderConstantF(THIS_ UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetPixelShaderConstantF called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetPixelShaderConstantF(THIS_ UINT StartRegister,float* pConstantData,UINT Vector4fCount)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetPixelShaderConstantF called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetPixelShaderConstantI(THIS_ UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetPixelShaderConstantI called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetPixelShaderConstantI(THIS_ UINT StartRegister,int* pConstantData,UINT Vector4iCount)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetPixelShaderConstantI called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetPixelShaderConstantB(THIS_ UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::SetPixelShaderConstantB called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetPixelShaderConstantB(THIS_ UINT StartRegister,BOOL* pConstantData,UINT BoolCount)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetPixelShaderConstantB called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::DrawRectPatch(THIS_ UINT Handle,CONST float* pNumSegs,CONST D3D9Base::D3DRECTPATCH_INFO* pRectPatchInfo)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::DrawRectPatch called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::DrawTriPatch(THIS_ UINT Handle,CONST float* pNumSegs,CONST D3D9Base::D3DTRIPATCH_INFO* pTriPatchInfo)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::DrawTriPatch called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::DeletePatch(THIS_ UINT Handle)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::DeletePatch called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->DeletePatch(Handle);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateQuery(THIS_ D3D9Base::D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::CreateQuery called with Type=%d, ppQuery=%x\n", Type, ppQuery);
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);

	D3D9Base::LPDIRECT3DQUERY9 baseQuery = 0;
	HRESULT hr = GetD3D9Device()->CreateQuery(Type, &baseQuery);
    if (FAILED(hr) || !baseQuery)
    {
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DDevice9::CreateQuery called.\n");
		if (LogFile) fprintf(LogFile, "  failed with hr=%x\n", hr);
		if (LogFile) fflush(LogFile);
		if (ppQuery) *ppQuery = 0;
        return hr;
    } 
	if (ppQuery && baseQuery) *ppQuery = IDirect3DQuery9::GetDirect3DQuery9(baseQuery);
	if (LogFile && LogDebug && ppQuery) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseQuery, *ppQuery);
	if (LogFile && LogDebug && !ppQuery) fprintf(LogFile, "  returns result=%x, handle=%x\n", hr, baseQuery);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetDisplayModeEx(THIS_ UINT iSwapChain,D3D9Base::D3DDISPLAYMODEEX* pMode,D3D9Base::D3DDISPLAYROTATION* pRotation)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DDevice9::GetDisplayModeEx called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckDevice(this);
	return GetD3D9Device()->GetDisplayModeEx(iSwapChain, pMode, pRotation);
}
