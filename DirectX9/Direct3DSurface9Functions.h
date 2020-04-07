#include "HookedSurface.h"
#include "d3d9Wrapper.h"
inline ::IDirect3DSurface9 * D3D9Wrapper::IDirect3DSurface9::DirectModeGetMono()
{
	return DirectModeGetLeft();
}

inline ::IDirect3DSurface9 * D3D9Wrapper::IDirect3DSurface9::DirectModeGetLeft()
{
	return GetD3DSurface9();
}
inline ::IDirect3DSurface9 * D3D9Wrapper::IDirect3DSurface9::DirectModeGetRight()
{
	return m_pDirectSurfaceRight;
}
HRESULT D3D9Wrapper::IDirect3DSurface9::copyDepthSurfaceToTexture()
{
	NvAPI_Status nret = NvAPI_D3D9_StretchRectEx(hackerDevice->GetD3D9Device(), depthstencil_replacement_surface, NULL, depthstencil_replacement_texture, NULL, D3DTEXF_POINT);
	if (nret == NVAPI_OK)
		return S_OK;
	else
		return E_FAIL;
}
HRESULT D3D9Wrapper::IDirect3DSurface9::resolveDepthReplacement()
{
	HRESULT hr;
	if (depthstencil_replacement_direct_sample)
		return S_OK;

	if (hackerDevice->m_pD3D->m_isRESZ) {
		::IDirect3DVertexShader9 *initVertexShader;
		hackerDevice->GetD3D9Device()->GetVertexShader(&initVertexShader);
		::IDirect3DPixelShader9 *initPixelShader;
		hackerDevice->GetD3D9Device()->GetPixelShader(&initPixelShader);
		DWORD initFVF;
		hackerDevice->GetD3D9Device()->GetFVF(&initFVF);
		::IDirect3DBaseTexture9 *initTexture;
		hackerDevice->GetD3D9Device()->GetTexture(0, &initTexture);
		::IDirect3DSurface9 *initDSS;
		hackerDevice->GetD3D9Device()->GetDepthStencilSurface(&initDSS);
		::IDirect3DSurface9 *initRT;
		hackerDevice->GetD3D9Device()->GetDepthStencilSurface(&initRT);
		DWORD initZEnable;
		hackerDevice->GetD3D9Device()->GetRenderState(::D3DRS_ZENABLE, &initZEnable);
		DWORD initZWriteEnable;
		hackerDevice->GetD3D9Device()->GetRenderState(::D3DRS_ZWRITEENABLE, &initZWriteEnable);
		DWORD initColorWriteEnable;
		hackerDevice->GetD3D9Device()->GetRenderState(::D3DRS_COLORWRITEENABLE, &initColorWriteEnable);
		DWORD initPointSize;
		hackerDevice->GetD3D9Device()->GetRenderState(::D3DRS_POINTSIZE, &initPointSize);

		if (initDSS != depthstencil_replacement_surface)
			hackerDevice->GetD3D9Device()->SetDepthStencilSurface(depthstencil_replacement_surface);
		if (initRT != depthstencil_multisampled_rt_surface)
			hackerDevice->GetD3D9Device()->SetRenderTarget(0, depthstencil_multisampled_rt_surface);
		if (initVertexShader)
			hackerDevice->GetD3D9Device()->SetVertexShader(NULL);
		if (initPixelShader)
			hackerDevice->GetD3D9Device()->SetPixelShader(NULL);
		hackerDevice->GetD3D9Device()->SetFVF(D3DFVF_XYZ);
		// Bind depth stencil texture to texture sampler 0
		if (initTexture != depthstencil_replacement_texture)
			hackerDevice->GetD3D9Device()->SetTexture(0, depthstencil_replacement_texture);
		// Perform a dummy draw call to ensure texture sampler 0 is set before the // resolve is triggered
		// Vertex declaration and shaders may need to me adjusted to ensure no debug
		// error message is produced
		::D3DXVECTOR3 vDummyPoint(0.0f, 0.0f, 0.0f);
		hackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_ZENABLE, FALSE);
		hackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_ZWRITEENABLE, FALSE);
		hackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_COLORWRITEENABLE, 0);
		hackerDevice->GetD3D9Device()->DrawPrimitiveUP(::D3DPT_POINTLIST, 1, vDummyPoint, sizeof(::D3DXVECTOR3));
		hackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_ZWRITEENABLE, TRUE);
		hackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_ZENABLE, TRUE);
		hackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_COLORWRITEENABLE, 0x0F);

		// Trigger the depth buffer resolve; after this call texture sampler 0
		// will contain the contents of the resolve operation
		hr = hackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_POINTSIZE, RESZ_CODE);

		// This hack to fix resz hack, has been found by Maksym Bezus!!!
		// Without this line resz will be resolved only for first frame
		hackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_POINTSIZE, 0); // TROLOLO!!!

		//restore
		if (initVertexShader)
			hackerDevice->GetD3D9Device()->SetVertexShader(initVertexShader);
		if (initPixelShader)
			hackerDevice->GetD3D9Device()->SetPixelShader(initPixelShader);
		hackerDevice->GetD3D9Device()->SetFVF(initFVF);
		if (initTexture != depthstencil_replacement_texture)
			hackerDevice->GetD3D9Device()->SetTexture(0, initTexture);
		if (initDSS != depthstencil_replacement_surface)
			hackerDevice->GetD3D9Device()->SetDepthStencilSurface(initDSS);
		if (initRT != depthstencil_multisampled_rt_surface)
			hackerDevice->GetD3D9Device()->SetRenderTarget(0, initRT);
		if (initVertexShader)
			initVertexShader->Release();
		if (initPixelShader)
			initPixelShader->Release();
		if (initTexture)
			initTexture->Release();
		if (initRT)
			initRT->Release();
		if (initDSS)
			initDSS->Release();
		hackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_ZENABLE, initZEnable);
		hackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_ZWRITEENABLE, initZWriteEnable);
		hackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_COLORWRITEENABLE, initColorWriteEnable);
		hackerDevice->GetD3D9Device()->SetRenderState(::D3DRS_POINTSIZE, initPointSize);
	}
	else {
		hr = copyDepthSurfaceToTexture();
	}
	return hr;
}
inline void D3D9Wrapper::IDirect3DSurface9::Delete()
{
	LogInfo("IDirect3DSurface9::Delete handle=%p counter=%d this=%p\n", m_pUnk, m_ulRef, this);
	LogInfo("  deleting self\n");

	D3D9Wrapper::IDirect3DResource9::Delete();

	if (possibleDepthBuffer) {
		hackerDevice->depth_sources.erase(this);
	}

	if (depthstencil_replacement_texture) {
		if (depthstencil_replacement_nvapi_registered) {
			NvAPI_D3D9_UnregisterResource(depthstencil_replacement_surface);
			NvAPI_D3D9_UnregisterResource(depthstencil_replacement_texture);
		}
		if (depthstencil_replacement_multisampled && hackerDevice->m_pD3D->m_isRESZ && depthstencil_multisampled_rt_surface)
			depthstencil_multisampled_rt_surface->Release();
		depthstencil_replacement_texture->Release();
		--hackerDevice->migotoResourceCount;
	}

	if (m_pDirectLockableSysMemTexture)
		m_pDirectLockableSysMemTexture->Release();

	if (m_pDirectSurfaceRight)
		m_pDirectSurfaceRight->Release();

	if (m_pRealUnk) m_List.DeleteMember(m_pRealUnk);
	m_pUnk = 0;
	m_pRealUnk = 0;
	delete this;
}
inline void D3D9Wrapper::IDirect3DSurface9::Bound()
{
	switch (m_OwningContainerType)
	{
	case SurfaceContainerOwnerType::SwapChain:
		m_OwningSwapChain->Bound();
		break;
	case SurfaceContainerOwnerType::Texture:
		//m_OwningTexture->Bound(); // Unclear what slot to indicate it is bound to
		m_OwningTexture->AddRef(); // So just bump the refcount instead. -DSS
		break;
	case SurfaceContainerOwnerType::CubeTexture:
		// m_OwningCubeTexture->Bound(); // Unclear what slot to indicate it is bound to
		m_OwningCubeTexture->AddRef(); // So just bump the refcount instead. -DSS
		break;
	default:
		bound = true;
	}
}
inline void D3D9Wrapper::IDirect3DSurface9::Unbound()
{
	switch (m_OwningContainerType)
	{
	case SurfaceContainerOwnerType::SwapChain:
		if (m_OwningSwapChain != hackerDevice->deviceSwapChain)
			m_OwningSwapChain->Unbound();
		break;
	case SurfaceContainerOwnerType::Texture:
		// m_OwningTexture->Unbound();
		m_OwningTexture->Release();
		break;
	case SurfaceContainerOwnerType::CubeTexture:
		// m_OwningCubeTexture->Unbound();
		m_OwningCubeTexture->Release();
		break;
	default:
		bound = false;
		if (zero_d3d_ref_count)
			Delete();
	}
}
inline bool D3D9Wrapper::IDirect3DSurface9::IsDirectStereoSurface()
{
	return m_pDirectSurfaceRight != NULL;
}
inline void D3D9Wrapper::IDirect3DSurface9::HookSurface()
{
	m_pUnk = hook_surface(GetD3DSurface9(), (::IDirect3DSurface9*)this);
}
D3D9Wrapper::IDirect3DSurface9::IDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface)
    : D3D9Wrapper::IDirect3DResource9((::LPDIRECT3DRESOURCE9) pSurface, hackerDevice),
	pendingGetSurfaceLevel(false),
	magic(0x7da43feb),
	m_OwningSwapChain(NULL),
	m_OwningTexture(NULL),
	m_OwningCubeTexture(NULL),
	m_pDirectLockableSysMemTexture(NULL),
	m_pDirectSurfaceRight(pDirectModeRightSurface),
	DirectLockRectNoMemTex(false),
	DirectFullSurface(false),
	depthstencil_replacement_texture(NULL),
	depthstencil_replacement_surface(NULL),
	possibleDepthBuffer(false),
	depthstencil_replacement_multisampled(false),
	depthstencil_replacement_direct_sample(false),
	depthstencil_replacement_nvapi_registered(false),
	depthstencil_replacement_resolvedAA(false),
	bound(false),
	zero_d3d_ref_count(false)
{
	m_OwningContainerType = SurfaceContainerOwnerType::Device;
	if ((G->enable_hooks >= EnableHooksDX9::ALL) && pSurface) {
		this->HookSurface();
	}
}

D3D9Wrapper::IDirect3DSurface9::IDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface, D3D9Wrapper::IDirect3DSwapChain9 *owningContainer)
	: D3D9Wrapper::IDirect3DResource9((::LPDIRECT3DRESOURCE9) pSurface, hackerDevice),
	pendingGetSurfaceLevel(false),
	magic(0x7da43feb),
	m_OwningSwapChain(owningContainer),
	m_OwningTexture(NULL),
	m_OwningCubeTexture(NULL),
	m_pDirectLockableSysMemTexture(NULL),
	m_pDirectSurfaceRight(pDirectModeRightSurface),
	DirectLockRectNoMemTex(false),
	DirectFullSurface(false),
	depthstencil_replacement_texture(NULL),
	depthstencil_replacement_surface(NULL),
	possibleDepthBuffer(false),
	depthstencil_replacement_multisampled(false),
	depthstencil_replacement_direct_sample(false),
	depthstencil_replacement_nvapi_registered(false),
	depthstencil_replacement_resolvedAA(false),
	bound(false),
	zero_d3d_ref_count(false)
{
	m_OwningContainerType = SurfaceContainerOwnerType::SwapChain;
	if ((G->enable_hooks >= EnableHooksDX9::ALL) && pSurface) {
		this->HookSurface();
	}
}
D3D9Wrapper::IDirect3DSurface9::IDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface, D3D9Wrapper::IDirect3DTexture9 *owningContainer)
	: D3D9Wrapper::IDirect3DResource9((::LPDIRECT3DRESOURCE9) pSurface, hackerDevice),
	pendingGetSurfaceLevel(false),
	magic(0x7da43feb),
	m_OwningSwapChain(NULL),
	m_OwningTexture(owningContainer),
	m_OwningCubeTexture(NULL),
	m_pDirectLockableSysMemTexture(NULL),
	m_pDirectSurfaceRight(pDirectModeRightSurface),
	DirectLockRectNoMemTex(false),
	DirectFullSurface(false),
	depthstencil_replacement_texture(NULL),
	depthstencil_replacement_surface(NULL),
	possibleDepthBuffer(false),
	depthstencil_replacement_multisampled(false),
	depthstencil_replacement_direct_sample(false),
	depthstencil_replacement_nvapi_registered(false),
	depthstencil_replacement_resolvedAA(false),
	bound(false),
	zero_d3d_ref_count(false)
{
	m_OwningContainerType = SurfaceContainerOwnerType::Texture;
	if ((G->enable_hooks >= EnableHooksDX9::ALL) && pSurface) {
		this->HookSurface();
	}
}

D3D9Wrapper::IDirect3DSurface9::IDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface, D3D9Wrapper::IDirect3DCubeTexture9 *owningContainer)
	: D3D9Wrapper::IDirect3DResource9((::LPDIRECT3DRESOURCE9) pSurface, hackerDevice),
	pendingGetSurfaceLevel(false),
	magic(0x7da43feb),
	m_OwningSwapChain(NULL),
	m_OwningTexture(NULL),
	m_OwningCubeTexture(owningContainer),
	m_pDirectLockableSysMemTexture(NULL),
	m_pDirectSurfaceRight(pDirectModeRightSurface),
	DirectLockRectNoMemTex(false),
	DirectFullSurface(false),
	depthstencil_replacement_texture(NULL),
	depthstencil_replacement_surface(NULL),
	possibleDepthBuffer(false),
	depthstencil_replacement_multisampled(false),
	depthstencil_replacement_direct_sample(false),
	depthstencil_replacement_nvapi_registered(false),
	depthstencil_replacement_resolvedAA(false),
	bound(false),
	zero_d3d_ref_count(false)
{
	m_OwningContainerType = SurfaceContainerOwnerType::CubeTexture;
	if ((G->enable_hooks >= EnableHooksDX9::ALL) && pSurface) {
		this->HookSurface();
	}
}
D3D9Wrapper::IDirect3DSurface9* D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface)
{
	D3D9Wrapper::IDirect3DSurface9 *p = new D3D9Wrapper::IDirect3DSurface9(pSurface, hackerDevice, pDirectModeRightSurface);
    if (pSurface) m_List.AddMember(pSurface, p);
	return p;
}
D3D9Wrapper::IDirect3DSurface9* D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface, D3D9Wrapper::IDirect3DSwapChain9 *owningContainer)
{
	D3D9Wrapper::IDirect3DSurface9 *p = new D3D9Wrapper::IDirect3DSurface9(pSurface, hackerDevice, pDirectModeRightSurface, owningContainer);
	if (pSurface) m_List.AddMember(pSurface, p);
	return p;
}
D3D9Wrapper::IDirect3DSurface9* D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface, D3D9Wrapper::IDirect3DTexture9 *owningContainer)
{
	D3D9Wrapper::IDirect3DSurface9 *p = new D3D9Wrapper::IDirect3DSurface9(pSurface, hackerDevice, pDirectModeRightSurface, owningContainer);
	if (pSurface) m_List.AddMember(pSurface, p);
	return p;
}
D3D9Wrapper::IDirect3DSurface9* D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface, D3D9Wrapper::IDirect3DCubeTexture9 *owningContainer)
{
	D3D9Wrapper::IDirect3DSurface9 *p = new D3D9Wrapper::IDirect3DSurface9(pSurface, hackerDevice, pDirectModeRightSurface, owningContainer);
	if (pSurface) m_List.AddMember(pSurface, p);
	return p;
}
STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::QueryInterface(THIS_ REFIID riid, void ** ppvObj)
{
	LogDebug("D3D9Wrapper::IDirect3DSurface9::QueryInterface called\n");// at 'this': %s\n", type_name_dx9((IUnknown*)this));
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
				switch (m_OwningContainerType)
				{
				case SurfaceContainerOwnerType::SwapChain:
					++m_OwningSwapChain->shared_ref_count;
					break;
				case SurfaceContainerOwnerType::Texture:
					++m_OwningTexture->shared_ref_count;
					break;
				case SurfaceContainerOwnerType::CubeTexture:
					++m_OwningCubeTexture->shared_ref_count;
					break;
				}
				zero_d3d_ref_count = false;
				LogInfo("  interface replaced with IDirect3DSurface9 wrapper.\n");
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

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DSurface9::AddRef(THIS)
{
	++m_ulRef;
	switch (m_OwningContainerType)
	{
	case SurfaceContainerOwnerType::SwapChain:
		++m_OwningSwapChain->shared_ref_count;
		break;
	case SurfaceContainerOwnerType::Texture:
		++m_OwningTexture->shared_ref_count;
		break;
	case SurfaceContainerOwnerType::CubeTexture:
		++m_OwningCubeTexture->shared_ref_count;
		break;
	}
	zero_d3d_ref_count = false;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DSurface9::Release(THIS)
{
	LogDebug("IDirect3DSurface9::Release handle=%p, counter=%d, this=%p\n", m_pUnk, m_ulRef, this);
	ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);

	--m_ulRef;

	bool prev_non_zero = !zero_d3d_ref_count;
	if (ulRef == 0)
    {
		if (!gLogDebug) LogInfo("IDirect3DSurface9::Release handle=%p, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		zero_d3d_ref_count = true;
		if (m_OwningContainerType == SurfaceContainerOwnerType::Device && !bound)
			Delete();
    }

	if (prev_non_zero) {
		switch (m_OwningContainerType)
		{
		case SurfaceContainerOwnerType::SwapChain:
			if (m_OwningSwapChain != hackerDevice->deviceSwapChain) {
				--m_OwningSwapChain->shared_ref_count;
				if (m_OwningSwapChain->shared_ref_count == 0 && !m_OwningSwapChain->bound)
					m_OwningSwapChain->Delete();
			}
			break;
		case SurfaceContainerOwnerType::Texture:
			--m_OwningTexture->shared_ref_count;
			if (m_OwningTexture->shared_ref_count == 0 && m_OwningTexture->bound.empty())
				m_OwningTexture->Delete();
			break;
		case SurfaceContainerOwnerType::CubeTexture:
			--m_OwningCubeTexture->shared_ref_count;
			if (m_OwningCubeTexture->shared_ref_count == 0 && m_OwningCubeTexture->bound.empty())
				m_OwningCubeTexture->Delete();
			break;
		}
	}
    return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::GetDevice(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice)
{
	LogDebug("IDirect3DSurface9::GetDevice called\n");

	CheckSurface9(this);
	HRESULT hr;
	if (hackerDevice) {
		hackerDevice->AddRef();
		hr = S_OK;
	}
	else {
		return D3DERR_NOTFOUND;
	}
	if (!(G->enable_hooks & EnableHooksDX9::DEVICE)) {
		*ppDevice = hackerDevice;
	}
	else {
		*ppDevice = reinterpret_cast<D3D9Wrapper::IDirect3DDevice9*>(hackerDevice->GetRealOrig());
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::SetPrivateData(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags)
{
	LogDebug("IDirect3DSurface9::SetPrivateData called\n");
	CheckSurface9(this);
	HRESULT hr = GetD3DSurface9()->SetPrivateData(refguid, pData, SizeOfData, Flags);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::GetPrivateData(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData)
{
	LogDebug("IDirect3DSurface9::GetPrivateData called\n");
	CheckSurface9(this);
	return GetD3DSurface9()->GetPrivateData(refguid, pData, pSizeOfData);
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::FreePrivateData(THIS_ REFGUID refguid)
{
	LogDebug("IDirect3DSurface9::GetPrivateData called\n");
	CheckSurface9(this);
	HRESULT hr = GetD3DSurface9()->FreePrivateData(refguid);
	return hr;
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DSurface9::SetPriority(THIS_ DWORD PriorityNew)
{
	LogDebug("IDirect3DSurface9::SetPriority called\n");
	CheckSurface9(this);
	if (G->gForceStereo == 2) {
		if (IsDirectStereoSurface()) {
			m_pDirectSurfaceRight->SetPriority(PriorityNew);
			return GetD3DSurface9()->SetPriority(PriorityNew);
		}
		else {
			return GetD3DSurface9()->SetPriority(PriorityNew);
		}
	}
	else {
		return GetD3DSurface9()->SetPriority(PriorityNew);
	}
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DSurface9::GetPriority(THIS)
{
	LogDebug("IDirect3DSurface9::GetPriority called\n");
	CheckSurface9(this);
	return GetD3DSurface9()->GetPriority();
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DSurface9::PreLoad(THIS)
{
	//managed resources only?
	LogDebug("IDirect3DSurface9::PreLoad called\n");
	CheckSurface9(this);
	if (G->gForceStereo == 2) {
		if (IsDirectStereoSurface()) {
			m_pDirectSurfaceRight->PreLoad();
			return GetD3DSurface9()->PreLoad();
		}
		else {
			return GetD3DSurface9()->PreLoad();
		}
	}
	else {
		return GetD3DSurface9()->PreLoad();
	}

}

STDMETHODIMP_(::D3DRESOURCETYPE) D3D9Wrapper::IDirect3DSurface9::GetType(THIS)
{
	LogDebug("IDirect3DSurface9::GetType called\n");
	CheckSurface9(this);
	return GetD3DSurface9()->GetType();
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::GetContainer(THIS_ REFIID riid,void** ppContainer)
{
	LogDebug("IDirect3DSurface9::GetContainer called\n");

	CheckSurface9(this);
	HRESULT hr;
	void *pContainer = NULL;
	HRESULT queryResult;
	switch (m_OwningContainerType) {
	case SwapChain:
		queryResult = m_OwningSwapChain->GetSwapChain9()->QueryInterface(riid, &pContainer);
		if (queryResult == S_OK) {
			++m_OwningSwapChain->m_ulRef;
			++m_OwningSwapChain->shared_ref_count;
			m_OwningSwapChain->zero_d3d_ref_count = false;
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppContainer = m_OwningSwapChain;
			}
			else {
				*ppContainer = m_OwningSwapChain->GetRealOrig();
			}
		}
		break;
	case Texture:
		queryResult = m_OwningTexture->GetD3DTexture9()->QueryInterface(riid, &pContainer);
		if (queryResult == S_OK) {
			++m_OwningTexture->m_ulRef;
			++m_OwningTexture->shared_ref_count;
			m_OwningTexture->zero_d3d_ref_count = false;
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppContainer = m_OwningTexture;
			}
			else {
				*ppContainer = m_OwningTexture->GetRealOrig();
			}
		}
		break;
	case CubeTexture:
		queryResult = m_OwningCubeTexture->GetD3DCubeTexture9()->QueryInterface(riid, &pContainer);
		if (queryResult == S_OK) {
			++m_OwningCubeTexture->m_ulRef;
			++m_OwningCubeTexture->shared_ref_count;
			m_OwningCubeTexture->zero_d3d_ref_count = false;
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppContainer = m_OwningCubeTexture;
			}
			else {
				*ppContainer = m_OwningCubeTexture->GetRealOrig();
			}
		}
		break;
	default:
		queryResult = hackerDevice->GetD3D9Device()->QueryInterface(riid, &pContainer);
		if (queryResult == S_OK) {
			++hackerDevice->m_ulRef;
			if (!(G->enable_hooks & EnableHooksDX9::DEVICE)) {
				*ppContainer = hackerDevice;
			}
			else {
				*ppContainer = hackerDevice->GetRealOrig();
			}
		}
		break;
	}

	if (queryResult == S_OK)
	{
		hr = D3D_OK;
	}
	else if (queryResult == E_NOINTERFACE) {
		hr = E_NOINTERFACE;
	}
	else
	{
		hr = D3DERR_INVALIDCALL;
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::GetDesc(THIS_ ::D3DSURFACE_DESC *pDesc)
{
	LogDebug("IDirect3DSurface9::GetDesc called\n");

	CheckSurface9(this);

	return GetD3DSurface9()->GetDesc(pDesc);
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::LockRect(THIS_ ::D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
{
	LogDebug("IDirect3DSurface9::LockRect called\n");

	CheckSurface9(this);

	::IDirect3DSurface9 *pBaseSurface = GetD3DSurface9();
	hackerDevice->FrameAnalysisLog("LockRect(pResource:0x%p, pRect:0x%p, LockFlags:%u)",
		pBaseSurface, pRect, Flags);
	hackerDevice->FrameAnalysisLogResourceHash(pBaseSurface);

	HRESULT hr = NULL;
	if (G->gForceStereo == 2){
		if (IsDirectStereoSurface()) {
			if (!(Flags & D3DLOCK_READONLY)) {
				::D3DSURFACE_DESC desc;
				GetD3DSurface9()->GetDesc(&desc);
				//Guard against multithreaded access as this could be causing us problems
				std::lock_guard<std::mutex> lck(m_mtx);
				if (desc.Pool == ::D3DPOOL_DEFAULT && !DirectLockRectNoMemTex) {
					//Create lockable system memory surfaces
					::IDirect3DSurface9 *pSurface = NULL;
					bool createTexture = false;
					if (!m_pDirectLockableSysMemTexture)
					{
						if (hackerDevice) {
							hr = hackerDevice->GetD3D9Device()->CreateTexture(desc.Width, desc.Height, 1, 0,
								desc.Format, ::D3DPOOL_SYSTEMMEM, &m_pDirectLockableSysMemTexture, NULL);
							if (FAILED(hr)) {
								LogDebug("IDirect3DSurface9::LockRect Direct Mode, failed to create system memory texture, hr = 0x%0.8x\n", hr);
								DirectLockRectNoMemTex = true;
								//goto postLock;
							}
							createTexture = true;
						}
					}
					if (!DirectLockRectNoMemTex) {
						m_pDirectLockableSysMemTexture->GetSurfaceLevel(0, &pSurface);
						//Only copy the render taget (if possible) on the creation of the memory texture
						if (createTexture)
						{
							hr = hackerDevice->GetD3D9Device()->GetRenderTargetData(pBaseSurface, pSurface);
							if (FAILED(hr))
							{
								LogDebug("IDirect3DSurface9::LockRect Direct Mode, failed to update system memory texture surface, hr = 0x%0.8x\n", hr);
								goto postLock;
							}
						}
						//And finally, lock the memory surface
						hr = pSurface->LockRect(pLockedRect, pRect, Flags);
						if (FAILED(hr))
						{
							LogDebug("IDirect3DSurface9::LockRect Direct Mode, failed to LockRect hr = 0x%0.8x\n", hr);
							goto postLock;
						}
						pSurface->Release();
					}
				}

				if (desc.Pool != ::D3DPOOL_DEFAULT || DirectLockRectNoMemTex)
				{
					//lock the memory surface
					hr = pBaseSurface->LockRect(pLockedRect, pRect, Flags);
					if (FAILED(hr))
					{
						LogDebug("IDirect3DSurface9::LockRect Direct Mode, failed to LockRect hr = 0x%0.8x\n", hr);
						goto postLock;
					}

					if (!pRect)
					{
						DirectFullSurface = true;
						DirectLockedRects.clear();
						DirectLockedFlags = Flags;
						DirectLockedRect = *pLockedRect;
					}
					else if (!DirectFullSurface)
					{
						D3DLockedRect lockedRect;
						lockedRect.lockedRect = *pLockedRect;
						lockedRect.rect = *pRect;
						lockedRect.flags = Flags;
						DirectLockedRects.push_back(lockedRect);
					}
				}
				else {
					if (!pRect)
					{
						DirectFullSurface = true;
						DirectLockedRects.clear();
						DirectLockedFlags = Flags;
						DirectLockedRect = *pLockedRect;
					}
					else if (!DirectFullSurface)
					{
						D3DLockedRect lockedRect;
						lockedRect.rect = *pRect;
						lockedRect.flags = Flags;
						DirectLockedRects.push_back(lockedRect);
					}
				}
			}
			else {
				hr = pBaseSurface->LockRect(pLockedRect, pRect, Flags);
			}
		}
		else {
			hr = pBaseSurface->LockRect(pLockedRect, pRect, Flags);
		}
	}
	else {
		hr = pBaseSurface->LockRect(pLockedRect, pRect, Flags);
	}
postLock:
	hackerDevice->TrackAndDivertLock<D3D9Wrapper::IDirect3DSurface9>(hr, this, pLockedRect, Flags);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::UnlockRect(THIS)
{
	LogDebug("IDirect3DSurface9::UnlockRect called\n");
	CheckSurface9(this);
	::IDirect3DSurface9 *pBaseSurface = GetD3DSurface9();
	hackerDevice->FrameAnalysisLog("Unlock(pResource:0x%p)",
		pBaseSurface);
	hackerDevice->FrameAnalysisLogResourceHash(pBaseSurface);
	hackerDevice->TrackAndDivertUnlock(this);
	HRESULT hr;
	if (G->gForceStereo == 2 && IsDirectStereoSurface()) {
		if (DirectFullSurface || DirectLockedRects.size() > 0) {
			::D3DSURFACE_DESC desc;
			GetD3DSurface9()->GetDesc(&desc);
			//Guard against multithreaded access as this could be causing us problems
			std::lock_guard<std::mutex> lck(m_mtx);
			//This would mean nothing to do
			if (DirectLockedRects.size() == 0 && !DirectFullSurface) {
				hr = S_OK;
				goto postUnlock;
			}
			if (desc.Pool == ::D3DPOOL_DEFAULT && !DirectLockRectNoMemTex) {
				::IDirect3DSurface9 *pSurface = NULL;
				HRESULT hr = m_pDirectLockableSysMemTexture ? m_pDirectLockableSysMemTexture->GetSurfaceLevel(0, &pSurface) : D3DERR_INVALIDCALL;
				if (FAILED(hr))
					goto postUnlock;
				hr = pSurface->UnlockRect();
				if (DirectFullSurface)
				{
					hr = hackerDevice->GetD3D9Device()->UpdateSurface(pSurface, NULL, m_pDirectSurfaceRight, NULL);
					if (FAILED(hr))
					{
						LogDebug("IDirect3DSurface9::UnlockRect Direct Mode, failed to update full right surface, hr = 0x%0.8x\n", hr);
						//Just ignore the failed copy back, not much we can do
						hr = S_OK;
					}

					hr = hackerDevice->GetD3D9Device()->UpdateSurface(pSurface, NULL, pBaseSurface, NULL);
					if (FAILED(hr))
					{
						LogDebug("IDirect3DSurface9::UnlockRect Direct Mode, failed to update full left surface, hr = 0x%0.8x\n", hr);
						//Just ignore the failed copy back, not much we can do
						hr = S_OK;
					}
				}
				else
				{
					std::vector<D3DLockedRect>::iterator rectIter = DirectLockedRects.begin();
					while (rectIter != DirectLockedRects.end())
					{
						POINT p;
						p.x = (*rectIter).rect.left;
						p.y = (*rectIter).rect.top;
						hr = hackerDevice->GetD3D9Device()->UpdateSurface(pSurface, &(*rectIter).rect, m_pDirectSurfaceRight, &p);
						if (FAILED(hr))
						{
							LogDebug("IDirect3DSurface9::UnlockRect Direct Mode, failed to update rect right surface, hr = 0x%0.8x\n", hr);
							//Just ignore the failed copy back, not much we can do
							hr = S_OK;
						}
						hr = hackerDevice->GetD3D9Device()->UpdateSurface(pSurface, &(*rectIter).rect, pBaseSurface, &p);
						if (FAILED(hr))
						{
							LogDebug("IDirect3DSurface9::UnlockRect Direct Mode, failed to update rect left surface, hr = 0x%0.8x\n", hr);
							//Just ignore the failed copy back, not much we can do
							hr = S_OK;
						}
						rectIter++;
					}
				}
				pSurface->Release();
			}
			else {
				::D3DLOCKED_RECT lRect;
				if (DirectFullSurface) {
					hr = m_pDirectSurfaceRight->LockRect(&lRect, NULL, DirectLockedFlags);
					if (FAILED(hr))
					{
						LogDebug("IDirect3DSurface9::UnlockRect Direct Mode, failed to lock right surface, hr = 0x%0.8x\n", hr);
						hr = S_OK;
					}
					else {
						unsigned char* sptr = static_cast<unsigned char*>(DirectLockedRect.pBits);
						unsigned char* dptr = static_cast<unsigned char*>(lRect.pBits);
						memcpy(dptr, sptr, (DirectLockedRect.Pitch * desc.Height));
						m_pDirectSurfaceRight->UnlockRect();
					}
				}
				else {
					bool lockedRight = false;
					std::vector<D3DLockedRect>::iterator rectIter = DirectLockedRects.begin();
					while (rectIter != DirectLockedRects.end())
					{
						hr = m_pDirectSurfaceRight->LockRect(&lRect, &(*rectIter).rect, (*rectIter).flags);
						if (FAILED(hr))
						{
							LogDebug("IDirect3DSurface9::UnlockRect Direct Mode, failed to lock rect on right surface, hr = 0x%0.8x\n", hr);
							hr = S_OK;
						}
						else {
							lockedRight = true;
							unsigned char* sptr = static_cast<unsigned char*>((*rectIter).lockedRect.pBits);
							unsigned char* dptr = static_cast<unsigned char*>(lRect.pBits);
							memcpy(dptr, sptr, ((*rectIter).lockedRect.Pitch * desc.Height));
						}
						rectIter++;
					}
					if (lockedRight)
						hr = m_pDirectSurfaceRight->UnlockRect();
				}
				hr = pBaseSurface->UnlockRect();
			}
			DirectLockedRects.clear();
			DirectFullSurface = false;
		}
		else {
			hr = pBaseSurface->UnlockRect();
		}
	}
	else {
		hr = pBaseSurface->UnlockRect();
	}
postUnlock:
	if (G->analyse_frame)
		hackerDevice->FrameAnalysisAfterUnlock(pBaseSurface);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::GetDC(THIS_ HDC *phdc)
{
	LogDebug("IDirect3DSurface9::GetDC called\n");
	CheckSurface9(this);
	return GetD3DSurface9()->GetDC(phdc);
}

STDMETHODIMP D3D9Wrapper::IDirect3DSurface9::ReleaseDC(THIS_ HDC hdc)
{
	LogDebug("IDirect3DSurface9::ReleaseDC called\n");
	CheckSurface9(this);
	HRESULT hr = GetD3DSurface9()->ReleaseDC(hdc);
	return hr;
}
