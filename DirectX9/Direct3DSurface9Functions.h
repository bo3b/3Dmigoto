
D3D9Wrapper::IDirect3DSurface9::IDirect3DSurface9(D3D9Base::LPDIRECT3DSURFACE9 pSurface)
    : D3D9Wrapper::IDirect3DUnknown((IUnknown*) pSurface),
	pendingGetSurfaceLevel(false),
	magic(0x7da43feb)
{
}

D3D9Wrapper::IDirect3DSurface9* D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(D3D9Base::LPDIRECT3DSURFACE9 pSurface)
{
    D3D9Wrapper::IDirect3DSurface9* p = (D3D9Wrapper::IDirect3DSurface9*) m_List.GetDataPtr(pSurface);
    if (!p)
    {
        p = new D3D9Wrapper::IDirect3DSurface9(pSurface);
        if (pSurface) m_List.AddMember(pSurface, p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DSurface9::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DSurface9::Release(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	if (LogFile && LogDebug) fflush(LogFile);
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	if (LogFile && LogDebug) fprintf(LogFile, "  internal counter = %d\n", ulRef);
	if (LogFile && LogDebug) fflush(LogFile);
	--m_ulRef;

    if (ulRef == 0)
    {
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DSurface9::Release handle=%x, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		if (LogFile) fprintf(LogFile, "  deleting self\n");
		if (LogFile) fflush(LogFile);
        if (m_pUnk) m_List.DeleteMember(m_pUnk); 
		m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::GetDevice(THIS_ IDirect3DDevice9** ppDevice)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::GetDevice called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckSurface9(this);
	D3D9Base::IDirect3DDevice9 *origDevice;
	HRESULT hr = GetD3DSurface9()->GetDevice(&origDevice);
	if (hr != S_OK)
	{
		if (LogFile) fprintf(LogFile, "  failed with hr = %x\n", hr);
		if (LogFile) fflush(LogFile);
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
		if (LogFile) fflush(LogFile);
		if (ppDevice) *ppDevice = 0;
		return hr;
	}
	*ppDevice = D3D9Wrapper::IDirect3DDevice9::GetDirect3DDevice(origDeviceEx);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::SetPrivateData(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::SetPrivateData called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckSurface9(this);
	return GetD3DSurface9()->SetPrivateData(refguid, pData, SizeOfData, Flags);
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::GetPrivateData(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::GetPrivateData called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckSurface9(this);
	return GetD3DSurface9()->GetPrivateData(refguid, pData, pSizeOfData);
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::FreePrivateData(THIS_ REFGUID refguid)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::GetPrivateData called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckSurface9(this);
	return GetD3DSurface9()->FreePrivateData(refguid);
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DSurface9::SetPriority(THIS_ DWORD PriorityNew)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::SetPriority called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckSurface9(this);
	return GetD3DSurface9()->SetPriority(PriorityNew);
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DSurface9::GetPriority(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::GetPriority called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckSurface9(this);
	return GetD3DSurface9()->GetPriority();
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DSurface9::PreLoad(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::GetPriority called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckSurface9(this);
	return GetD3DSurface9()->PreLoad();
}

STDMETHODIMP_(D3D9Base::D3DRESOURCETYPE) D3D9Wrapper::IDirect3DSurface9::GetType(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::GetType called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckSurface9(this);
	return GetD3DSurface9()->GetType();
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::GetContainer(THIS_ REFIID riid,void** ppContainer)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::GetContainer called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckSurface9(this);
	return GetD3DSurface9()->GetContainer(riid, ppContainer);
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::GetDesc(THIS_ D3D9Base::D3DSURFACE_DESC *pDesc)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::GetDesc called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckSurface9(this);
	return GetD3DSurface9()->GetDesc(pDesc);
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::LockRect(THIS_ D3D9Base::D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::LockRect called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckSurface9(this);
	return GetD3DSurface9()->LockRect(pLockedRect, pRect, Flags);
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::UnlockRect(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::UnlockRect called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckSurface9(this);
	return GetD3DSurface9()->UnlockRect();
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::GetDC(THIS_ HDC *phdc)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::GetDC called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckSurface9(this);
	return GetD3DSurface9()->GetDC(phdc);
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::ReleaseDC(THIS_ HDC hdc)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DSurface9::ReleaseDC called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckSurface9(this);
	return GetD3DSurface9()->ReleaseDC(hdc);
}
