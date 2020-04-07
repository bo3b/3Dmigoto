#include "HookedVolume.h"
#include "d3d9Wrapper.h"

inline void D3D9Wrapper::IDirect3DVolume9::Delete()
{
	LogInfo("IDirect3DVolume9::Delete\n");
	LogInfo("  deleting self\n");

	if (m_pRealUnk) m_List.DeleteMember(m_pRealUnk);
	m_pUnk = 0;
	m_pRealUnk = 0;
	delete this;
}
inline void D3D9Wrapper::IDirect3DVolume9::HookVolume()
{
	m_pUnk = hook_volume(GetD3DVolume9(), (::IDirect3DVolume9*)this);

}
D3D9Wrapper::IDirect3DVolume9::IDirect3DVolume9(::LPDIRECT3DVOLUME9 pVolume, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, D3D9Wrapper::IDirect3DVolumeTexture9 *owningContainer)
	: D3D9Wrapper::IDirect3DUnknown((IUnknown*)pVolume),
	pendingGetVolumeLevel(false),
	magic(0x7da43feb),
	m_OwningContainer(owningContainer),
	hackerDevice(hackerDevice),
	zero_d3d_ref_count(false)
{
	if ((G->enable_hooks >= EnableHooksDX9::ALL) && pVolume) {
		this->HookVolume();
	}

}

D3D9Wrapper::IDirect3DVolume9* D3D9Wrapper::IDirect3DVolume9::GetDirect3DVolume9(::LPDIRECT3DVOLUME9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, D3D9Wrapper::IDirect3DVolumeTexture9 *owningContainer)
{
	D3D9Wrapper::IDirect3DVolume9* p = new D3D9Wrapper::IDirect3DVolume9(pSurface, hackerDevice, owningContainer);
	if (pSurface) m_List.AddMember(pSurface, p);
	return p;
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolume9::QueryInterface(THIS_ REFIID riid, void ** ppvObj)
{
	LogDebug("D3D9Wrapper::IDirect3DVolume9::QueryInterface called\n");// at 'this': %s\n", type_name_dx9((IUnknown*)this));
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
				++m_OwningContainer->shared_ref_count;
				zero_d3d_ref_count = false;
				LogInfo("  interface replaced with IDirect3DVolume9 wrapper.\n");
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

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DVolume9::AddRef(THIS)
{
	++m_ulRef;
	++m_OwningContainer->shared_ref_count;
	zero_d3d_ref_count = false;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DVolume9::Release(THIS)
{
	LogDebug("IDirect3DVolume9::Release handle=%p, counter=%d, this=%p\n", m_pUnk, m_ulRef, this);

	ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);

	--m_ulRef;
	bool prev_non_zero = !zero_d3d_ref_count;
	if (ulRef == 0)
	{
		if (!gLogDebug) LogInfo("IDirect3DVolume9::Release handle=%p, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		zero_d3d_ref_count = true;
	}
	if (prev_non_zero) {
		--m_OwningContainer->shared_ref_count;
		if (m_OwningContainer->shared_ref_count == 0 && m_OwningContainer->bound.empty())
			m_OwningContainer->Delete();
	}
	return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolume9::GetDevice(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice)
{
	LogDebug("IDirect3DVolume9::GetDevice called\n");

	CheckVolume9(this);
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

STDMETHODIMP D3D9Wrapper::IDirect3DVolume9::SetPrivateData(THIS_ REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags)
{
	LogDebug("IDirect3DVolume9::SetPrivateData called\n");

	CheckVolume9(this);
	return GetD3DVolume9()->SetPrivateData(refguid, pData, SizeOfData, Flags);
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolume9::GetPrivateData(THIS_ REFGUID refguid, void* pData, DWORD* pSizeOfData)
{
	LogDebug("IDirect3DVolume9::GetPrivateData called\n");

	CheckVolume9(this);
	return GetD3DVolume9()->GetPrivateData(refguid, pData, pSizeOfData);
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolume9::FreePrivateData(THIS_ REFGUID refguid)
{
	LogDebug("IDirect3DVolume9::GetPrivateData called\n");

	CheckVolume9(this);
	return GetD3DVolume9()->FreePrivateData(refguid);
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolume9::GetContainer(THIS_ REFIID riid, void** ppContainer)
{
	LogDebug("IDirect3DVolume9::GetContainer called\n");

	CheckVolume9(this);
	HRESULT hr;
	void *pContainer = NULL;
	HRESULT queryResult = m_OwningContainer->QueryInterface(riid, &pContainer);
	if (queryResult == S_OK) {
		++m_OwningContainer->m_ulRef;
		++m_OwningContainer->shared_ref_count;
		m_OwningContainer->zero_d3d_ref_count = false;
		if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
			*ppContainer = m_OwningContainer;
		}
		else {
			*ppContainer = m_OwningContainer->GetRealOrig();
		}
		hr = D3D_OK;
	}
	else if (queryResult == E_NOINTERFACE) {

		hr = E_NOINTERFACE;
	}
	else {
		hr = D3DERR_INVALIDCALL;
	}

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolume9::GetDesc(THIS_ ::D3DVOLUME_DESC *pDesc)
{
	LogDebug("IDirect3DVolume9::GetDesc called\n");

	CheckVolume9(this);
	return GetD3DVolume9()->GetDesc(pDesc);
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolume9::LockBox(THIS_ ::D3DLOCKED_BOX *pLockedVolume, CONST ::D3DBOX *pBox, DWORD Flags)
{
	LogDebug("IDirect3DVolume9::LockBox called\n");

	CheckVolume9(this);
	return GetD3DVolume9()->LockBox(pLockedVolume, pBox, Flags);
}

STDMETHODIMP D3D9Wrapper::IDirect3DVolume9::UnlockBox(THIS)
{
	LogDebug("IDirect3DVolume9::UnlockBox called\n");

	CheckVolume9(this);
	return GetD3DVolume9()->UnlockBox();
}
