#include "HookedCubeTexture.h"
#include "d3d9Wrapper.h"

inline ::IDirect3DCubeTexture9 * D3D9Wrapper::IDirect3DCubeTexture9::DirectModeGetMono()
{
	return DirectModeGetLeft();
}
inline ::IDirect3DCubeTexture9 * D3D9Wrapper::IDirect3DCubeTexture9::DirectModeGetLeft()
{
	return GetD3DCubeTexture9();
}
inline ::IDirect3DCubeTexture9 * D3D9Wrapper::IDirect3DCubeTexture9::DirectModeGetRight()
{
	return m_pDirectCubeTextureRight;
}
inline void D3D9Wrapper::IDirect3DCubeTexture9::Delete()
{
	LogInfo("IDirect3DCubeTexture9::Delete\n");
	LogInfo("  deleting self\n");

	D3D9Wrapper::IDirect3DResource9::Delete();
	for (auto it = m_wrappedSurfaceLevels.cbegin(); it != m_wrappedSurfaceLevels.cend();)
	{
		(*it).second->Delete();
		++it;
	}

	auto it2 = m_pDirectLockableSysMemTextures.begin();
	while (it2 != m_pDirectLockableSysMemTextures.end())
	{
		if (it2->second) it2->second->Release();
		++it2;
	}
	if (m_pDirectCubeTextureRight)
		m_pDirectCubeTextureRight->Release();

	if (m_pRealUnk) m_List.DeleteMember(m_pRealUnk);
	m_pUnk = 0;
	m_pRealUnk = 0;
	delete this;
}
inline bool D3D9Wrapper::IDirect3DCubeTexture9::IsDirectStereoCubeTexture()
{
	return m_pDirectCubeTextureRight != NULL;
}
inline void D3D9Wrapper::IDirect3DCubeTexture9::HookCubeTexture()
{
	m_pUnk = hook_cube_texture(GetD3DCubeTexture9(), (::IDirect3DCubeTexture9*)this);
}

D3D9Wrapper::IDirect3DCubeTexture9::IDirect3DCubeTexture9(::LPDIRECT3DCUBETEXTURE9 pTexture, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DCUBETEXTURE9 pDirectModeRightCubeTexture)
	: D3D9Wrapper::IDirect3DBaseTexture9(pTexture, hackerDevice, TextureType::Cube),
	m_pDirectCubeTextureRight(pDirectModeRightCubeTexture),
	m_pDirectLockableSysMemTextures(NULL),
	DirectLockRectNoMemTex(false)
{
	if ((G->enable_hooks >= EnableHooksDX9::ALL) && pTexture) {
		this->HookCubeTexture();
	}
}

D3D9Wrapper::IDirect3DCubeTexture9* D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(::LPDIRECT3DCUBETEXTURE9 pTexture, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DCUBETEXTURE9 pDirectModeRightCubeTexture)
{
	D3D9Wrapper::IDirect3DCubeTexture9* p = new D3D9Wrapper::IDirect3DCubeTexture9(pTexture, hackerDevice, pDirectModeRightCubeTexture);
	if (pTexture) m_List.AddMember(pTexture, p);
	return p;
}

STDMETHODIMP D3D9Wrapper::IDirect3DCubeTexture9::QueryInterface(THIS_ REFIID riid, void ** ppvObj)
{
	LogDebug("D3D9Wrapper::IDirect3DCubeTexture9::QueryInterface called\n");// at 'this': %s\n", type_name_dx9((IUnknown*)this));
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
				++shared_ref_count;
				zero_d3d_ref_count = false;
				LogInfo("  interface replaced with IDirect3DCubeTexture9 wrapper.\n");
				LogInfo("  result = %x, handle = %p\n", hr, *ppvObj);
				return hr;
			}
		}
		/* IID_IDirect3DSurface9 */
		/* {0CFBAF3A-9FF6-429a-99B3-A2796AF8B89B} */
		IF_GUID(riid, 0x0cfbaf3a, 0x9ff6, 0x429a, 0x99, 0xb3, 0xa2, 0x79) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				auto it = m_wrappedSurfaceLevels.find(0);
				if (it != m_wrappedSurfaceLevels.end()) {
					D3D9Wrapper::IDirect3DSurface9 *wrapper = it->second;
					*ppvObj = wrapper;
					++wrapper->m_ulRef;
					++shared_ref_count;
					wrapper->zero_d3d_ref_count = false;
					LogInfo("  interface replaced with IDirect3DSurface9 wrapper.\n");
					LogInfo("  result = %x, handle = %p\n", hr, *ppvObj);
					return hr;
				}
				else {
					D3D9Wrapper::IDirect3DSurface9 *wrapper = IDirect3DSurface9::GetDirect3DSurface9((::IDirect3DSurface9*)(*ppvObj), hackerDevice, NULL, this);
					m_wrappedSurfaceLevels.emplace(0, wrapper);
					++wrapper->m_ulRef;
					++shared_ref_count;
					wrapper->zero_d3d_ref_count = false;
					LogInfo("  interface replaced with IDirect3DSurface9 wrapper.\n");
					LogInfo("  result = %x, handle = %p\n", hr, *ppvObj);
					return hr;
				}
			}
		}
		D3D9Wrapper::IDirect3DUnknown *unk = QueryInterface_Find_Wrapper(*ppvObj);
		if (unk)
			*ppvObj = unk;
	}
	LogInfo("  result = %x, handle = %p\n", hr, *ppvObj);
	return hr;
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DCubeTexture9::AddRef(THIS)
{
	++m_ulRef;
	++shared_ref_count;
	zero_d3d_ref_count = false;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DCubeTexture9::Release(THIS)
{
	LogDebug("IDirect3DCubeTexture9::Release handle=%p, counter=%d, this=%p\n", m_pUnk, m_ulRef, this);

	ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);

	--m_ulRef;
	bool prev_non_zero = !zero_d3d_ref_count;
	if (ulRef == 0)
	{
		if (!gLogDebug) LogInfo("IDirect3DCubeTexture9::Release handle=%p, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		zero_d3d_ref_count = true;
	}

	if (prev_non_zero) {
		--shared_ref_count;
		if (shared_ref_count == 0) {
			if (bound.empty())
				Delete();
		}
	}

	return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3DCubeTexture9::GetDevice(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice)
{
	LogDebug("IDirect3DCubeTexture9::GetDevice called\n");

	CheckCubeTexture9(this);
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

STDMETHODIMP D3D9Wrapper::IDirect3DCubeTexture9::SetPrivateData(THIS_ REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags)
{
	LogDebug("IDirect3DCubeTexture9::SetPrivateData called\n");
	CheckCubeTexture9(this);
	HRESULT hr = GetD3DCubeTexture9()->SetPrivateData(refguid, pData, SizeOfData, Flags);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DCubeTexture9::GetPrivateData(THIS_ REFGUID refguid, void* pData, DWORD* pSizeOfData)
{
	LogDebug("IDirect3DCubeTexture9::GetPrivateData called\n");

	CheckCubeTexture9(this);
	return GetD3DCubeTexture9()->GetPrivateData(refguid, pData, pSizeOfData);
}

STDMETHODIMP D3D9Wrapper::IDirect3DCubeTexture9::FreePrivateData(THIS_ REFGUID refguid)
{
	LogDebug("IDirect3DCubeTexture9::FreePrivateData called\n");

	CheckCubeTexture9(this);
	HRESULT hr = GetD3DCubeTexture9()->FreePrivateData(refguid);
	return hr;

}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DCubeTexture9::SetPriority(THIS_ DWORD PriorityNew)
{
	LogDebug("IDirect3DCubeTexture9::SetPriority called\n");

	CheckCubeTexture9(this);
	if (G->gForceStereo == 2) {
		if (IsDirectStereoCubeTexture()) {
			m_pDirectCubeTextureRight->SetPriority(PriorityNew);
			return GetD3DCubeTexture9()->SetPriority(PriorityNew);
		}
		else {
			return GetD3DCubeTexture9()->SetPriority(PriorityNew);
		}
	}
	else {
		return GetD3DCubeTexture9()->SetPriority(PriorityNew);
	}
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DCubeTexture9::GetPriority(THIS)
{
	LogDebug("IDirect3DCubeTexture9::GetPriority called\n");

	CheckCubeTexture9(this);
	return GetD3DCubeTexture9()->GetPriority();
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DCubeTexture9::PreLoad(THIS)
{
	//managed resources only?
	LogDebug("IDirect3DCubeTexture9::PreLoad called\n");

	CheckCubeTexture9(this);
	if (G->gForceStereo == 2) {
		if (IsDirectStereoCubeTexture()) {
			m_pDirectCubeTextureRight->PreLoad();
			GetD3DCubeTexture9()->PreLoad();
		}
		else {
			GetD3DCubeTexture9()->PreLoad();
		}
	}
	else {
		GetD3DCubeTexture9()->PreLoad();
	}
	return;

}

STDMETHODIMP_(::D3DRESOURCETYPE) D3D9Wrapper::IDirect3DCubeTexture9::GetType(THIS)
{
	LogDebug("IDirect3DCubeTexture9::GetType called\n");

	CheckCubeTexture9(this);
	return GetD3DCubeTexture9()->GetType();
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DCubeTexture9::SetLOD(THIS_ DWORD LODNew)
{
	//managed resources only?
	LogDebug("IDirect3DCubeTexture9::SetLOD called\n");

	CheckCubeTexture9(this);
	hackerDevice->FrameAnalysisLog("SetLOD(cubetexture:0x%p)",
		this->GetD3DCubeTexture9());
	hackerDevice->FrameAnalysisLogResourceHash(this->GetD3DCubeTexture9());
	if (G->gForceStereo == 2) {
		if (IsDirectStereoCubeTexture()) {
			m_pDirectCubeTextureRight->SetLOD(LODNew);
			return GetD3DCubeTexture9()->SetLOD(LODNew);
		}
		else {
			return GetD3DCubeTexture9()->SetLOD(LODNew);
		}
	}
	else {
		return GetD3DCubeTexture9()->SetLOD(LODNew);
	}
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DCubeTexture9::GetLOD(THIS)
{
	LogDebug("IDirect3DCubeTexture9::GetLOD called\n");

	CheckCubeTexture9(this);
	DWORD ret = GetD3DCubeTexture9()->GetLOD();
	hackerDevice->FrameAnalysisLog("GetLOD(cubetexture:0x%p) = %x",
		this->GetD3DCubeTexture9(), ret);
	hackerDevice->FrameAnalysisLogResourceHash(this->GetD3DCubeTexture9());
	return ret;
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DCubeTexture9::GetLevelCount(THIS)
{
	LogDebug("IDirect3DCubeTexture9::GetLevelCount called\n");

	CheckCubeTexture9(this);
	return GetD3DCubeTexture9()->GetLevelCount();
}

STDMETHODIMP D3D9Wrapper::IDirect3DCubeTexture9::SetAutoGenFilterType(THIS_ ::D3DTEXTUREFILTERTYPE FilterType)
{
	LogDebug("IDirect3DCubeTexture9::SetAutoGenFilterType called\n");

	CheckCubeTexture9(this);
	HRESULT hr;
	if (G->gForceStereo == 2) {
		if (IsDirectStereoCubeTexture()) {
			m_pDirectCubeTextureRight->SetAutoGenFilterType(FilterType);
			hr = GetD3DCubeTexture9()->SetAutoGenFilterType(FilterType);
		}
		else {
			hr = GetD3DCubeTexture9()->SetAutoGenFilterType(FilterType);
		}
	}
	else {
		hr = GetD3DCubeTexture9()->SetAutoGenFilterType(FilterType);
	}
	return hr;
}

STDMETHODIMP_(::D3DTEXTUREFILTERTYPE) D3D9Wrapper::IDirect3DCubeTexture9::GetAutoGenFilterType(THIS)
{
	LogDebug("IDirect3DCubeTexture9::GetAutoGenFilterType called\n");

	CheckCubeTexture9(this);
	return GetD3DCubeTexture9()->GetAutoGenFilterType();
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DCubeTexture9::GenerateMipSubLevels(THIS)
{
	LogDebug("IDirect3DCubeTexture9::GenerateMipSubLevels called\n");

	CheckCubeTexture9(this);
	hackerDevice->FrameAnalysisLog("GenerateMipSubLevels(cubetexture:0x%p\n)",
		this->GetD3DCubeTexture9());
	hackerDevice->FrameAnalysisLogResource(-1, NULL, this->GetD3DCubeTexture9());
	if (G->gForceStereo == 2) {
		if (IsDirectStereoCubeTexture()) {
			m_pDirectCubeTextureRight->GenerateMipSubLevels();
			return GetD3DCubeTexture9()->GenerateMipSubLevels();
		}
		else {
			return GetD3DCubeTexture9()->GenerateMipSubLevels();
		}
	}
	else {
		return GetD3DCubeTexture9()->GenerateMipSubLevels();
	}


}

STDMETHODIMP D3D9Wrapper::IDirect3DCubeTexture9::GetLevelDesc(THIS_ UINT Level, ::D3DSURFACE_DESC *pDesc)
{
	LogDebug("IDirect3DCubeTexture9::GetLevelDesc called\n");

	CheckCubeTexture9(this);
	return GetD3DCubeTexture9()->GetLevelDesc(Level, pDesc);
}

STDMETHODIMP D3D9Wrapper::IDirect3DCubeTexture9::GetCubeMapSurface(::D3DCUBEMAP_FACES FaceType, UINT Level, D3D9Wrapper::IDirect3DSurface9 ** ppCubeMapSurface)
{

	LogDebug("IDirect3DCubeTexture9::GetCubeMapSurface called\n");


	if (!GetD3DCubeTexture9())
	{
		LogInfo("  postponing call because cube texture was not created yet.\n");

		D3D9Wrapper::IDirect3DSurface9 *wrapper = D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9((::LPDIRECT3DSURFACE9) 0, hackerDevice, NULL, this);
		wrapper->_Level = Level;
		wrapper->_FaceType = FaceType;
		wrapper->_CubeTexture = this;
		wrapper->pendingGetCubeMapSurface = true;
		*ppCubeMapSurface = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}
	::IDirect3DSurface9 *baseSurfaceLevel = 0;
	HRESULT hr = NULL;
	UINT faceLevel = (FaceType * 6) + Level;
	auto it = m_wrappedSurfaceLevels.find(faceLevel);
	if (it != m_wrappedSurfaceLevels.end()){
		D3D9Wrapper::IDirect3DSurface9 *wrapper = it->second;
		if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
			*ppCubeMapSurface = wrapper;
		}
		else {
			*ppCubeMapSurface = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(wrapper->GetRealOrig());
		}
		baseSurfaceLevel = wrapper->GetD3DSurface9();
		wrapper->AddRef();
		hr = S_OK;
	}
	else {
		hr = GetD3DCubeTexture9()->GetCubeMapSurface(FaceType, Level, &baseSurfaceLevel);
		if (FAILED(hr)) {
			*ppCubeMapSurface = NULL;
		}
		else {
			D3D9Wrapper::IDirect3DSurface9 * pWrappedSurfaceLevel = NULL;
			if (G->gForceStereo == 2 && IsDirectStereoCubeTexture()) {
				::IDirect3DSurface9* pDirectSurfaceLevelRight = NULL;
				hr = m_pDirectCubeTextureRight->GetCubeMapSurface(FaceType, Level, &pDirectSurfaceLevelRight);
				pWrappedSurfaceLevel = IDirect3DSurface9::GetDirect3DSurface9(baseSurfaceLevel, hackerDevice, pDirectSurfaceLevelRight, this);
			}
			else {
				pWrappedSurfaceLevel = IDirect3DSurface9::GetDirect3DSurface9(baseSurfaceLevel, hackerDevice, NULL, this);
			}
			if (!m_wrappedSurfaceLevels.emplace(faceLevel, pWrappedSurfaceLevel).second) {
				// insertion of wrapped surface level into m_wrappedSurfaceLevels succeeded
				*ppCubeMapSurface = NULL;
				hr = D3DERR_INVALIDCALL;
			}
			else {
				++shared_ref_count;
				++pWrappedSurfaceLevel->m_ulRef;
				pWrappedSurfaceLevel->zero_d3d_ref_count = false;
				if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
					*ppCubeMapSurface = pWrappedSurfaceLevel;
				}
				else {
					*ppCubeMapSurface = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(baseSurfaceLevel);
				}
			}
		}
	}
	if (ppCubeMapSurface) LogInfo("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseSurfaceLevel, *ppCubeMapSurface);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DCubeTexture9::LockRect(THIS_ ::D3DCUBEMAP_FACES FaceType, UINT Level, ::D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags)
{
	LogDebug("IDirect3DCubeTexture9::LockRect called with FaceType=%d, Level=%d, Rect=l:%d,u:%d,r:%d,b:%d\n", FaceType, Level,
		pRect ? pRect->left : 0, pRect ? pRect->top : 0, pRect ? pRect->right : 0, pRect ? pRect->bottom : 0);

	::IDirect3DCubeTexture9 *pBaseCubeTexture = GetD3DCubeTexture9();

	if (!pBaseCubeTexture)
	{
		if (!gLogDebug) LogInfo("IDirect3DCubeTexture9::LockRect called\n");
		LogInfo("  postponing call because cube texture was not created yet.\n");

		if (!pendingLockUnlock)
		{
			_FaceType = FaceType;
			_Flags = Flags;
			_Level = Level;
			_Buffer = new char[_EdgeLength*_EdgeLength * 4];
			pendingLockUnlock = true;
		}
		if (pLockedRect)
		{
			pLockedRect->Pitch = _EdgeLength * 4;
			pLockedRect->pBits = _Buffer;
		}
		return S_OK;
	}
	hackerDevice->FrameAnalysisLog("LockRect(pResource:0x%p, FaceType:0x%p, Level:%u, pRect:0x%p, LockFlags:%u)",
		pBaseCubeTexture, FaceType, Level, pRect, Flags);
	hackerDevice->FrameAnalysisLogResourceHash(pBaseCubeTexture);

	HRESULT hr = NULL;
	if (G->gForceStereo == 2) {
		if (IsDirectStereoCubeTexture()) {
			if (DirectFullSurfaces.find(Level) == DirectFullSurfaces.end())
			{
				DirectFullSurfaces[Level] = false;
			}
			if (!(Flags & D3DLOCK_READONLY)) {
				::D3DSURFACE_DESC desc;
				pBaseCubeTexture->GetLevelDesc(Level, &desc);
				UINT faceLevel = (FaceType * 6) + Level;
				//UINT faceLevel = (FaceType * 6) + Level;
				//Guard against multithreaded access as this could be causing us problems
				std::lock_guard<std::mutex> lck(m_mtx);
				if (desc.Pool == ::D3DPOOL_DEFAULT && !DirectLockRectNoMemTex) {
					//Initialise
					if (m_pDirectLockableSysMemTextures.find(Level) == m_pDirectLockableSysMemTextures.end())
					{
						m_pDirectLockableSysMemTextures[Level] = NULL;
					}

					bool newTexture = false;
					HRESULT hr = D3DERR_INVALIDCALL;
					if (!m_pDirectLockableSysMemTextures[Level])
					{
						hr = hackerDevice->GetD3D9Device()->CreateCubeTexture(desc.Width, 1, 0,
							desc.Format, ::D3DPOOL_SYSTEMMEM, &m_pDirectLockableSysMemTextures[Level], NULL);
						newTexture = true;
						if (FAILED(hr))
						{
							////DXT textures can't be less than 4 pixels in either width or height
							//if (desc.Width < 4) desc.Width = 4;
							//if (desc.Height < 4) desc.Height = 4;
							//hr = wrappedDevice->GetD3D9Device()->CreateCubeTexture(desc.Width, 1, 0,
							//	desc.Format, ::D3DPOOL_SYSTEMMEM, &m_pDirectLockableSysMemTextures[Level], NULL);
							//newTexture = true;
							//if (FAILED(hr))
							//{
							LogDebug("IDirect3DCubeTexture9::LockRect Direct Mode, failed to create system memory texture, hr = 0x%0.8x\n", hr);
							DirectLockRectNoMemTex = true;

						}
					}
					if (!DirectLockRectNoMemTex) {
						::IDirect3DSurface9 *pSurface = NULL;
						hr = m_pDirectLockableSysMemTextures[Level]->GetCubeMapSurface(FaceType, 0, &pSurface);
						if (FAILED(hr))
						{
							LogDebug("IDirect3DCubeTexture9::LockRect Direct Mode, failed get surface level from system memory texture, hr = 0x%0.8x\n", hr);
							goto postLock;
						}
						if (newTexture)
						{
							::IDirect3DSurface9 *pActualSurface = NULL;
							pBaseCubeTexture->GetCubeMapSurface(FaceType, Level, &pActualSurface);
							if (FAILED(hr))
							{
								LogDebug("IDirect3DCubeTexture9::LockRect Direct Mode, failed get surface level from actual texture, hr = 0x%0.8x\n", hr);
								goto postLock;
							}
							hr = hackerDevice->GetD3D9Device()->GetRenderTargetData(pActualSurface, pSurface);
							if (FAILED(hr)) {
								LogDebug("IDirect3DCubeTexture9::LockRect Direct Mode, failed to update system memory texture surface, hr = 0x%0.8x\n", hr);
								goto postLock;
							}
							pActualSurface->Release();
							//Not a new level any more
							DirectNewSurface[faceLevel] = false;
						}
						if (((Flags | D3DLOCK_NO_DIRTY_UPDATE) != D3DLOCK_NO_DIRTY_UPDATE) &&
							((Flags | D3DLOCK_READONLY) != D3DLOCK_READONLY))
						{
							hr = pBaseCubeTexture->AddDirtyRect(FaceType, pRect);
							if (FAILED(hr))
							{
								LogDebug("IDirect3DCubeTexture9::LockRect Direct Mode, failed to add dirty rect to actual texture, hr = 0x%0.8x\n", hr);
								goto postLock;
							}
						}

						hr = pSurface->LockRect(pLockedRect, pRect, Flags);
						if (FAILED(hr))
						{
							LogDebug("IDirect3DCubeTexture9::LockRect Direct Mode, failed to LockRect hr = 0x%0.8x\n", hr);
							goto postLock;
						}
						pSurface->Release();
					}
				}
				if (desc.Pool != ::D3DPOOL_DEFAULT || DirectLockRectNoMemTex) {
					hr = pBaseCubeTexture->LockRect(FaceType, Level, pLockedRect, pRect, Flags);

					if (FAILED(hr))
					{
						LogDebug("IDirect3DCubeTexture9::LockRect Direct Mode, failed to LockRect hr = 0x%0.8x\n", hr);
						goto postLock;
					}
					if (!pRect) {
						DirectFullSurfaces[faceLevel] = true;
						DirectLockedRects.erase(Level);
						DirectLockedFlags = Flags;
						DirectLockedRect = *pLockedRect;
					}
					else if (!DirectFullSurfaces[faceLevel])
					{
						D3DLockedRect lRect;
						lRect.lockedRect = *pLockedRect;
						lRect.rect = *pRect;
						lRect.flags = Flags;
						DirectLockedRects[faceLevel].push_back(lRect);
					}

				}
				else {
					if (!pRect) {
						DirectFullSurfaces[faceLevel] = true;
						DirectLockedRects.erase(Level);
						DirectLockedFlags = Flags;
						DirectLockedRect = *pLockedRect;
					}
					else if (!DirectFullSurfaces[faceLevel])
					{
						D3DLockedRect lRect;
						lRect.rect = *pRect;
						lRect.flags = Flags;
						DirectLockedRects[faceLevel].push_back(lRect);
					}
				}

			}
			else {
				hr = pBaseCubeTexture->LockRect(FaceType, Level, pLockedRect, pRect, Flags);
			}
		}
		else {
			hr = pBaseCubeTexture->LockRect(FaceType, Level, pLockedRect, pRect, Flags);
		}
	}
	else {
		hr = pBaseCubeTexture->LockRect(FaceType, Level, pLockedRect, pRect, Flags);
	}
postLock:
	hackerDevice->TrackAndDivertLock<D3D9Wrapper::IDirect3DCubeTexture9>(hr, this, pLockedRect, Flags, Level);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DCubeTexture9::UnlockRect(THIS_ ::D3DCUBEMAP_FACES FaceType, UINT Level)
{
	LogDebug("IDirect3DCubeTexture9::UnlockRect called\n");
	::IDirect3DCubeTexture9 *pBaseCubeTexture;
	pBaseCubeTexture = GetD3DCubeTexture9();

	if (!pBaseCubeTexture)
	{
		if (!gLogDebug) LogInfo("IDirect3DCubeTexture9::UnlockRect called\n");
		LogInfo("  postponing call because cube texture was not created yet.\n");

		return S_OK;
	}
	hackerDevice->FrameAnalysisLog("Unlock(pResource:0x%p)",
		pBaseCubeTexture);
	hackerDevice->FrameAnalysisLogResourceHash(pBaseCubeTexture);
	hackerDevice->TrackAndDivertUnlock(this, Level);
	HRESULT hr;
	if (G->gForceStereo == 2 && IsDirectStereoCubeTexture()) {
		UINT faceLevel = (FaceType * 6) + Level;
		if (DirectFullSurfaces[faceLevel] || DirectLockedRects[Level].size() > 0) {
			::D3DSURFACE_DESC desc;
			pBaseCubeTexture->GetLevelDesc(Level, &desc);
			//Guard against multithreaded access as this could be causing us problems
			std::lock_guard<std::mutex> lck(m_mtx);

			if (desc.Pool == ::D3DPOOL_DEFAULT && !DirectLockRectNoMemTex) {
				if (m_pDirectLockableSysMemTextures.find(Level) == m_pDirectLockableSysMemTextures.end())
					goto postUnlock;
				::IDirect3DSurface9 *pSurface = NULL;
				HRESULT hr = m_pDirectLockableSysMemTextures[Level] ? m_pDirectLockableSysMemTextures[Level]->GetCubeMapSurface(FaceType, 0, &pSurface) : D3DERR_INVALIDCALL;
				if (FAILED(hr))
					goto postUnlock;
				pSurface->UnlockRect();
				::IDirect3DSurface9 *pDirectSurfaceRight = NULL;
				hr = m_pDirectCubeTextureRight->GetCubeMapSurface(FaceType, Level, &pDirectSurfaceRight);
				if (FAILED(hr))
					goto postUnlock;
				::IDirect3DSurface9 *pActualSurface = NULL;
				hr = pBaseCubeTexture->GetCubeMapSurface(FaceType, Level, &pActualSurface);
				if (FAILED(hr))
					goto postUnlock;
				if (DirectFullSurfaces[faceLevel])
				{
					hr = hackerDevice->GetD3D9Device()->UpdateSurface(pSurface, NULL, pDirectSurfaceRight, NULL);
					if (FAILED(hr))
					{
						LogDebug("IDirect3DCubeTexture9::UnlockRect Direct Mode, failed to update full right surface, hr = 0x%0.8x\n", hr);
						//Just ignore the failed copy back, not much we can do
						hr = S_OK;
					}
					hr = hackerDevice->GetD3D9Device()->UpdateSurface(pSurface, NULL, pActualSurface, NULL);
					if (FAILED(hr))
					{
						LogDebug("IDirect3DCubeTexture9::UnlockRect Direct Mode, failed to update full left surface, hr = 0x%0.8x\n", hr);
						//Just ignore the failed copy back, not much we can do
						hr = S_OK;
					}
				}
				else
				{
					std::vector<D3DLockedRect>::iterator rectIter = DirectLockedRects[faceLevel].begin();
					while (rectIter != DirectLockedRects[faceLevel].end())
					{
						POINT p;
						p.x = (*rectIter).rect.left;
						p.y = (*rectIter).rect.top;
						hr = hackerDevice->GetD3D9Device()->UpdateSurface(pSurface, &(*rectIter).rect, pDirectSurfaceRight, &p);
						if (FAILED(hr))
						{
							LogDebug("IDirect3DCubeTexture9::UnlockRect Direct Mode, failed to update rect right surface, hr = 0x%0.8x\n", hr);
							//Just ignore the failed copy back, not much we can do
							hr = S_OK;
						}
						hr = hackerDevice->GetD3D9Device()->UpdateSurface(pSurface, &(*rectIter).rect, pActualSurface, &p);
						if (FAILED(hr))
						{
							LogDebug("IDirect3DCubeTexture9::UnlockRect Direct Mode, failed to update rect left surface, hr = 0x%0.8x\n", hr);
							//Just ignore the failed copy back, not much we can do
							hr = S_OK;
						}
						rectIter++;
						//}
					}
				}
				pDirectSurfaceRight->Release();
				//Release everything
				pActualSurface->Release();
				pSurface->Release();
			}
			else {
				::D3DLOCKED_RECT lRect;
				if (DirectFullSurfaces[Level]) {
					hr = m_pDirectCubeTextureRight->LockRect(FaceType, Level, &lRect, NULL, DirectLockedFlags);
					if (FAILED(hr))
					{
						LogDebug("IDirect3DCubeTexture9::UnlockRect Direct Mode, failed to lock right surface, hr = 0x%0.8x\n", hr);
						hr = S_OK;
					}
					else {
						unsigned char* sptr = static_cast<unsigned char*>(DirectLockedRect.pBits);
						unsigned char* dptr = static_cast<unsigned char*>(lRect.pBits);
						memcpy(dptr, sptr, (DirectLockedRect.Pitch * desc.Height));
						m_pDirectCubeTextureRight->UnlockRect(FaceType, Level);
					}
				}
				else {
					bool lockedRight = false;
					std::vector<D3DLockedRect>::iterator rectIter = DirectLockedRects[Level].begin();
					while (rectIter != DirectLockedRects[Level].end())
					{
						if (!((*rectIter).flags & D3DLOCK_READONLY)) {
							hr = m_pDirectCubeTextureRight->LockRect(FaceType, Level, &lRect, &(*rectIter).rect, (*rectIter).flags);
							if (FAILED(hr))
							{
								LogDebug("IDirect3DCubeTexture9::UnlockRect Direct Mode, failed to lock rect on right surface, hr = 0x%0.8x\n", hr);
								hr = S_OK;
							}
							else {
								lockedRight = true;
								unsigned char* sptr = static_cast<unsigned char*>((*rectIter).lockedRect.pBits);
								unsigned char* dptr = static_cast<unsigned char*>(lRect.pBits);
								memcpy(dptr, sptr, ((*rectIter).lockedRect.Pitch * desc.Height));
							}
						}
						rectIter++;
					}
					if (lockedRight)
						hr = m_pDirectCubeTextureRight->UnlockRect(FaceType, Level);
				}
				hr = pBaseCubeTexture->UnlockRect(FaceType, Level);
			}
			DirectLockedRects.erase(Level);
			DirectFullSurfaces[faceLevel] = false;

		}
		else {
			hr = pBaseCubeTexture->UnlockRect(FaceType, Level);
		}
	}
	else {
		hr = pBaseCubeTexture->UnlockRect(FaceType, Level);
	}
postUnlock:
	if (G->analyse_frame)
		hackerDevice->FrameAnalysisAfterUnlock(pBaseCubeTexture);

	return hr;
}


STDMETHODIMP D3D9Wrapper::IDirect3DCubeTexture9::AddDirtyRect(THIS_ ::D3DCUBEMAP_FACES FaceType, CONST RECT* pDirtyRect)
{
	LogDebug("IDirect3DCubeTexture9::AddDirtyRect called\n");

	CheckCubeTexture9(this);
	HRESULT hr;
	if (G->gForceStereo == 2) {
		if (IsDirectStereoCubeTexture()) {
			m_pDirectCubeTextureRight->AddDirtyRect(FaceType, pDirtyRect);
			hr = GetD3DCubeTexture9()->AddDirtyRect(FaceType, pDirtyRect);
		}
		else {
			hr = GetD3DCubeTexture9()->AddDirtyRect(FaceType, pDirtyRect);
		}
	}
	else {
		hr = GetD3DCubeTexture9()->AddDirtyRect(FaceType, pDirtyRect);
	}
	return hr;
}
