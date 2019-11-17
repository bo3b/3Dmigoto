#include "HookedQuery.h"
#include "d3d9Wrapper.h"
inline void D3D9Wrapper::IDirect3DQuery9::Delete()
{
	if (m_pRealUnk) m_List.DeleteMember(m_pRealUnk);
	m_pUnk = 0;
	m_pRealUnk = 0;
	delete this;
}
inline void D3D9Wrapper::IDirect3DQuery9::HookQuery()
{
	m_pUnk = hook_query(GetD3DQuery9(), (::IDirect3DQuery9*)this);
}
D3D9Wrapper::IDirect3DQuery9::IDirect3DQuery9(::LPDIRECT3DQUERY9 pQuery, D3D9Wrapper::IDirect3DDevice9 *hackerDevice)
    : D3D9Wrapper::IDirect3DUnknown((IUnknown*) pQuery),
	magic(0x7da43feb),
	hackerDevice(hackerDevice)
{
	if ((G->enable_hooks >= EnableHooksDX9::ALL) && pQuery) {
		this->HookQuery();
	}
}

D3D9Wrapper::IDirect3DQuery9* D3D9Wrapper::IDirect3DQuery9::GetDirect3DQuery9(::LPDIRECT3DQUERY9 pQuery, D3D9Wrapper::IDirect3DDevice9 *hackerDevice)
{
	D3D9Wrapper::IDirect3DQuery9* p = new D3D9Wrapper::IDirect3DQuery9(pQuery, hackerDevice);
	if (pQuery) m_List.AddMember(pQuery, p);
    return p;
}

STDMETHODIMP D3D9Wrapper::IDirect3DQuery9::QueryInterface(THIS_ REFIID riid, void ** ppvObj)
{
	LogDebug("D3D9Wrapper::IDirect3DQuery9::QueryInterface called\n");// at 'this': %s\n", type_name_dx9((IUnknown*)this));
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
				LogInfo("  interface replaced with IDirect3DQuery9 wrapper.\n");
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

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DQuery9::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DQuery9::Release(THIS)
{
	LogDebug("IDirect3DQuery9::Release handle=%p, counter=%d, this=%p\n", m_pUnk, m_ulRef, this);

    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);

	--m_ulRef;

    if (ulRef == 0)
    {
		if (!gLogDebug) LogInfo("IDirect3DQuery9::Release handle=%p, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		LogInfo("  deleting self\n");

		Delete();
    }
    return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3DQuery9::GetDevice(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice)
{
	LogDebug("IDirect3DQuery9::GetDevice called\n");
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

STDMETHODIMP_(::D3DQUERYTYPE) D3D9Wrapper::IDirect3DQuery9::GetType(THIS)
{
	LogDebug("IDirect3DQuery9::GetType called\n");

	::D3DQUERYTYPE hr = GetD3DQuery9()->GetType();
	LogDebug("  returns QueryType=%x\n", hr);

	return hr;
}

STDMETHODIMP_(DWORD) D3D9Wrapper::IDirect3DQuery9::GetDataSize(THIS)
{
	LogDebug("IDirect3DQuery9::GetDataSize called\n");

	DWORD hr = GetD3DQuery9()->GetDataSize();
	LogDebug("  returns size=%d\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DQuery9::Issue(THIS_ DWORD dwIssueFlags)
{
	LogDebug("IDirect3DQuery9::Issue called with IssueFlags=%x, this=%p\n", dwIssueFlags, this);
	hackerDevice->FrameAnalysisLog("IDirect3DQuery9::Issue Async:0x%p(dwIssueFlags:%x)", this, dwIssueFlags);
	hackerDevice->FrameAnalysisLogQuery(this->GetD3DQuery9());

	HRESULT hr = GetD3DQuery9()->Issue(dwIssueFlags);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DQuery9::GetData(THIS_ void* pData,DWORD dwSize,DWORD dwGetDataFlags)
{
	LogDebug("IDirect3DQuery9::GetData called with Data=%p, Size=%d, GetDataFlags=%x\n", pData, dwSize, dwGetDataFlags);

	HRESULT hr = GetD3DQuery9()->GetData(pData, dwSize, dwGetDataFlags);
	LogDebug("  returns result=%x\n", hr);

	hackerDevice->FrameAnalysisLog("GetData(Async:0x%p, pData:0x%p, dwSize:%x, dwGetDataFlags:%x) = %u",
		this, pData, dwSize, dwGetDataFlags);
	hackerDevice->FrameAnalysisLogQuery(this->GetD3DQuery9());
	if (SUCCEEDED(hr))
		hackerDevice->FrameAnalysisLogData(pData, dwSize);

	return hr;
}
