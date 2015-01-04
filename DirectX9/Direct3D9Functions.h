
D3D9Wrapper::IDirect3D9::IDirect3D9(D3D9Base::LPDIRECT3D9EX pD3D)
    : D3D9Wrapper::IDirect3D9Base((IUnknown*) pD3D)
{
}

D3D9Wrapper::IDirect3D9* D3D9Wrapper::IDirect3D9::GetDirect3D(D3D9Base::LPDIRECT3D9EX pD3D)
{
    D3D9Wrapper::IDirect3D9* p = (D3D9Wrapper::IDirect3D9*) m_List.GetDataPtr(pD3D);
    if (!p)
    {
        p = new D3D9Wrapper::IDirect3D9(pD3D);
		if (pD3D) m_List.AddMember(pD3D,p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3D9Base::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3D9Base::Release(THIS)
{
	LogDebug("ID3D9Device::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);

	ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);
	
	--m_ulRef;

    if (ulRef == 0)
    {
		if (!LogDebug) LogInfo("ID3D9Device::Release handle=%x, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		LogInfo("  deleting self\n");

		if (m_pUnk) m_List.DeleteMember(m_pUnk); 
		m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9Base::RegisterSoftwareDevice(THIS_ void* pInitializeFunction)
{
	LogInfo("IDirect3D9::RegisterSoftwareDevice called\n");
	
	return ((IDirect3D9*)m_pUnk)->RegisterSoftwareDevice(pInitializeFunction);
}

STDMETHODIMP_(UINT) D3D9Wrapper::IDirect3D9::GetAdapterCount(THIS)
{
	LogInfo("IDirect3D9::GetAdapterCount called\n");
	
	UINT ret = GetDirect3D9()->GetAdapterCount();
	LogInfo("  return value = %d\n", ret);
	
	return ret;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::GetAdapterIdentifier(THIS_ UINT Adapter,DWORD Flags,D3D9Base::D3DADAPTER_IDENTIFIER9* pIdentifier)
{
	LogInfo("IDirect3D9::GetAdapterIdentifier called\n");
	
	HRESULT ret = GetDirect3D9()->GetAdapterIdentifier(Adapter, Flags, pIdentifier);
	if (ret == S_OK && LogFile)
	{
		LogInfo("  returns driver=%s, description=%s, GDI=%s\n", pIdentifier->Driver, pIdentifier->Description, pIdentifier->DeviceName);
	}
	return ret;
}

STDMETHODIMP_(UINT) D3D9Wrapper::IDirect3D9::GetAdapterModeCount(THIS_ UINT Adapter,D3D9Base::D3DFORMAT Format)
{
	LogInfo("IDirect3D9::GetAdapterModeCount called\n");
	
	UINT ret = GetDirect3D9()->GetAdapterModeCount(Adapter, Format);
	LogInfo("  return value = %d\n", ret);
	
	return ret;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::EnumAdapterModes(THIS_ UINT Adapter,D3D9Base::D3DFORMAT Format,UINT Mode,D3D9Base::D3DDISPLAYMODE* pMode)
{
	LogInfo("IDirect3D9::EnumAdapterModes called: adapter #%d requested with mode #%d\n", Adapter, Mode);
	
	HRESULT hr = GetDirect3D9()->EnumAdapterModes(Adapter, Format, Mode, pMode);
	if (hr == S_OK)
	{
		LogInfo("  driver returned width=%d, height=%d, refresh rate=%d\n", pMode->Width, pMode->Height, pMode->RefreshRate);
	
		/*
		if (SCREEN_REFRESH >= 0 && pMode->RefreshRate != SCREEN_REFRESH) 
		{
			LogInfo("  video mode ignored because of mismatched refresh rate.\n");

			pMode->Width = pMode->Height = 0;
		}
		else if (SCREEN_WIDTH >= 0 && pMode->Width != SCREEN_WIDTH) 
		{
			LogInfo("  video mode ignored because of mismatched screen width.\n");

			pMode->Width = pMode->Height = 0;
		}
		else if (SCREEN_HEIGHT >= 0 && pMode->Height != SCREEN_HEIGHT) 
		{
			LogInfo("  video mode ignored because of mismatched screen height.\n");

			pMode->Width = pMode->Height = 0;
		}
		*/
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::GetAdapterDisplayMode(THIS_ UINT Adapter,D3D9Base::D3DDISPLAYMODE* pMode)
{
	LogInfo("IDirect3D9::GetAdapterDisplayMode called: adapter #%d\n", Adapter);
	
	HRESULT hr = GetDirect3D9()->GetAdapterDisplayMode(Adapter, pMode);
	if (hr == S_OK && pMode)
	{
		if (pMode->Width != SCREEN_WIDTH && SCREEN_WIDTH != -1)
		{
			LogInfo("  overriding Width %d with %d\n", pMode->Width, SCREEN_WIDTH);
			pMode->Width = SCREEN_WIDTH;
		}
		if (pMode->Height != SCREEN_HEIGHT && SCREEN_HEIGHT != -1)
		{
			LogInfo("  overriding Height %d with %d\n", pMode->Height, SCREEN_HEIGHT);
			pMode->Height = SCREEN_HEIGHT;
		}
		if (pMode->RefreshRate != SCREEN_REFRESH && SCREEN_REFRESH != -1)
		{
			LogInfo("  overriding RefreshRate %d with %d\n", pMode->RefreshRate, SCREEN_REFRESH);
			pMode->RefreshRate = SCREEN_REFRESH;
		}
		LogInfo("  returns result=%x, width=%d, height=%d, refresh rate=%d\n", hr, pMode->Width, pMode->Height, pMode->RefreshRate);
	}
	else if (LogFile)
	{
		LogInfo("  returns result=%x\n", hr);
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::CheckDeviceType(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DevType,D3D9Base::D3DFORMAT AdapterFormat,D3D9Base::D3DFORMAT BackBufferFormat,BOOL bWindowed)
{
	LogInfo("IDirect3D9::CheckDeviceType called with adapter=%d\n", Adapter);

	HRESULT hr = GetDirect3D9()->CheckDeviceType(Adapter, DevType, AdapterFormat, BackBufferFormat, bWindowed);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::CheckDeviceFormat(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DFORMAT AdapterFormat,DWORD Usage,D3D9Base::D3DRESOURCETYPE RType,D3D9Base::D3DFORMAT CheckFormat)
{
	LogInfo("IDirect3D9::CheckDeviceFormat called with adapter=%d\n", Adapter);

	HRESULT hr = GetDirect3D9()->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::CheckDeviceMultiSampleType(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DFORMAT SurfaceFormat,BOOL Windowed,D3D9Base::D3DMULTISAMPLE_TYPE MultiSampleType,DWORD* pQualityLevels)
{
	LogInfo("IDirect3D9::CheckDeviceMultiSampleType called: adapter #%d\n", Adapter);

	return GetDirect3D9()->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels);
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::CheckDepthStencilMatch(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DFORMAT AdapterFormat,D3D9Base::D3DFORMAT RenderTargetFormat,D3D9Base::D3DFORMAT DepthStencilFormat)
{
	LogInfo("IDirect3D9::CheckDepthStencilMatch called: adapter #%d\n", Adapter);

	HRESULT hr = GetDirect3D9()->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::CheckDeviceFormatConversion(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DFORMAT SourceFormat,D3D9Base::D3DFORMAT TargetFormat)
{
	LogInfo("IDirect3D9::CheckDeviceFormatConversion called: adapter #%d\n", Adapter);

	HRESULT hr = GetDirect3D9()->CheckDeviceFormatConversion(Adapter, DeviceType, SourceFormat, TargetFormat);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::GetDeviceCaps(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DCAPS9* pCaps)
{
	LogInfo("IDirect3D9::GetDeviceCaps called: adapter #%d\n", Adapter);

	HRESULT hr = GetDirect3D9()->GetDeviceCaps(Adapter, DeviceType, pCaps);
	LogInfo("  returns result = %x\n", hr);

	return hr;
}

STDMETHODIMP_(HMONITOR) D3D9Wrapper::IDirect3D9::GetAdapterMonitor(THIS_ UINT Adapter)
{
	LogInfo("IDirect3D9::GetAdapterMonitor called: adapter #%d\n", Adapter);

	return GetDirect3D9()->GetAdapterMonitor(Adapter);
}

DWORD WINAPI DeviceCreateThread(LPVOID lpParam) 
{
	D3D9Wrapper::IDirect3DDevice9* newDevice = (D3D9Wrapper::IDirect3DDevice9*)lpParam;
	CheckDevice(newDevice);
	return S_OK;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::CreateDeviceEx(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,HWND hFocusWindow,DWORD BehaviorFlags,D3D9Base::D3DPRESENT_PARAMETERS* pPresentationParameters,D3D9Base::D3DDISPLAYMODEEX* pFullscreenDisplayMode,D3D9Wrapper::IDirect3DDevice9** ppReturnedDeviceInterface)
{
	LogInfo("IDirect3D9::CreateDevice called with parameters FocusWindow = %x\n", hFocusWindow);

	if (pPresentationParameters)
	{
		LogInfo("  BackBufferWidth = %d\n", pPresentationParameters->BackBufferWidth);
		LogInfo("  BackBufferHeight = %d\n", pPresentationParameters->BackBufferHeight);
		LogInfo("  BackBufferFormat = %d\n", pPresentationParameters->BackBufferFormat);
		LogInfo("  BackBufferCount = %d\n", pPresentationParameters->BackBufferCount);
		LogInfo("  SwapEffect = %x\n", pPresentationParameters->SwapEffect);
		LogInfo("  Flags = %x\n", pPresentationParameters->Flags);
		LogInfo("  FullScreen_RefreshRateInHz = %d\n", pPresentationParameters->FullScreen_RefreshRateInHz);
		LogInfo("  PresentationInterval = %d\n", pPresentationParameters->PresentationInterval);
		LogInfo("  Windowed = %d\n", pPresentationParameters->Windowed);
		LogInfo("  EnableAutoDepthStencil = %d\n", pPresentationParameters->EnableAutoDepthStencil);
		LogInfo("  AutoDepthStencilFormat = %d\n", pPresentationParameters->AutoDepthStencilFormat);
	}
	if (pFullscreenDisplayMode)
	{
		LogInfo("  FullscreenDisplayFormat = %x\n", pFullscreenDisplayMode->Format);
		LogInfo("  FullscreenDisplayWidth = %d\n", pFullscreenDisplayMode->Width);
		LogInfo("  FullscreenDisplayHeight = %d\n", pFullscreenDisplayMode->Height);
		LogInfo("  FullscreenRefreshRate = %d\n", pFullscreenDisplayMode->RefreshRate);
		LogInfo("  ScanLineOrdering = %d\n", pFullscreenDisplayMode->ScanLineOrdering);
	}

	if (pPresentationParameters && SCREEN_REFRESH >= 0) 
	{
		LogInfo("    overriding refresh rate = %d\n", SCREEN_REFRESH);

		pPresentationParameters->FullScreen_RefreshRateInHz = SCREEN_REFRESH;
	}
	if (pFullscreenDisplayMode && SCREEN_REFRESH >= 0) 
	{
		LogInfo("    overriding full screen refresh rate = %d\n", SCREEN_REFRESH);

		pFullscreenDisplayMode->RefreshRate = SCREEN_REFRESH;
	}
	if (pPresentationParameters && SCREEN_WIDTH >= 0) 
	{
		LogInfo("    overriding width = %d\n", SCREEN_WIDTH);

		pPresentationParameters->BackBufferWidth = SCREEN_WIDTH;
	}
	if (pFullscreenDisplayMode && SCREEN_WIDTH >= 0) 
	{
		LogInfo("    overriding full screen width = %d\n", SCREEN_WIDTH);

		pFullscreenDisplayMode->Width = SCREEN_WIDTH;
	}
	if (pPresentationParameters && SCREEN_HEIGHT >= 0) 
	{
		LogInfo("    overriding height = %d\n", SCREEN_HEIGHT);

		pPresentationParameters->BackBufferHeight = SCREEN_HEIGHT;
	}
	if (pFullscreenDisplayMode && SCREEN_HEIGHT >= 0) 
	{
		LogInfo("    overriding full screen height = %d\n", SCREEN_HEIGHT);

		pFullscreenDisplayMode->Height = SCREEN_HEIGHT;
	}
	D3D9Base::D3DDISPLAYMODEEX fullScreenDisplayMode;
	if (pPresentationParameters && SCREEN_FULLSCREEN >= 0 && SCREEN_FULLSCREEN < 2)
	{
		LogInfo("    overriding full screen = %d\n", SCREEN_FULLSCREEN);

		pPresentationParameters->Windowed = !SCREEN_FULLSCREEN;
		if (SCREEN_FULLSCREEN && !pFullscreenDisplayMode)
		{
			LogInfo("    creating full screen parameter structure.\n");

			fullScreenDisplayMode.Size = sizeof(D3D9Base::D3DDISPLAYMODEEX);
			fullScreenDisplayMode.Format = pPresentationParameters->BackBufferFormat;
			fullScreenDisplayMode.Height = pPresentationParameters->BackBufferHeight;
			fullScreenDisplayMode.Width = pPresentationParameters->BackBufferWidth;
			fullScreenDisplayMode.RefreshRate = pPresentationParameters->FullScreen_RefreshRateInHz;
			fullScreenDisplayMode.ScanLineOrdering = D3D9Base::D3DSCANLINEORDERING_PROGRESSIVE;
			pFullscreenDisplayMode = &fullScreenDisplayMode;
		}
	}

	/*
	if (!pPresentationParameters->Windowed)
	{
		WNDCLASSEX wndClass;
		LPCWSTR className = L"3Dmigoto";
		wndClass.cbSize = sizeof(wndClass);
		wndClass.style  = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
		wndClass.lpfnWndProc = (WNDPROC) GetWindowLong(hFocusWindow, GWLP_WNDPROC);
		wndClass.cbClsExtra = 0;
		wndClass.cbWndExtra = 0;
		wndClass.hInstance = GetModuleHandle(0);
		wndClass.hIcon = 0;
		wndClass.hCursor = NULL;
		wndClass.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
		wndClass.lpszMenuName = NULL;
		wndClass.lpszClassName = className;
		wndClass.hIconSm = 0;
		HRESULT rhr = RegisterClassEx(&wndClass);
		hFocusWindow = CreateWindow(className, L"3Dmigoto hidden", WS_OVERLAPPEDWINDOW, 0, 0, pPresentationParameters->BackBufferWidth,
			pPresentationParameters->BackBufferHeight, GetDesktopWindow(), NULL, GetModuleHandle(0), NULL);
		// Fix auto depth stencil.
		if (pPresentationParameters->EnableAutoDepthStencil != TRUE)
		{
			pPresentationParameters->EnableAutoDepthStencil = TRUE;
			pPresentationParameters->AutoDepthStencilFormat = D3D9Base::D3DFMT_D24S8;
		}
		pPresentationParameters->SwapEffect = D3D9Base::D3DSWAPEFFECT_DISCARD;
		pPresentationParameters->PresentationInterval = 0; // D3DPRESENT_INTERVAL_DEFAULT
		pPresentationParameters->BackBufferFormat = D3D9Base::D3DFMT_A8R8G8B8; // 21
		pPresentationParameters->BackBufferCount = 1;
	}
	BehaviorFlags &= ~D3DCREATE_MULTITHREADED;
	*/

	D3D9Base::LPDIRECT3DDEVICE9EX baseDevice = NULL;
	HRESULT hr = S_OK;
	if (!gDelayDeviceCreation)
	{
		HRESULT hr = GetDirect3D9()->CreateDeviceEx(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, &baseDevice);
		if (FAILED(hr))
		{
			LogInfo("  returns result = %x\n", hr);

			if (ppReturnedDeviceInterface) *ppReturnedDeviceInterface = NULL;
			return hr;
		}
	}
	else
	{
		LogInfo("  postponing device creation and returning only wrapper\n");
	}

	D3D9Wrapper::IDirect3DDevice9* newDevice = D3D9Wrapper::IDirect3DDevice9::GetDirect3DDevice(baseDevice);
	if (newDevice == NULL)
	{
		LogInfo("  error creating IDirect3DDevice9 wrapper.\n");

		if (baseDevice) 
			baseDevice->Release();
		return E_OUTOFMEMORY;
	}
	else if (!baseDevice)
	{
		newDevice->_pD3D = GetDirect3D9();
		newDevice->_Adapter = Adapter;
		newDevice->_DeviceType = DeviceType;
		newDevice->_hFocusWindow = hFocusWindow;
		newDevice->_BehaviorFlags = BehaviorFlags;
		if (pFullscreenDisplayMode) newDevice->_pFullscreenDisplayMode = *pFullscreenDisplayMode;
		newDevice->_pPresentationParameters = *pPresentationParameters;
		// newDevice->_CreateThread = CreateThread(NULL, 0, DeviceCreateThread, newDevice, 0, NULL);
		// WaitForSingleObject(newDevice->_CreateThread, INFINITE);
	}
	
	LogInfo("  returns result=%x, handle=%x, wrapper=%x\n", hr, baseDevice, newDevice);

	if (ppReturnedDeviceInterface) *ppReturnedDeviceInterface = newDevice;
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::CreateDevice(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,HWND hFocusWindow,DWORD BehaviorFlags,D3D9Base::D3DPRESENT_PARAMETERS* pPresentationParameters, D3D9Wrapper::IDirect3DDevice9** ppReturnedDeviceInterface)
{
	LogInfo("IDirect3D9::CreateDevice called: adapter #%d\n", Adapter);
	LogInfo("  forwarding to CreateDeviceEx.\n");

	D3D9Base::D3DDISPLAYMODEEX fullScreenDisplayMode;
	fullScreenDisplayMode.Size = sizeof(D3D9Base::D3DDISPLAYMODEEX);
	if (pPresentationParameters)
	{
		fullScreenDisplayMode.Format = pPresentationParameters->BackBufferFormat;
		fullScreenDisplayMode.Height = pPresentationParameters->BackBufferHeight;
		fullScreenDisplayMode.Width = pPresentationParameters->BackBufferWidth;
		fullScreenDisplayMode.RefreshRate = pPresentationParameters->FullScreen_RefreshRateInHz;
		fullScreenDisplayMode.ScanLineOrdering = D3D9Base::D3DSCANLINEORDERING_PROGRESSIVE;
	}
	D3D9Base::D3DDISPLAYMODEEX *pFullScreenDisplayMode = &fullScreenDisplayMode;
	if (pPresentationParameters && pPresentationParameters->Windowed)
		pFullScreenDisplayMode = 0;
	return CreateDeviceEx(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullScreenDisplayMode, ppReturnedDeviceInterface);
}

STDMETHODIMP_(UINT) D3D9Wrapper::IDirect3D9::GetAdapterModeCountEx(THIS_ UINT Adapter,CONST D3D9Base::D3DDISPLAYMODEFILTER* pFilter )
{
	LogInfo("IDirect3D9::GetAdapterModeCountEx called: adapter #%d\n", Adapter);

	return GetDirect3D9()->GetAdapterModeCountEx(Adapter, pFilter);
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::EnumAdapterModesEx(THIS_ UINT Adapter,CONST D3D9Base::D3DDISPLAYMODEFILTER* pFilter,UINT Mode,D3D9Base::D3DDISPLAYMODEEX* pMode)
{
	LogInfo("IDirect3D9::EnumAdapterModesEx called: adapter #%d\n", Adapter);

	return GetDirect3D9()->EnumAdapterModesEx(Adapter, pFilter, Mode, pMode);
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::GetAdapterDisplayModeEx(THIS_ UINT Adapter,D3D9Base::D3DDISPLAYMODEEX* pMode,D3D9Base::D3DDISPLAYROTATION* pRotation)
{
	LogInfo("IDirect3D9::GetAdapterDisplayModeEx called: adapter #%d\n", Adapter);

	return GetDirect3D9()->GetAdapterDisplayModeEx(Adapter, pMode, pRotation);
}

STDMETHODIMP D3D9Wrapper::IDirect3D9::GetAdapterLUID(THIS_ UINT Adapter,LUID * pLUID)
{
	LogInfo("IDirect3D9::GetAdapterLUID called: adapter #%d\n", Adapter);

	return GetDirect3D9()->GetAdapterLUID(Adapter, pLUID);
}
