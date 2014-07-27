
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
	if (LogFile) fprintf(LogFile, "IDXGIFactory::Release handle=%p, counter=%d, this=%p\n", m_pUnk, m_ulRef, this);
	//if (LogFile) fprintf(LogFile, "  ignoring call\n");
	
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

STDMETHODIMP D3D11Wrapper::IDXGIFactory::EnumAdapters(THIS_
            /* [in] */ UINT Adapter,
            /* [annotation][out] */ 
            __out IDXGIAdapter1 **ppAdapter)
{
	if (LogFile) fprintf(LogFile, "IDXGIFactory::EnumAdapters adapter %d requested\n", Adapter);
	if (LogFile) fprintf(LogFile, "  routing call to IDXGIFactory1::EnumAdapters1\n");
	

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
				fprintf(LogFile, " Returning adapter: %s, sysmem=%d, vidmem=%d\n", instance, desc.DedicatedSystemMemory, desc.DedicatedVideoMemory);
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
		fprintf(LogFile, "IDXGIFactory::MakeWindowAssociation called with WindowHandle = %p, Flags = %x\n", WindowHandle, Flags);
		if (Flags) fprintf(LogFile, "  Flags =");
		if (Flags & DXGI_MWA_NO_WINDOW_CHANGES) fprintf(LogFile, " DXGI_MWA_NO_WINDOW_CHANGES(no monitoring)");
		if (Flags & DXGI_MWA_NO_ALT_ENTER) fprintf(LogFile, " DXGI_MWA_NO_ALT_ENTER");
		if (Flags & DXGI_MWA_NO_PRINT_SCREEN) fprintf(LogFile, " DXGI_MWA_NO_PRINT_SCREEN");
		if (Flags) fprintf(LogFile, "\n");
	}

	if (gAllowWindowCommands && Flags)
	{
		if (LogFile) fprintf(LogFile, "  overriding Flags to allow all window commands\n");
		
		Flags = 0;
	}
	HRESULT hr = m_pFactory->MakeWindowAssociation(WindowHandle, Flags);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory::GetWindowAssociation(THIS_
            /* [annotation][out] */ 
            __out  HWND *pWindowHandle)
{
	if (LogFile) fprintf(LogFile, "IDXGIFactory::GetWindowAssociation called\n");
	
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
	if (LogFile) fprintf(LogFile, "IDXGIFactory::CreateSwapChain called with parameters\n");
	if (LogFile) fprintf(LogFile, "  Device = %p\n", pDevice);
	if (pDesc && LogFile) fprintf(LogFile, "  Windowed = %d\n", pDesc->Windowed);
	if (pDesc && LogFile) fprintf(LogFile, "  Width = %d\n", pDesc->BufferDesc.Width);
	if (pDesc && LogFile) fprintf(LogFile, "  Height = %d\n", pDesc->BufferDesc.Height);
	if (pDesc && LogFile) fprintf(LogFile, "  Refresh rate = %f\n", 
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
	if (LogFile) fprintf(LogFile, "  return value = %x\n", hr);

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
	if (LogFile) fprintf(LogFile, "IDXGIFactory2::CreateSwapChainForHwnd called with parameters\n");
	if (LogFile) fprintf(LogFile, "  Device = %p\n", pDevice);
	if (LogFile) fprintf(LogFile, "  HWND = %x\n", hWnd);
	if (pDesc && LogFile) fprintf(LogFile, "  Stereo = %d\n", pDesc->Stereo);
	if (pDesc && LogFile) fprintf(LogFile, "  Width = %d\n", pDesc->Width);
	if (pDesc && LogFile) fprintf(LogFile, "  Height = %d\n", pDesc->Height);
	if (pFullscreenDesc && LogFile) fprintf(LogFile, "  Refresh rate = %f\n", 
		(float) pFullscreenDesc->RefreshRate.Numerator / (float) pFullscreenDesc->RefreshRate.Denominator);
	if (pFullscreenDesc && LogFile) fprintf(LogFile, "  Windowed = %d\n", pFullscreenDesc->Windowed);

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
	if (LogFile) fprintf(LogFile, "  return value = %x\n", hr);

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
	if (LogFile) fprintf(LogFile, "IDXGIFactory2::CreateSwapChainForCoreWindow called with parameters\n");
	if (LogFile) fprintf(LogFile, "  Device = %x\n", pDevice);
	if (pDesc && LogFile) fprintf(LogFile, "  Width = %d\n", pDesc->Width);
	if (pDesc && LogFile) fprintf(LogFile, "  Height = %d\n", pDesc->Height);
	if (pDesc && LogFile) fprintf(LogFile, "  Stereo = %d\n", pDesc->Stereo);

	if (pDesc && SCREEN_WIDTH >= 0) pDesc->Width = SCREEN_WIDTH;
	if (pDesc && SCREEN_HEIGHT >= 0) pDesc->Height = SCREEN_HEIGHT;

	D3D11Base::IDXGISwapChain1 *origSwapChain;
	IUnknown *realDevice = ReplaceDevice(pDevice);
	HRESULT hr = -1;
	if (pRestrictToOutput)
		hr = GetFactory2()->CreateSwapChainForCoreWindow(realDevice, pWindow, pDesc, pRestrictToOutput->m_pOutput, &origSwapChain);
	if (LogFile) fprintf(LogFile, "  return value = %x\n", hr);

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
	if (LogFile) fprintf(LogFile, "IDXGIFactory2::CreateSwapChainForComposition called with parameters\n");
	if (LogFile) fprintf(LogFile, "  Device = %x\n", pDevice);
	if (pDesc && LogFile) fprintf(LogFile, "  Width = %d\n", pDesc->Width);
	if (pDesc && LogFile) fprintf(LogFile, "  Height = %d\n", pDesc->Height);
	if (pDesc && LogFile) fprintf(LogFile, "  Stereo = %d\n", pDesc->Stereo);

	if (pDesc && SCREEN_WIDTH >= 0) pDesc->Width = SCREEN_WIDTH;
	if (pDesc && SCREEN_HEIGHT >= 0) pDesc->Height = SCREEN_HEIGHT;

	D3D11Base::IDXGISwapChain1 *origSwapChain;
	IUnknown *realDevice = ReplaceDevice(pDevice);
	HRESULT hr = -1;
	if (pRestrictToOutput)
		hr = GetFactory2()->CreateSwapChainForComposition(realDevice, pDesc, pRestrictToOutput->m_pOutput, &origSwapChain);
	if (LogFile) fprintf(LogFile, "  return value = %x\n", hr);

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
	if (LogFile) fprintf(LogFile, "IDXGIFactory::CreateSoftwareAdapter called\n");
	
	return m_pFactory->CreateSoftwareAdapter(Module, ppAdapter);
}

STDMETHODIMP D3D11Wrapper::IDXGIFactory1::EnumAdapters1(THIS_
        /* [in] */ UINT Adapter,
        /* [annotation][out] */ 
        __out  IDXGIAdapter1 **ppAdapter)
{
	if (LogFile) fprintf(LogFile, "IDXGIFactory1::EnumAdapters1 called: adapter #%d requested\n", Adapter);
	
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
				fprintf(LogFile, "  returns adapter: %s, sysmem=%d, vidmem=%d, flags=%x\n", instance, desc.DedicatedSystemMemory, desc.DedicatedVideoMemory, desc.Flags);
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
					fprintf(LogFile, "  Output found: %s, desktop=%d\n", instance, outputDesc.AttachedToDesktop);
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
							fprintf(LogFile, "   Mode found: width=%d, height=%d, refresh rate=%f\n", pDescs[j].Width, pDescs[j].Height, 
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
	if (LogFile) fprintf(LogFile, "IDXGIFactory1::IsCurrent called\n");
	
	return GetFactory1()->IsCurrent();
}

STDMETHODIMP D3D11Wrapper::IDXGIFactory::SetPrivateData(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [in] */ UINT DataSize,
            /* [annotation][in] */ 
            __in_bcount(DataSize)  const void *pData)
{
	if (LogFile) fprintf(LogFile, "IDXGIFactory::SetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	if (LogFile) fprintf(LogFile, "  DataSize = %d\n", DataSize);
	
	HRESULT hr = GetFactory()->SetPrivateData(Name, DataSize, pData);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory::SetPrivateDataInterface(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][in] */ 
            __in  const IUnknown *pUnknown)
{
	if (LogFile) fprintf(LogFile, "IDXGIFactory::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	
	HRESULT hr = GetFactory()->SetPrivateDataInterface(Name, pUnknown);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
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
	if (LogFile) fprintf(LogFile, "IDXGIFactory::GetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	
	HRESULT hr = GetFactory()->GetPrivateData(Name, pDataSize, pData);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory::GetParent(THIS_ 
            /* [annotation][in] */ 
            __in  REFIID riid,
            /* [annotation][retval][out] */ 
            __out  void **ppParent)
{
	if (LogFile) fprintf(LogFile, "IDXGIFactory::GetParent called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);
	
	HRESULT hr = GetFactory()->GetParent(riid, ppParent);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}

STDMETHODIMP_(BOOL) D3D11Wrapper::IDXGIFactory2::IsWindowedStereoEnabled(THIS)
{
	if (LogFile) fprintf(LogFile, "IDXGIFactory2::IsWindowedStereoEnabled called\n");
	BOOL ret = GetFactory2()->IsWindowedStereoEnabled();
	if (LogFile) fprintf(LogFile, "  returns %d\n", ret);
	return ret;
}
                 
STDMETHODIMP D3D11Wrapper::IDXGIFactory2::GetSharedResourceAdapterLuid(THIS_
            /* [annotation] */ 
            _In_  HANDLE hResource,
            /* [annotation] */ 
            _Out_  LUID *pLuid)
{
	if (LogFile) fprintf(LogFile, "IDXGIFactory2::GetSharedResourceAdapterLuid called\n");
	HRESULT ret = GetFactory2()->GetSharedResourceAdapterLuid(hResource, pLuid);
	if (LogFile) fprintf(LogFile, "  returns %d\n", ret);
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
	if (LogFile) fprintf(LogFile, "IDXGIFactory2::RegisterStereoStatusWindow called\n");
	HRESULT ret = GetFactory2()->RegisterStereoStatusWindow(WindowHandle, wMsg, pdwCookie);
	if (LogFile) fprintf(LogFile, "  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory2::RegisterStereoStatusEvent(THIS_
            /* [annotation][in] */ 
            _In_  HANDLE hEvent,
            /* [annotation][out] */ 
            _Out_  DWORD *pdwCookie)
{
	if (LogFile) fprintf(LogFile, "IDXGIFactory2::RegisterStereoStatusEvent called\n");
	HRESULT ret = GetFactory2()->RegisterStereoStatusEvent(hEvent, pdwCookie);
	if (LogFile) fprintf(LogFile, "  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP_(void) D3D11Wrapper::IDXGIFactory2::UnregisterStereoStatus(THIS_
            /* [annotation][in] */ 
            _In_  DWORD dwCookie)
{
	if (LogFile) fprintf(LogFile, "IDXGIFactory2::UnregisterStereoStatus called\n");
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
	if (LogFile) fprintf(LogFile, "IDXGIFactory2::RegisterOcclusionStatusWindow called\n");
	HRESULT ret = GetFactory2()->RegisterOcclusionStatusWindow(WindowHandle, wMsg, pdwCookie);
	if (LogFile) fprintf(LogFile, "  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIFactory2::RegisterOcclusionStatusEvent(THIS_
            /* [annotation][in] */ 
            _In_  HANDLE hEvent,
            /* [annotation][out] */ 
            _Out_  DWORD *pdwCookie)
{
	if (LogFile) fprintf(LogFile, "IDXGIFactory2::RegisterOcclusionStatusEvent called\n");
	HRESULT ret = GetFactory2()->RegisterOcclusionStatusEvent(hEvent, pdwCookie);
	if (LogFile) fprintf(LogFile, "  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP_(void) D3D11Wrapper::IDXGIFactory2::UnregisterOcclusionStatus(THIS_
            /* [annotation][in] */ 
            _In_  DWORD dwCookie)
{
	if (LogFile) fprintf(LogFile, "IDXGIFactory2::UnregisterOcclusionStatus called\n");
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
	if (LogFile) fprintf(LogFile, "IDXGIAdapter1::GetDesc1 called\n");
	
	HRESULT hr = GetAdapter()->GetDesc1(pDesc);
	if (LogFile)
	{
		char s[MAX_PATH];
		if (hr == S_OK)
		{
			wcstombs(s, pDesc->Description, MAX_PATH);
			fprintf(LogFile, "  returns adapter: %s, sysmem=%d, vidmem=%d, flags=%x\n", s, pDesc->DedicatedSystemMemory, pDesc->DedicatedVideoMemory, pDesc->Flags);
		}
	}
	return hr;
}

STDMETHODIMP_(ULONG) D3D11Wrapper::IDXGIAdapter::AddRef(THIS)
{
//	if (LogFile) fprintf(LogFile, "IDXGIAdapter::AddRef called\n");
//	
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D11Wrapper::IDXGIAdapter::Release(THIS)
{
	if (LogFile) fprintf(LogFile, "IDXGIAdapter::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	//if (LogFile) fprintf(LogFile, "  ignoring call\n");
	
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

STDMETHODIMP D3D11Wrapper::IDXGIAdapter::EnumOutputs(THIS_
            /* [in] */ UINT Output,
            /* [annotation][out][in] */ 
            __out IDXGIOutput **ppOutput)
{
	if (LogFile) fprintf(LogFile, "IDXGIAdapter::EnumOutputs called: output #%d requested\n", Output);
	
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
	if (LogFile) fprintf(LogFile, "IDXGIAdapter::GetDesc called\n");
	
	HRESULT hr = GetAdapter()->GetDesc(pDesc);
	if (LogFile && hr == S_OK)
	{
		char s[MAX_PATH];
		wcstombs(s, pDesc->Description, MAX_PATH);
		fprintf(LogFile, "  returns adapter: %s, sysmem=%d, vidmem=%d\n", s, pDesc->DedicatedSystemMemory, pDesc->DedicatedVideoMemory);
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
	if (LogFile) fprintf(LogFile, "IDXGIAdapter::SetPrivateData called with Name=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx, DataSize = %d\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7], DataSize);
	
	HRESULT hr = GetAdapter()->SetPrivateData(Name, DataSize, pData);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIAdapter::SetPrivateDataInterface(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][in] */ 
            __in  const IUnknown *pUnknown)
{
	if (LogFile) fprintf(LogFile, "IDXGIAdapter::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
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
	if (LogFile) fprintf(LogFile, "IDXGIAdapter::GetPrivateData called\n");
	
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
		if (LogFile) fprintf(LogFile, "Callback from d3d11.dll wrapper: requesting real IDXGIAdapter handle.\n");
		if (LogFile) fprintf(LogFile, "  returning handle = %x\n", GetAdapter());
		
		*ppParent = GetAdapter();
		return 0x13bc7e32;
	}

	if (LogFile) fprintf(LogFile, "IDXGIAdapter::GetParent called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
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
	if (LogFile) fprintf(LogFile, "IDXGIAdapter::CheckInterfaceSupport called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		InterfaceName.Data1, InterfaceName.Data2, InterfaceName.Data3, InterfaceName.Data4[0], InterfaceName.Data4[1], InterfaceName.Data4[2], InterfaceName.Data4[3], 
		InterfaceName.Data4[4], InterfaceName.Data4[5], InterfaceName.Data4[6], InterfaceName.Data4[7]);
	if (LogFile && InterfaceName.Data1 == 0x9b7e4c0f && InterfaceName.Data2 == 0x342c && InterfaceName.Data3 == 0x4106 && InterfaceName.Data4[0] == 0xa1 && 
		InterfaceName.Data4[1] == 0x9f && InterfaceName.Data4[2] == 0x4f && InterfaceName.Data4[3] == 0x27 && InterfaceName.Data4[4] == 0x04 && 
		InterfaceName.Data4[5] == 0xf6 && InterfaceName.Data4[6] == 0x89 && InterfaceName.Data4[7] == 0xf0)
		fprintf(LogFile, "  9b7e4c0f-342c-4106-a19f-4f2704f689f0 = IID_ID3D10Device\n");
	
	HRESULT hr = GetAdapter()->CheckInterfaceSupport(InterfaceName, pUMDVersion);
	if (LogFile && hr == S_OK && pUMDVersion) fprintf(LogFile, "  UMDVersion high=%x, low=%x\n", pUMDVersion->HighPart, pUMDVersion->LowPart);
	if (LogFile) fprintf(LogFile, "  returns hr=%x\n", hr);
	
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
//	if (LogFile) fprintf(LogFile, "IDXGIOutput::AddRef called\n", m_pUnk);
//	
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D11Wrapper::IDXGIOutput::Release(THIS)
{
	if (LogFile) fprintf(LogFile, "IDXGIOutput::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	
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

STDMETHODIMP D3D11Wrapper::IDXGIOutput::GetDesc(THIS_ 
        /* [annotation][out] */ 
        __out  D3D11Base::DXGI_OUTPUT_DESC *pDesc)
{
	if (LogFile) fprintf(LogFile, "IDXGIOutput::GetDesc called\n");
	
	HRESULT ret = GetOutput()->GetDesc(pDesc);
	if (LogFile)
	{
		char str[MAX_PATH];
		wcstombs(str, pDesc->DeviceName, MAX_PATH);
		fprintf(LogFile, "  returned %s, desktop=%d\n", str, pDesc->AttachedToDesktop);
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
	if (LogFile) fprintf(LogFile, "IDXGIOutput::GetDisplayModeList called\n");
	
	HRESULT ret = GetOutput()->GetDisplayModeList(EnumFormat, Flags, pNumModes, pDesc);
	if (ret == S_OK && pDesc)
	{
		for (UINT j=0; j < *pNumModes; ++j)
		{
			int rate = pDesc[j].RefreshRate.Numerator / pDesc[j].RefreshRate.Denominator;
			if (FilterRate(rate))
			{
				if (LogFile) fprintf(LogFile, "  Skipping mode: width=%d, height=%d, refresh rate=%f\n", pDesc[j].Width, pDesc[j].Height, 
					(float) pDesc[j].RefreshRate.Numerator / (float) pDesc[j].RefreshRate.Denominator);
				pDesc[j].Width = 8; pDesc[j].Height = 8;
			}
			else
			{
				if (LogFile) fprintf(LogFile, "  Mode detected: width=%d, height=%d, refresh rate=%f\n", pDesc[j].Width, pDesc[j].Height, 
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
	if (pModeToMatch && LogFile) fprintf(LogFile, "IDXGIOutput::FindClosestMatchingMode called: width=%d, height=%d, refresh rate=%f\n", 
		pModeToMatch->Width, pModeToMatch->Height, (float) pModeToMatch->RefreshRate.Numerator / (float) pModeToMatch->RefreshRate.Denominator);
	
	HRESULT ret = GetOutput()->FindClosestMatchingMode(pModeToMatch, pClosestMatch, ReplaceDevice(pConcernedDevice));
	if (pClosestMatch && SCREEN_REFRESH >= 0)
	{
		pClosestMatch->RefreshRate.Numerator = SCREEN_REFRESH;
		pClosestMatch->RefreshRate.Denominator = 1;
	}
	if (pClosestMatch && SCREEN_WIDTH >= 0) pClosestMatch->Width = SCREEN_WIDTH;
	if (pClosestMatch && SCREEN_HEIGHT >= 0) pClosestMatch->Height = SCREEN_HEIGHT;
	if (pClosestMatch && LogFile) fprintf(LogFile, "  returning width=%d, height=%d, refresh rate=%f\n", 
		pClosestMatch->Width, pClosestMatch->Height, (float) pClosestMatch->RefreshRate.Numerator / (float) pClosestMatch->RefreshRate.Denominator);
	
	return ret;
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::WaitForVBlank(THIS_ )
{
	if (LogFile) fprintf(LogFile, "IDXGIOutput::WaitForVBlank called\n");
	
	return GetOutput()->WaitForVBlank();
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::TakeOwnership(THIS_  
        /* [annotation][in] */ 
        __in  IUnknown *pDevice,
        BOOL Exclusive)
{
	if (LogFile) fprintf(LogFile, "IDXGIOutput::TakeOwnership called\n");
	
	return GetOutput()->TakeOwnership(ReplaceDevice(pDevice), Exclusive);
}
        
void STDMETHODCALLTYPE D3D11Wrapper::IDXGIOutput::ReleaseOwnership(void)
{
	if (LogFile) fprintf(LogFile, "IDXGIOutput::ReleaseOwnership called\n");
	
	return GetOutput()->ReleaseOwnership();
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::GetGammaControlCapabilities(THIS_  
        /* [annotation][out] */ 
        __out  D3D11Base::DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps)
{
	if (LogFile) fprintf(LogFile, "IDXGIOutput::GetGammaControlCapabilities called\n");
	
	return GetOutput()->GetGammaControlCapabilities(pGammaCaps);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::SetGammaControl(THIS_ 
        /* [annotation][in] */ 
        __in  const D3D11Base::DXGI_GAMMA_CONTROL *pArray)
{
	if (LogFile) fprintf(LogFile, "IDXGIOutput::SetGammaControl called\n");
	
	return GetOutput()->SetGammaControl(pArray);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::GetGammaControl(THIS_ 
        /* [annotation][out] */ 
        __out  D3D11Base::DXGI_GAMMA_CONTROL *pArray)
{
	if (LogFile) fprintf(LogFile, "IDXGIOutput::GetGammaControl called\n");
	
	return GetOutput()->GetGammaControl(pArray);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::SetDisplaySurface(THIS_ 
        /* [annotation][in] */ 
        __in  D3D11Base::IDXGISurface *pScanoutSurface)
{
	if (LogFile) fprintf(LogFile, "IDXGIOutput::SetDisplaySurface called\n");
	
	return GetOutput()->SetDisplaySurface(pScanoutSurface);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::GetDisplaySurfaceData(THIS_ 
        /* [annotation][in] */ 
        __in  D3D11Base::IDXGISurface *pDestination)
{
	if (LogFile) fprintf(LogFile, "IDXGIOutput::GetDisplaySurfaceData called\n");
	
	return GetOutput()->GetDisplaySurfaceData(pDestination);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::GetFrameStatistics(THIS_ 
        /* [annotation][out] */ 
        __out  D3D11Base::DXGI_FRAME_STATISTICS *pStats)
{
	if (LogFile) fprintf(LogFile, "IDXGIOutput::GetFrameStatistics called\n");
	
	return GetOutput()->GetFrameStatistics(pStats);
}
               
STDMETHODIMP D3D11Wrapper::IDXGIOutput::SetPrivateData(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [in] */ UINT DataSize,
            /* [annotation][in] */ 
            __in_bcount(DataSize)  const void *pData)
{
	if (LogFile) fprintf(LogFile, "IDXGIOutput::SetPrivateData called\n");
	
	return GetOutput()->SetPrivateData(Name, DataSize, pData);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::SetPrivateDataInterface(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][in] */ 
            __in  const IUnknown *pUnknown)
{
	if (LogFile) fprintf(LogFile, "IDXGIOutput::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
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
	if (LogFile) fprintf(LogFile, "IDXGIOutput::GetPrivateData called\n");
	
	return GetOutput()->GetPrivateData(Name, pDataSize, pData);
}
        
STDMETHODIMP D3D11Wrapper::IDXGIOutput::GetParent(THIS_ 
            /* [annotation][in] */ 
            __in  REFIID riid,
            /* [annotation][retval][out] */ 
            __out  void **ppParent)
{
	if (LogFile) fprintf(LogFile, "IDXGIOutput::GetParent called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);
	
	HRESULT hr = GetOutput()->GetParent(riid, ppParent);
	if (hr == S_OK)
	{
		ReplaceInterface(ppParent);
	}
	if (LogFile) fprintf(LogFile, "  returns result = %x, handle = %x\n", hr, *ppParent);
	
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
	if (LogFile) fprintf(LogFile, "IDXGISwapChain::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	if (LogFile) fprintf(LogFile, "  internal counter = %d\n", ulRef);
	
	--m_ulRef;
    if (ulRef == 0)
    {
		if (LogFile) fprintf(LogFile, "  deleting self\n");
		
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
	if (LogFile) fprintf(LogFile, "IDXGISwapChain::SetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	if (LogFile) fprintf(LogFile, "  DataSize = %d\n", DataSize);
	
	HRESULT hr = GetSwapChain()->SetPrivateData(Name, DataSize, pData);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::SetPrivateDataInterface(THIS_
            /* [annotation][in] */ 
            _In_  REFGUID Name,
            /* [annotation][in] */ 
            _In_  const IUnknown *pUnknown)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	
	HRESULT hr = GetSwapChain()->SetPrivateDataInterface(Name, pUnknown);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
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
	if (LogFile) fprintf(LogFile, "IDXGISwapChain::GetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	
	HRESULT hr = GetSwapChain()->GetPrivateData(Name, pDataSize, pData);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::GetParent(THIS_
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][retval][out] */ 
            _Out_  void **ppParent)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain::GetParent called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);
	
	HRESULT hr = GetSwapChain()->GetParent(riid, ppParent);
	if (hr == S_OK)
	{
		ReplaceInterface(ppParent);
		// Check if parent is wrapped device.
		if (m_RealDevice == *ppParent)
		{
			if (LogFile) fprintf(LogFile, "  real IDXGIDevice %x replaced with wrapper %x\n", m_RealDevice, m_WrappedDevice);
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
	if (LogFile) fprintf(LogFile, "IDXGISwapChain::GetDevice called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);

	// Get old device pointer.
	HRESULT hr = GetSwapChain()->GetDevice(riid, ppDevice);
	if (hr == S_OK)
	{
		// Create device wrapper. We assume the wrapper has already been created.
	}
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGISwapChain::Present(THIS_
            /* [in] */ UINT SyncInterval,
            /* [in] */ UINT Flags)
{
	/*
	if (LogFile) fprintf(LogFile, "IDXGISwapChain::Present called with\n");
	if (LogFile) fprintf(LogFile, "  SyncInterval = %d\n", SyncInterval);
	if (LogFile) fprintf(LogFile, "  Flags = %d\n", Flags);
	if (LogFile && !m_WrappedDevice) fprintf(LogFile, "  Warning: no parent wrapped device available!\n");
	*/
	HRESULT hr = GetSwapChain()->Present(SyncInterval, Flags);

	if (m_WrappedDevice)
	{
		// Forward call to device.
		//if (LogFile) fprintf(LogFile, "  forwarding Present call to device %x\n", m_WrappedDevice);
		const static IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x02 } };
		IUnknown *deviceIU = (IUnknown *)m_WrappedDevice;
		IUnknown *param = m_RealDevice;
		//if (LogFile) fprintf(LogFile, "D3D11Wrapper::IDXGISwapChain::Present calling m_WrappedDevice->QueryInterface\n"
		//								"   m_WrappedDevice: %s\n", typeid(*m_WrappedDevice).name());

		if (deviceIU->QueryInterface(marker, (void **)&param) == 0x13bc7e31)
		{
			//if (D3D11Wrapper::LogFile) fprintf(D3D11Wrapper::LogFile, "    forward was successful.\n");
			//
		}
	}

	//if (LogFile) fprintf(LogFile, "  returns %x\n", hr);
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
	if (LogFile) fprintf(LogFile, "IDXGISwapChain::SetFullscreenState called with\n");
	if (LogFile) fprintf(LogFile, "  Fullscreen = %d\n", Fullscreen);
	if (LogFile) fprintf(LogFile, "  Target = %x\n", pTarget);

	HRESULT hr;
	if (pTarget)	
		hr = GetSwapChain()->SetFullscreenState(Fullscreen, pTarget->m_pOutput);
	else
		hr = GetSwapChain()->SetFullscreenState(Fullscreen, 0);

	if (LogFile) fprintf(LogFile, "  returns %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::GetFullscreenState(THIS_
            /* [annotation][out] */ 
            _Out_opt_  BOOL *pFullscreen,
            /* [annotation][out] */ 
            _Out_opt_  IDXGIOutput **ppTarget)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain::GetFullscreenState called\n");
	
	D3D11Base::IDXGIOutput *origOutput;
	HRESULT hr = GetSwapChain()->GetFullscreenState(pFullscreen, &origOutput);
	if (hr == S_OK)
	{
		*ppTarget = IDXGIOutput::GetDirectOutput(origOutput);
		if (LogFile && pFullscreen) fprintf(LogFile, "  returns Fullscreen = %d\n", *pFullscreen);
		if (LogFile && ppTarget) fprintf(LogFile, "  returns target IDXGIOutput = %x, wrapper = %x\n", origOutput, *ppTarget);
	}
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::GetDesc(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_SWAP_CHAIN_DESC *pDesc)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain::GetDesc called\n");
	
	HRESULT hr = GetSwapChain()->GetDesc(pDesc);
	if (hr == S_OK)
	{
		if (LogFile && pDesc) fprintf(LogFile, "  returns Windowed = %d\n", pDesc->Windowed);
		if (LogFile && pDesc) fprintf(LogFile, "  returns Width = %d\n", pDesc->BufferDesc.Width);
		if (LogFile && pDesc) fprintf(LogFile, "  returns Height = %d\n", pDesc->BufferDesc.Height);
		if (LogFile && pDesc) fprintf(LogFile, "  returns Refresh rate = %f\n", 
			(float) pDesc->BufferDesc.RefreshRate.Numerator / (float) pDesc->BufferDesc.RefreshRate.Denominator);
	}
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
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
	if (LogFile) fprintf(LogFile, "IDXGISwapChain::GetContainingOutput called\n");
	
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
	if (LogFile) fprintf(LogFile, "IDXGISwapChain::GetFrameStatistics called\n");
	
	HRESULT hr = GetSwapChain()->GetFrameStatistics(pStats);
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain::GetLastPresentCount(THIS_
            /* [annotation][out] */ 
            _Out_  UINT *pLastPresentCount)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain::GetLastPresentCount called\n");
	
	HRESULT hr = GetSwapChain()->GetLastPresentCount(pLastPresentCount);
	return hr;
}

STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::GetDesc1(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_SWAP_CHAIN_DESC1 *pDesc)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain1::GetDesc1 called\n");
	
	HRESULT hr = GetSwapChain1()->GetDesc1(pDesc);
	if (hr == S_OK)
	{
		if (LogFile && pDesc) fprintf(LogFile, "  returns Stereo = %d\n", pDesc->Stereo);
		if (LogFile && pDesc) fprintf(LogFile, "  returns Width = %d\n", pDesc->Width);
		if (LogFile && pDesc) fprintf(LogFile, "  returns Height = %d\n", pDesc->Height);
	}
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::GetFullscreenDesc(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain1::GetFullscreenDesc called\n");
	
	HRESULT hr = GetSwapChain1()->GetFullscreenDesc(pDesc);
	if (hr == S_OK)
	{
		if (LogFile && pDesc) fprintf(LogFile, "  returns Windowed = %d\n", pDesc->Windowed);
		if (LogFile && pDesc) fprintf(LogFile, "  returns Refresh rate = %f\n", 
			(float) pDesc->RefreshRate.Numerator / (float) pDesc->RefreshRate.Denominator);
	}
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::GetHwnd(THIS_
            /* [annotation][out] */ 
            _Out_  HWND *pHwnd)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain1::GetHwnd called\n");
	
	HRESULT hr = GetSwapChain1()->GetHwnd(pHwnd);
	if (hr == S_OK && LogFile && pHwnd) fprintf(LogFile, "  returns Hwnd = %x\n", *pHwnd);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::GetCoreWindow(THIS_
            /* [annotation][in] */ 
            _In_  REFIID refiid,
            /* [annotation][out] */ 
            _Out_  void **ppUnk)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain1::GetCoreWindow called\n");
	
	HRESULT hr = GetSwapChain1()->GetCoreWindow(refiid, ppUnk);
	if (hr == S_OK && LogFile && ppUnk) fprintf(LogFile, "  returns IUnknown = %x\n", *ppUnk);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::Present1(THIS_
            /* [in] */ UINT SyncInterval,
            /* [in] */ UINT PresentFlags,
            /* [annotation][in] */ 
            _In_  const D3D11Base::DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain1::Present1 called\n");
	if (LogFile) fprintf(LogFile, "  SyncInterval = %d\n", SyncInterval);
	if (LogFile) fprintf(LogFile, "  PresentFlags = %d\n", PresentFlags);
	
	HRESULT hr = GetSwapChain1()->Present1(SyncInterval, PresentFlags, pPresentParameters);

	if (m_WrappedDevice)
	{
		// Forward call to device.
		if (LogFile) fprintf(LogFile, "  forwarding Present call to device %x\n", m_WrappedDevice);

		const static IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x02 } };
		IUnknown *deviceIU = (IUnknown *)m_WrappedDevice;
		int param = 0;
		//if (LogFile) fprintf(LogFile, "D3D11Wrapper::IDXGISwapChain::Present1 calling m_WrappedDevice->QueryInterface\n"
		//								"   m_WrappedDevice: %s\n", typeid(*m_WrappedDevice).name());

		if (deviceIU->QueryInterface(marker, (void **)&param) == 0x13bc7e31)
		{
			if (D3D11Wrapper::LogFile) fprintf(D3D11Wrapper::LogFile, "    forward was successful.\n");
		}
	}

	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP_(BOOL) D3D11Wrapper::IDXGISwapChain1::IsTemporaryMonoSupported(THIS)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain1::IsTemporaryMonoSupported called\n");
	
	BOOL ret = GetSwapChain1()->IsTemporaryMonoSupported();
	if (LogFile) fprintf(LogFile, "  returns %d\n", ret);
	
	return ret;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::GetRestrictToOutput(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppRestrictToOutput)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain1::GetRestrictToOutput called\n");
	
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
	if (LogFile) fprintf(LogFile, "IDXGISwapChain1::SetBackgroundColor called\n");
	
	HRESULT hr = GetSwapChain1()->SetBackgroundColor(pColor);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::GetBackgroundColor(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_RGBA *pColor)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain1::GetBackgroundColor called\n");
	
	HRESULT hr = GetSwapChain1()->GetBackgroundColor(pColor);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::SetRotation(THIS_
            /* [annotation][in] */ 
            _In_  D3D11Base::DXGI_MODE_ROTATION Rotation)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain1::SetRotation called\n");
	
	HRESULT hr = GetSwapChain1()->SetRotation(Rotation);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::IDXGISwapChain1::GetRotation(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::DXGI_MODE_ROTATION *pRotation)
{
	if (LogFile) fprintf(LogFile, "IDXGISwapChain1::GetRotation called\n");
	
	HRESULT hr = GetSwapChain1()->GetRotation(pRotation);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
