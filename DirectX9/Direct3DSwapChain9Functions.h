D3D9Wrapper::IDirect3DSwapChain9::IDirect3DSwapChain9( D3D9Base::LPDIRECT3DSWAPCHAIN9EX pSwapChain, D3D9Wrapper::IDirect3DDevice9* pDevice )
    : IDirect3DUnknown((IUnknown*) pSwapChain),
	pendingGetSwapChain(false)
{
//    m_pDevice = pDevice;
}

D3D9Wrapper::IDirect3DSwapChain9* D3D9Wrapper::IDirect3DSwapChain9::GetSwapChain(D3D9Base::LPDIRECT3DSWAPCHAIN9EX pSwapChain, D3D9Wrapper::IDirect3DDevice9* pDevice )
{
    D3D9Wrapper::IDirect3DSwapChain9* p = (D3D9Wrapper::IDirect3DSwapChain9*) m_List.GetDataPtr(pSwapChain);
    if (!p)
    {
        p = new D3D9Wrapper::IDirect3DSwapChain9(pSwapChain, pDevice);
        if (pSwapChain) m_List.AddMember(pSwapChain, p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DSwapChain9::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DSwapChain9::Release(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSwapChain9::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	if (LogFile && LogDebug) fprintf(LogFile, "  internal counter = %d\n", ulRef);
	
	--m_ulRef;

    if (ulRef == 0)
    {
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DSwapChain9::Release handle=%x, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		if (LogFile) fprintf(LogFile, "  deleting self\n");
		
        if (m_pUnk) m_List.DeleteMember(m_pUnk); 
		m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

static void CheckSwapChain(D3D9Wrapper::IDirect3DSwapChain9 *me)
{
	if (!me->pendingGetSwapChain)
		return;
	me->pendingGetSwapChain = false;
	CheckDevice(me->pendingDevice);

	if (LogFile) fprintf(LogFile, "  calling postponed GetSwapChain.\n");
	D3D9Base::LPDIRECT3DSWAPCHAIN9 baseSwapChain = NULL;
	HRESULT hr = me->pendingDevice->GetD3D9Device()->GetSwapChain(me->_SwapChain, &baseSwapChain);
	if (FAILED(hr))
	{
		if (LogFile) fprintf(LogFile, "    failed getting swap chain with result = %x\n", hr);
		
		return;
	}
	const IID IID_IDirect3DSwapChain9Ex = { 0x91886caf, 0x1c3d, 0x4d2e, { 0xa0, 0xab, 0x3e, 0x4c, 0x7d, 0x8d, 0x33, 0x3 }};
	hr = baseSwapChain->QueryInterface(IID_IDirect3DSwapChain9Ex, (void **)&me->m_pUnk);
	baseSwapChain->Release();
	if (FAILED(hr))
	{
		if (LogFile) fprintf(LogFile, "    failed casting swap chain to IDirect3DSwapChain9 with result = %x\n", hr);
		
		return;
	}	
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::Present(THIS_ CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion,DWORD dwFlags)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSwapChain9::Present called\n");
	
	CheckSwapChain(this);
	HRESULT hr = GetSwapChain9()->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetFrontBufferData(THIS_ IDirect3DSurface9 *pDestSurface)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSwapChain9::GetFrontBufferData called\n");
	
	CheckSwapChain(this);
	HRESULT hr = GetSwapChain9()->GetFrontBufferData(replaceSurface9(pDestSurface));
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetBackBuffer(THIS_ UINT iBackBuffer,D3D9Base::D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSwapChain9::GetBackBuffer called\n");
	
	CheckSwapChain(this);
	D3D9Base::LPDIRECT3DSURFACE9 baseSurface = 0;
    HRESULT hr = GetSwapChain9()->GetBackBuffer(iBackBuffer, Type, &baseSurface);
    if (FAILED(hr) || !baseSurface)
    {
		if (LogFile) fprintf(LogFile, "  failed with hr = %x\n", hr);
		
		if (ppBackBuffer) *ppBackBuffer = 0;
        return hr;
    }
	if (ppBackBuffer && baseSurface) *ppBackBuffer = IDirect3DSurface9::GetDirect3DSurface9(baseSurface);
	if (LogFile && LogDebug && ppBackBuffer) fprintf(LogFile, "  returns result=%x, handle=%x, wrapper=%x\n", hr, baseSurface, *ppBackBuffer);
	
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetRasterStatus(THIS_ D3D9Base::D3DRASTER_STATUS* pRasterStatus)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSwapChain9::GetRasterStatus called\n");
	
	CheckSwapChain(this);
	return GetSwapChain9()->GetRasterStatus(pRasterStatus);
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetDisplayMode(THIS_ D3D9Base::D3DDISPLAYMODE* pMode)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSwapChain9::GetDisplayMode called\n");
	
	CheckSwapChain(this);
	return GetSwapChain9()->GetDisplayMode(pMode);
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetDevice(THIS_ IDirect3DDevice9** ppDevice)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSwapChain9::GetDevice called\n");
	
	CheckSwapChain(this);
	D3D9Base::IDirect3DDevice9 *origDevice;
	HRESULT hr = GetSwapChain9()->GetDevice(&origDevice);
	if (hr != S_OK)
	{
		if (LogFile) fprintf(LogFile, "  failed with hr = %x\n", hr);
		
		if (ppDevice) *ppDevice = 0;
		return hr;
	}
	D3D9Base::IDirect3DDevice9Ex *origDeviceEx;
	const IID IID_IDirect3DDevice9Ex = { 0xb18b10ce, 0x2649, 0x405a, { 0x87, 0xf, 0x95, 0xf7, 0x77, 0xd4, 0x31, 0x3a } };
	hr = origDevice->QueryInterface(IID_IDirect3DDevice9Ex, (void **) &origDeviceEx);
	origDevice->Release();
	if (hr != S_OK)
	{
		if (LogFile) fprintf(LogFile, "  failed IID_IDirect3DDevice9Ex cast with hr = %x\n", hr);
		
		if (ppDevice) *ppDevice = 0;
		return hr;
	}
	*ppDevice = D3D9Wrapper::IDirect3DDevice9::GetDirect3DDevice(origDeviceEx);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetPresentParameters(THIS_ D3D9Base::D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSwapChain9::GetPresentParameters called.\n");
	
	CheckSwapChain(this);
	return GetSwapChain9()->GetPresentParameters(pPresentationParameters);
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetDisplayModeEx(THIS_ D3D9Base::D3DDISPLAYMODEEX* pMode,D3D9Base::D3DDISPLAYROTATION* pRotation)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSwapChain9::GetDisplayModeEx called.\n");
	
	CheckSwapChain(this);
	return GetSwapChain9()->GetDisplayModeEx(pMode, pRotation);
}
