#include "HookedVolumeTexture.h"
#include "d3d9Wrapper.h"
inline void D3D9Wrapper::IDirect3DVolumeTexture9::Delete()
{
	LogInfo("IDirect3DVolumeTexture9::Delete\n");
	LogInfo("  deleting self\n");

	D3D9Wrapper::IDirect3DResource9::Delete();
	for (auto it = m_wrappedVolumeLevels.cbegin(); it != m_wrappedVolumeLevels.cend();)
	{
		(*it).second->Delete();
		++it;
	}
	if (m_pRealUnk) m_List.DeleteMember(m_pRealUnk);
	m_pUnk = 0;
	m_pRealUnk = 0;
	delete this;
}
inline void D3D9Wrapper::IDirect3DVolumeTexture9::HookVolumeTexture()
{
	m_pUnk = hook_volume_texture(GetD3DVolumeTexture9(), (::IDirect3DVolumeTexture9*)this);
}
D3D9Wrapper::IDirect3DVolumeTexture9::IDirect3DVolumeTexture9(::LPDIRECT3DVOLUMETEXTURE9 pTexture, D3D9Wrapper::IDirect3DDevice9 *hackerDevice)
	: D3D9Wrapper::IDirect3DBaseTexture9(pTexture, hackerDevice, TextureType::Volume)
{
	if ((G->enable_hooks >= EnableHooksDX9::ALL) && pTexture) {
		this->HookVolumeTexture();
	}
}

D3D9Wrapper::IDirect3DVolumeTexture9* D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9(::LPDIRECT3DVOLUMETEXTURE9 pTexture, D3D9Wrapper::IDirect3DDevice9 *hackerDevice)
{
	D3D9Wrapper::IDirect3DVolumeTexture9* p = new D3D9Wrapper::IDirect3DVolumeTexture9(pTexture, hackerDevice);
	if (pTexture) m_List.AddMember(pTexture, p);
	return p;
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolumeTexture9::QueryInterface(THIS_ REFIID riid, void ** ppvObj)
{
	LogDebug("D3D9Wrapper::IDirect3DVolumeTexture9::QueryInterface called\n");// at 'this': %s\n", type_name_dx9((IUnknown*)this));
	HRESULT hr = NULL;
	if (QueryInterface_DXGI_Callback(riid, ppvObj, &hr))
		return hr;
	LogInfo("QueryInterface request for %s on %p\n", NameFromIID(riid), this);
	hr = m_pUnk->QueryInterface(riid, ppvObj);
	if (hr == S_OK) {
		if ((*ppvObj) == GetRealOrig()) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppvObj = this;
				m_ulRef++;
				zero_d3d_ref_count = false;
				++shared_ref_count;
				LogInfo("  interface replaced with IDirect3DVolumeTexture9 wrapper.\n");
				LogInfo("  result = %x, handle = %p\n", hr, *ppvObj);
				return hr;
			}
		}
		/* IID_IDirect3DVolume9 */
		/* {24F416E6-1F67-4aa7-B88E-D33F6F3128A1} */
		//DEFINE_GUID(IID_IDirect3DVolume9, 0x24f416e6, 0x1f67, 0x4aa7, 0xb8, 0x8e, 0xd3, 0x3f, 0x6f, 0x31, 0x28, 0xa1);
		IF_GUID(riid, 0x24f416e6, 0x1f67, 0x4aa7, 0xb8, 0x8e, 0xd3, 0x3f) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				auto it = m_wrappedVolumeLevels.find(0);
				if (it != m_wrappedVolumeLevels.end()) {
					D3D9Wrapper::IDirect3DVolume9 *wrapper = it->second;
					*ppvObj = wrapper;
					wrapper->m_ulRef++;
					wrapper->zero_d3d_ref_count = false;
					++shared_ref_count;
					LogInfo("  interface replaced with IDirect3DVolume9 wrapper.\n");
					LogInfo("  result = %x, handle = %p\n", hr, *ppvObj);
					return hr;
				}
				else {
					D3D9Wrapper::IDirect3DVolume9 *wrapper = IDirect3DVolume9::GetDirect3DVolume9((::IDirect3DVolume9*)(*ppvObj), hackerDevice, this);
					m_wrappedVolumeLevels.emplace(0, wrapper);
					wrapper->m_ulRef++;
					wrapper->zero_d3d_ref_count = false;
					++shared_ref_count;
					LogInfo("  interface replaced with IDirect3DVolume9 wrapper.\n");
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

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DVolumeTexture9::AddRef(THIS)
{
	++m_ulRef;
	zero_d3d_ref_count = false;
	++shared_ref_count;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DVolumeTexture9::Release(THIS)
{
	LogDebug("IDirect3DVolumeTexture9::Release handle=%p, counter=%d, this=%p\n", m_pUnk, m_ulRef, this);

	ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);

	--m_ulRef;
	bool prev_non_zero = !zero_d3d_ref_count;
	if (ulRef == 0)
	{
		if (!gLogDebug) LogInfo("IDirect3DVolumeTexture9::Release handle=%p, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
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

STDMETHODIMP D3D9Wrapper::IDirect3DVolumeTexture9::GetDevice(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice)
{
	LogDebug("IDirect3DVolumeTexture9::GetDevice called\n");

	CheckVolumeTexture9(this);
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

STDMETHODIMP D3D9Wrapper::IDirect3DVolumeTexture9::SetPrivateData(THIS_ REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags)
{
	LogDebug("IDirect3DVolumeTexture9::SetPrivateData called\n");

	CheckVolumeTexture9(this);
	return GetD3DVolumeTexture9()->SetPrivateData(refguid, pData, SizeOfData, Flags);
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolumeTexture9::GetPrivateData(THIS_ REFGUID refguid, void* pData, DWORD* pSizeOfData)
{
	LogDebug("IDirect3DVolumeTexture9::GetPrivateData called\n");

	CheckVolumeTexture9(this);
	return GetD3DVolumeTexture9()->GetPrivateData(refguid, pData, pSizeOfData);
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolumeTexture9::FreePrivateData(THIS_ REFGUID refguid)
{
	LogDebug("IDirect3DVolumeTexture9::FreePrivateData called\n");

	CheckVolumeTexture9(this);
	return GetD3DVolumeTexture9()->FreePrivateData(refguid);
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DVolumeTexture9::SetPriority(THIS_ DWORD PriorityNew)
{
	LogDebug("IDirect3DVolumeTexture9::SetPriority called\n");

	CheckVolumeTexture9(this);
	return GetD3DVolumeTexture9()->SetPriority(PriorityNew);
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DVolumeTexture9::GetPriority(THIS)
{
	LogDebug("IDirect3DVolumeTexture9::GetPriority called\n");

	CheckVolumeTexture9(this);
	return GetD3DVolumeTexture9()->GetPriority();
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DVolumeTexture9::PreLoad(THIS)
{
	//managed resources only?
	LogDebug("IDirect3DVolumeTexture9::PreLoad called\n");

	CheckVolumeTexture9(this);
	return GetD3DVolumeTexture9()->PreLoad();
}

STDMETHODIMP_(::D3DRESOURCETYPE) D3D9Wrapper::IDirect3DVolumeTexture9::GetType(THIS)
{
	LogDebug("IDirect3DVolumeTexture9::GetType called\n");

	CheckVolumeTexture9(this);
	return GetD3DVolumeTexture9()->GetType();
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DVolumeTexture9::SetLOD(THIS_ DWORD LODNew)
{
	//managed resources only?
	LogDebug("IDirect3DVolumeTexture9::SetLOD called\n");

	CheckVolumeTexture9(this);
	hackerDevice->FrameAnalysisLog("SetLOD(volumetexture:0x%p)",
		this->GetD3DVolumeTexture9());
	hackerDevice->FrameAnalysisLogResourceHash(this->GetD3DVolumeTexture9());
	return GetD3DVolumeTexture9()->SetLOD(LODNew);
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DVolumeTexture9::GetLOD(THIS)
{
	LogDebug("IDirect3DVolumeTexture9::GetLOD called\n");

	CheckVolumeTexture9(this);
	DWORD ret = GetD3DVolumeTexture9()->GetLOD();
	hackerDevice->FrameAnalysisLog("GetLOD(volumetexture:0x%p) = %x",
		this->GetD3DVolumeTexture9(), ret);
	hackerDevice->FrameAnalysisLogResourceHash(this->GetD3DVolumeTexture9());
	return ret;
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DVolumeTexture9::GetLevelCount(THIS)
{
	LogDebug("IDirect3DVolumeTexture9::GetLevelCount called\n");

	CheckVolumeTexture9(this);
	return GetD3DVolumeTexture9()->GetLevelCount();
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolumeTexture9::SetAutoGenFilterType(THIS_ ::D3DTEXTUREFILTERTYPE FilterType)
{
	LogDebug("IDirect3DVolumeTexture9::SetAutoGenFilterType called\n");

	CheckVolumeTexture9(this);
	return GetD3DVolumeTexture9()->SetAutoGenFilterType(FilterType);
}

STDMETHODIMP_(::D3DTEXTUREFILTERTYPE) D3D9Wrapper::IDirect3DVolumeTexture9::GetAutoGenFilterType(THIS)
{
	LogDebug("IDirect3DVolumeTexture9::GetAutoGenFilterType called\n");

	CheckVolumeTexture9(this);
	return GetD3DVolumeTexture9()->GetAutoGenFilterType();
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DVolumeTexture9::GenerateMipSubLevels(THIS)
{
	LogDebug("IDirect3DVolumeTexture9::GenerateMipSubLevels called\n");

	CheckVolumeTexture9(this);
	hackerDevice->FrameAnalysisLog("GenerateMipSubLevels(volumetexture:0x%p\n)",
		this->GetD3DVolumeTexture9());
	hackerDevice->FrameAnalysisLogResource(-1, NULL, this->GetD3DVolumeTexture9());
	return GetD3DVolumeTexture9()->GenerateMipSubLevels();
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolumeTexture9::GetLevelDesc(THIS_ UINT Level, ::D3DVOLUME_DESC *pDesc)
{
	LogDebug("IDirect3DVolumeTexture9::GetLevelDesc called\n");

	CheckVolumeTexture9(this);
	return GetD3DVolumeTexture9()->GetLevelDesc(Level, pDesc);
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolumeTexture9::GetVolumeLevel(THIS_ UINT Level, D3D9Wrapper::IDirect3DVolume9** ppVolumeLevel)
{
	LogDebug("IDirect3DVolumeTexture9::GetSurfaceLevel called\n");


	if (!GetD3DVolumeTexture9())
	{
		LogInfo("  postponing call because volume texture was not created yet.\n");

		D3D9Wrapper::IDirect3DVolume9 *wrapper = D3D9Wrapper::IDirect3DVolume9::GetDirect3DVolume9((::LPDIRECT3DVOLUME9) 0, hackerDevice, this);
		wrapper->_Level = Level;
		wrapper->_VolumeTexture = this;
		wrapper->pendingGetVolumeLevel = true;
		*ppVolumeLevel = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	::IDirect3DVolume9 *baseVolumeLevel = 0;
	HRESULT hr = NULL;
	auto it = m_wrappedVolumeLevels.find(Level);
	if (it != m_wrappedVolumeLevels.end()) {
		D3D9Wrapper::IDirect3DVolume9 *wrapper = it->second;
		if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
			*ppVolumeLevel = wrapper;
		}
		else {
			*ppVolumeLevel = reinterpret_cast<D3D9Wrapper::IDirect3DVolume9*>(wrapper->GetRealOrig());
		}
		baseVolumeLevel = wrapper->GetD3DVolume9();
		wrapper->AddRef();
		hr = S_OK;
	}
	else {
		hr = GetD3DVolumeTexture9()->GetVolumeLevel(Level, &baseVolumeLevel);
		if (FAILED(hr)) {
			*ppVolumeLevel = NULL;
		}else{
			D3D9Wrapper::IDirect3DVolume9 * pWrappedVolumeLevel = IDirect3DVolume9::GetDirect3DVolume9(baseVolumeLevel, hackerDevice, this);
			if (!m_wrappedVolumeLevels.emplace(Level, pWrappedVolumeLevel).second) {
				*ppVolumeLevel = NULL;
				hr = D3DERR_INVALIDCALL;
			}
			else {
				++shared_ref_count;
				++pWrappedVolumeLevel->m_ulRef;
				pWrappedVolumeLevel->zero_d3d_ref_count = false;
				if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
					*ppVolumeLevel = pWrappedVolumeLevel;
				}
				else {
					*ppVolumeLevel = reinterpret_cast<D3D9Wrapper::IDirect3DVolume9*>(baseVolumeLevel);
				}
			}
		}
	}
	if (ppVolumeLevel) LogInfo("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseVolumeLevel, *ppVolumeLevel);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolumeTexture9::LockBox(THIS_ UINT Level, ::D3DLOCKED_BOX *pLockedVolume, CONST ::D3DBOX *pBox, DWORD Flags)
{
	LogDebug("IDirect3DVolumeTexture9::LockBox called with Level=%d, Rect=l:%d,u:%d,r:%d,b:%d, f:%d, bk:%d\n", Level,
		pBox ? pBox->Left : 0, pBox ? pBox->Top : 0, pBox ? pBox->Right : 0, pBox ? pBox->Bottom : 0, pBox ? pBox->Front : 0, pBox ? pBox->Back : 0);

	::IDirect3DVolumeTexture9 *pBaseVolumeTexture = GetD3DVolumeTexture9();

	if (!pBaseVolumeTexture)
	{
		if (!gLogDebug) LogInfo("IDirect3DVolumeTexture9::LockBox called\n");
		LogInfo("  postponing call because volume texture was not created yet.\n");

		if (!pendingLockUnlock)
		{
			_Flags = Flags;
			_Level = Level;
			_Buffer = new char[_Width*_Height*_Depth * 4];
			pendingLockUnlock = true;
		}
		if (pLockedVolume)
		{
			pLockedVolume->RowPitch = _Width * 4;
			pLockedVolume->SlicePitch = _Width*_Height * 4;
			pLockedVolume->pBits = _Buffer;
		}
		return S_OK;
	}
	hackerDevice->FrameAnalysisLog("LockBox(pResource:0x%p, Level:%u, pBox:0x%p, LockFlags:%u)",
		pBaseVolumeTexture, Level, pBox, Flags);
	hackerDevice->FrameAnalysisLogResourceHash(pBaseVolumeTexture);
	HRESULT hr = pBaseVolumeTexture->LockBox(Level, pLockedVolume, pBox, Flags);
	hackerDevice->TrackAndDivertLock(hr, this, pLockedVolume, Flags, Level);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolumeTexture9::UnlockBox(THIS_ UINT Level)
{
	LogDebug("IDirect3DVolumeTexture9::UnlockBox called\n");
	::IDirect3DVolumeTexture9 *pBaseVolumeTexture;
	pBaseVolumeTexture = GetD3DVolumeTexture9();

	if (!pBaseVolumeTexture)
	{
		if (!gLogDebug) LogInfo("IDirect3DVolumeTexture9::UnlockBox called\n");
		LogInfo("  postponing call because volume texture was not created yet.\n");

		return S_OK;
	}
	hackerDevice->FrameAnalysisLog("Unlock(pResource:0x%p)",
		pBaseVolumeTexture);
	hackerDevice->FrameAnalysisLogResourceHash(pBaseVolumeTexture);
	hackerDevice->TrackAndDivertUnlock(this, Level);
	return GetD3DVolumeTexture9()->UnlockBox(Level);

	if (G->analyse_frame)
		hackerDevice->FrameAnalysisAfterUnlock(pBaseVolumeTexture);
}


STDMETHODIMP D3D9Wrapper::IDirect3DVolumeTexture9::AddDirtyBox(THIS_ CONST ::D3DBOX *pDirtyBox)
{
	LogDebug("IDirect3DVolumeTexture9::AddDirtyBox called\n");

	CheckVolumeTexture9(this);
	return GetD3DVolumeTexture9()->AddDirtyBox(pDirtyBox);
}
