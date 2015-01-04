
D3D9Wrapper::IDirect3DIndexBuffer9::IDirect3DIndexBuffer9(D3D9Base::LPDIRECT3DINDEXBUFFER9 pIndexBuffer)
    : D3D9Wrapper::IDirect3DUnknown((IUnknown*) pIndexBuffer),
	pendingCreateIndexBuffer(false),
	pendingLockUnlock(false),
	magic(0x7da43feb)
{
}

D3D9Wrapper::IDirect3DIndexBuffer9* D3D9Wrapper::IDirect3DIndexBuffer9::GetDirect3DIndexBuffer9(D3D9Base::LPDIRECT3DINDEXBUFFER9 pIndexBuffer)
{
    D3D9Wrapper::IDirect3DIndexBuffer9* p = (D3D9Wrapper::IDirect3DIndexBuffer9*) m_List.GetDataPtr(pIndexBuffer);
    if (!p)
    {
        p = new D3D9Wrapper::IDirect3DIndexBuffer9(pIndexBuffer);
        if (pIndexBuffer) m_List.AddMember(pIndexBuffer, p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DIndexBuffer9::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DIndexBuffer9::Release(THIS)
{
	LogDebug("IDirect3DIndexBuffer9::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);
	
	--m_ulRef;

    if (ulRef == 0)
    {
		if (!LogDebug) LogInfo("IDirect3DIndexBuffer9::Release handle=%x, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		LogInfo("  deleting self\n");
		
        if (m_pUnk) m_List.DeleteMember(m_pUnk); 
		m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3DIndexBuffer9::GetDevice(THIS_ IDirect3DDevice9** ppDevice)
{
	LogDebug("IDirect3DIndexBuffer9::GetDevice called\n");
	
	CheckIndexBuffer9(this);
	D3D9Base::IDirect3DDevice9 *origDevice;
	HRESULT hr = GetD3DIndexBuffer9()->GetDevice(&origDevice);
	if (hr != S_OK)
	{
		LogInfo("  failed with hr = %x\n", hr);
		
		return hr;
	}
	D3D9Base::IDirect3DDevice9Ex *origDeviceEx;
	const IID IID_IDirect3DDevice9Ex = { 0xb18b10ce, 0x2649, 0x405a, { 0x87, 0xf, 0x95, 0xf7, 0x77, 0xd4, 0x31, 0x3a } };
	hr = origDevice->QueryInterface(IID_IDirect3DDevice9Ex, (void **) &origDeviceEx);
	origDevice->Release();
	if (hr != S_OK)
	{
		LogInfo("  failed IID_IDirect3DDevice9Ex cast with hr = %x\n", hr);
		
		return hr;
	}
	*ppDevice = D3D9Wrapper::IDirect3DDevice9::GetDirect3DDevice(origDeviceEx);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DIndexBuffer9::SetPrivateData(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags)
{
	LogDebug("IDirect3DIndexBuffer9::SetPrivateData called\n");
	
	CheckIndexBuffer9(this);
	return GetD3DIndexBuffer9()->SetPrivateData(refguid, pData, SizeOfData, Flags);
}

STDMETHODIMP D3D9Wrapper::IDirect3DIndexBuffer9::GetPrivateData(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData)
{
	LogDebug("IDirect3DIndexBuffer9::GetPrivateData called\n");
	
	CheckIndexBuffer9(this);
	return GetD3DIndexBuffer9()->GetPrivateData(refguid, pData, pSizeOfData);
}

STDMETHODIMP D3D9Wrapper::IDirect3DIndexBuffer9::FreePrivateData(THIS_ REFGUID refguid)
{
	LogDebug("IDirect3DIndexBuffer9::GetPrivateData called\n");
	
	CheckIndexBuffer9(this);
	return GetD3DIndexBuffer9()->FreePrivateData(refguid);
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DIndexBuffer9::SetPriority(THIS_ DWORD PriorityNew)
{
	LogDebug("IDirect3DIndexBuffer9::SetPriority called\n");
	
	CheckIndexBuffer9(this);
	return GetD3DIndexBuffer9()->SetPriority(PriorityNew);
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DIndexBuffer9::GetPriority(THIS)
{
	LogDebug("IDirect3DIndexBuffer9::GetPriority called\n");
	
	CheckIndexBuffer9(this);
	return GetD3DIndexBuffer9()->GetPriority();
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DIndexBuffer9::PreLoad(THIS)
{
	LogDebug("IDirect3DIndexBuffer9::GetPriority called\n");
	
	CheckIndexBuffer9(this);
	return GetD3DIndexBuffer9()->PreLoad();
}

STDMETHODIMP_(D3D9Base::D3DRESOURCETYPE) D3D9Wrapper::IDirect3DIndexBuffer9::GetType(THIS)
{
	LogDebug("IDirect3DIndexBuffer9::GetType called\n");
	
	CheckIndexBuffer9(this);
	D3D9Base::D3DRESOURCETYPE hr = GetD3DIndexBuffer9()->GetType();
	LogDebug("  returns ResourceType=%x\n", hr);
	
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DIndexBuffer9::Lock(THIS_ UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags)
{
	LogDebug("IDirect3DIndexBuffer9::Lock called with OffsetToLock=%d, SizeToLock=%d, Flags=%x\n", OffsetToLock, SizeToLock, Flags);
	
	if (!GetD3DIndexBuffer9())
	{
		if (!LogDebug) LogInfo("IDirect3DVertexBuffer9::Lock called\n");
		LogInfo("  postponing call because vertex buffer was not created yet.\n");
		
		if (!pendingLockUnlock)
		{
			_Flags = Flags;
			_Buffer = new char[_Length];
			pendingLockUnlock = true;
		}
		if (ppbData) *ppbData = _Buffer+OffsetToLock;
		return S_OK;
	}

	HRESULT hr = GetD3DIndexBuffer9()->Lock(OffsetToLock, SizeToLock, ppbData, Flags);
	LogDebug("  returns result=%x\n", hr);
	
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DIndexBuffer9::Unlock(THIS)
{
	LogDebug("IDirect3DIndexBuffer9::Unlock called\n");

	if (!GetD3DIndexBuffer9())
	{
		if (!LogDebug) LogInfo("IDirect3DIndexBuffer9::Unlock called\n");
		LogInfo("  postponing call because vertex buffer was not created yet.\n");
		
		return S_OK;
	}

	CheckIndexBuffer9(this);
	HRESULT hr = GetD3DIndexBuffer9()->Unlock();
	LogDebug("  returns result=%x\n", hr);
	
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DIndexBuffer9::GetDesc(THIS_ D3D9Base::D3DINDEXBUFFER_DESC *pDesc)
{
	LogDebug("IDirect3DIndexBuffer9::GetDesc called\n");
	
	CheckIndexBuffer9(this);
	return GetD3DIndexBuffer9()->GetDesc(pDesc);
}
