#include "HookedVertexShader.h"
#include "d3d9Wrapper.h"
void D3D9Wrapper::IDirect3DVertexShader9::HookVertexShader()
{
	m_pUnk = hook_vertex_shader(GetD3DVertexShader9(), (::IDirect3DVertexShader9*)this);
}
inline void D3D9Wrapper::IDirect3DVertexShader9::Delete()
{
	LogInfo("IDirect3DVertexShader9::Delete\n");
	LogInfo("  deleting self\n");
	D3D9Wrapper::IDirect3DShader9::Delete();
	if (m_pRealUnk) m_List.DeleteMember(m_pRealUnk);
	m_pUnk = 0;
	m_pRealUnk = 0;
	delete this;
}
D3D9Wrapper::IDirect3DVertexShader9::IDirect3DVertexShader9(::LPDIRECT3DVERTEXSHADER9 pVS, D3D9Wrapper::IDirect3DDevice9 *hackerDevice)
    : D3D9Wrapper::IDirect3DShader9((IUnknown*) pVS, hackerDevice, Vertex),
	bound(false),
	zero_d3d_ref_count(false)
{
	if ((G->enable_hooks >= EnableHooksDX9::ALL) && pVS) {
		this->HookVertexShader();
	}
}

D3D9Wrapper::IDirect3DVertexShader9* D3D9Wrapper::IDirect3DVertexShader9::GetDirect3DVertexShader9(::LPDIRECT3DVERTEXSHADER9 pVS, D3D9Wrapper::IDirect3DDevice9 *hackerDevice)
{
	D3D9Wrapper::IDirect3DVertexShader9* p = new D3D9Wrapper::IDirect3DVertexShader9(pVS, hackerDevice);
	if (pVS) m_List.AddMember(pVS, p);
    return p;
}

STDMETHODIMP D3D9Wrapper::IDirect3DVertexShader9::QueryInterface(THIS_ REFIID riid, void ** ppvObj)
{
	LogDebug("D3D9Wrapper::IDirect3DVertexShader9::QueryInterface called\n");// at 'this': %s\n", type_name_dx9((IUnknown*)this));
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
				zero_d3d_ref_count = false;
				LogInfo("  interface replaced with IDirect3DVertexShader9 wrapper.\n");
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

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DVertexShader9::AddRef(THIS)
{
	++m_ulRef;
	zero_d3d_ref_count = false;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DVertexShader9::Release(THIS)
{
	LogDebug("IDirect3DVertexShader9::Release handle=%p, counter=%d, this=%p\n", m_pUnk, m_ulRef, this);
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);

	--m_ulRef;

    if (ulRef == 0)
    {
		if (!gLogDebug) LogInfo("IDirect3DVertexShader9::Release handle=%p, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		zero_d3d_ref_count = true;
		if (!bound)
			Delete();
    }
    return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3DVertexShader9::GetDevice(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice)
{
	LogDebug("IDirect3DVertexShader9::GetDevice called\n");
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

STDMETHODIMP D3D9Wrapper::IDirect3DVertexShader9::GetFunction(THIS_ void *data,UINT* pSizeOfData)
{
	LogDebug("IDirect3DVertexShader9::GetFunction called\n");

	HRESULT hr = GetD3DVertexShader9()->GetFunction(data, pSizeOfData);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}
