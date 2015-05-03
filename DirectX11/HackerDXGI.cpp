
#include "HackerDXGI.h"

#include "log.h"
#include "util.h"
#include "globals.h"


bool gAllowWindowCommands = false;
int SCREEN_FULLSCREEN = -1;
int SCREEN_WIDTH = -1;
int SCREEN_HEIGHT = -1;
int SCREEN_REFRESH = -1;
int FILTER_REFRESH[11] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };


// -----------------------------------------------------------------------------
// Constructors:

HackerUnknown::HackerUnknown(IUnknown *pUnknown)
{
	mOrigUnknown = pUnknown;
}

HackerDXGIObject::HackerDXGIObject(IDXGIObject *pObject)
	: HackerUnknown(pObject)
{
	mOrigObject = pObject;
}

// Worth noting- the device and context for the secrete path are the Hacker 
// versions, because we need access to their fields later.
HackerDXGIDevice::HackerDXGIDevice(IDXGIDevice *pDXGIDevice, HackerDevice *pDevice, HackerContext *pContext)
	: HackerDXGIObject(pDXGIDevice)
{
	mOrigDXGIDevice = pDXGIDevice;
	mHackerDevice = pDevice;
	mHackerContext = pContext;
}

HackerDXGIAdapter::HackerDXGIAdapter(IDXGIAdapter *pAdapter, HackerDevice *pDevice, HackerContext *pContext)
	: HackerDXGIObject(pAdapter)
{
	mOrigAdapter = pAdapter;
	mHackerDevice = pDevice;
	mHackerContext = pContext;
}

HackerDXGIOutput::HackerDXGIOutput(IDXGIOutput *pOutput)
	: IDXGIOutput()
{
	mOrigOutput = pOutput;
}


HackerDXGIDeviceSubObject::HackerDXGIDeviceSubObject(IDXGIDeviceSubObject *pSubObject)
	: HackerDXGIObject(pSubObject)
{
	mOrigDeviceSubObject = pSubObject;
}

HackerDXGISwapChain::HackerDXGISwapChain(IDXGISwapChain *pSwapChain, HackerDevice *pDevice, HackerContext *pContext)
	: HackerDXGIDeviceSubObject(pSwapChain)
{
	mOrigSwapChain = pSwapChain;
	pDevice->SetHackerSwapChain(this);

	// Create Overlay class that will be responsible for drawing any text
	// info over the game. Using the original Device and Context.
	mOverlay = new Overlay(pDevice, pContext, this);
}

HackerDXGISwapChain1::HackerDXGISwapChain1(IDXGISwapChain1 *pSwapChain, HackerDevice *pDevice, HackerContext *pContext)
	: HackerDXGISwapChain(pSwapChain, pDevice, pContext)
{
	mOrigSwapChain1 = pSwapChain;
}


HackerDXGIFactory::HackerDXGIFactory(IDXGIFactory *pFactory, HackerDevice *pDevice, HackerContext *pContext)
	: HackerDXGIObject(pFactory)
{
	mOrigFactory = pFactory;
	mHackerDevice = pDevice;
	mHackerContext = pContext;
}

HackerDXGIFactory1::HackerDXGIFactory1(IDXGIFactory1 *pFactory, HackerDevice *pDevice, HackerContext *pContext)
	: HackerDXGIFactory(pFactory, pDevice, pContext)
{
	mOrigFactory1 = pFactory;
}

HackerDXGIFactory2::HackerDXGIFactory2(IDXGIFactory2 *pFactory, HackerDevice *pDevice, HackerContext *pContext)
	: HackerDXGIFactory1(pFactory, pDevice, pContext)
{
	mOrigFactory2 = pFactory;
}


// -----------------------------------------------------------------------------

STDMETHODIMP_(ULONG) HackerUnknown::AddRef(THIS)
{
	ULONG ulRef = mOrigUnknown->AddRef();
	LogInfo("HackerUnknown::AddRef counter=%d, this=%p \n", ulRef, this);
	return ulRef;
}

STDMETHODIMP_(ULONG) HackerUnknown::Release(THIS)
{
	ULONG ulRef = mOrigUnknown->Release();
	LogInfo("HackerUnknown::Release counter=%d, this=%p \n", ulRef, this);

	if (ulRef <= 0)
	{
		LogInfo("HackerUnknown::Release counter=%d, this=%p \n", ulRef, this);
		LogInfo("  deleting self\n");

		delete this;
		return 0L;
	}
	return ulRef;
}

STDMETHODIMP HackerUnknown::QueryInterface(THIS_
	/* [in] */ REFIID riid,
	/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	LogDebug("HackerUnknown::QueryInterface called with IID: %s \n", NameFromIID(riid).c_str());
	HRESULT hr = mOrigUnknown->QueryInterface(riid, ppvObject);
	LogDebug("  returns result = %x for %p \n", hr, ppvObject);
	return hr;
}


// -----------------------------------------------------------------------------

STDMETHODIMP HackerDXGIObject::SetPrivateData(THIS_
	/* [annotation][in] */
	__in  REFGUID Name,
	/* [in] */ UINT DataSize,
	/* [annotation][in] */
	__in_bcount(DataSize)  const void *pData)
{
	LogInfo("HackerDXGIObject::SetPrivateData called with GUID: %s \n", NameFromIID(Name).c_str());
	LogInfo("  DataSize = %d\n", DataSize);

	HRESULT hr = mOrigObject->SetPrivateData(Name, DataSize, pData);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIObject::SetPrivateDataInterface(THIS_
	/* [annotation][in] */
	__in  REFGUID Name,
	/* [annotation][in] */
	__in  const IUnknown *pUnknown)
{
	LogInfo("HackerDXGIObject::SetPrivateDataInterface called with GUID: %s \n", NameFromIID(Name).c_str());

	HRESULT hr = mOrigObject->SetPrivateDataInterface(Name, pUnknown);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIObject::GetPrivateData(THIS_
	/* [annotation][in] */
	__in  REFGUID Name,
	/* [annotation][out][in] */
	__inout  UINT *pDataSize,
	/* [annotation][out] */
	__out_bcount(*pDataSize)  void *pData)
{
	LogInfo("HackerDXGIObject::GetPrivateData called with GUID: %s \n", NameFromIID(Name).c_str());

	HRESULT hr = mOrigObject->GetPrivateData(Name, pDataSize, pData);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIObject::GetParent(THIS_
	/* [annotation][in] */
	__in  REFIID riid,
	/* [annotation][retval][out] */
	__out  void **ppParent)
{
	LogInfo("HackerDXGIObject::GetParent called with IID: %s \n", NameFromIID(riid).c_str());

	HRESULT hr = mOrigObject->GetParent(riid, ppParent);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}


// -----------------------------------------------------------------------------

STDMETHODIMP HackerDXGIDevice::GetAdapter(
	/* [annotation][out] */
	_Out_  IDXGIAdapter **pAdapter)
{
	LogInfo("HackerDXGIDevice::GetAdapter called \n");
	HRESULT hr = mOrigDXGIDevice->GetAdapter(pAdapter);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIDevice::CreateSurface(
	/* [annotation][in] */
	_In_  const DXGI_SURFACE_DESC *pDesc,
	/* [in] */ UINT NumSurfaces,
	/* [in] */ DXGI_USAGE Usage,
	/* [annotation][in] */
	_In_opt_  const DXGI_SHARED_RESOURCE *pSharedResource,
	/* [annotation][out] */
	_Out_  IDXGISurface **ppSurface)
{
	LogInfo("HackerDXGIDevice::CreateSurface called \n");
	HRESULT hr = mOrigDXGIDevice->CreateSurface(pDesc, NumSurfaces, Usage, pSharedResource, ppSurface);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIDevice::QueryResourceResidency(
	/* [annotation][size_is][in] */
	_In_reads_(NumResources)  IUnknown *const *ppResources,
	/* [annotation][size_is][out] */
	_Out_writes_(NumResources)  DXGI_RESIDENCY *pResidencyStatus,
	/* [in] */ UINT NumResources)
{
	LogInfo("HackerDXGIDevice::QueryResourceResidency called \n");
	HRESULT hr = mOrigDXGIDevice->QueryResourceResidency(ppResources, pResidencyStatus, NumResources);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIDevice::SetGPUThreadPriority(
	/* [in] */ INT Priority)
{
	LogInfo("HackerDXGIDevice::SetGPUThreadPriority called \n");
	HRESULT hr = mOrigDXGIDevice->SetGPUThreadPriority(Priority);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIDevice::GetGPUThreadPriority(
	/* [annotation][retval][out] */
	_Out_  INT *pPriority)
{
	LogInfo("HackerDXGIDevice::GetGPUThreadPriority called \n");
	HRESULT hr = mOrigDXGIDevice->GetGPUThreadPriority(pPriority);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

// Override of HackerDXGIObject::GetParent, as we are wrapping specific calls
// expected to call the HackerDXGIDevice to find the DXGIFactory.
//
// If the parent request is for the IDXGIAdapter, that must mean we are taking the secret
// path for getting the swap chain.  Return a wrapped version whenever this happens, so
// we can get access later.
// 
// It might make sense to drop these into the HackerDXGIObject::GetParent call, 
// and not override these.

STDMETHODIMP HackerDXGIDevice::GetParent(THIS_
	/* [annotation][in] */
	__in  REFIID riid,
	/* [annotation][retval][out] */
	__out  void **ppParent)
{
	HRESULT hr;

	LogInfo("HackerDXGIDevice::GetParent called with IID: %s \n", NameFromIID(riid).c_str());

	if (riid == __uuidof(IDXGIAdapter))
	{
		IDXGIAdapter *origAdapter;
		hr = mOrigDXGIDevice->GetParent(riid, (void**)(&origAdapter));

		HackerDXGIAdapter *adapterWrap = new HackerDXGIAdapter(origAdapter, mHackerDevice, mHackerContext);
		if (adapterWrap == NULL)
		{
			LogInfo("  error allocating dxgiAdapterWrap. \n");
			return E_OUTOFMEMORY;
		}
		if (ppParent)
			*ppParent = adapterWrap;

		LogInfo("  created HackerDXGIAdapter wrapper = %p of %p \n", adapterWrap, origAdapter);
	}
	else
	{
		hr = mOrigDXGIDevice->GetParent(riid, ppParent);
	}

	LogInfo("  returns result = %x\n", hr);
	return hr;
}


// -----------------------------------------------------------------------------

STDMETHODIMP HackerDXGIFactory::EnumAdapters(THIS_
            /* [in] */ UINT Adapter,
            /* [annotation][out] */ 
            __out IDXGIAdapter **ppAdapter)
{
	LogInfo("HackerDXGIFactory::EnumAdapters adapter %d requested\n", Adapter);
	HRESULT hr = mOrigFactory->EnumAdapters(Adapter, ppAdapter);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIFactory::MakeWindowAssociation(THIS_
            HWND WindowHandle,
            UINT Flags)
{
	if (LogFile)
	{
		LogInfo("HackerDXGIFactory::MakeWindowAssociation called with WindowHandle = %p, Flags = %x\n", WindowHandle, Flags);
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
	HRESULT hr = mOrigFactory->MakeWindowAssociation(WindowHandle, Flags);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP HackerDXGIFactory::GetWindowAssociation(THIS_
            /* [annotation][out] */ 
            __out  HWND *pWindowHandle)
{
	LogInfo("HackerDXGIFactory::GetWindowAssociation called\n");
	HRESULT hr = mOrigFactory->GetWindowAssociation(pWindowHandle);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
// For any given SwapChain created by the factory here, we want to wrap the SwapChain so that
// we can get called when Present() is called.

STDMETHODIMP HackerDXGIFactory::CreateSwapChain(THIS_
            /* [annotation][in] */ 
            __in  IUnknown *pDevice,
            /* [annotation][in] */ 
            __in  DXGI_SWAP_CHAIN_DESC *pDesc,
            /* [annotation][out] */ 
            __out  HackerDXGISwapChain **ppSwapChain)
{
	LogInfo("HackerDXGIFactory::CreateSwapChain called with parameters\n");
	LogInfo("  Device = %p\n", pDevice);
	if (pDesc)
	{
		LogInfo("  Windowed = %d\n", pDesc->Windowed);
		LogInfo("  Width = %d\n", pDesc->BufferDesc.Width);
		LogInfo("  Height = %d\n", pDesc->BufferDesc.Height);
		LogInfo("  Refresh rate = %f\n",
			(float)pDesc->BufferDesc.RefreshRate.Numerator / (float)pDesc->BufferDesc.RefreshRate.Denominator);

		// Force screen resolution or refresh if specified by d3dx.ini
		if (SCREEN_REFRESH >= 0)
		{
			pDesc->BufferDesc.RefreshRate.Numerator = SCREEN_REFRESH;
			pDesc->BufferDesc.RefreshRate.Denominator = 1;
		}
		if (SCREEN_WIDTH >= 0) pDesc->BufferDesc.Width = SCREEN_WIDTH;
		if (SCREEN_HEIGHT >= 0) pDesc->BufferDesc.Height = SCREEN_HEIGHT;
		if (SCREEN_FULLSCREEN >= 0) pDesc->Windowed = !SCREEN_FULLSCREEN;
	}

	IDXGISwapChain *origSwapChain = 0;
	HRESULT hr = mOrigFactory->CreateSwapChain(pDevice, pDesc, &origSwapChain);

	// This call can return 0x087A0001, which is DXGI_STATUS_OCCLUDED.  That means that the window
	// can be occluded when we return from creating the real swap chain.  
	// The check below was looking ONLY for S_OK, and that would lead to it skipping the sub-block
	// which set up ppSwapChain and returned it.  So, ppSwapChain==NULL and it would crash, sometimes.
	// There are other legitimate DXGI_STATUS results, so checking for SUCCEEDED is the correct way.
	// Same bug fix is applied for the other CreateSwapChain* variants.

	if (SUCCEEDED(hr))
	{
		HackerDXGISwapChain *swapchainWrap = new HackerDXGISwapChain(origSwapChain, mHackerDevice, mHackerContext);
		if (swapchainWrap == NULL)
		{
			LogInfo("  error allocating swapchainWrap. \n");
			origSwapChain->Release();
			return E_OUTOFMEMORY;
		}

		if (ppSwapChain)
			*ppSwapChain = swapchainWrap;
	}
	
	LogInfo("  return value = %x, swapchain: %p, swapchain wrap: %p \n", hr, origSwapChain, ppSwapChain);
	return hr;
}

STDMETHODIMP HackerDXGIFactory::CreateSoftwareAdapter(THIS_
	/* [in] */ HMODULE Module,
	/* [annotation][out] */
	__out  IDXGIAdapter **ppAdapter)
{
	LogInfo("HackerDXGIFactory::CreateSoftwareAdapter called\n");
	HRESULT hr = mOrigFactory->CreateSoftwareAdapter(Module, ppAdapter);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}


// -----------------------------------------------------------------------------

// ToDo: In between state here, where we duplicate the calls to the Factory1
// objects.  It's not clear at runtime whether they use a vtable that has any
// correlation to c++ objects, so even though Factory1 subclasses Factory,
// the calls may not be dispatched to Factory.
// This was an experiment to see if further wrapping would trigger them to
// be called, and hence logged, but this did change the runtime at all, as the
// the games don't use these APIs it appears.

STDMETHODIMP_(ULONG) HackerDXGIFactory1::AddRef(THIS)
{
	ULONG ulRef = mOrigFactory1->AddRef();
	LogInfo("HackerDXGIFactory1::AddRef counter=%d, this=%p \n", ulRef, this);
	return ulRef;
}

STDMETHODIMP_(ULONG) HackerDXGIFactory1::Release(THIS)
{
	ULONG ulRef = mOrigFactory1->Release();
	LogInfo("HackerDXGIFactory::Release counter=%d, this=%p\n", ulRef, this);

	if (ulRef <= 0)
	{
		LogInfo("HackerDXGIFactory::Release counter=%d, this=%p\n", ulRef, this);
		LogInfo("  deleting self\n");

		delete this;
		return 0L;
	}
	return ulRef;
}

STDMETHODIMP HackerDXGIFactory1::QueryInterface(THIS_
	/* [in] */ REFIID riid,
	/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	LogInfo("HackerDXGIFactory1::QueryInterface called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);
	HRESULT hr = mOrigFactory1->QueryInterface(riid, ppvObject);
	LogInfo("  returns result = %x for %p \n", hr, ppvObject);
	return hr;
}


STDMETHODIMP HackerDXGIFactory1::EnumAdapters(THIS_
	/* [in] */ UINT Adapter,
	/* [annotation][out] */
	__out IDXGIAdapter **ppAdapter)
{
	LogInfo("HackerDXGIFactory1::EnumAdapters adapter %d requested\n", Adapter);
	HRESULT hr = mOrigFactory->EnumAdapters(Adapter, ppAdapter);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIFactory1::MakeWindowAssociation(THIS_
	HWND WindowHandle,
	UINT Flags)
{
	if (LogFile)
	{
		LogInfo("HackerDXGIFactory1::MakeWindowAssociation called with WindowHandle = %p, Flags = %x\n", WindowHandle, Flags);
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
	HRESULT hr = mOrigFactory->MakeWindowAssociation(WindowHandle, Flags);
	LogInfo("  returns result = %x\n", hr);

	return hr;
}

STDMETHODIMP HackerDXGIFactory1::GetWindowAssociation(THIS_
	/* [annotation][out] */
	__out  HWND *pWindowHandle)
{
	LogInfo("HackerDXGIFactory1::GetWindowAssociation called\n");
	HRESULT hr = mOrigFactory->GetWindowAssociation(pWindowHandle);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIFactory1::CreateSwapChain(THIS_
	/* [annotation][in] */
	__in  IUnknown *pDevice,
	/* [annotation][in] */
	__in  DXGI_SWAP_CHAIN_DESC *pDesc,
	/* [annotation][out] */
	__out  IDXGISwapChain **ppSwapChain)
{
	LogInfo("HackerDXGIFactory1::CreateSwapChain called with parameters\n");
	LogInfo("  Device = %p\n", pDevice);
	if (pDesc)
	{
		LogInfo("  Windowed = %d\n", pDesc->Windowed);
		LogInfo("  Width = %d\n", pDesc->BufferDesc.Width);
		LogInfo("  Height = %d\n", pDesc->BufferDesc.Height);
		LogInfo("  Refresh rate = %f\n",
			(float)pDesc->BufferDesc.RefreshRate.Numerator / (float)pDesc->BufferDesc.RefreshRate.Denominator);

		// Force screen resolution or refresh if specified by d3dx.ini
		if (SCREEN_REFRESH >= 0)
		{
			pDesc->BufferDesc.RefreshRate.Numerator = SCREEN_REFRESH;
			pDesc->BufferDesc.RefreshRate.Denominator = 1;
		}
		if (SCREEN_WIDTH >= 0) pDesc->BufferDesc.Width = SCREEN_WIDTH;
		if (SCREEN_HEIGHT >= 0) pDesc->BufferDesc.Height = SCREEN_HEIGHT;
		if (SCREEN_FULLSCREEN >= 0) pDesc->Windowed = !SCREEN_FULLSCREEN;
	}

	HRESULT hr = mOrigFactory->CreateSwapChain(pDevice, pDesc, ppSwapChain);
	LogInfo("  return value = %x\n", hr);

	// This call can return 0x087A0001, which is DXGI_STATUS_OCCLUDED.  That means that the window
	// can be occluded when we return from creating the real swap chain.  
	// The check below was looking ONLY for S_OK, and that would lead to it skipping the sub-block
	// which set up ppSwapChain and returned it.  So, ppSwapChain==NULL and it would crash, sometimes.
	// There are other legitimate DXGI_STATUS results, so checking for SUCCEEDED is the correct way.
	// Same bug fix is applied for the other CreateSwapChain* variants.

	//if (SUCCEEDED(hr))
	//{
	//	*ppSwapChain = HackerDXGISwapChain::GetDirectSwapChain(origSwapChain);
	//	if ((*ppSwapChain)->m_WrappedDevice) (*ppSwapChain)->m_WrappedDevice->Release();
	//	(*ppSwapChain)->m_WrappedDevice = pDevice; pDevice->AddRef();
	//	(*ppSwapChain)->m_RealDevice = realDevice;
	//	if (pDesc) SendScreenResolution(pDevice, pDesc->BufferDesc.Width, pDesc->BufferDesc.Height);
	//}

	return hr;
}

STDMETHODIMP HackerDXGIFactory1::CreateSoftwareAdapter(THIS_
	/* [in] */ HMODULE Module,
	/* [annotation][out] */
	__out  IDXGIAdapter **ppAdapter)
{
	LogInfo("HackerDXGIFactory1::CreateSoftwareAdapter called\n");
	HRESULT hr = mOrigFactory->CreateSoftwareAdapter(Module, ppAdapter);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}



STDMETHODIMP HackerDXGIFactory1::EnumAdapters1(THIS_
	/* [in] */ UINT Adapter,
	/* [annotation][out] */
	__out  IDXGIAdapter1 **ppAdapter)
{
	LogInfo("HackerDXGIFactory1::EnumAdapters1 called: adapter #%d requested\n", Adapter);

	HRESULT ret = mOrigFactory1->EnumAdapters1(Adapter, ppAdapter);

	if (SUCCEEDED(ret) && LogFile)
	{
		DXGI_ADAPTER_DESC1 desc;
		if (SUCCEEDED((*ppAdapter)->GetDesc1(&desc)))
		{
			char instance[MAX_PATH];
			wcstombs(instance, desc.Description, MAX_PATH);
			LogInfo("  returns adapter: %s, sysmem=%d, vidmem=%d, flags=%x\n", instance, desc.DedicatedSystemMemory, desc.DedicatedVideoMemory, desc.Flags);
		}
	}
		/*
		IDXGIOutput *output;
		HRESULT h = S_OK;
		for (int i = 0; h == S_OK; ++i)
		{
		if ((h = (*ppAdapter)->EnumOutputs(i, &output)) == S_OK)
		{
		DXGI_OUTPUT_DESC outputDesc;
		if (output->GetDesc(&outputDesc) == S_OK)
		{
		wcstombs(instance, outputDesc.DeviceName, MAX_PATH);
		LogInfo("  Output found: %s, desktop=%d\n", instance, outputDesc.AttachedToDesktop);
		}

		UINT num = 0;
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		UINT flags = DXGI_ENUM_MODES_INTERLACED | DXGI_ENUM_MODES_SCALING;
		if (output->GetDisplayModeList(format, flags, &num, 0) == S_OK)
		{
		DXGI_MODE_DESC *pDescs = new DXGI_MODE_DESC[num];
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
	
	LogInfo("  returns result = %x, handle = %p \n", ret, *ppAdapter);
	return ret;
}

STDMETHODIMP_(BOOL) HackerDXGIFactory1::IsCurrent(THIS)
{
	LogInfo("HackerDXGIFactory1::IsCurrent called\n");
	HRESULT hr = mOrigFactory1->IsCurrent();
	LogInfo("  returns result = %x\n", hr);
	return hr;
}


// -----------------------------------------------------------------------------

STDMETHODIMP HackerDXGIFactory2::CreateSwapChainForHwnd(THIS_
            /* [annotation][in] */ 
            _In_  IUnknown *pDevice,
            /* [annotation][in] */ 
            _In_  HWND hWnd,
            /* [annotation][in] */ 
            _In_ const DXGI_SWAP_CHAIN_DESC1 *pDesc,
            /* [annotation][in] */ 
            _In_opt_ const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pRestrictToOutput,
            /* [annotation][out] */ 
            _Out_  IDXGISwapChain1 **ppSwapChain)
{
	LogInfo("HackerDXGIFactory2::CreateSwapChainForHwnd called with parameters\n");
	LogInfo("  Device = %p\n", pDevice);
	LogInfo("  HWND = %p\n", hWnd);
	if (pDesc) LogInfo("  Stereo = %d\n", pDesc->Stereo);
	if (pDesc) LogInfo("  Width = %d\n", pDesc->Width);
	if (pDesc) LogInfo("  Height = %d\n", pDesc->Height);
	if (pFullscreenDesc) LogInfo("  Refresh rate = %f\n", 
		(float) pFullscreenDesc->RefreshRate.Numerator / (float) pFullscreenDesc->RefreshRate.Denominator);
	if (pFullscreenDesc) LogInfo("  Windowed = %d\n", pFullscreenDesc->Windowed);

	//if (pDesc && SCREEN_WIDTH >= 0) pDesc->Width = SCREEN_WIDTH;
	//if (pDesc && SCREEN_HEIGHT >= 0) pDesc->Height = SCREEN_HEIGHT;
	//if (pFullscreenDesc && SCREEN_REFRESH >= 0)
	//{
	//	pFullscreenDesc->RefreshRate.Numerator = SCREEN_REFRESH;
	//	pFullscreenDesc->RefreshRate.Denominator = 1;
	//}
	//if (pFullscreenDesc && SCREEN_FULLSCREEN >= 0) pFullscreenDesc->Windowed = !SCREEN_FULLSCREEN;

	//HRESULT hr = -1;
	//if (pRestrictToOutput)
	//hr = mOrigFactory2->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput->m_pOutput, &origSwapChain);
	HRESULT hr = mOrigFactory2->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
	LogInfo("  return value = %x\n", hr);

	//if (SUCCEEDED(hr))
	//{
	//	*ppSwapChain = HackerDXGISwapChain1::GetDirectSwapChain(origSwapChain);
	//	if ((*ppSwapChain)->m_WrappedDevice) (*ppSwapChain)->m_WrappedDevice->Release();
	//	(*ppSwapChain)->m_WrappedDevice = pDevice; pDevice->AddRef();
	//	(*ppSwapChain)->m_RealDevice = realDevice;
	//	if (pDesc) SendScreenResolution(pDevice, pDesc->Width, pDesc->Height);
	//}
	
	return hr;
}

STDMETHODIMP HackerDXGIFactory2::CreateSwapChainForCoreWindow(THIS_
            /* [annotation][in] */ 
            _In_  IUnknown *pDevice,
            /* [annotation][in] */ 
            _In_  IUnknown *pWindow,
            /* [annotation][in] */ 
			_In_ const DXGI_SWAP_CHAIN_DESC1 *pDesc,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pRestrictToOutput,
            /* [annotation][out] */ 
            _Out_  IDXGISwapChain1 **ppSwapChain)
{
	LogInfo("HackerDXGIFactory2::CreateSwapChainForCoreWindow called with parameters\n");
	LogInfo("  Device = %p\n", pDevice);
	if (pDesc) LogInfo("  Width = %d\n", pDesc->Width);
	if (pDesc) LogInfo("  Height = %d\n", pDesc->Height);
	if (pDesc) LogInfo("  Stereo = %d\n", pDesc->Stereo);

	//if (pDesc && SCREEN_WIDTH >= 0) pDesc->Width = SCREEN_WIDTH;
	//if (pDesc && SCREEN_HEIGHT >= 0) pDesc->Height = SCREEN_HEIGHT;

	//IDXGISwapChain1 *origSwapChain;
	//IUnknown *realDevice = ReplaceDevice(pDevice);
	//HRESULT hr = -1;
	//if (pRestrictToOutput)
	//	hr = mOrigFactory->CreateSwapChainForCoreWindow(realDevice, pWindow, pDesc, pRestrictToOutput->m_pOutput, &origSwapChain);
	HRESULT	hr = mOrigFactory2->CreateSwapChainForCoreWindow(pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
	LogInfo("  return value = %x\n", hr);

	//if (SUCCEEDED(hr))
	//{
	//	*ppSwapChain = HackerDXGISwapChain1::GetDirectSwapChain(origSwapChain);
	//	if ((*ppSwapChain)->m_WrappedDevice) (*ppSwapChain)->m_WrappedDevice->Release();
	//	(*ppSwapChain)->m_WrappedDevice = pDevice; pDevice->AddRef();
	//	(*ppSwapChain)->m_RealDevice = realDevice;
	//	if (pDesc) SendScreenResolution(pDevice, pDesc->Width, pDesc->Height);
	//}

	return hr;
}

STDMETHODIMP HackerDXGIFactory2::CreateSwapChainForComposition(THIS_
            /* [annotation][in] */ 
            _In_  IUnknown *pDevice,
            /* [annotation][in] */ 
            _In_ const DXGI_SWAP_CHAIN_DESC1 *pDesc,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pRestrictToOutput,
            /* [annotation][out] */ 
            _Outptr_  IDXGISwapChain1 **ppSwapChain)
{
	LogInfo("HackerDXGIFactory2::CreateSwapChainForComposition called with parameters\n");
	LogInfo("  Device = %p\n", pDevice);
	if (pDesc) LogInfo("  Width = %d\n", pDesc->Width);
	if (pDesc) LogInfo("  Height = %d\n", pDesc->Height);
	if (pDesc) LogInfo("  Stereo = %d\n", pDesc->Stereo);

	//if (pDesc && SCREEN_WIDTH >= 0) pDesc->Width = SCREEN_WIDTH;
	//if (pDesc && SCREEN_HEIGHT >= 0) pDesc->Height = SCREEN_HEIGHT;

	//IDXGISwapChain1 *origSwapChain;
	//IUnknown *realDevice = ReplaceDevice(pDevice);
	//HRESULT hr = -1;
	//if (pRestrictToOutput)
	//	hr = mOrigFactory->CreateSwapChainForComposition(realDevice, pDesc, pRestrictToOutput->m_pOutput, &origSwapChain);
	//LogInfo("  return value = %x\n", hr);

	//if (SUCCEEDED(hr))
	//{
	//	*ppSwapChain = HackerDXGISwapChain1::GetDirectSwapChain(origSwapChain);
	//	if ((*ppSwapChain)->m_WrappedDevice) (*ppSwapChain)->m_WrappedDevice->Release();
	//	(*ppSwapChain)->m_WrappedDevice = pDevice; pDevice->AddRef();
	//	(*ppSwapChain)->m_RealDevice = realDevice;
	//	if (pDesc) SendScreenResolution(pDevice, pDesc->Width, pDesc->Height);
	//}

	HRESULT	hr = mOrigFactory2->CreateSwapChainForComposition(pDevice, pDesc, pRestrictToOutput, ppSwapChain);
	LogInfo("  return value = %x\n", hr);
	return hr;
}

STDMETHODIMP_(BOOL) HackerDXGIFactory2::IsWindowedStereoEnabled(THIS)
{
	LogInfo("HackerDXGIFactory2::IsWindowedStereoEnabled called\n");
	BOOL ret = mOrigFactory2->IsWindowedStereoEnabled();
	LogInfo("  returns %d\n", ret);
	return ret;
}
                 
STDMETHODIMP HackerDXGIFactory2::GetSharedResourceAdapterLuid(THIS_
            /* [annotation] */ 
            _In_  HANDLE hResource,
            /* [annotation] */ 
            _Out_  LUID *pLuid)
{
	LogInfo("HackerDXGIFactory2::GetSharedResourceAdapterLuid called\n");
	HRESULT ret = mOrigFactory2->GetSharedResourceAdapterLuid(hResource, pLuid);
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP HackerDXGIFactory2::RegisterStereoStatusWindow(THIS_
            /* [annotation][in] */ 
            _In_  HWND WindowHandle,
            /* [annotation][in] */ 
            _In_  UINT wMsg,
            /* [annotation][out] */ 
            _Out_  DWORD *pdwCookie)
{
	LogInfo("HackerDXGIFactory2::RegisterStereoStatusWindow called\n");
	HRESULT ret = mOrigFactory2->RegisterStereoStatusWindow(WindowHandle, wMsg, pdwCookie);
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP HackerDXGIFactory2::RegisterStereoStatusEvent(THIS_
            /* [annotation][in] */ 
            _In_  HANDLE hEvent,
            /* [annotation][out] */ 
            _Out_  DWORD *pdwCookie)
{
	LogInfo("HackerDXGIFactory2::RegisterStereoStatusEvent called\n");
	HRESULT ret = mOrigFactory2->RegisterStereoStatusEvent(hEvent, pdwCookie);
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP_(void) HackerDXGIFactory2::UnregisterStereoStatus(THIS_
            /* [annotation][in] */ 
            _In_  DWORD dwCookie)
{
	LogInfo("HackerDXGIFactory2::UnregisterStereoStatus called\n");
	mOrigFactory2->UnregisterStereoStatus(dwCookie);
}
        
STDMETHODIMP HackerDXGIFactory2::RegisterOcclusionStatusWindow(THIS_
            /* [annotation][in] */ 
            _In_  HWND WindowHandle,
            /* [annotation][in] */ 
            _In_  UINT wMsg,
            /* [annotation][out] */ 
            _Out_  DWORD *pdwCookie)
{
	LogInfo("HackerDXGIFactory2::RegisterOcclusionStatusWindow called\n");
	HRESULT ret = mOrigFactory2->RegisterOcclusionStatusWindow(WindowHandle, wMsg, pdwCookie);
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP HackerDXGIFactory2::RegisterOcclusionStatusEvent(THIS_
            /* [annotation][in] */ 
            _In_  HANDLE hEvent,
            /* [annotation][out] */ 
            _Out_  DWORD *pdwCookie)
{
	LogInfo("HackerDXGIFactory2::RegisterOcclusionStatusEvent called\n");
	HRESULT ret = mOrigFactory2->RegisterOcclusionStatusEvent(hEvent, pdwCookie);
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP_(void) HackerDXGIFactory2::UnregisterOcclusionStatus(THIS_
            /* [annotation][in] */ 
            _In_  DWORD dwCookie)
{
	LogInfo("HackerDXGIFactory2::UnregisterOcclusionStatus called\n");
	mOrigFactory2->UnregisterOcclusionStatus(dwCookie);
}
        

// -----------------------------------------------------------------------------

STDMETHODIMP HackerDXGIAdapter::EnumOutputs(THIS_
            /* [in] */ UINT Output,
            /* [annotation][out][in] */ 
            __out IDXGIOutput **ppOutput)
{
	LogInfo("HackerDXGIAdapter::EnumOutputs called: output #%d requested\n", Output);
	HRESULT hr = mOrigAdapter->EnumOutputs(Output, ppOutput);
	LogInfo("  returns result = %x, handle = %p\n", hr, *ppOutput);
	return hr;
}
        
STDMETHODIMP HackerDXGIAdapter::GetDesc(THIS_
            /* [annotation][out] */ 
            __out DXGI_ADAPTER_DESC *pDesc)
{
	LogInfo("HackerDXGIAdapter::GetDesc called\n");
	
	HRESULT hr = mOrigAdapter->GetDesc(pDesc);
	if (LogFile && hr == S_OK)
	{
		char s[MAX_PATH];
		wcstombs(s, pDesc->Description, MAX_PATH);
		LogInfo("  returns adapter: %s, sysmem=%d, vidmem=%d\n", s, pDesc->DedicatedSystemMemory, pDesc->DedicatedVideoMemory);
	}
	return hr;
}

STDMETHODIMP HackerDXGIAdapter::CheckInterfaceSupport(THIS_
            /* [annotation][in] */ 
            __in  REFGUID InterfaceName,
            /* [annotation][out] */ 
            __out  LARGE_INTEGER *pUMDVersion)
{
	LogInfo("HackerDXGIAdapter::CheckInterfaceSupport called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		InterfaceName.Data1, InterfaceName.Data2, InterfaceName.Data3, InterfaceName.Data4[0], InterfaceName.Data4[1], InterfaceName.Data4[2], InterfaceName.Data4[3], 
		InterfaceName.Data4[4], InterfaceName.Data4[5], InterfaceName.Data4[6], InterfaceName.Data4[7]);
	if (LogFile && InterfaceName.Data1 == 0x9b7e4c0f && InterfaceName.Data2 == 0x342c && InterfaceName.Data3 == 0x4106 && InterfaceName.Data4[0] == 0xa1 && 
		InterfaceName.Data4[1] == 0x9f && InterfaceName.Data4[2] == 0x4f && InterfaceName.Data4[3] == 0x27 && InterfaceName.Data4[4] == 0x04 && 
		InterfaceName.Data4[5] == 0xf6 && InterfaceName.Data4[6] == 0x89 && InterfaceName.Data4[7] == 0xf0)
		LogInfo("  9b7e4c0f-342c-4106-a19f-4f2704f689f0 = IID_ID3D10Device\n");
	
	HRESULT hr = mOrigAdapter->CheckInterfaceSupport(InterfaceName, pUMDVersion);
	if (hr == S_OK && pUMDVersion) LogInfo("  UMDVersion high=%x, low=%x\n", pUMDVersion->HighPart, pUMDVersion->LowPart);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}

// expected to call the HackerDXGIAdapter to find the DXGIFactory.
//
// If the parent request is for the IDXGIFactory, that must mean we are taking the secret
// path for getting the swap chain.  Return a wrapped version whenever this happens, so
// we can get access later.

STDMETHODIMP HackerDXGIAdapter::GetParent(THIS_
	/* [annotation][in] */
	__in  REFIID riid,
	/* [annotation][retval][out] */
	__out  void **ppParent)
{
	HRESULT hr;

	LogInfo("HackerDXGIAdapter::GetParent called with IID: %s \n", NameFromIID(riid).c_str());

	if (riid == __uuidof(IDXGIFactory))
	{
		IDXGIFactory *origFactory;
		hr = mOrigAdapter->GetParent(riid, (void**)(&origFactory));

		HackerDXGIFactory *factoryWrap = new HackerDXGIFactory(origFactory, mHackerDevice, mHackerContext);
		if (factoryWrap == NULL)
		{
			LogInfo("  error allocating dxgiFactoryWrap. \n");
			return E_OUTOFMEMORY;
		}
		if (ppParent)
			*ppParent = factoryWrap;

		LogInfo("  created HackerDXGIFactory wrapper = %p of %p \n", factoryWrap, origFactory);
	}
	else
	{
		hr = mOrigAdapter->GetParent(riid, ppParent);
	}

	LogInfo("  returns result = %x\n", hr);
	return hr;
}


// -----------------------------------------------------------------------------

STDMETHODIMP HackerDXGIAdapter1::GetDesc1(THIS_
	/* [annotation][out] */
	__out  DXGI_ADAPTER_DESC1 *pDesc)
{
	LogInfo("HackerDXGIAdapter1::GetDesc1 called\n");

	HRESULT hr = mOrigAdapter1->GetDesc1(pDesc);
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


// -----------------------------------------------------------------------------

STDMETHODIMP_(ULONG) HackerDXGIOutput::AddRef(THIS)
{
	return mOrigOutput->AddRef();
}

STDMETHODIMP_(ULONG) HackerDXGIOutput::Release(THIS)
{
	ULONG ulRef = mOrigOutput->Release();
	LogInfo("HackerDXGIOutput::Release counter=%d, this=%p\n", ulRef, this);

	if (ulRef <= 0)
	{
		LogInfo("  deleting self\n");

		delete this;
		return 0L;
	}
	return ulRef;
}

STDMETHODIMP HackerDXGIOutput::GetDesc(THIS_ 
        /* [annotation][out] */ 
        __out  DXGI_OUTPUT_DESC *pDesc)
{
	LogInfo("HackerDXGIOutput::GetDesc called\n");
	
	HRESULT ret = mOrigOutput->GetDesc(pDesc);
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

STDMETHODIMP HackerDXGIOutput::GetDisplayModeList(THIS_ 
        /* [in] */ DXGI_FORMAT EnumFormat,
        /* [in] */ UINT Flags,
        /* [annotation][out][in] */ 
        __inout  UINT *pNumModes,
        /* [annotation][out] */ 
        __out_ecount_part_opt(*pNumModes,*pNumModes)  DXGI_MODE_DESC *pDesc)
{
	LogInfo("HackerDXGIOutput::GetDisplayModeList called\n");
	
	HRESULT ret = mOrigOutput->GetDisplayModeList(EnumFormat, Flags, pNumModes, pDesc);
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
        
STDMETHODIMP HackerDXGIOutput::FindClosestMatchingMode(THIS_ 
        /* [annotation][in] */ 
        __in  const DXGI_MODE_DESC *pModeToMatch,
        /* [annotation][out] */ 
        __out  DXGI_MODE_DESC *pClosestMatch,
        /* [annotation][in] */ 
        __in_opt  IUnknown *pConcernedDevice)
{
	if (pModeToMatch) LogInfo("HackerDXGIOutput::FindClosestMatchingMode called: width=%d, height=%d, refresh rate=%f\n", 
		pModeToMatch->Width, pModeToMatch->Height, (float) pModeToMatch->RefreshRate.Numerator / (float) pModeToMatch->RefreshRate.Denominator);
	
	HRESULT hr = mOrigOutput->FindClosestMatchingMode(pModeToMatch, pClosestMatch, pConcernedDevice);

	if (pClosestMatch && SCREEN_REFRESH >= 0)
	{
		pClosestMatch->RefreshRate.Numerator = SCREEN_REFRESH;
		pClosestMatch->RefreshRate.Denominator = 1;
	}
	if (pClosestMatch && SCREEN_WIDTH >= 0) pClosestMatch->Width = SCREEN_WIDTH;
	if (pClosestMatch && SCREEN_HEIGHT >= 0) pClosestMatch->Height = SCREEN_HEIGHT;
	if (pClosestMatch) LogInfo("  returning width=%d, height=%d, refresh rate=%f\n", 
		pClosestMatch->Width, pClosestMatch->Height, (float) pClosestMatch->RefreshRate.Numerator / (float) pClosestMatch->RefreshRate.Denominator);
	
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::WaitForVBlank(THIS_ )
{
	LogInfo("HackerDXGIOutput::WaitForVBlank called\n");
	HRESULT hr = mOrigOutput->WaitForVBlank();
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::TakeOwnership(THIS_  
        /* [annotation][in] */ 
        __in  IUnknown *pDevice,
        BOOL Exclusive)
{
	LogInfo("HackerDXGIOutput::TakeOwnership called\n");
	HRESULT hr = mOrigOutput->TakeOwnership(pDevice, Exclusive);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
void STDMETHODCALLTYPE HackerDXGIOutput::ReleaseOwnership(void)
{
	LogInfo("HackerDXGIOutput::ReleaseOwnership called\n");
	return mOrigOutput->ReleaseOwnership();
}
        
STDMETHODIMP HackerDXGIOutput::GetGammaControlCapabilities(THIS_  
        /* [annotation][out] */ 
        __out  DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps)
{
	LogInfo("HackerDXGIOutput::GetGammaControlCapabilities called\n");
	HRESULT hr = mOrigOutput->GetGammaControlCapabilities(pGammaCaps);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::SetGammaControl(THIS_ 
        /* [annotation][in] */ 
        __in  const DXGI_GAMMA_CONTROL *pArray)
{
	LogInfo("HackerDXGIOutput::SetGammaControl called\n");
	HRESULT hr = mOrigOutput->SetGammaControl(pArray);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::GetGammaControl(THIS_ 
        /* [annotation][out] */ 
        __out  DXGI_GAMMA_CONTROL *pArray)
{
	LogInfo("HackerDXGIOutput::GetGammaControl called\n");
	HRESULT hr = mOrigOutput->GetGammaControl(pArray);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::SetDisplaySurface(THIS_ 
        /* [annotation][in] */ 
        __in  IDXGISurface *pScanoutSurface)
{
	LogInfo("HackerDXGIOutput::SetDisplaySurface called\n");
	HRESULT hr = mOrigOutput->SetDisplaySurface(pScanoutSurface);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::GetDisplaySurfaceData(THIS_ 
        /* [annotation][in] */ 
        __in  IDXGISurface *pDestination)
{
	LogInfo("HackerDXGIOutput::GetDisplaySurfaceData called\n");
	HRESULT hr = mOrigOutput->GetDisplaySurfaceData(pDestination);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::GetFrameStatistics(THIS_ 
        /* [annotation][out] */ 
        __out  DXGI_FRAME_STATISTICS *pStats)
{
	LogInfo("HackerDXGIOutput::GetFrameStatistics called\n");
	HRESULT hr = mOrigOutput->GetFrameStatistics(pStats);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
               
STDMETHODIMP HackerDXGIOutput::SetPrivateData(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [in] */ UINT DataSize,
            /* [annotation][in] */ 
            __in_bcount(DataSize)  const void *pData)
{
	LogInfo("HackerDXGIOutput::SetPrivateData called\n");
	HRESULT hr = mOrigOutput->SetPrivateData(Name, DataSize, pData);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::SetPrivateDataInterface(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][in] */ 
            __in  const IUnknown *pUnknown)
{
	LogInfo("HackerDXGIOutput::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		Name.Data1, Name.Data2, Name.Data3, Name.Data4[0], Name.Data4[1], Name.Data4[2], Name.Data4[3], 
		Name.Data4[4], Name.Data4[5], Name.Data4[6], Name.Data4[7]);
	
	HRESULT hr = mOrigOutput->SetPrivateDataInterface(Name, pUnknown);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::GetPrivateData(THIS_
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][out][in] */ 
            __inout  UINT *pDataSize,
            /* [annotation][out] */ 
            __out_bcount(*pDataSize)  void *pData)
{
	LogInfo("HackerDXGIOutput::GetPrivateData called\n");
	HRESULT hr = mOrigOutput->GetPrivateData(Name, pDataSize, pData);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::GetParent(THIS_ 
            /* [annotation][in] */ 
            __in  REFIID riid,
            /* [annotation][retval][out] */ 
            __out  void **ppParent)
{
	LogInfo("HackerDXGIOutput::GetParent called with riid=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);
	
	HRESULT hr = mOrigOutput->GetParent(riid, ppParent);
	LogInfo("  returns result = %x, handle = %p\n", hr, *ppParent);
	return hr;
}


// -----------------------------------------------------------------------------

STDMETHODIMP HackerDXGIDeviceSubObject::GetDevice(
	/* [annotation][in] */
	_In_  REFIID riid,
	/* [annotation][retval][out] */
	_Out_  void **ppDevice)
{
	LogInfo("HackerDXGIDeviceSubObject::GetDevice called with IID: %s \n", NameFromIID(riid).c_str());

	HRESULT hr = mOrigDeviceSubObject->GetDevice(riid, ppDevice);
	LogInfo("  returns result = %x, handle = %p\n", hr, *ppDevice);
	return hr;
}


// -----------------------------------------------------------------------------

IDXGISwapChain* HackerDXGISwapChain::GetOrigSwapChain()
{
	return mOrigSwapChain;
}

STDMETHODIMP HackerDXGISwapChain::Present(THIS_
            /* [in] */ UINT SyncInterval,
            /* [in] */ UINT Flags)
{
	LogDebug("HackerDXGISwapChain::Present called with\n");
	LogDebug("  SyncInterval = %d\n", SyncInterval);
	LogDebug("  Flags = %d\n", Flags);

	// Draw the on-screen overlay text with hunting info, before final Present.
	// But only when hunting is enabled, this will also make it obvious when
	// hunting is on.
	if (G->hunting)
		mOverlay->DrawOverlay();

	HRESULT hr = mOrigSwapChain->Present(SyncInterval, Flags);

	LogDebug("  returns %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::GetBuffer(THIS_
            /* [in] */ UINT Buffer,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][out][in] */ 
            _Out_  void **ppSurface)
{
	LogInfo("HackerDXGISwapChain::GetBuffer called \n");
	HRESULT hr = mOrigSwapChain->GetBuffer(Buffer, riid, ppSurface);
	LogInfo("  returns %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::SetFullscreenState(THIS_
            /* [in] */ BOOL Fullscreen,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pTarget)
{
	LogInfo("HackerDXGISwapChain::SetFullscreenState called with\n");
	LogInfo("  Fullscreen = %d\n", Fullscreen);
	LogInfo("  Target = %p\n", pTarget);

	HRESULT hr;
	//if (pTarget)	
	//	hr = mOrigSwapChain->SetFullscreenState(Fullscreen, pTarget->m_pOutput);
	//else
	//	hr = mOrigSwapChain->SetFullscreenState(Fullscreen, 0);

	hr = mOrigSwapChain->SetFullscreenState(Fullscreen, pTarget);
	LogInfo("  returns %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::GetFullscreenState(THIS_
            /* [annotation][out] */ 
            _Out_opt_  BOOL *pFullscreen,
            /* [annotation][out] */ 
            _Out_opt_  IDXGIOutput **ppTarget)
{
	LogDebug("HackerDXGISwapChain::GetFullscreenState called\n");
	
	//IDXGIOutput *origOutput;
	//HRESULT hr = mOrigSwapChain->GetFullscreenState(pFullscreen, &origOutput);
	//if (hr == S_OK)
	//{
	//	*ppTarget = IDXGIOutput::GetDirectOutput(origOutput);
	//	if (pFullscreen) LogInfo("  returns Fullscreen = %d\n", *pFullscreen);
	//	if (ppTarget) LogInfo("  returns target IDXGIOutput = %x, wrapper = %x\n", origOutput, *ppTarget);
	//}

	HRESULT hr = mOrigSwapChain->GetFullscreenState(pFullscreen, ppTarget);
	LogDebug("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::GetDesc(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_SWAP_CHAIN_DESC *pDesc)
{
	LogInfo("HackerDXGISwapChain::GetDesc called\n");
	
	HRESULT hr = mOrigSwapChain->GetDesc(pDesc);
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
        
STDMETHODIMP HackerDXGISwapChain::ResizeBuffers(THIS_
            /* [in] */ UINT BufferCount,
            /* [in] */ UINT Width,
            /* [in] */ UINT Height,
            /* [in] */ DXGI_FORMAT NewFormat,
            /* [in] */ UINT SwapChainFlags)
{
	LogInfo("HackerDXGISwapChain::ResizeBuffers called\n");
	HRESULT hr = mOrigSwapChain->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);
	LogInfo("  returns result = %x\n", hr); 
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::ResizeTarget(THIS_
            /* [annotation][in] */ 
            _In_  const DXGI_MODE_DESC *pNewTargetParameters)
{
	LogInfo("HackerDXGISwapChain::ResizeTarget called\n");
	HRESULT hr = mOrigSwapChain->ResizeTarget(pNewTargetParameters);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::GetContainingOutput(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppOutput)
{
	LogInfo("HackerDXGISwapChain::GetContainingOutput called\n");
	
	//IDXGIOutput *origOutput;
	//HRESULT hr = mOrigSwapChain->GetContainingOutput(&origOutput);
	//if (hr == S_OK)
	//{
	//	*ppOutput = IDXGIOutput::GetDirectOutput(origOutput);
	//}

	HRESULT hr = mOrigSwapChain->GetContainingOutput(ppOutput);
	LogInfo("  returns result = %x, handle = %p \n", hr, *ppOutput);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::GetFrameStatistics(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_FRAME_STATISTICS *pStats)
{
	LogInfo("HackerDXGISwapChain::GetFrameStatistics called\n");
	HRESULT hr = mOrigSwapChain->GetFrameStatistics(pStats);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::GetLastPresentCount(THIS_
            /* [annotation][out] */ 
            _Out_  UINT *pLastPresentCount)
{
	LogInfo("HackerDXGISwapChain::GetLastPresentCount called\n");
	HRESULT hr = mOrigSwapChain->GetLastPresentCount(pLastPresentCount);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}



// -----------------------------------------------------------------------------

STDMETHODIMP HackerDXGISwapChain1::GetDesc1(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_SWAP_CHAIN_DESC1 *pDesc)
{
	LogInfo("HackerDXGISwapChain1::GetDesc1 called\n");
	
	HRESULT hr = mOrigSwapChain1->GetDesc1(pDesc);
	if (hr == S_OK)
	{
		if (pDesc) LogInfo("  returns Stereo = %d\n", pDesc->Stereo);
		if (pDesc) LogInfo("  returns Width = %d\n", pDesc->Width);
		if (pDesc) LogInfo("  returns Height = %d\n", pDesc->Height);
	}
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::GetFullscreenDesc(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc)
{
	LogInfo("HackerDXGISwapChain1::GetFullscreenDesc called\n");
	
	HRESULT hr = mOrigSwapChain1->GetFullscreenDesc(pDesc);
	if (hr == S_OK)
	{
		if (pDesc) LogInfo("  returns Windowed = %d\n", pDesc->Windowed);
		if (pDesc) LogInfo("  returns Refresh rate = %f\n", 
			(float) pDesc->RefreshRate.Numerator / (float) pDesc->RefreshRate.Denominator);
	}
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::GetHwnd(THIS_
            /* [annotation][out] */ 
            _Out_  HWND *pHwnd)
{
	LogInfo("HackerDXGISwapChain1::GetHwnd called\n");
	HRESULT hr = mOrigSwapChain1->GetHwnd(pHwnd);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::GetCoreWindow(THIS_
            /* [annotation][in] */ 
            _In_  REFIID refiid,
            /* [annotation][out] */ 
            _Out_  void **ppUnk)
{
	LogInfo("HackerDXGISwapChain1::GetCoreWindow called\n");
	HRESULT hr = mOrigSwapChain1->GetCoreWindow(refiid, ppUnk);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::Present1(THIS_
            /* [in] */ UINT SyncInterval,
            /* [in] */ UINT PresentFlags,
            /* [annotation][in] */ 
            _In_  const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
	LogInfo("HackerDXGISwapChain1::Present1 called\n");
	LogInfo("  SyncInterval = %d\n", SyncInterval);
	LogInfo("  PresentFlags = %d\n", PresentFlags);
	
	HRESULT hr = mOrigSwapChain1->Present1(SyncInterval, PresentFlags, pPresentParameters);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP_(BOOL) HackerDXGISwapChain1::IsTemporaryMonoSupported(THIS)
{
	LogInfo("HackerDXGISwapChain1::IsTemporaryMonoSupported called\n");
	BOOL ret = mOrigSwapChain1->IsTemporaryMonoSupported();
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP HackerDXGISwapChain1::GetRestrictToOutput(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppRestrictToOutput)
{
	LogInfo("HackerDXGISwapChain1::GetRestrictToOutput called\n");
	HRESULT hr = mOrigSwapChain1->GetRestrictToOutput(ppRestrictToOutput);
	LogInfo("  returns result = %x, handle = %p \n", hr, *ppRestrictToOutput);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::SetBackgroundColor(THIS_
            /* [annotation][in] */ 
            _In_  const DXGI_RGBA *pColor)
{
	LogInfo("HackerDXGISwapChain1::SetBackgroundColor called\n");
	HRESULT hr = mOrigSwapChain1->SetBackgroundColor(pColor);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::GetBackgroundColor(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_RGBA *pColor)
{
	LogInfo("HackerDXGISwapChain1::GetBackgroundColor called\n");
	HRESULT hr = mOrigSwapChain1->GetBackgroundColor(pColor);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::SetRotation(THIS_
            /* [annotation][in] */ 
            _In_  DXGI_MODE_ROTATION Rotation)
{
	LogInfo("HackerDXGISwapChain1::SetRotation called\n");
	HRESULT hr = mOrigSwapChain1->SetRotation(Rotation);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::GetRotation(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_MODE_ROTATION *pRotation)
{
	LogInfo("HackerDXGISwapChain1::GetRotation called\n");
	HRESULT hr = mOrigSwapChain1->GetRotation(pRotation);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

