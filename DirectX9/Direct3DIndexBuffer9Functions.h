#include "HookedIndexBuffer.h"
#include "d3d9Wrapper.h"
inline void D3D9Wrapper::IDirect3DIndexBuffer9::Delete()
{
	LogInfo("IDirect3DIndexBuffer9::Delete\n");
	LogInfo("  deleting self\n");

	D3D9Wrapper::IDirect3DResource9::Delete();

	if (m_pRealUnk) m_List.DeleteMember(m_pRealUnk);
	m_pUnk = 0;
	m_pRealUnk = 0;
	delete this;
}
inline void D3D9Wrapper::IDirect3DIndexBuffer9::HookIndexBuffer()
{

	m_pUnk = hook_index_buffer(GetD3DIndexBuffer9(), (::IDirect3DIndexBuffer9*)this);
}
D3D9Wrapper::IDirect3DIndexBuffer9::IDirect3DIndexBuffer9(::LPDIRECT3DINDEXBUFFER9 pIndexBuffer, D3D9Wrapper::IDirect3DDevice9 *hackerDevice)
    : D3D9Wrapper::IDirect3DResource9((::LPDIRECT3DRESOURCE9) pIndexBuffer, hackerDevice),
	pendingCreateIndexBuffer(false),
	pendingLockUnlock(false),
	magic(0x7da43feb),
	bound(false),
	zero_d3d_ref_count(false)
{
	if ((G->enable_hooks >= EnableHooksDX9::ALL) && pIndexBuffer) {
		this->HookIndexBuffer();
	}
}

D3D9Wrapper::IDirect3DIndexBuffer9* D3D9Wrapper::IDirect3DIndexBuffer9::GetDirect3DIndexBuffer9(::LPDIRECT3DINDEXBUFFER9 pIndexBuffer, D3D9Wrapper::IDirect3DDevice9 *hackerDevice)
{
    D3D9Wrapper::IDirect3DIndexBuffer9* p = new D3D9Wrapper::IDirect3DIndexBuffer9(pIndexBuffer, hackerDevice);
    if (pIndexBuffer) m_List.AddMember(pIndexBuffer, p);
    return p;
}
STDMETHODIMP D3D9Wrapper::IDirect3DIndexBuffer9::QueryInterface(THIS_ REFIID riid, void ** ppvObj)
{
	LogDebug("D3D9Wrapper::IDirect3DIndexBuffer9::QueryInterface called\n");// at 'this': %s\n", type_name_dx9((IUnknown*)this));
	HRESULT hr = NULL;
	if (QueryInterface_DXGI_Callback(riid, ppvObj, &hr))
		return hr;
	LogInfo("QueryInterface request for %s on %p\n", NameFromIID(riid), this);
	hr = m_pUnk->QueryInterface(riid, ppvObj);
	if (hr == S_OK) {
		if ((*ppvObj) == GetRealOrig()) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppvObj = this;
				zero_d3d_ref_count = false;
				++m_ulRef;
				LogInfo("  interface replaced with IDirect3DIndexBuffer9 wrapper.\n");
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

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DIndexBuffer9::AddRef(THIS)
{
	++m_ulRef;
	zero_d3d_ref_count = false;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DIndexBuffer9::Release(THIS)
{
	LogDebug("IDirect3DIndexBuffer9::Release handle=%p, counter=%d, this=%p\n", m_pUnk, m_ulRef, this);

	ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);
    if (ulRef == 0)
    {
		if (!gLogDebug) LogInfo("IDirect3DIndexBuffer9::Release handle=%p, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		zero_d3d_ref_count = true;
		if (!bound)
			Delete();
    }
    return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3DIndexBuffer9::GetDevice(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice)
{
	LogDebug("IDirect3DIndexBuffer9::GetDevice called\n");

	CheckIndexBuffer9(this);
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

STDMETHODIMP_(::D3DRESOURCETYPE) D3D9Wrapper::IDirect3DIndexBuffer9::GetType(THIS)
{
	LogDebug("IDirect3DIndexBuffer9::GetType called\n");

	CheckIndexBuffer9(this);
	::D3DRESOURCETYPE hr = GetD3DIndexBuffer9()->GetType();
	LogDebug("  returns ResourceType=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DIndexBuffer9::Lock(THIS_ UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags)
{
	LogDebug("IDirect3DIndexBuffer9::Lock called with OffsetToLock=%d, SizeToLock=%d, Flags=%x\n", OffsetToLock, SizeToLock, Flags);

	::IDirect3DIndexBuffer9 *baseIndexBuffer = GetD3DIndexBuffer9();

	if (!baseIndexBuffer)
	{
		if (!gLogDebug) LogInfo("IDirect3DVertexBuffer9::Lock called\n");
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
	hackerDevice->FrameAnalysisLog("Lock(pResource:0x%p, OffsetToLock:%u, SizeToLock:%u, ppbData:0x%p, LockFlags:%u)",
		baseIndexBuffer, OffsetToLock, SizeToLock, ppbData, Flags);
	hackerDevice->FrameAnalysisLogResourceHash(baseIndexBuffer);
	HRESULT hr = GetD3DIndexBuffer9()->Lock(OffsetToLock, SizeToLock, ppbData, Flags);
	hackerDevice->TrackAndDivertLock<D3D9Wrapper::IDirect3DIndexBuffer9, ::D3DINDEXBUFFER_DESC>(hr, this, SizeToLock, ppbData, Flags);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DIndexBuffer9::Unlock(THIS)
{
	LogDebug("IDirect3DIndexBuffer9::Unlock called\n");

	::IDirect3DIndexBuffer9 *baseIndexBuffer = GetD3DIndexBuffer9();

	if (!baseIndexBuffer)
	{
		if (!gLogDebug) LogInfo("IDirect3DIndexBuffer9::Unlock called\n");
		LogInfo("  postponing call because vertex buffer was not created yet.\n");

		return S_OK;
	}

	CheckIndexBuffer9(this);
	hackerDevice->FrameAnalysisLog("Unlock(pResource:0x%p)",
		baseIndexBuffer);
	hackerDevice->FrameAnalysisLogResourceHash(baseIndexBuffer);

	hackerDevice->TrackAndDivertUnlock(this);
	HRESULT hr = GetD3DIndexBuffer9()->Unlock();
	LogDebug("  returns result=%x\n", hr);

	if (G->analyse_frame)
		hackerDevice->FrameAnalysisAfterUnlock(baseIndexBuffer);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DIndexBuffer9::GetDesc(THIS_ ::D3DINDEXBUFFER_DESC *pDesc)
{
	LogDebug("IDirect3DIndexBuffer9::GetDesc called\n");

	CheckIndexBuffer9(this);
	return GetD3DIndexBuffer9()->GetDesc(pDesc);
}
