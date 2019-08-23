#pragma once
#include "d3d9Wrapper.h"
#include "HookedStateBlock.h"

inline void D3D9Wrapper::IDirect3DStateBlock9::Delete()
{
	LogInfo("IDirect3DStateBlock9::Delete\n");
	LogInfo("  deleting self\n");
	if (mDirectModeStateBlockDuplication)
		mDirectModeStateBlockDuplication->Release();
	if (m_pRealUnk) m_List.DeleteMember(m_pRealUnk);
	m_pUnk = 0;
	m_pRealUnk = 0;
	delete this;
}

void D3D9Wrapper::IDirect3DStateBlock9::HookStateBlock()
{
	m_pUnk = hook_stateblock(GetD3DStateBlock9(), (D3D9Base::IDirect3DStateBlock9*)this);
}

inline D3D9Wrapper::IDirect3DStateBlock9::IDirect3DStateBlock9(D3D9Base::LPDIRECT3DSTATEBLOCK9 pStateBlock, D3D9Wrapper::IDirect3DDevice9 *hackerDevice)
	: D3D9Wrapper::IDirect3DUnknown((IUnknown*)pStateBlock), 
	mDirectModeStateBlockDuplication(NULL), 
	hackerDevice(hackerDevice)
{
	if ((G->enable_hooks >= EnableHooksDX9::ALL) && pStateBlock) {
		this->HookStateBlock();
	}
}

inline D3D9Wrapper::IDirect3DStateBlock9 * D3D9Wrapper::IDirect3DStateBlock9::GetDirect3DStateBlock9(D3D9Base::LPDIRECT3DSTATEBLOCK9 pStateBlock, D3D9Wrapper::IDirect3DDevice9 *hackerDevice)
{
	D3D9Wrapper::IDirect3DStateBlock9* p = new D3D9Wrapper::IDirect3DStateBlock9(pStateBlock, hackerDevice);
	if (pStateBlock) m_List.AddMember(pStateBlock, p);
	return p;
}

STDMETHODIMP D3D9Wrapper::IDirect3DStateBlock9::QueryInterface(THIS_ REFIID riid, void ** ppvObj)
{
	LogDebug("D3D9Wrapper::IDirect3DStateBlock9::QueryInterface called\n");// at 'this': %s\n", type_name_dx9((IUnknown*)this));
	HRESULT hr = NULL;
	if (QueryInterface_DXGI_Callback(riid, ppvObj, &hr))
		return hr;
	LogInfo("QueryInterface request for %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx on %x\n",
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7], this);
	hr = m_pUnk->QueryInterface(riid, ppvObj);
	if (hr == S_OK) {
		if ((*ppvObj) == GetRealOrig()) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppvObj = this;
				++m_ulRef;
				LogInfo("  interface replaced with IDirect3DStateBlock9 wrapper.\n");
				LogInfo("  result = %x, handle = %x\n", hr, *ppvObj);
				return hr;
			}
		}
		D3D9Wrapper::IDirect3DUnknown *unk = QueryInterface_Find_Wrapper(*ppvObj);
		if (unk)
			*ppvObj = unk;
	}
	LogInfo("  result = %x, handle = %x\n", hr, *ppvObj);
	return hr;
}

inline STDMETHODIMP_(ULONG __stdcall) D3D9Wrapper::IDirect3DStateBlock9::AddRef(void)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

inline STDMETHODIMP_(ULONG __stdcall) D3D9Wrapper::IDirect3DStateBlock9::Release(void)
{
	LogDebug("IDirect3DStateBlock9::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);

	ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);

	--m_ulRef;

	if (ulRef == 0)
	{
		if (!gLogDebug) LogInfo("IDirect3DStateBlock9::Release handle=%x, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		
		Delete();
	}
	return ulRef;
}

inline STDMETHODIMP_(HRESULT __stdcall) D3D9Wrapper::IDirect3DStateBlock9::Apply()
{
	LogDebug("IDirect3DStateBlock9::Apply called\n");
	return GetD3DStateBlock9()->Apply();
}

inline STDMETHODIMP_(HRESULT __stdcall) D3D9Wrapper::IDirect3DStateBlock9::Capture()
{
	LogDebug("IDirect3DStateBlock9::Capture called\n");
	return GetD3DStateBlock9()->Capture();
}

inline STDMETHODIMP_(HRESULT __stdcall) D3D9Wrapper::IDirect3DStateBlock9::GetDevice(D3D9Wrapper::IDirect3DDevice9 ** ppDevice)
{
	LogDebug("IDirect3DStateBlock9::GetDevice called\n");
	if (hackerDevice) {
		hackerDevice->GetD3D9Device()->AddRef();
	}
	else {
		return D3DERR_NOTFOUND;
	}
	if (!(G->enable_hooks & EnableHooksDX9::DEVICE)) {
		*ppDevice = hackerDevice;
	}
	else {
		*ppDevice = reinterpret_cast<D3D9Wrapper::IDirect3DDevice9*>(hackerDevice->GetD3D9Device());
	}
	return D3D_OK;
}

