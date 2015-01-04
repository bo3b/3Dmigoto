
D3D9Wrapper::IDirect3DTexture9::IDirect3DTexture9(D3D9Base::LPDIRECT3DTEXTURE9 pTexture)
    : D3D9Wrapper::IDirect3DUnknown((IUnknown*) pTexture),
	pendingCreateTexture(false),
	pendingLockUnlock(false),
	magic(0x7da43feb)
{
}

D3D9Wrapper::IDirect3DTexture9* D3D9Wrapper::IDirect3DTexture9::GetDirect3DVertexDeclaration9(D3D9Base::LPDIRECT3DTEXTURE9 pTexture)
{
    D3D9Wrapper::IDirect3DTexture9* p = (D3D9Wrapper::IDirect3DTexture9*) m_List.GetDataPtr(pTexture);
    if (p == NULL)
    {
        p = new D3D9Wrapper::IDirect3DTexture9(pTexture);
		if (pTexture) m_List.AddMember(pTexture, p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DTexture9::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DTexture9::Release(THIS)
{
	LogDebug("IDirect3DTexture9::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);
	
	--m_ulRef;

    if (ulRef == 0)
    {
		if (!LogDebug) LogInfo("IDirect3DTexture9::Release handle=%x, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		LogInfo("  deleting self\n");
		
        if (m_pUnk) m_List.DeleteMember(m_pUnk); 
		m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3DTexture9::GetDevice(THIS_ IDirect3DDevice9** ppDevice)
{
	LogDebug("IDirect3DTexture9::GetDevice called\n");
	
	CheckTexture9(this);
	D3D9Base::IDirect3DDevice9 *origDevice;
	HRESULT hr = GetD3DTexture9()->GetDevice(&origDevice);
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

STDMETHODIMP D3D9Wrapper::IDirect3DTexture9::SetPrivateData(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags)
{
	LogDebug("IDirect3DTexture9::SetPrivateData called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->SetPrivateData(refguid, pData, SizeOfData, Flags);
}

STDMETHODIMP D3D9Wrapper::IDirect3DTexture9::GetPrivateData(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData)
{
	LogDebug("IDirect3DTexture9::GetPrivateData called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->GetPrivateData(refguid, pData, pSizeOfData);
}

STDMETHODIMP D3D9Wrapper::IDirect3DTexture9::FreePrivateData(THIS_ REFGUID refguid)
{
	LogDebug("IDirect3DTexture9::FreePrivateData called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->FreePrivateData(refguid);
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DTexture9::SetPriority(THIS_ DWORD PriorityNew)
{
	LogDebug("IDirect3DTexture9::SetPriority called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->SetPriority(PriorityNew);
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DTexture9::GetPriority(THIS)
{
	LogDebug("IDirect3DTexture9::GetPriority called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->GetPriority();
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DTexture9::PreLoad(THIS)
{
	LogDebug("IDirect3DTexture9::PreLoad called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->PreLoad();
}

STDMETHODIMP_(D3D9Base::D3DRESOURCETYPE) D3D9Wrapper::IDirect3DTexture9::GetType(THIS)
{
	LogDebug("IDirect3DTexture9::GetType called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->GetType();
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DTexture9::SetLOD(THIS_ DWORD LODNew)
{
	LogDebug("IDirect3DTexture9::SetLOD called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->SetLOD(LODNew);
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DTexture9::GetLOD(THIS)
{
	LogDebug("IDirect3DTexture9::GetLOD called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->GetLOD();
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DTexture9::GetLevelCount(THIS)
{
	LogDebug("IDirect3DTexture9::GetLevelCount called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->GetLevelCount();
}

STDMETHODIMP D3D9Wrapper::IDirect3DTexture9::SetAutoGenFilterType(THIS_ D3D9Base::D3DTEXTUREFILTERTYPE FilterType)
{
	LogDebug("IDirect3DTexture9::SetAutoGenFilterType called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->SetAutoGenFilterType(FilterType);
}

STDMETHODIMP_(D3D9Base::D3DTEXTUREFILTERTYPE) D3D9Wrapper::IDirect3DTexture9::GetAutoGenFilterType(THIS)
{
	LogDebug("IDirect3DTexture9::GetAutoGenFilterType called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->GetAutoGenFilterType();
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DTexture9::GenerateMipSubLevels(THIS)
{
	LogDebug("IDirect3DTexture9::GenerateMipSubLevels called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->GenerateMipSubLevels();
}

STDMETHODIMP D3D9Wrapper::IDirect3DTexture9::GetLevelDesc(THIS_ UINT Level,D3D9Base::D3DSURFACE_DESC *pDesc)
{
	LogDebug("IDirect3DTexture9::GetLevelDesc called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->GetLevelDesc(Level, pDesc);
}

STDMETHODIMP D3D9Wrapper::IDirect3DTexture9::GetSurfaceLevel(THIS_ UINT Level,IDirect3DSurface9** ppSurfaceLevel)
{
	LogDebug("IDirect3DTexture9::GetSurfaceLevel called\n");
	

	if (!GetD3DTexture9())
	{
		LogInfo("  postponing call because texture was not created yet.\n");
		
		D3D9Wrapper::IDirect3DSurface9 *wrapper = D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9((D3D9Base::LPDIRECT3DSURFACE9) 0);
		wrapper->_Level = Level;
		wrapper->_Texture = this;
		wrapper->pendingGetSurfaceLevel = true;
		*ppSurfaceLevel = wrapper;
		LogInfo("  returns handle=%x\n", wrapper);
		
		return S_OK;
	}

	D3D9Base::IDirect3DSurface9 *baseSurfaceLevel = 0;
	HRESULT hr = GetD3DTexture9()->GetSurfaceLevel(Level, &baseSurfaceLevel);
	if (ppSurfaceLevel && baseSurfaceLevel) *ppSurfaceLevel = IDirect3DSurface9::GetDirect3DSurface9(baseSurfaceLevel);
	if (ppSurfaceLevel) LogInfo("  returns result=%x, handle=%x, wrapper=%x\n", hr, baseSurfaceLevel, *ppSurfaceLevel);
	
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DTexture9::LockRect(THIS_ UINT Level,D3D9Base::D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
{
	LogDebug("IDirect3DTexture9::LockRect called with Level=%d, Rect=l:%d,u:%d,r:%d,b:%d\n", Level,
		pRect ? pRect->left : 0, pRect ? pRect->top : 0, pRect ? pRect->right : 0, pRect ? pRect->bottom : 0);
	
	if (!GetD3DTexture9())
	{
		if (!LogDebug) LogInfo("IDirect3DTexture9::LockRect called\n");
		LogInfo("  postponing call because texture was not created yet.\n");
		
		if (!pendingLockUnlock)
		{
			_Flags = Flags;
			_Level = Level;
			_Buffer = new char[_Width*_Height*4];
			pendingLockUnlock = true;
		}
		if (pLockedRect)
		{
			pLockedRect->Pitch = _Width*4;
			pLockedRect->pBits = _Buffer;
		}
		return S_OK;
	}

	return GetD3DTexture9()->LockRect(Level, pLockedRect, pRect, Flags);
}

STDMETHODIMP D3D9Wrapper::IDirect3DTexture9::UnlockRect(THIS_ UINT Level)
{
	LogDebug("IDirect3DTexture9::UnlockRect called\n");

	if (!GetD3DTexture9())
	{
		if (!LogDebug) LogInfo("IDirect3DTexture9::UnlockRect called\n");
		LogInfo("  postponing call because texture was not created yet.\n");
		
		return S_OK;
	}

	return GetD3DTexture9()->UnlockRect(Level);
}

STDMETHODIMP D3D9Wrapper::IDirect3DTexture9::AddDirtyRect(THIS_ CONST RECT* pDirtyRect)
{
	LogDebug("IDirect3DTexture9::AddDirtyRect called\n");
	
	CheckTexture9(this);
	return GetD3DTexture9()->AddDirtyRect(pDirtyRect);
}
