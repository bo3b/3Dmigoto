
D3D9Wrapper::IDirect3DPixelShader9::IDirect3DPixelShader9(D3D9Base::LPDIRECT3DPIXELSHADER9 pVS)
    : D3D9Wrapper::IDirect3DUnknown((IUnknown*) pVS),
	magic(0x7da43feb)
{
}

D3D9Wrapper::IDirect3DPixelShader9* D3D9Wrapper::IDirect3DPixelShader9::GetDirect3DPixelShader9(D3D9Base::LPDIRECT3DPIXELSHADER9 pVS)
{
    D3D9Wrapper::IDirect3DPixelShader9* p = (D3D9Wrapper::IDirect3DPixelShader9*) m_List.GetDataPtr(pVS);
    if (!p)
    {
        p = new D3D9Wrapper::IDirect3DPixelShader9(pVS);
        if (pVS) m_List.AddMember(pVS, p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DPixelShader9::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DPixelShader9::Release(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DPixelShader9::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	if (LogFile && LogDebug) fprintf(LogFile, "  internal counter = %d\n", ulRef);
	
	--m_ulRef;

    if (ulRef == 0)
    {
		if (LogFile && !LogDebug) fprintf(LogFile, "IDirect3DPixelShader9::Release handle=%x, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		if (LogFile) fprintf(LogFile, "  deleting self\n");
		
        if (m_pUnk) m_List.DeleteMember(m_pUnk); 
		m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3DPixelShader9::GetDevice(THIS_ IDirect3DDevice9** ppDevice)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DPixelShader9::GetDevice called\n");
	
	D3D9Base::IDirect3DDevice9 *origDevice;
	HRESULT hr = GetD3DPixelShader9()->GetDevice(&origDevice);
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

STDMETHODIMP D3D9Wrapper::IDirect3DPixelShader9::GetFunction(THIS_ void *data,UINT* pSizeOfData)
{
	if (LogFile && LogDebug) fprintf(LogFile, "IDirect3DVertexShader9::GetFunction called\n");
	
	HRESULT hr = GetD3DPixelShader9()->GetFunction(data, pSizeOfData);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result=%x\n", hr);
	
	return hr;
}
