
D3D9Wrapper::IDirect3DQuery9::IDirect3DQuery9(D3D9Base::LPDIRECT3DQUERY9 pQuery)
    : D3D9Wrapper::IDirect3DUnknown((IUnknown*) pQuery),
	magic(0x7da43feb)
{
}

D3D9Wrapper::IDirect3DQuery9* D3D9Wrapper::IDirect3DQuery9::GetDirect3DQuery9(D3D9Base::LPDIRECT3DQUERY9 pIndexBuffer)
{
    D3D9Wrapper::IDirect3DQuery9* p = (D3D9Wrapper::IDirect3DQuery9*) m_List.GetDataPtr(pIndexBuffer);
    if (!p)
    {
        p = new D3D9Wrapper::IDirect3DQuery9(pIndexBuffer);
        if (pIndexBuffer) m_List.AddMember(pIndexBuffer, p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DQuery9::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DQuery9::Release(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DQuery9::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	if (LogFile && LogDebug) fprintf(LogFile, "  internal counter = %d\n", ulRef);
	
	--m_ulRef;

    if (ulRef == 0)
    {
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DQuery9::Release handle=%x, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		if (LogFile) fprintf(LogFile, "  deleting self\n");
		
        if (m_pUnk) m_List.DeleteMember(m_pUnk); 
		m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3DQuery9::GetDevice(THIS_ IDirect3DDevice9** ppDevice)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DQuery9::GetDevice called\n");
	
	D3D9Base::IDirect3DDevice9 *origDevice;
	HRESULT hr = GetD3DQuery9()->GetDevice(&origDevice);
	if (hr != S_OK)
	{
		if (LogFile) fprintf(LogFile, "  failed with hr = %x\n", hr);
		
		return hr;
	}
	D3D9Base::IDirect3DDevice9Ex *origDeviceEx;
	const IID IID_IDirect3DDevice9Ex = { 0xb18b10ce, 0x2649, 0x405a, { 0x87, 0xf, 0x95, 0xf7, 0x77, 0xd4, 0x31, 0x3a } };
	hr = origDevice->QueryInterface(IID_IDirect3DDevice9Ex, (void **) &origDeviceEx);
	origDevice->Release();
	if (hr != S_OK)
	{
		if (LogFile) fprintf(LogFile, "  failed IID_IDirect3DDevice9Ex cast with hr = %x\n", hr);
		
		return hr;
	}
	*ppDevice = D3D9Wrapper::IDirect3DDevice9::GetDirect3DDevice(origDeviceEx);
	return hr;
}

STDMETHODIMP_(D3D9Base::D3DQUERYTYPE) D3D9Wrapper::IDirect3DQuery9::GetType(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DQuery9::GetType called\n");
	
	D3D9Base::D3DQUERYTYPE hr = GetD3DQuery9()->GetType();
	if (LogFile && LogDebug) fprintf(LogFile, "  returns QueryType=%x\n", hr);
	
	return hr;
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DQuery9::GetDataSize(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DQuery9::GetDataSize called\n");
	
	DWORD hr = GetD3DQuery9()->GetDataSize();
	if (LogFile && LogDebug) fprintf(LogFile, "  returns size=%d\n", hr);
	
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DQuery9::Issue(THIS_ DWORD dwIssueFlags)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DQuery9::Issue called with IssueFlags=%x, this=%x\n", dwIssueFlags, this);
	
	HRESULT hr = GetD3DQuery9()->Issue(dwIssueFlags);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DQuery9::GetData(THIS_ void* pData,DWORD dwSize,DWORD dwGetDataFlags)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DQuery9::GetData called with Data=%x, Size=%d, GetDataFlags=%x\n", pData, dwSize, dwGetDataFlags);
	
	HRESULT hr = GetD3DQuery9()->GetData(pData, dwSize, dwGetDataFlags);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	
	return hr;
}
