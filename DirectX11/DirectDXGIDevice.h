
D3D11Wrapper::IDXGIDevice2::IDXGIDevice2(D3D11Base::IDXGIDevice2 *pDevice)
	: D3D11Wrapper::IDirect3DUnknown((IUnknown*)pDevice)
{
}

D3D11Wrapper::IDXGIDevice2* D3D11Wrapper::IDXGIDevice2::GetDirectDevice2(D3D11Base::IDXGIDevice2 *pOrig)
{
	D3D11Wrapper::IDXGIDevice2* p = (D3D11Wrapper::IDXGIDevice2*) m_List.GetDataPtr(pOrig);
	if (!p)
	{
		p = new D3D11Wrapper::IDXGIDevice2(pOrig);
		if (pOrig) m_List.AddMember(pOrig, p);
	}
	return p;
}

STDMETHODIMP_(ULONG) D3D11Wrapper::IDXGIDevice2::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D11Wrapper::IDXGIDevice2::Release(THIS)
{
	if (LogFile) fprintf(LogFile, "IDXGIDevice2::Release handle=%p, counter=%d, this=%p\n", m_pUnk, m_ulRef, this);

	ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	if (LogFile) fprintf(LogFile, "  internal counter = %d\n", ulRef);

	--m_ulRef;

	if (ulRef == 0)
	{
		if (LogFile) fprintf(LogFile, "  deleting self\n");

		if (m_pUnk) m_List.DeleteMember(m_pUnk); m_pUnk = 0;
		delete this;
		return 0L;
	}
	return ulRef;
}

STDMETHODIMP D3D11Wrapper::IDXGIDevice2::GetPrivateData(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__inout  UINT *pDataSize,
	/* [annotation] */
	__out_bcount_opt(*pDataSize)  void *pData)
{
	return GetDevice2()->GetPrivateData(guid, pDataSize, pData);
}

STDMETHODIMP D3D11Wrapper::IDXGIDevice2::SetPrivateData(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__in  UINT DataSize,
	/* [annotation] */
	__in_bcount_opt(DataSize)  const void *pData)
{
	return GetDevice2()->SetPrivateData(guid, DataSize, pData);
}

STDMETHODIMP D3D11Wrapper::IDXGIDevice2::SetPrivateDataInterface(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__in_opt  const IUnknown *pData)
{
	if (LogFile) fprintf(LogFile, "IDXGIDevice2::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

	return GetDevice2()->SetPrivateDataInterface(guid, pData);
}

STDMETHODIMP D3D11Wrapper::IDXGIDevice2::GetParent(THIS_
	/* [annotation][in] */
	__in  REFIID riid,
	/* [annotation][retval][out] */
	__out  void **ppParent)
{
	if (LogFile) fprintf(LogFile, "IDXGIDevice2::GetParent called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);

	HRESULT hr = GetDevice2()->GetParent(riid, ppParent);
	if (hr == S_OK)
	{
		// Create Wrapper. We assume, the wrapper was already created using directX DLL wrapper.
	}
	if (LogFile) fprintf(LogFile, "  returns result = %x, handle = %p\n", hr, *ppParent);

	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIDevice2::GetAdapter(THIS_
	/* [annotation][out] */
	_Out_  D3D11Base::IDXGIAdapter **pAdapter)
{
	if (LogFile) fprintf(LogFile, "IDXGIDevice2::GetAdapter called.\n");

	HRESULT hr = GetDevice2()->GetAdapter(pAdapter);
	if (hr == S_OK)
	{
		D3D11Base::IDXGIAdapter *wrapper = (D3D11Base::IDXGIAdapter *) G->m_AdapterList.GetDataPtr(*pAdapter);
		if (wrapper)
		{
			if (LogFile) fprintf(LogFile, "  adapter replaced by wrapper. Original IDXGIAdapter = %p.\n", *pAdapter);

			*pAdapter = wrapper;
			wrapper->AddRef();
		}
	}
	if (LogFile) fprintf(LogFile, "  returns result = %x, handle = %p\n", hr, *pAdapter);

	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIDevice2::CreateSurface(THIS_
	/* [annotation][in] */
	_In_  const D3D11Base::DXGI_SURFACE_DESC *pDesc,
	/* [in] */ UINT NumSurfaces,
	/* [in] */ D3D11Base::DXGI_USAGE Usage,
	/* [annotation][in] */
	_In_opt_  const D3D11Base::DXGI_SHARED_RESOURCE *pSharedResource,
	/* [annotation][out] */
	_Out_  D3D11Base::IDXGISurface **ppSurface)
{
	if (LogFile) fprintf(LogFile, "IDXGIDevice2::CreateSurface called.\n");

	HRESULT hr = GetDevice2()->CreateSurface(pDesc, NumSurfaces, Usage, pSharedResource, ppSurface);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);

	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIDevice2::QueryResourceResidency(THIS_
	/* [annotation][size_is][in] */
	_In_reads_(NumResources)  IUnknown *const *ppResources,
	/* [annotation][size_is][out] */
	_Out_writes_(NumResources)  D3D11Base::DXGI_RESIDENCY *pResidencyStatus,
	/* [in] */ UINT NumResources)
{
	HRESULT hr = GetDevice2()->QueryResourceResidency(ppResources, pResidencyStatus, NumResources);
	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIDevice2::SetGPUThreadPriority(THIS_
	/* [in] */ INT Priority)
{
	HRESULT hr = GetDevice2()->SetGPUThreadPriority(Priority);
	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIDevice2::GetGPUThreadPriority(THIS_
	/* [annotation][retval][out] */
	_Out_  INT *pPriority)
{
	HRESULT hr = GetDevice2()->GetGPUThreadPriority(pPriority);
	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIDevice2::SetMaximumFrameLatency(THIS_
	/* [in] */ UINT MaxLatency)
{
	HRESULT hr = GetDevice2()->SetMaximumFrameLatency(MaxLatency);
	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIDevice2::GetMaximumFrameLatency(THIS_
	/* [annotation][out] */
	_Out_  UINT *pMaxLatency)
{
	HRESULT hr = GetDevice2()->GetMaximumFrameLatency(pMaxLatency);
	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIDevice2::OfferResources(THIS_
	/* [annotation][in] */
	_In_  UINT NumResources,
	/* [annotation][size_is][in] */
	_In_reads_(NumResources)  D3D11Base::IDXGIResource *const *ppResources,
	/* [annotation][in] */
	_In_  D3D11Base::DXGI_OFFER_RESOURCE_PRIORITY Priority)
{
	HRESULT hr = GetDevice2()->OfferResources(NumResources, ppResources, Priority);
	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIDevice2::ReclaimResources(THIS_
	/* [annotation][in] */
	_In_  UINT NumResources,
	/* [annotation][size_is][in] */
	_In_reads_(NumResources)  D3D11Base::IDXGIResource *const *ppResources,
	/* [annotation][size_is][out] */
	_Out_writes_all_opt_(NumResources)  BOOL *pDiscarded)
{
	HRESULT hr = GetDevice2()->ReclaimResources(NumResources, ppResources, pDiscarded);
	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIDevice2::EnqueueSetEvent(THIS_
	/* [annotation][in] */
	_In_  HANDLE hEvent)
{
	HRESULT hr = GetDevice2()->EnqueueSetEvent(hEvent);
	return hr;
}
