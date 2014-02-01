
D3D9Wrapper::IDirect3DVertexBuffer9::IDirect3DVertexBuffer9(D3D9Base::LPDIRECT3DVERTEXBUFFER9 pVertexBuffer)
    : D3D9Wrapper::IDirect3DUnknown((IUnknown*) pVertexBuffer),
	pendingCreateVertexBuffer(false),
	pendingLockUnlock(false),
	magic(0x7da43feb)
{
}

D3D9Wrapper::IDirect3DVertexBuffer9* D3D9Wrapper::IDirect3DVertexBuffer9::GetDirect3DVertexBuffer9(D3D9Base::LPDIRECT3DVERTEXBUFFER9 pVertexBuffer)
{
    D3D9Wrapper::IDirect3DVertexBuffer9* p = (D3D9Wrapper::IDirect3DVertexBuffer9*) m_List.GetDataPtr(pVertexBuffer);
    if (!p)
    {
        p = new D3D9Wrapper::IDirect3DVertexBuffer9(pVertexBuffer);
        if (pVertexBuffer) m_List.AddMember(pVertexBuffer, p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DVertexBuffer9::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DVertexBuffer9::Release(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	if (LogFile && LogDebug) fflush(LogFile);
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	if (LogFile && LogDebug) fprintf(LogFile, "  internal counter = %d\n", ulRef);
	if (LogFile && LogDebug) fflush(LogFile);
	--m_ulRef;

    if (ulRef == 0)
    {
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::Release handle=%x, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		if (LogFile) fprintf(LogFile, "  deleting self\n");
		if (LogFile) fflush(LogFile);
        if (m_pUnk) m_List.DeleteMember(m_pUnk); 
		m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3DVertexBuffer9::GetDevice(THIS_ IDirect3DDevice9** ppDevice)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::GetDevice called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckVertexBuffer9(this);
	D3D9Base::IDirect3DDevice9 *origDevice;
	HRESULT hr = GetD3DVertexBuffer9()->GetDevice(&origDevice);
	if (hr != S_OK)
	{
		if (LogFile) fprintf(LogFile, "  failed with hr = %x\n", hr);
		if (LogFile) fflush(LogFile);
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
		return hr;
	}
	*ppDevice = D3D9Wrapper::IDirect3DDevice9::GetDirect3DDevice(origDeviceEx);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DVertexBuffer9::SetPrivateData(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::SetPrivateData called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckVertexBuffer9(this);
	return GetD3DVertexBuffer9()->SetPrivateData(refguid, pData, SizeOfData, Flags);
}

STDMETHODIMP D3D9Wrapper::IDirect3DVertexBuffer9::GetPrivateData(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::GetPrivateData called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckVertexBuffer9(this);
	return GetD3DVertexBuffer9()->GetPrivateData(refguid, pData, pSizeOfData);
}

STDMETHODIMP D3D9Wrapper::IDirect3DVertexBuffer9::FreePrivateData(THIS_ REFGUID refguid)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::GetPrivateData called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckVertexBuffer9(this);
	return GetD3DVertexBuffer9()->FreePrivateData(refguid);
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DVertexBuffer9::SetPriority(THIS_ DWORD PriorityNew)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::SetPriority called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckVertexBuffer9(this);
	return GetD3DVertexBuffer9()->SetPriority(PriorityNew);
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DVertexBuffer9::GetPriority(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::GetPriority called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckVertexBuffer9(this);
	return GetD3DVertexBuffer9()->GetPriority();
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DVertexBuffer9::PreLoad(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::GetPriority called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckVertexBuffer9(this);
	return GetD3DVertexBuffer9()->PreLoad();
}

STDMETHODIMP_(D3D9Base::D3DRESOURCETYPE) D3D9Wrapper::IDirect3DVertexBuffer9::GetType(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::GetType called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckVertexBuffer9(this);
	D3D9Base::D3DRESOURCETYPE hr = GetD3DVertexBuffer9()->GetType();
	if (LogFile && LogDebug) fprintf(LogFile, "  returns ResourceType=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DVertexBuffer9::Lock(THIS_ UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::Lock called with OffsetToLock=%d, SizeToLock=%d, Flags=%x\n", OffsetToLock, SizeToLock, Flags);
	if (LogFile && LogDebug) fflush(LogFile);

	if (!GetD3DVertexBuffer9())
	{
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::Lock called\n");
		if (LogFile) fprintf(LogFile, "  postponing call because vertex buffer was not created yet.\n");
		if (LogFile) fflush(LogFile);
		if (!pendingLockUnlock)
		{
			_Flags = Flags;
			_Buffer = new char[_Length];
			pendingLockUnlock = true;
		}
		if (ppbData) *ppbData = _Buffer+OffsetToLock;
		return S_OK;
	}

	HRESULT hr = GetD3DVertexBuffer9()->Lock(OffsetToLock, SizeToLock, ppbData, Flags);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DVertexBuffer9::Unlock(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::Unlock called\n");
	if (LogFile && LogDebug) fflush(LogFile);

	if (!GetD3DVertexBuffer9())
	{
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::Unlock called\n");
		if (LogFile) fprintf(LogFile, "  postponing call because vertex buffer was not created yet.\n");
		if (LogFile) fflush(LogFile);
		return S_OK;
	}

	CheckVertexBuffer9(this);
	HRESULT hr = GetD3DVertexBuffer9()->Unlock();
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	if (LogFile && LogDebug) fflush(LogFile);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DVertexBuffer9::GetDesc(THIS_ D3D9Base::D3DVERTEXBUFFER_DESC *pDesc)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DVertexBuffer9::GetDesc called\n");
	if (LogFile && LogDebug) fflush(LogFile);
	CheckVertexBuffer9(this);
	return GetD3DVertexBuffer9()->GetDesc(pDesc);
}
