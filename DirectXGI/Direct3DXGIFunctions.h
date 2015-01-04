
D3D11Wrapper::IDXGIFactory::IDXGIFactory(D3D11Base::IDXGIFactory *pFactory)
    : D3D11Wrapper::IDirect3DUnknown((IUnknown*) pFactory)
{
    m_pFactory = pFactory;
}
D3D11Wrapper::IDXGIFactory1::IDXGIFactory1(D3D11Base::IDXGIFactory1 *pFactory)
	: D3D11Wrapper::IDXGIFactory(pFactory)
{
}
D3D11Wrapper::IDXGIFactory2::IDXGIFactory2(D3D11Base::IDXGIFactory2 *pFactory)
	: D3D11Wrapper::IDXGIFactory1(pFactory)
{
}

D3D11Wrapper::IDXGIFactory* D3D11Wrapper::IDXGIFactory::GetDirectFactory(D3D11Base::IDXGIFactory *pOrig)
{
    D3D11Wrapper::IDXGIFactory* p = (D3D11Wrapper::IDXGIFactory*) m_List.GetDataPtr(pOrig);
    if (!p)
    {
        p = new D3D11Wrapper::IDXGIFactory(pOrig);
        if (pOrig) m_List.AddMember(pOrig,p);
    }
    return p;
}
D3D11Wrapper::IDXGIFactory1* D3D11Wrapper::IDXGIFactory1::GetDirectFactory(D3D11Base::IDXGIFactory1 *pOrig)
{
    D3D11Wrapper::IDXGIFactory1* p = (D3D11Wrapper::IDXGIFactory1*) m_List.GetDataPtr(pOrig);
    if (!p)
    {
        p = new D3D11Wrapper::IDXGIFactory1(pOrig);
        if (pOrig) m_List.AddMember(pOrig,p);
    }
    return p;
}
D3D11Wrapper::IDXGIFactory2* D3D11Wrapper::IDXGIFactory2::GetDirectFactory(D3D11Base::IDXGIFactory2 *pOrig)
{
    D3D11Wrapper::IDXGIFactory2* p = (D3D11Wrapper::IDXGIFactory2*) m_List.GetDataPtr(pOrig);
    if (!p)
    {
        p = new D3D11Wrapper::IDXGIFactory2(pOrig);
        if (pOrig) m_List.AddMember(pOrig,p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D11Wrapper::IDXGIFactory::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D11Wrapper::IDXGIFactory::Release(THIS)
{
	LogInfo("IDXGIFactory::Release handle=%p, counter=%d, this=%p\n", m_pUnk, m_ulRef, this);
	//LogInfo("  ignoring call\n");
	
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogInfo("  internal counter = %d\n", ulRef);
	
	--m_ulRef;
    if (ulRef == 0)
    {
		LogInfo("  deleting self\n");
		
        if (m_pUnk) m_List.DeleteMember(m_pUnk); m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP D3D11Wrapper::IDXGIFactory::EnumAdapters(THIS_
            /* [in] */ UINT Adapter,
            /* [annotation][out] */ 
            __out IDXGIAdapter1 **ppAdapter)
{
	LogInfo("IDXGIFactory::EnumAdapters adapter %d requested\n", Adapter);
	LogInfo("  routing call to IDXGIFactory1::EnumAdapters1\n");
	

	D3D11Wrapper::IDXGIFactory1 *factory1 = (D3D11Wrapper::IDXGIFactory1*)this;
	return factory1->EnumAdapters1(Adapter, ppAdapter);
	/*
	D3D11Base::IDXGIAdapter *origAdapter;
	HRESULT ret = m_pFactory->EnumAdapters(Adapter, &origAdapter);
	if (ret == S_OK)
	{
		if (LogFile)
		{
			char instance[MAX_PATH];
			D3D11Base::DXGI_ADAPTER_DESC desc;
			if (origAdapter->GetDesc(&desc) == S_OK)
			{
				wcstombs(instance, desc.Description, MAX_PATH);
				LogInfo(" Returning adapter: %s, sysmem=%d, vidmem=%d\n", instance, desc.DedicatedSystemMemory, desc.DedicatedVideoMemory);
			}
		}
		*ppAdapter = D3D11Wrapper::IDXGIAdapter::GetDirectAdapter(origAdapter);
	}
	return ret;
	*/
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory::MakeWindowAssociation(THIS_
            HWND WindowHandle,
            UINT Flags)
{
	if (LogFile)
	{
		LogInfo("IDXGIFactory::MakeWindowAssociation called with WindowHandle = %p, Flags = %x\n", WindowHandle, Flags);
		if (Flags) LogInfo("  Flags =");
		if (Flags & DXGI_MWA_NO_WINDOW_CHANGES) LogInfo(" DXGI_MWA_NO_WINDOW_CHANGES(no monitoring)");
		if (Flags & DXGI_MWA_NO_ALT_ENTER) LogInfo(" DXGI_MWA_NO_ALT_ENTER");
		if (Flags & DXGI_MWA_NO_PRINT_SCREEN) LogInfo(" DXGI_MWA_NO_PRINT_SCREEN");
		if (Flags) LogInfo("\n");
	}

	if (gAllowWindowCommands && Flags)
	{
		LogInfo("  overriding Flags to allow all window commands\n");
		
		Flags = 0;
	}
	HRESULT hr = m_pFactory->MakeWindowAssociation(WindowHandle, Flags);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory::GetWindowAssociation(THIS_
            /* [annotation][out] */ 
            __out  HWND *pWindowHandle)
{
	LogInfo("IDXGIFactory::GetWindowAssociation called\n");
	
	return m_pFactory->GetWindowAssociation(pWindowHandle);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory::CreateSwapChain(THIS_
            /* [annotation][in] */ 
            __in  IUnknown *pDevice,
            /* [annotation][in] */ 
            __in  D3D11Base::DXGI_SWAP_CHAIN_DESC *pDesc,
            /* [annotation][out] */ 
            __out  IDXGISwapChain **ppSwapChain)
{
	LogInfo("IDXGIFactory::CreateSwapChain called with parameters\n");
	LogInfo("  Device = %p\n", pDevice);
	if (pDesc) LogInfo("  Windowed = %d\n", pDesc->Windowed);
	if (pDesc) LogInfo("  Width = %d\n", pDesc->BufferDesc.Width);
	if (pDesc) LogInfo("  Height = %d\n", pDesc->BufferDesc.Height);
	if (pDesc) LogInfo("  Refresh rate = %f\n", 
		(float) pDesc->BufferDesc.RefreshRate.Numerator / (float) pDesc->BufferDesc.RefreshRate.Denominator);

	if (pDesc && SCREEN_REFRESH >= 0)
	{
		pDesc->BufferDesc.RefreshRate.Numerator = SCREEN_REFRESH;
		pDesc->BufferDesc.RefreshRate.Denominator = 1;
	}
	if (pDesc && SCREEN_WIDTH >= 0) pDesc->BufferDesc.Width = SCREEN_WIDTH;
	if (pDesc && SCREEN_HEIGHT >= 0) pDesc->BufferDesc.Height = SCREEN_HEIGHT;
	if (pDesc && SCREEN_FULLSCREEN >= 0) pDesc->Windowed = !SCREEN_FULLSCREEN;

	D3D11Base::IDXGISwapChain *origSwapChain;
	IUnknown *realDevice = ReplaceDevice(pDevice);
	HRESULT hr = m_pFactory->CreateSwapChain(realDevice, pDesc, &origSwapChain);
	LogInfo("  return value = %x\n", hr);

	// This call can return 0x087A0001, which is DXGI_STATUS_OCCLUDED.  That means that the window
	// can be occluded when we return from creating the real swap chain.  
	// The check below was looking ONLY for S_OK, and that would lead to it skipping the sub-block
	// which set up ppSwapChain and returned it.  So, ppSwapChain==NULL and it would crash, sometimes.
	// There are other legitimate DXGI_STATUS results, so checking for SUCCEEDED is the correct way.
	// Same bug fix is applied for the other CreateSwapChain* variants.

	if (SUCCEEDED(hr))
	{
		*ppSwapChain = D3D11Wrapper::IDXGISwapChain::GetDirectSwapChain(origSwapChain);
		if ((*ppSwapChain)->m_WrappedDevice) (*ppSwapChain)->m_WrappedDevice->Release();
		(*ppSwapChain)->m_WrappedDevice = pDevice; pDevice->AddRef();
		(*ppSwapChain)->m_RealDevice = realDevice;
		if (pDesc) SendScreenResolution(pDevice, pDesc->BufferDesc.Width, pDesc->BufferDesc.Height);
	}
	
	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIFactory2::CreateSwapChainForHwnd(THIS_
            /* [annotation][in] */ 
            _In_  IUnknown *pDevice,
            /* [annotation][in] */ 
            _In_  HWND hWnd,
            /* [annotation][in] */ 
            _In_  D3D11Base::DXGI_SWAP_CHAIN_DESC1 *pDesc,
            /* [annotation][in] */ 
            _In_opt_  D3D11Base::DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pRestrictToOutput,
            /* [annotation][out] */ 
            _Out_  IDXGISwapChain1 **ppSwapChain)
{
	LogInfo("IDXGIFactory2::CreateSwapChainForHwnd called with parameters\n");
	LogInfo("  Device = %p\n", pDevice);
	LogInfo("  HWND = %x\n", hWnd);
	if (pDesc) LogInfo("  Stereo = %d\n", pDesc->Stereo);
	if (pDesc) LogInfo("  Width = %d\n", pDesc->Width);
	if (pDesc) LogInfo("  Height = %d\n", pDesc->Height);
	if (pFullscreenDesc) LogInfo("  Refresh rate = %f\n", 
		(float) pFullscreenDesc->RefreshRate.Numerator / (float) pFullscreenDesc->RefreshRate.Denominator);
	if (pFullscreenDesc) LogInfo("  Windowed = %d\n", pFullscreenDesc->Windowed);

	if (pFullscreenDesc && SCREEN_REFRESH >= 0)
	{
		pFullscreenDesc->RefreshRate.Numerator = SCREEN_REFRESH;
		pFullscreenDesc->RefreshRate.Denominator = 1;
	}
	if (pDesc && SCREEN_WIDTH >= 0) pDesc->Width = SCREEN_WIDTH;
	if (pDesc && SCREEN_HEIGHT >= 0) pDesc->Height = SCREEN_HEIGHT;
	if (pFullscreenDesc && SCREEN_FULLSCREEN >= 0) pFullscreenDesc->Windowed = !SCREEN_FULLSCREEN;

	D3D11Base::IDXGISwapChain1 *origSwapChain;
	IUnknown *realDevice = ReplaceDevice(pDevice);
	HRESULT hr = -1;
	if (pRestrictToOutput)
		hr = GetFactory2()->CreateSwapChainForHwnd(realDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput->m_pOutput, &origSwapChain);
	LogInfo("  return value = %x\n", hr);

	if (SUCCEEDED(hr))
	{
		*ppSwapChain = D3D11Wrapper::IDXGISwapChain1::GetDirectSwapChain(origSwapChain);
		if ((*ppSwapChain)->m_WrappedDevice) (*ppSwapChain)->m_WrappedDevice->Release();
		(*ppSwapChain)->m_WrappedDevice = pDevice; pDevice->AddRef();
		(*ppSwapChain)->m_RealDevice = realDevice;
		if (pDesc) SendScreenResolution(pDevice, pDesc->Width, pDesc->Height);
	}
	
	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIFactory2::CreateSwapChainForCoreWindow(THIS_
            /* [annotation][in] */ 
            _In_  IUnknown *pDevice,
            /* [annotation][in] */ 
            _In_  IUnknown *pWindow,
            /* [annotation][in] */ 
            _In_  D3D11Base::DXGI_SWAP_CHAIN_DESC1 *pDesc,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pRestrictToOutput,
            /* [annotation][out] */ 
            _Out_  IDXGISwapChain1 **ppSwapChain)
{
	LogInfo("IDXGIFactory2::CreateSwapChainForCoreWindow called with parameters\n");
	LogInfo("  Device = %x\n", pDevice);
	if (pDesc) LogInfo("  Width = %d\n", pDesc->Width);
	if (pDesc) LogInfo("  Height = %d\n", pDesc->Height);
	if (pDesc) LogInfo("  Stereo = %d\n", pDesc->Stereo);

	if (pDesc && SCREEN_WIDTH >= 0) pDesc->Width = SCREEN_WIDTH;
	if (pDesc && SCREEN_HEIGHT >= 0) pDesc->Height = SCREEN_HEIGHT;

	D3D11Base::IDXGISwapChain1 *origSwapChain;
	IUnknown *realDevice = ReplaceDevice(pDevice);
	HRESULT hr = -1;
	if (pRestrictToOutput)
		hr = GetFactory2()->CreateSwapChainForCoreWindow(realDevice, pWindow, pDesc, pRestrictToOutput->m_pOutput, &origSwapChain);
	LogInfo("  return value = %x\n", hr);

	if (SUCCEEDED(hr))
	{
		*ppSwapChain = D3D11Wrapper::IDXGISwapChain1::GetDirectSwapChain(origSwapChain);
		if ((*ppSwapChain)->m_WrappedDevice) (*ppSwapChain)->m_WrappedDevice->Release();
		(*ppSwapChain)->m_WrappedDevice = pDevice; pDevice->AddRef();
		(*ppSwapChain)->m_RealDevice = realDevice;
		if (pDesc) SendScreenResolution(pDevice, pDesc->Width, pDesc->Height);
	}

	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIFactory2::CreateSwapChainForComposition(THIS_
            /* [annotation][in] */ 
            _In_  IUnknown *pDevice,
            /* [annotation][in] */ 
            _In_  D3D11Base::DXGI_SWAP_CHAIN_DESC1 *pDesc,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pRestrictToOutput,
            /* [annotation][out] */ 
            _Outptr_  IDXGISwapChain1 **ppSwapChain)
{
	LogInfo("IDXGIFactory2::CreateSwapChainForComposition called with parameters\n");
	LogInfo("  Device = %x\n", pDevice);
	if (pDesc) LogInfo("  Width = %d\n", pDesc->Width);
	if (pDesc) LogInfo("  Height = %d\n", pDesc->Height);
	if (pDesc) LogInfo("  Stereo = %d\n", pDesc->Stereo);

	if (pDesc && SCREEN_WIDTH >= 0) pDesc->Width = SCREEN_WIDTH;
	if (pDesc && SCREEN_HEIGHT >= 0) pDesc->Height = SCREEN_HEIGHT;

	D3D11Base::IDXGISwapChain1 *origSwapChain;
	IUnknown *realDevice = ReplaceDevice(pDevice);
	HRESULT hr = -1;
	if (pRestrictToOutput)
		hr = GetFactory2()->CreateSwapChainForComposition(realDevice, pDesc, pRestrictToOutput->m_pOutput, &origSwapChain);
	LogInfo("  return value = %x\n", hr);

	if (SUCCEEDED(hr))
	{
		*ppSwapChain = D3D11Wrapper::IDXGISwapChain1::GetDirectSwapChain(origSwapChain);
		if ((*ppSwapChain)->m_WrappedDevice) (*ppSwapChain)->m_WrappedDevice->Release();
		(*ppSwapChain)->m_WrappedDevice = pDevice; pDevice->AddRef();
		(*ppSwapChain)->m_RealDevice = realDevice;
		if (pDesc) SendScreenResolution(pDevice, pDesc->Width, pDesc->Height);
	}

	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIFactory::CreateSoftwareAdapter(THIS_
            /* [in] */ HMODULE Module,
            /* [annotation][out] */ 
            __out  D3D11Base::IDXGIAdapter **ppAdapter)
{
	LogInfo("IDXGIFactory::CreateSoftwareAdapter called\n");
	
	return m_pFactory->CreateSoftwareAdapter(Module, ppAdapter);
}

STDMETHODIMP D3D11Wrapper::IDXGIFactory1::EnumAdapters1(THIS_
        /* [in] */ UINT Adapter,
        /* [annotation][out] */ 
        __out  IDXGIAdapter1 **ppAdapter)
{
	LogInfo("IDXGIFactory1::EnumAdapters1 called: adapter #%d requested\n", Adapter);
	
	D3D11Base::IDXGIAdapter1 *origAdapter;
	HRESULT ret = GetFactory1()->EnumAdapters1(Adapter, &origAdapter);

	if (ret == S_OK)
	{
		if (LogFile)
		{
			char instance[MAX_PATH];
			D3D11Base::DXGI_ADAPTER_DESC1 desc;
			if (origAdapter->GetDesc1(&desc) == S_OK)
			{
				wcstombs(instance, desc.Description, MAX_PATH);
				LogInfo("  returns adapter: %s, sysmem=%d, vidmem=%d, flags=%x\n", instance, desc.DedicatedSystemMemory, desc.DedicatedVideoMemory, desc.Flags);
			}
		}
		/*
		D3D11Base::IDXGIOutput *output;
		HRESULT h = S_OK;
		for (int i = 0; h == S_OK; ++i)
		{
			if ((h = (*ppAdapter)->EnumOutputs(i, &output)) == S_OK)
			{
				D3D11Base::DXGI_OUTPUT_DESC outputDesc;
				if (output->GetDesc(&outputDesc) == S_OK)
				{
					wcstombs(instance, outputDesc.DeviceName, MAX_PATH);
					LogInfo("  Output found: %s, desktop=%d\n", instance, outputDesc.AttachedToDesktop);
				}
				
				UINT num = 0;
				D3D11Base::DXGI_FORMAT format = D3D11Base::DXGI_FORMAT_R8G8B8A8_UNORM;
				UINT flags = DXGI_ENUM_MODES_INTERLACED | DXGI_ENUM_MODES_SCALING;
				if (output->GetDisplayModeList(format, flags, &num, 0) == S_OK)
				{
					D3D11Base::DXGI_MODE_DESC *pDescs = new D3D11Base::DXGI_MODE_DESC[num];
					if (output->GetDisplayModeList(format, flags, &num, pDescs) == S_OK)
					{
						for (int j=0; j < num; ++j)
						{
							LogInfo("   Mode found: width=%d, height=%d, refresh rate=%f\n", pDescs[j].Width, pDescs[j].Height, 
								(float) pDescs[j].RefreshRate.Numerator / (float) pDescs[j].RefreshRate.Denominator);
						}
					}
					delete pDescs;
				}
				
				output->Release();
			}
		}
		*/
		*ppAdapter = D3D11Wrapper::IDXGIAdapter1::GetDirectAdapter(origAdapter);
	}
	if (D3D11Wrapper::LogFile) fprintf(D3D11Wrapper::LogFile, "  returns result = %x, handle = %x, wrapper = %x\n", ret, origAdapter, *ppAdapter);
	
	return ret;
}
   
STDMETHODIMP_(BOOL) D3D11Wrapper::IDXGIFactory1::IsCurrent(THIS)
{
	LogInfo("IDXGIFactory1::IsCurrent called\n");
	
	return GetFactory1()->IsCurrent();
}

STDMETHODIMP D3D11Wrapper::IDXGIFactory::SetPrivateData(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [in] */ UINT DataSize,
            /* [annotation][in] */ 
            __in_bcount(DataSize)  const void *pData)
{
	LogInfo("IDXGIFactory::SetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	LogInfo("  DataSize = %d\n", DataSize);
	
	HRESULT hr = GetFactory()->SetPrivateData(Name, DataSize, pData);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory::SetPrivateDataInterface(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][in] */ 
            __in  const IUnknown *pUnknown)
{
	LogInfo("IDXGIFactory::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	
	HRESULT hr = GetFactory()->SetPrivateDataInterface(Name, pUnknown);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory::GetPrivateData(THIS_
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][out][in] */ 
            __inout  UINT *pDataSize,
            /* [annotation][out] */ 
            __out_bcount(*pDataSize)  void *pData)
{
	LogInfo("IDXGIFactory::GetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	
	HRESULT hr = GetFactory()->GetPrivateData(Name, pDataSize, pData);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory::GetParent(THIS_ 
            /* [annotation][in] */ 
            __in  REFIID riid,
            /* [annotation][retval][out] */ 
            __out  void **ppParent)
{
	LogInfo("IDXGIFactory::GetParent called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);
	
	HRESULT hr = GetFactory()->GetParent(riid, ppParent);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}

STDMETHODIMP_(BOOL) D3D11Wrapper::IDXGIFactory2::IsWindowedStereoEnabled(THIS)
{
	LogInfo("IDXGIFactory2::IsWindowedStereoEnabled called\n");
	BOOL ret = GetFactory2()->IsWindowedStereoEnabled();
	LogInfo("  returns %d\n", ret);
	return ret;
}
                 
STDMETHODIMP D3D11Wrapper::IDXGIFactory2::GetSharedResourceAdapterLuid(THIS_
            /* [annotation] */ 
            _In_  HANDLE hResource,
            /* [annotation] */ 
            _Out_  LUID *pLuid)
{
	LogInfo("IDXGIFactory2::GetSharedResourceAdapterLuid called\n");
	HRESULT ret = GetFactory2()->GetSharedResourceAdapterLuid(hResource, pLuid);
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory2::RegisterStereoStatusWindow(THIS_
            /* [annotation][in] */ 
            _In_  HWND WindowHandle,
            /* [annotation][in] */ 
            _In_  UINT wMsg,
            /* [annotation][out] */ 
            _Out_  DWORD *pdwCookie)
{
	LogInfo("IDXGIFactory2::RegisterStereoStatusWindow called\n");
	HRESULT ret = GetFactory2()->RegisterStereoStatusWindow(WindowHandle, wMsg, pdwCookie);
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory2::RegisterStereoStatusEvent(THIS_
            /* [annotation][in] */ 
            _In_  HANDLE hEvent,
            /* [annotation][out] */ 
            _Out_  DWORD *pdwCookie)
{
	LogInfo("IDXGIFactory2::RegisterStereoStatusEvent called\n");
	HRESULT ret = GetFactory2()->RegisterStereoStatusEvent(hEvent, pdwCookie);
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP_(void) D3D11Wrapper::IDXGIFactory2::UnregisterStereoStatus(THIS_
            /* [annotation][in] */ 
            _In_  DWORD dwCookie)
{
	LogInfo("IDXGIFactory2::UnregisterStereoStatus called\n");
	GetFactory2()->UnregisterStereoStatus(dwCookie);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory2::RegisterOcclusionStatusWindow(THIS_
            /* [annotation][in] */ 
            _In_  HWND WindowHandle,
            /* [annotation][in] */ 
            _In_  UINT wMsg,
            /* [annotation][out] */ 
            _Out_  DWORD *pdwCookie)
{
	LogInfo("IDXGIFactory2::RegisterOcclusionStatusWindow called\n");
	HRESULT ret = GetFactory2()->RegisterOcclusionStatusWindow(WindowHandle, wMsg, pdwCookie);
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory2::RegisterOcclusionStatusEvent(THIS_
            /* [annotation][in] */ 
            _In_  HANDLE hEvent,
            /* [annotation][out] */ 
            _Out_  DWORD *pdwCookie)
{
	LogInfo("IDXGIFactory2::RegisterOcclusionStatusEvent called\n");
	HRESULT ret = GetFactory2()->RegisterOcclusionStatusEvent(hEvent, pdwCookie);
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP_(void) D3D11Wrapper::IDXGIFactory2::UnregisterOcclusionStatus(THIS_
            /* [annotation][in] */ 
            _In_  DWORD dwCookie)
{
	LogInfo("IDXGIFactory2::UnregisterOcclusionStatus called\n");
	GetFactory2()->UnregisterOcclusionStatus(dwCookie);
}
        
D3D11Wrapper::IDXGIAdapter::IDXGIAdapter(D3D11Base::IDXGIAdapter *pAdapter)
    : D3D11Wrapper::IDirect3DUnknown((IUnknown*) pAdapter)
{
    m_pAdapter = pAdapter;
}

D3D11Wrapper::IDXGIAdapter* D3D11Wrapper::IDXGIAdapter::GetDirectAdapter(D3D11Base::IDXGIAdapter *pOrig)
{
    D3D11Wrapper::IDXGIAdapter* p = (D3D11Wrapper::IDXGIAdapter*) m_List.GetDataPtr(pOrig);
    if (!p)
    {
        p = new D3D11Wrapper::IDXGIAdapter(pOrig);
        if (pOrig) m_List.AddMember(pOrig,p);
    }
    return p;
}

D3D11Wrapper::IDXGIAdapter1* D3D11Wrapper::IDXGIAdapter1::GetDirectAdapter(D3D11Base::IDXGIAdapter1 *pOrig)
{
    D3D11Wrapper::IDXGIAdapter1* p = (D3D11Wrapper::IDXGIAdapter1*) m_List.GetDataPtr(pOrig);
    if (!p)
    {
        p = new D3D11Wrapper::IDXGIAdapter1(pOrig);
        if (pOrig) m_List.AddMember(pOrig,p);
    }
    return p;
}

STDMETHODIMP D3D11Wrapper::IDXGIAdapter1::GetDesc1(THIS_ 
	        /* [annotation][out] */ 
            __out  D3D11Base::DXGI_ADAPTER_DESC1 *pDesc)
{
	LogInfo("IDXGIAdapter1::GetDesc1 called\n");
	
	HRESULT hr = GetAdapter()->GetDesc1(pDesc);
	if (LogFile)
	{
		char s[MAX_PATH];
		if (hr == S_OK)
		{
			wcstombs(s, pDesc->Description, MAX_PATH);
			LogInfo("  returns adapter: %s, sysmem=%d, vidmem=%d, flags=%x\n", s, pDesc->DedicatedSystemMemory, pDesc->DedicatedVideoMemory, pDesc->Flags);
		}
	}
	return hr;
}

STDMETHODIMP_(ULONG) D3D11Wrapper::IDXGIAdapter::AddRef(THIS)
{
//	LogInfo("IDXGIAdapter::AddRef called\n");
//	
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D11Wrapper::IDXGIAdapter::Release(THIS)
{
	LogInfo("IDXGIAdapter::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	//LogInfo("  ignoring call\n");
	
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogInfo("  internal counter = %d\n", ulRef);
	
	--m_ulRef;
    if (ulRef == 0)
    {
		LogInfo("  deleting self\n");
		
        if (m_pUnk) m_List.DeleteMember(m_pUnk); m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP D3D11Wrapper::IDXGIAdapter::EnumOutputs(THIS_
            /* [in] */ UINT Output,
            /* [annotation][out][in] */ 
            __out IDXGIOutput **ppOutput)
{
	LogInfo("IDXGIAdapter::EnumOutputs called: output #%d requested\n", Output);
	
	HRESULT hr;
	D3D11Base::IDXGIOutput *pOrig;
	if ((hr = GetAdapter()->EnumOutputs(Output, &pOrig)) == S_OK)
	{
		*ppOutput = IDXGIOutput::GetDirectOutput(pOrig);
	}
	if (D3D11Wrapper::LogFile) fprintf(D3D11Wrapper::LogFile, "  returns result = %x, handle = %x, wrapper = %x\n", hr, pOrig, *ppOutput);

	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIAdapter::GetDesc(THIS_
            /* [annotation][out] */ 
            __out D3D11Base::DXGI_ADAPTER_DESC *pDesc)
{
	LogInfo("IDXGIAdapter::GetDesc called\n");
	
	HRESULT hr = GetAdapter()->GetDesc(pDesc);
	if (LogFile && hr == S_OK)
	{
		char s[MAX_PATH];
		wcstombs(s, pDesc->Description, MAX_PATH);
		LogInfo("  returns adapter: %s, sysmem=%d, vidmem=%d\n", s, pDesc->DedicatedSystemMemory, pDesc->DedicatedVideoMemory);
	}
	return hr;
}
       
STDMETHODIMP D3D11Wrapper::IDXGIAdapter::SetPrivateData(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [in] */ UINT DataSize,
            /* [annotation][in] */ 
            __in_bcount(DataSize)  const void *pData)
{
	LogInfo("IDXGIAdapter::SetPrivateData called with Name=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx, DataSize = %d\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7], DataSize);
	
	HRESULT hr = GetAdapter()->SetPrivateData(Name, DataSize, pData);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIAdapter::SetPrivateDataInterface(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][in] */ 
            __in  const IUnknown *pUnknown)
{
	LogInfo("IDXGIAdapter::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	
	return GetAdapter()->SetPrivateDataInterface(Name, pUnknown);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIAdapter::GetPrivateData(THIS_
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][out][in] */ 
            __inout  UINT *pDataSize,
            /* [annotation][out] */ 
            __out_bcount(*pDataSize)  void *pData)
{
	LogInfo("IDXGIAdapter::GetPrivateData called\n");
	
	return GetAdapter()->GetPrivateData(Name, pDataSize, pData);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIAdapter::GetParent(THIS_ 
            /* [annotation][in] */ 
            __in  REFIID riid,
            /* [annotation][retval][out] */ 
            __out  void **ppParent)
{
	IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x00 } };
	if (riid.Data1 == marker.Data1 && riid.Data2 == marker.Data2 && riid.Data3 == marker.Data3 && 
		riid.Data4[0] == marker.Data4[0] && riid.Data4[1] == marker.Data4[1] && riid.Data4[2] == marker.Data4[2] && riid.Data4[3] == marker.Data4[3] && 
		riid.Data4[4] == marker.Data4[4] && riid.Data4[5] == marker.Data4[5] && riid.Data4[6] == marker.Data4[6] && riid.Data4[7] == marker.Data4[7])
	{
		LogInfo("Callback from d3d11.dll wrapper: requesting real IDXGIAdapter handle.\n");
		LogInfo("  returning handle = %x\n", GetAdapter());
		
		*ppParent = GetAdapter();
		return 0x13bc7e32;
	}

	LogInfo("IDXGIAdapter::GetParent called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);
	
	HRESULT hr = GetAdapter()->GetParent(riid, ppParent);
	if (hr == S_OK)
	{
		ReplaceInterface(ppParent);
	}
	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGIAdapter::CheckInterfaceSupport(THIS_
            /* [annotation][in] */ 
            __in  REFGUID InterfaceName,
            /* [annotation][out] */ 
            __out  LARGE_INTEGER *pUMDVersion)
{
	LogInfo("IDXGIAdapter::CheckInterfaceSupport called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		InterfaceName.Data1, InterfaceName.Data2, InterfaceName.Data3, InterfaceName.Data4[0], InterfaceName.Data4[1], InterfaceName.Data4[2], InterfaceName.Data4[3], 
		InterfaceName.Data4[4], InterfaceName.Data4[5], InterfaceName.Data4[6], InterfaceName.Data4[7]);
	if (LogFile && InterfaceName.Data1 == 0x9b7e4c0f && InterfaceName.Data2 == 0x342c && InterfaceName.Data3 == 0x4106 && InterfaceName.Data4[0] == 0xa1 && 
		InterfaceName.Data4[1] == 0x9f && InterfaceName.Data4[2] == 0x4f && InterfaceName.Data4[3] == 0x27 && InterfaceName.Data4[4] == 0x04 && 
		InterfaceName.Data4[5] == 0xf6 && InterfaceName.Data4[6] == 0x89 && InterfaceName.Data4[7] == 0xf0)
		LogInfo("  9b7e4c0f-342c-4106-a19f-4f2704f689f0 = IID_ID3D10Device\n");
	
	HRESULT hr = GetAdapter()->CheckInterfaceSupport(InterfaceName, pUMDVersion);
	if (hr == S_OK && pUMDVersion) LogInfo("  UMDVersion high=%x, low=%x\n", pUMDVersion->HighPart, pUMDVersion->LowPart);
	LogInfo("  returns hr=%x\n", hr);
	
	return hr;
}

D3D11Wrapper::IDXGIOutput::IDXGIOutput(D3D11Base::IDXGIOutput *pOutput)
    : D3D11Wrapper::IDirect3DUnknown((IUnknown*) pOutput)
{
	m_pOutput = pOutput;
}

D3D11Wrapper::IDXGIOutput* D3D11Wrapper::IDXGIOutput::GetDirectOutput(D3D11Base::IDXGIOutput *pOrig)
{
    D3D11Wrapper::IDXGIOutput* p = (D3D11Wrapper::IDXGIOutput*) m_List.GetDataPtr(pOrig);
    if (!p)
    {
        p = new D3D11Wrapper::IDXGIOutput(pOrig);
        if (pOrig) m_List.AddMember(pOrig,p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D11Wrapper::IDXGIOutput::AddRef(THIS)
{
//	LogInfo("IDXGIOutput::AddRef called\n", m_pUnk);
//	
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D11Wrapper::IDXGIOutput::Release(THIS)
{
	LogInfo("IDXGIOutput::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogInfo("  internal counter = %d\n", ulRef);
	
	--m_ulRef;

    if (ulRef == 0)
    {
		LogInfo("  deleting self\n");
		
        if (m_pUnk) m_List.DeleteMember(m_pUnk); m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP D3D11Wrapper::IDXGIOutput::GetDesc(THIS_ 
        /* [annotation][out] */ 
        __out  D3D11Base::DXGI_OUTPUT_DESC *pDesc)
{
	LogInfo("IDXGIOutput::GetDesc called\n");
	
	HRESULT ret = GetOutput()->GetDesc(pDesc);
	if (LogFile)
	{
		char str[MAX_PATH];
		wcstombs(str, pDesc->DeviceName, MAX_PATH);
		LogInfo("  returned %s, desktop=%d\n", str, pDesc->AttachedToDesktop);
	}
	return ret;
}
 
static bool FilterRate(int rate)
{
	if (!FILTER_REFRESH[0]) return false;
	int i = 0;
	while (FILTER_REFRESH[i] && FILTER_REFRESH[i] != rate)
		++i;
	return FILTER_REFRESH[i] == 0;
}

STDMETHODIMP D3D11Wrapper::IDXGIOutput::GetDisplayModeList(THIS_ 
        /* [in] */ D3D11Base::DXGI_FORMAT EnumFormat,
        /* [in] */ UINT Flags,
        /* [annotation][out][in] */ 
        __inout  UINT *pNumModes,
        /* [annotation][out] */ 
        __out_ecount_part_opt(*pNumModes,*pNumModes)  D3D11Base::DXGI_MODE_DESC *pDesc)
{
	LogInfo("IDXGIOutput::GetDisplayModeList called\n");
	
	HRESULT ret = GetOutput()->GetDisplayModeList(EnumFormat, Flags, pNumModes, pDesc);
	if (ret == S_OK && pDesc)
	{
		for (UINT j=0; j < *pNumModes; ++j)
		{
			int rate = pDesc[j].RefreshRate.Numerator / pDesc[j].RefreshRate.Denominator;
			if (FilterRate(rate))
			{
				LogInfo("  Skipping mode: width=%d, height=%d, refresh rate=%f\n", pDesc[j].Width, pDesc[j].Height, 
					(float) pDesc[j].RefreshRate.Numerator / (float) pDesc[j].RefreshRate.Denominator);
				pDesc[j].Width = 8; pDesc[j].Height = 8;
			}
			else
			{
				LogInfo("  Mode detected: width=%d, height=%d, refresh rate=%f\n", pDesc[j].Width, pDesc[j].Height, 
					(float) pDesc[j].RefreshRate.Numerator / (float) pDesc[j].RefreshRate.Denominator);
			}
		}
	}

	return ret;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::FindClosestMatchingMode(THIS_ 
        /* [annotation][in] */ 
        __in  const D3D11Base::DXGI_MODE_DESC *pModeToMatch,
        /* [annotation][out] */ 
        __out  D3D11Base::DXGI_MODE_DESC *pClosestMatch,
        /* [annotation][in] */ 
        __in_opt  IUnknown *pConcernedDevice)
{
	if (pModeToMatch) LogInfo("IDXGIOutput::FindClosestMatchingMode called: width=%d, height=%d, refresh rate=%f\n", 
		pModeToMatch->Width, pModeToMatch->Height, (float) pModeToMatch->RefreshRate.Numerator / (float) pModeToMatch->RefreshRate.Denominator);
	
	HRESULT ret = GetOutput()->FindClosestMatchingMode(pModeToMatch, pClosestMatch, ReplaceDevice(pConcernedDevice));
	if (pClosestMatch && SCREEN_REFRESH >= 0)
	{
		pClosestMatch->RefreshRate.Numerator = SCREEN_REFRESH;
		pClosestMatch->RefreshRate.Denominator = 1;
	}
	if (pClosestMatch && SCREEN_WIDTH >= 0) pClosestMatch->Width = SCREEN_WIDTH;
	if (pClosestMatch && SCREEN_HEIGHT >= 0) pClosestMatch->Height = SCREEN_HEIGHT;
	if (pClosestMatch) LogInfo("  returning width=%d, height=%d, refresh rate=%f\n", 
		pClosestMatch->Width, pClosestMatch->Height, (float) pClosestMatch->RefreshRate.Numerator / (float) pClosestMatch->RefreshRate.Denominator);
	
	return ret;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::WaitForVBlank(THIS_ )
{
	LogInfo("IDXGIOutput::WaitForVBlank called\n");
	
	return GetOutput()->WaitForVBlank();
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::TakeOwnership(THIS_  
        /* [annotation][in] */ 
        __in  IUnknown *pDevice,
        BOOL Exclusive)
{
	LogInfo("IDXGIOutput::TakeOwnership called\n");
	
	return GetOutput()->TakeOwnership(ReplaceDevice(pDevice), Exclusive);
}
        
void STDMETHODCALLTYPE D3D11Wrapper::IDXGIOutput::ReleaseOwnership(void)
{
	LogInfo("IDXGIOutput::ReleaseOwnership called\n");
	
	return GetOutput()->ReleaseOwnership();
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::GetGammaControlCapabilities(THIS_  
        /* [annotation][out] */ 
        __out  D3D11Base::DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps)
{
	LogInfo("IDXGIOutput::GetGammaControlCapabilities called\n");
	
	return GetOutput()->GetGammaControlCapabilities(pGammaCaps);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::SetGammaControl(THIS_ 
        /* [annotation][in] */ 
        __in  const D3D11Base::DXGI_GAMMA_CONTROL *pArray)
{
	LogInfo("IDXGIOutput::SetGammaControl called\n");
	
	return GetOutput()->SetGammaControl(pArray);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::GetGammaControl(THIS_ 
        /* [annotation][out] */ 
        __out  D3D11Base::DXGI_GAMMA_CONTROL *pArray)
{
	LogInfo("IDXGIOutput::GetGammaControl called\n");
	
	return GetOutput()->GetGammaControl(pArray);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::SetDisplaySurface(THIS_ 
        /* [annotation][in] */ 
        __in  D3D11Base::IDXGISurface *pScanoutSurface)
{
	LogInfo("IDXGIOutput::SetDisplaySurface called\n");
	
	return GetOutput()->SetDisplaySurface(pScanoutSurface);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::GetDisplaySurfaceData(THIS_ 
        /* [annotation][in] */ 
        __in  D3D11Base::IDXGISurface *pDestination)
{
	LogInfo("IDXGIOutput::GetDisplaySurfaceData called\n");
	
	return GetOutput()->GetDisplaySurfaceData(pDestination);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::GetFrameStatistics(THIS_ 
        /* [annotation][out] */ 
        __out  D3D11Base::DXGI_FRAME_STATISTICS *pStats)
{
	LogInfo("IDXGIOutput::GetFrameStatistics called\n");
	
	return GetOutput()->GetFrameStatistics(pStats);
}
               
STDMETHODIMP D3D11Wrapper::IDXGIOutput::SetPrivateData(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [in] */ UINT DataSize,
            /* [annotation][in] */ 
            __in_bcount(DataSize)  const void *pData)
{
	LogInfo("IDXGIOutput::SetPrivateData called\n");
	
	return GetOutput()->SetPrivateData(Name, DataSize, pData);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::SetPrivateDataInterface(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][in] */ 
            __in  const IUnknown *pUnknown)
{
	LogInfo("IDXGIOutput::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	
	return GetOutput()->SetPrivateDataInterface(Name, pUnknown);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::GetPrivateData(THIS_
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][out][in] */ 
            __inout  UINT *pDataSize,
            /* [annotation][out] */ 
            __out_bcount(*pDataSize)  void *pData)
{
	LogInfo("IDXGIOutput::GetPrivateData called\n");
	
	return GetOutput()->GetPrivateData(Name, pDataSize, pData);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::GetParent(THIS_ 
            /* [annotation][in] */ 
            __in  REFIID riid,
            /* [annotation][retval][out] */ 
            __out  void **ppParent)
{
	LogInfo("IDXGIOutput::GetParent called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);
	
	HRESULT hr = GetOutput()->GetParent(riid, ppParent);
	if (hr == S_OK)
	{
		ReplaceInterface(ppParent);
	}
	LogInfo("  returns result = %x, handle = %x\n", hr, *ppParent);
	
	return hr;
}

D3D11Wrapper::IDXGISwapChain::IDXGISwapChain(D3D11Base::IDXGISwapChain *pSwapChain)
    : D3D11Wrapper::IDirect3DUnknown((IUnknown*) pSwapChain),
	m_pSwapChain(pSwapChain), m_WrappedDevice(0), m_RealDevice(0)
{
}

D3D11Wrapper::IDXGISwapChain* D3D11Wrapper::IDXGISwapChain::GetDirectSwapChain(D3D11Base::IDXGISwapChain *pOrig)
{
    D3D11Wrapper::IDXGISwapChain* p = (D3D11Wrapper::IDXGISwapChain*) m_List.GetDataPtr(pOrig);
    if (!p)
    {
        p = new D3D11Wrapper::IDXGISwapChain(pOrig);
        if (pOrig) m_List.AddMember(pOrig,p);
    }
    return p;
}

D3D11Wrapper::IDXGISwapChain1* D3D11Wrapper::IDXGISwapChain1::GetDirectSwapChain(D3D11Base::IDXGISwapChain1 *pOrig)
{
    D3D11Wrapper::IDXGISwapChain1* p = (D3D11Wrapper::IDXGISwapChain1*) m_List.GetDataPtr(pOrig);
    if (!p)
    {
        p = new D3D11Wrapper::IDXGISwapChain1(pOrig);
        if (pOrig) m_List.AddMember(pOrig,p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D11Wrapper::IDXGISwapChain::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D11Wrapper::IDXGISwapChain::Release(THIS)
{
	LogInfo("IDXGISwapChain::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogInfo("  internal counter = %d\n", ulRef);
	
	--m_ulRef;
    if (ulRef == 0)
    {
		LogInfo("  deleting self\n");
		
        if (m_pUnk) m_List.DeleteMember(m_pUnk); m_pUnk = 0;
		if (m_WrappedDevice) m_WrappedDevice->Release(); m_WrappedDevice = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP D3D11Wrapper::IDXGISwapChain::SetPrivateData(THIS_
            /* [annotation][in] */ 
            _In_  REFGUID Name,
            /* [in] */ UINT DataSize,
            /* [annotation][in] */ 
            _In_reads_bytes_(DataSize)  const void *pData)
{
	LogInfo("IDXGISwapChain::SetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	LogInfo("  DataSize = %d\n", DataSize);
	
	HRESULT hr = GetSwapChain()->SetPrivateData(Name, DataSize, pData);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::SetPrivateDataInterface(THIS_
            /* [annotation][in] */ 
            _In_  REFGUID Name,
            /* [annotation][in] */ 
            _In_  const IUnknown *pUnknown)
{
	LogInfo("IDXGISwapChain::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	
	HRESULT hr = GetSwapChain()->SetPrivateDataInterface(Name, pUnknown);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::GetPrivateData(THIS_
            /* [annotation][in] */ 
            _In_  REFGUID Name,
            /* [annotation][out][in] */ 
            _Inout_  UINT *pDataSize,
            /* [annotation][out] */ 
            _Out_writes_bytes_(*pDataSize)  void *pData)
{
	LogInfo("IDXGISwapChain::GetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	
	HRESULT hr = GetSwapChain()->GetPrivateData(Name, pDataSize, pData);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::GetParent(THIS_
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][retval][out] */ 
            _Out_  void **ppParent)
{
	LogInfo("IDXGISwapChain::GetParent called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);
	
	HRESULT hr = GetSwapChain()->GetParent(riid, ppParent);
	if (hr == S_OK)
	{
		ReplaceInterface(ppParent);
		// Check if parent is wrapped device.
		if (m_RealDevice == *ppParent)
		{
			LogInfo("  real IDXGIDevice %x replaced with wrapper %x\n", m_RealDevice, m_WrappedDevice);
			*ppParent = m_WrappedDevice;
		}
	}
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::GetDevice(THIS_
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][retval][out] */ 
            _Out_  void **ppDevice)
{
	LogInfo("IDXGISwapChain::GetDevice called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);

	// Get old device pointer.
	HRESULT hr = GetSwapChain()->GetDevice(riid, ppDevice);
	if (hr == S_OK)
	{
		// Create device wrapper. We assume the wrapper has already been created.
	}
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGISwapChain::Present(THIS_
            /* [in] */ UINT SyncInterval,
            /* [in] */ UINT Flags)
{
	/*
	LogInfo("IDXGISwapChain::Present called with\n");
	LogInfo("  SyncInterval = %d\n", SyncInterval);
	LogInfo("  Flags = %d\n", Flags);
	if (!m_WrappedDevice) LogInfo("  Warning: no parent wrapped device available!\n");
	*/
	HRESULT hr = GetSwapChain()->Present(SyncInterval, Flags);

	if (m_WrappedDevice)
	{
		// Forward call to device.
		//LogInfo("  forwarding Present call to device %x\n", m_WrappedDevice);
		const static IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x02 } };
		IUnknown *deviceIU = (IUnknown *)m_WrappedDevice;
		IUnknown *param = m_RealDevice;
		//LogInfo("D3D11Wrapper::IDXGISwapChain::Present calling m_WrappedDevice->QueryInterface\n"
		//								"   m_WrappedDevice: %s\n", typeid(*m_WrappedDevice).name());

		if (deviceIU->QueryInterface(marker, (void **)&param) == 0x13bc7e31)
		{
			//if (D3D11Wrapper::LogFile) fprintf(D3D11Wrapper::LogFile, "    forward was successful.\n");
			//
		}
	}

	//LogInfo("  returns %x\n", hr);
	//
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::GetBuffer(THIS_
            /* [in] */ UINT Buffer,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][out][in] */ 
            _Out_  void **ppSurface)
{
	HRESULT hr = GetSwapChain()->GetBuffer(Buffer, riid, ppSurface);
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::SetFullscreenState(THIS_
            /* [in] */ BOOL Fullscreen,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pTarget)
{
	LogInfo("IDXGISwapChain::SetFullscreenState called with\n");
	LogInfo("  Fullscreen = %d\n", Fullscreen);
	LogInfo("  Target = %x\n", pTarget);

	HRESULT hr;
	if (pTarget)	
		hr = GetSwapChain()->SetFullscreenState(Fullscreen, pTarget->m_pOutput);
	else
		hr = GetSwapChain()->SetFullscreenState(Fullscreen, 0);

	LogInfo("  returns %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::GetFullscreenState(THIS_
            /* [annotation][out] */ 
            _Out_opt_  BOOL *pFullscreen,
            /* [annotation][out] */ 
            _Out_opt_  IDXGIOutput **ppTarget)
{
	LogInfo("IDXGISwapChain::GetFullscreenState called\n");
	
	D3D11Base::IDXGIOutput *origOutput;
	HRESULT hr = GetSwapChain()->GetFullscreenState(pFullscreen, &origOutput);
	if (hr == S_OK)
	{
		*ppTarget = IDXGIOutput::GetDirectOutput(origOutput);
		if (pFullscreen) LogInfo("  returns Fullscreen = %d\n", *pFullscreen);
		if (ppTarget) LogInfo("  returns target IDXGIOutput = %x, wrapper = %x\n", origOutput, *ppTarget);
	}
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::GetDesc(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_SWAP_CHAIN_DESC *pDesc)
{
	LogInfo("IDXGISwapChain::GetDesc called\n");
	
	HRESULT hr = GetSwapChain()->GetDesc(pDesc);
	if (hr == S_OK)
	{
		if (pDesc) LogInfo("  returns Windowed = %d\n", pDesc->Windowed);
		if (pDesc) LogInfo("  returns Width = %d\n", pDesc->BufferDesc.Width);
		if (pDesc) LogInfo("  returns Height = %d\n", pDesc->BufferDesc.Height);
		if (pDesc) LogInfo("  returns Refresh rate = %f\n", 
			(float) pDesc->BufferDesc.RefreshRate.Numerator / (float) pDesc->BufferDesc.RefreshRate.Denominator);
	}
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::ResizeBuffers(THIS_
            /* [in] */ UINT BufferCount,
            /* [in] */ UINT Width,
            /* [in] */ UINT Height,
            /* [in] */ D3D11Base::DXGI_FORMAT NewFormat,
            /* [in] */ UINT SwapChainFlags)
{
	HRESULT hr = GetSwapChain()->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::ResizeTarget(THIS_
            /* [annotation][in] */ 
            _In_  const D3D11Base::DXGI_MODE_DESC *pNewTargetParameters)
{
	HRESULT hr = GetSwapChain()->ResizeTarget(pNewTargetParameters);
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::GetContainingOutput(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppOutput)
{
	LogInfo("IDXGISwapChain::GetContainingOutput called\n");
	
	D3D11Base::IDXGIOutput *origOutput;
	HRESULT hr = GetSwapChain()->GetContainingOutput(&origOutput);
	if (hr == S_OK)
	{
		*ppOutput = IDXGIOutput::GetDirectOutput(origOutput);
	}
	if (D3D11Wrapper::LogFile) fprintf(D3D11Wrapper::LogFile, "  returns result = %x, handle = %x, wrapper = %x\n", hr, origOutput, *ppOutput);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::GetFrameStatistics(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_FRAME_STATISTICS *pStats)
{
	LogInfo("IDXGISwapChain::GetFrameStatistics called\n");
	
	HRESULT hr = GetSwapChain()->GetFrameStatistics(pStats);
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::GetLastPresentCount(THIS_
            /* [annotation][out] */ 
            _Out_  UINT *pLastPresentCount)
{
	LogInfo("IDXGISwapChain::GetLastPresentCount called\n");
	
	HRESULT hr = GetSwapChain()->GetLastPresentCount(pLastPresentCount);
	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::GetDesc1(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_SWAP_CHAIN_DESC1 *pDesc)
{
	LogInfo("IDXGISwapChain1::GetDesc1 called\n");
	
	HRESULT hr = GetSwapChain1()->GetDesc1(pDesc);
	if (hr == S_OK)
	{
		if (pDesc) LogInfo("  returns Stereo = %d\n", pDesc->Stereo);
		if (pDesc) LogInfo("  returns Width = %d\n", pDesc->Width);
		if (pDesc) LogInfo("  returns Height = %d\n", pDesc->Height);
	}
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::GetFullscreenDesc(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc)
{
	LogInfo("IDXGISwapChain1::GetFullscreenDesc called\n");
	
	HRESULT hr = GetSwapChain1()->GetFullscreenDesc(pDesc);
	if (hr == S_OK)
	{
		if (pDesc) LogInfo("  returns Windowed = %d\n", pDesc->Windowed);
		if (pDesc) LogInfo("  returns Refresh rate = %f\n", 
			(float) pDesc->RefreshRate.Numerator / (float) pDesc->RefreshRate.Denominator);
	}
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::GetHwnd(THIS_
            /* [annotation][out] */ 
            _Out_  HWND *pHwnd)
{
	LogInfo("IDXGISwapChain1::GetHwnd called\n");
	
	HRESULT hr = GetSwapChain1()->GetHwnd(pHwnd);
	if (hr == S_OK && pHwnd) LogInfo("  returns Hwnd = %x\n", *pHwnd);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::GetCoreWindow(THIS_
            /* [annotation][in] */ 
            _In_  REFIID refiid,
            /* [annotation][out] */ 
            _Out_  void **ppUnk)
{
	LogInfo("IDXGISwapChain1::GetCoreWindow called\n");
	
	HRESULT hr = GetSwapChain1()->GetCoreWindow(refiid, ppUnk);
	if (hr == S_OK && ppUnk) LogInfo("  returns IUnknown = %x\n", *ppUnk);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::Present1(THIS_
            /* [in] */ UINT SyncInterval,
            /* [in] */ UINT PresentFlags,
            /* [annotation][in] */ 
            _In_  const D3D11Base::DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
	LogInfo("IDXGISwapChain1::Present1 called\n");
	LogInfo("  SyncInterval = %d\n", SyncInterval);
	LogInfo("  PresentFlags = %d\n", PresentFlags);
	
	HRESULT hr = GetSwapChain1()->Present1(SyncInterval, PresentFlags, pPresentParameters);

	if (m_WrappedDevice)
	{
		// Forward call to device.
		LogInfo("  forwarding Present call to device %x\n", m_WrappedDevice);

		const static IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x02 } };
		IUnknown *deviceIU = (IUnknown *)m_WrappedDevice;
		int param = 0;
		//LogInfo("D3D11Wrapper::IDXGISwapChain::Present1 calling m_WrappedDevice->QueryInterface\n"
		//								"   m_WrappedDevice: %s\n", typeid(*m_WrappedDevice).name());

		if (deviceIU->QueryInterface(marker, (void **)&param) == 0x13bc7e31)
		{
			if (D3D11Wrapper::LogFile) fprintf(D3D11Wrapper::LogFile, "    forward was successful.\n");
		}
	}

	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP_(BOOL) D3D11Wrapper::IDXGISwapChain1::IsTemporaryMonoSupported(THIS)
{
	LogInfo("IDXGISwapChain1::IsTemporaryMonoSupported called\n");
	
	BOOL ret = GetSwapChain1()->IsTemporaryMonoSupported();
	LogInfo("  returns %d\n", ret);
	
	return ret;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::GetRestrictToOutput(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppRestrictToOutput)
{
	LogInfo("IDXGISwapChain1::GetRestrictToOutput called\n");
	
	D3D11Base::IDXGIOutput *origOutput;
	HRESULT hr = GetSwapChain1()->GetRestrictToOutput(&origOutput);
	if (hr == S_OK)
	{
		*ppRestrictToOutput = IDXGIOutput::GetDirectOutput(origOutput);
	}
	if (D3D11Wrapper::LogFile) fprintf(D3D11Wrapper::LogFile, "  returns result = %x, handle = %x, wrapper = %x\n", hr, origOutput, *ppRestrictToOutput);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::SetBackgroundColor(THIS_
            /* [annotation][in] */ 
            _In_  const D3D11Base::DXGI_RGBA *pColor)
{
	LogInfo("IDXGISwapChain1::SetBackgroundColor called\n");
	
	HRESULT hr = GetSwapChain1()->SetBackgroundColor(pColor);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::GetBackgroundColor(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_RGBA *pColor)
{
	LogInfo("IDXGISwapChain1::GetBackgroundColor called\n");
	
	HRESULT hr = GetSwapChain1()->GetBackgroundColor(pColor);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::SetRotation(THIS_
            /* [annotation][in] */ 
            _In_  D3D11Base::DXGI_MODE_ROTATION Rotation)
{
	LogInfo("IDXGISwapChain1::SetRotation called\n");
	
	HRESULT hr = GetSwapChain1()->SetRotation(Rotation);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::GetRotation(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_MODE_ROTATION *pRotation)
{
	LogInfo("IDXGISwapChain1::GetRotation called\n");
	
	HRESULT hr = GetSwapChain1()->GetRotation(pRotation);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
