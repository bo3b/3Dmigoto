
#include "HackerDXGI.h"

#include "log.h"
#include "util.h"
#include "globals.h"
#include "Hunting.h"
#include "Override.h"
#include "IniHandler.h"


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

// Worth noting- the device and context for the secret path are the Hacker 
// versions, because we need access to their fields later.
HackerDXGIDevice::HackerDXGIDevice(IDXGIDevice *pDXGIDevice, HackerDevice *pDevice, HackerContext *pContext)
	: HackerDXGIObject(pDXGIDevice)
{
	mOrigDXGIDevice = pDXGIDevice;
	mHackerDevice = pDevice;
	mHackerContext = pContext;
}

HackerDXGIDevice1::HackerDXGIDevice1(IDXGIDevice1 *pDXGIDevice, HackerDevice *pDevice, HackerContext *pContext)
	: HackerDXGIDevice(pDXGIDevice, pDevice, pContext)
{
	mOrigDXGIDevice1 = pDXGIDevice;
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
	: HackerDXGIObject(pOutput)
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

	mHackerDevice = pDevice;
	pDevice->SetHackerSwapChain(this);

	// Create Overlay class that will be responsible for drawing any text
	// info over the game. Using the Hacker Device and Context we gave the game.
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
	LogInfo("HackerUnknown::AddRef(%s@%p), counter=%d, this=%p \n", typeid(*this).name(), this, ulRef, this);
	return ulRef;
}

STDMETHODIMP_(ULONG) HackerUnknown::Release(THIS)
{
	ULONG ulRef = mOrigUnknown->Release();
	LogInfo("HackerUnknown::Release(%s@%p), counter=%d, this=%p \n", typeid(*this).name(), this, ulRef, this);

	if (ulRef <= 0)
	{
		LogInfo("  counter=%d, this=%p, deleting self. \n", ulRef, this);

		delete this;
		return 0L;
	}
	return ulRef;
}

STDMETHODIMP HackerUnknown::QueryInterface(THIS_
	/* [in] */ REFIID riid,
	/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	LogDebug("HackerUnknown::QueryInterface(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(riid).c_str());
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
	LogInfo("HackerDXGIObject::SetPrivateData(%s@%p) called with GUID: %s \n", typeid(*this).name(), this, NameFromIID(Name).c_str());
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
	LogInfo("HackerDXGIObject::SetPrivateDataInterface(%s@%p) called with GUID: %s \n", typeid(*this).name(), this, NameFromIID(Name).c_str());

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
	LogInfo("HackerDXGIObject::GetPrivateData(%s@%p) called with GUID: %s \n", typeid(*this).name(), this, NameFromIID(Name).c_str());

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
	LogInfo("HackerDXGIObject::GetParent(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(riid).c_str());

	HRESULT hr = mOrigObject->GetParent(riid, ppParent);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}


// -----------------------------------------------------------------------------

// Let's wrap GetAdapter as well, it appears that Ori calls this instead of 
// GetParent, which is their bug, but we need to work around it.

STDMETHODIMP HackerDXGIDevice::GetAdapter(
	/* [annotation][out] */
	_Out_  HackerDXGIAdapter **pAdapter)
{
	LogInfo("HackerDXGIDevice::GetAdapter(%s@%p) called with: %p \n", typeid(*this).name(), this, pAdapter);

	IDXGIAdapter *origAdapter;
	HRESULT hr = mOrigDXGIDevice->GetAdapter(&origAdapter);

	HackerDXGIAdapter *adapterWrap = new HackerDXGIAdapter(origAdapter, mHackerDevice, mHackerContext);
	if (adapterWrap == NULL)
	{
		LogInfo("  error allocating dxgiAdapterWrap. \n");
		return E_OUTOFMEMORY;
	}

	// Return the wrapped version which the game will use for follow on calls.
	if (pAdapter)
		*pAdapter = adapterWrap;

	LogInfo("  created HackerDXGIAdapter wrapper = %p of %p \n", adapterWrap, origAdapter);
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
	LogInfo("HackerDXGIDevice::CreateSurface(%s@%p) called \n", typeid(*this).name(), this);
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
	LogInfo("HackerDXGIDevice::QueryResourceResidency(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigDXGIDevice->QueryResourceResidency(ppResources, pResidencyStatus, NumResources);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIDevice::SetGPUThreadPriority(
	/* [in] */ INT Priority)
{
	LogInfo("HackerDXGIDevice::SetGPUThreadPriority(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigDXGIDevice->SetGPUThreadPriority(Priority);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIDevice::GetGPUThreadPriority(
	/* [annotation][retval][out] */
	_Out_  INT *pPriority)
{
	LogInfo("HackerDXGIDevice::GetGPUThreadPriority(%s@%p) called \n", typeid(*this).name(), this);
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

	LogInfo("HackerDXGIDevice::GetParent(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(riid).c_str());

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

STDMETHODIMP HackerDXGIDevice1::SetMaximumFrameLatency(
	/* [in] */ UINT MaxLatency)
{
	LogInfo("HackerDXGIDevice1::SetMaximumFrameLatency(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigDXGIDevice1->SetMaximumFrameLatency(MaxLatency);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIDevice1::GetMaximumFrameLatency(
	/* [annotation][out] */
	_Out_  UINT *pMaxLatency)
{
	LogInfo("HackerDXGIDevice1::GetMaximumFrameLatency(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigDXGIDevice1->GetMaximumFrameLatency(pMaxLatency);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

// -----------------------------------------------------------------------------

// Given an input pDevice, we want to reset our mHackerDevice and mHackerContext
// references to use that object.  This can happen if the factory is created
// directly by the game, when no device is available.
//
// We really expect that any given input pDevice will already have be successfully
// wrapped to HackerDevice.

void HackerDXGIFactory::SetHackerObjects(IUnknown *pDevice)
{
	try
	{
		LogInfo("HackerDXGIFactory::SetHackerObjects(%s@%p) called with device: %s. \n", typeid(*this).name(), this, typeid(*pDevice).name());
		if (typeid(*pDevice) == typeid(HackerDevice))
		{
			mHackerDevice = static_cast<HackerDevice*>(pDevice);
			mHackerContext = mHackerDevice->GetHackerContext();
		}
	}
	catch (...)		// typeid throws exception if no RTTI
	{
		__debugbreak();
	}
}


// https://msdn.microsoft.com/en-us/library/windows/desktop/hh404556(v=vs.85).aspx
//
// We need to override the QueryInterface here, in the case the caller uses
// IDXGIFactory::QueryInterface to create a IDXGIFactory2.
//
// For our purposes, it presently makes more sense to return an error of E_NOINTERFACE
// for this request, because the caller must surely be able to handle a downlevel
// system, and that is better for us in general. Only early access and beta
// releases are requiring Factory2, and when wrapping those, we get a crash of
// some form, that may not be our fault.
// 
// Note that we can expect this QueryInterface to get called for any 
// HackerFactory1::QueryInterface, as the superclass, to return that Factory2.

STDMETHODIMP HackerDXGIFactory::QueryInterface(THIS_
	/* [in] */ REFIID riid,
	/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	LogDebug("HackerDXGIFactory::QueryInterface(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(riid).c_str());

	HRESULT hr;

	if (riid == __uuidof(IDXGIFactory2))
	{
		// If we are being requested to create a DXGIFactory2, lie and say it's not possible.
		LogInfo("  returns E_NOINTERFACE as error. \n");
		*ppvObject = NULL;
		return E_NOINTERFACE;

		// For when we need to return a legit Factory2.  Crashes at present.
		//hr = mOrigFactory->QueryInterface(riid, ppvObject);
		//HackerDXGIFactory2 *factory2Wrap = new HackerDXGIFactory2(static_cast<IDXGIFactory2*>(*ppvObject), mHackerDevice, mHackerContext);
		//LogInfo("  created HackerDXGIFactory2 wrapper = %p of %p \n", factory2Wrap, *ppvObject);

		//if (factory2Wrap == NULL)
		//{
		//	LogInfo("  error allocating factory2Wrap. \n");
		//	return E_OUTOFMEMORY;
		//}

		//*ppvObject = factory2Wrap;
	}
	
	hr = mOrigFactory->QueryInterface(riid, ppvObject);

	LogDebug("  returns result = %x for %p \n", hr, ppvObject);
	return hr;
}

STDMETHODIMP HackerDXGIFactory::EnumAdapters(THIS_
            /* [in] */ UINT Adapter,
            /* [annotation][out] */ 
			__out HackerDXGIAdapter **ppAdapter)
{
	LogInfo("HackerDXGIFactory::EnumAdapters(%s@%p) adapter %d requested\n", typeid(*this).name(), this, Adapter);

	IDXGIAdapter *origAdapter;
	HRESULT hr = mOrigFactory->EnumAdapters(Adapter, &origAdapter);

	HackerDXGIAdapter *adapterWrap = new HackerDXGIAdapter(origAdapter, mHackerDevice, mHackerContext);
	if (adapterWrap == NULL)
	{
		LogInfo("  error allocating dxgiAdapterWrap. \n");
		return E_OUTOFMEMORY;
	}

	// Return the wrapped version which the game will use for follow on calls.
	if (ppAdapter)
		*ppAdapter = adapterWrap;

	LogInfo("  created HackerDXGIAdapter wrapper = %p of %p \n", adapterWrap, origAdapter);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIFactory::MakeWindowAssociation(THIS_
            HWND WindowHandle,
            UINT Flags)
{
	if (LogFile)
	{
		LogInfo("HackerDXGIFactory::MakeWindowAssociation(%s@%p) called with WindowHandle = %p, Flags = %x\n", typeid(*this).name(), this, WindowHandle, Flags);
		if (Flags) LogInfo("  Flags =");
		if (Flags & DXGI_MWA_NO_WINDOW_CHANGES) LogInfo(" DXGI_MWA_NO_WINDOW_CHANGES(no monitoring)");
		if (Flags & DXGI_MWA_NO_ALT_ENTER) LogInfo(" DXGI_MWA_NO_ALT_ENTER");
		if (Flags & DXGI_MWA_NO_PRINT_SCREEN) LogInfo(" DXGI_MWA_NO_PRINT_SCREEN");
		if (Flags) LogInfo("\n");
	}

	if (G->SCREEN_ALLOW_COMMANDS && Flags)
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
	LogInfo("HackerDXGIFactory::GetWindowAssociation(%s@%p) called\n", typeid(*this).name(), this);
	HRESULT hr = mOrigFactory->GetWindowAssociation(pWindowHandle);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

// This tweaks the parameters passed to the real CreateSwapChain, to change behavior.
// These global parameters come originally from the d3dx.ini, so the user can
// change them.
// This is also used by D3D11::CreateSwapChainAndDevice.
//
// It might make sense to move this to Utils, where nvapi can access it too.

void ForceDisplayParams(DXGI_SWAP_CHAIN_DESC *pDesc)
{
	if (pDesc == NULL)
		return;

	LogInfo("     Windowed = %d \n", pDesc->Windowed);
	LogInfo("     Width = %d \n", pDesc->BufferDesc.Width);
	LogInfo("     Height = %d \n", pDesc->BufferDesc.Height);
	LogInfo("     Refresh rate = %f \n",
		(float)pDesc->BufferDesc.RefreshRate.Numerator / (float)pDesc->BufferDesc.RefreshRate.Denominator);

	if (G->SCREEN_FULLSCREEN)
	{
		pDesc->Windowed = false;
		LogInfo("->Forcing Windowed to = %d \n", pDesc->Windowed);
	}

	if (G->SCREEN_REFRESH >= 0 && !pDesc->Windowed)
	{
		pDesc->BufferDesc.RefreshRate.Numerator = G->SCREEN_REFRESH;
		pDesc->BufferDesc.RefreshRate.Denominator = 1;
		LogInfo("->Forcing refresh rate to = %f \n",
			(float)pDesc->BufferDesc.RefreshRate.Numerator / (float)pDesc->BufferDesc.RefreshRate.Denominator);
	}
	if (G->SCREEN_WIDTH >= 0)
	{
		pDesc->BufferDesc.Width = G->SCREEN_WIDTH;
		LogInfo("->Forcing Width to = %d \n", pDesc->BufferDesc.Width);
	}
	if (G->SCREEN_HEIGHT >= 0)
	{
		pDesc->BufferDesc.Height = G->SCREEN_HEIGHT;
		LogInfo("->Forcing Height to = %d \n", pDesc->BufferDesc.Height);
	}
}


// For any given SwapChain created by the factory here, we want to wrap the 
// SwapChain so that we can get called when Present() is called.
//
// When this is called by a game that creates their swap chain directly from the 
// Factory object, it's possible that mHackerDevice and mHackerContext are null,
// because they don't exist when the factory is instantiated.
// Because of this, we want to check for null, and if so, set those up here, 
// because we have the input pDevice that we can use to wrap.
//
// We always expect the pDevice passed in here to be a HackerDevice. If we
// get one that is not, we have an object leak/bug. It shouldn't be possible to
// create a ID3D11Device without us wrapping it.
//
// When creating the new swap chain, we need to pass the original device, not
// the wrapped version. For some reason, passing the wrapped version actually
// succeeds if the "evil" update is installed, which I would not expect.  Without
// the platform update, it would crash here.

STDMETHODIMP HackerDXGIFactory::CreateSwapChain(THIS_
            /* [annotation][in] */ 
			__in  HackerDevice *pDevice,
            /* [annotation][in] */ 
            __in  DXGI_SWAP_CHAIN_DESC *pDesc,
            /* [annotation][out] */ 
            __out  HackerDXGISwapChain **ppSwapChain)
{
	LogInfo("HackerDXGIFactory::CreateSwapChain(%s@%p) called with parameters\n", typeid(*this).name(), this);
	LogInfo("  Device = %p\n", pDevice);

	ForceDisplayParams(pDesc);

	IDXGISwapChain *origSwapChain = 0;
	HRESULT hr = mOrigFactory->CreateSwapChain(pDevice->GetOrigDevice(), pDesc, &origSwapChain);

	// This call can DXGI_STATUS_OCCLUDED.  That means that the window
	// can be occluded when we return from creating the real swap chain.  
	// The check below was looking ONLY for S_OK, and that would lead to it skipping the sub-block
	// which set up ppSwapChain and returned it.  So, ppSwapChain==NULL and it would crash, sometimes.
	// There are other legitimate DXGI_STATUS results, so checking for SUCCEEDED is the correct way.
	// Same bug fix is applied for the other CreateSwapChain* variants.

	if (SUCCEEDED(hr))
	{
		if (mHackerDevice == NULL || mHackerContext == NULL)
			this->SetHackerObjects(pDevice);

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
	
	LogInfo("  return value = %x, swapchain: %p, swapchain wrap: %p \n", hr, origSwapChain, *ppSwapChain);
	return hr;
}

STDMETHODIMP HackerDXGIFactory::CreateSoftwareAdapter(THIS_
	/* [in] */ HMODULE Module,
	/* [annotation][out] */
	__out  IDXGIAdapter **ppAdapter)
{
	LogInfo("HackerDXGIFactory::CreateSoftwareAdapter(%s@%p) called\n", typeid(*this).name(), this);
	HRESULT hr = mOrigFactory->CreateSoftwareAdapter(Module, ppAdapter);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}


// -----------------------------------------------------------------------------

// We do not need to override the QueryInterface here, in the case the caller uses
// IDXGIFactory1::QueryInterface to create a IDXGIFactory2.
// The superclass of HackerDXGIFactory::QueryInterface will be called, as long as
// we successfully wrapped the factory in the first place.


STDMETHODIMP HackerDXGIFactory1::EnumAdapters1(THIS_
	/* [in] */ UINT Adapter,
	/* [annotation][out] */
	__out  IDXGIAdapter1 **ppAdapter)

{
	LogInfo("HackerDXGIFactory1::EnumAdapters1(%s@%p) called: adapter #%d requested\n", typeid(*this).name(), this, Adapter);

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
	LogInfo("HackerDXGIFactory1::IsCurrent(%s@%p) called\n", typeid(*this).name(), this);
	HRESULT hr = mOrigFactory1->IsCurrent();
	LogInfo("  returns result = %x\n", hr);
	return hr;
}


// -----------------------------------------------------------------------------

STDMETHODIMP HackerDXGIFactory2::CreateSwapChainForHwnd(THIS_
            /* [annotation][in] */ 
            _In_  HackerDevice *pDevice,
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
	LogInfo("HackerDXGIFactory2::CreateSwapChainForHwnd(%s@%p) called with parameters\n", typeid(*this).name(), this);
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
	HRESULT hr = mOrigFactory2->CreateSwapChainForHwnd(pDevice->GetOrigDevice(), hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
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
			_In_  HackerDevice *pDevice,
            /* [annotation][in] */ 
            _In_  IUnknown *pWindow,
            /* [annotation][in] */ 
			_In_ const DXGI_SWAP_CHAIN_DESC1 *pDesc,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pRestrictToOutput,
            /* [annotation][out] */ 
            _Out_  IDXGISwapChain1 **ppSwapChain)
{
	LogInfo("HackerDXGIFactory2::CreateSwapChainForCoreWindow(%s@%p) called with parameters\n", typeid(*this).name(), this);
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
	HRESULT	hr = mOrigFactory2->CreateSwapChainForCoreWindow(pDevice->GetOrigDevice(), pWindow, pDesc, pRestrictToOutput, ppSwapChain);
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
			_In_  HackerDevice *pDevice,
            /* [annotation][in] */ 
            _In_ const DXGI_SWAP_CHAIN_DESC1 *pDesc,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pRestrictToOutput,
            /* [annotation][out] */ 
            _Outptr_  IDXGISwapChain1 **ppSwapChain)
{
	LogInfo("HackerDXGIFactory2::CreateSwapChainForComposition(%s@%p) called with parameters\n", typeid(*this).name(), this);
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

	HRESULT	hr = mOrigFactory2->CreateSwapChainForComposition(pDevice->GetOrigDevice(), pDesc, pRestrictToOutput, ppSwapChain);
	LogInfo("  return value = %x\n", hr);
	return hr;
}

STDMETHODIMP_(BOOL) HackerDXGIFactory2::IsWindowedStereoEnabled(THIS)
{
	LogInfo("HackerDXGIFactory2::IsWindowedStereoEnabled(%s@%p) called \n", typeid(*this).name(), this);
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
	LogInfo("HackerDXGIFactory2::GetSharedResourceAdapterLuid(%s@%p) called \n", typeid(*this).name(), this);
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
	LogInfo("HackerDXGIFactory2::RegisterStereoStatusWindow(%s@%p) called \n", typeid(*this).name(), this);
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
	LogInfo("HackerDXGIFactory2::RegisterStereoStatusEvent(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT ret = mOrigFactory2->RegisterStereoStatusEvent(hEvent, pdwCookie);
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP_(void) HackerDXGIFactory2::UnregisterStereoStatus(THIS_
            /* [annotation][in] */ 
            _In_  DWORD dwCookie)
{
	LogInfo("HackerDXGIFactory2::UnregisterStereoStatus(%s@%p) called \n", typeid(*this).name(), this);
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
	LogInfo("HackerDXGIFactory2::RegisterOcclusionStatusWindow(%s@%p) called \n", typeid(*this).name(), this);
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
	LogInfo("HackerDXGIFactory2::RegisterOcclusionStatusEvent(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT ret = mOrigFactory2->RegisterOcclusionStatusEvent(hEvent, pdwCookie);
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP_(void) HackerDXGIFactory2::UnregisterOcclusionStatus(THIS_
            /* [annotation][in] */ 
            _In_  DWORD dwCookie)
{
	LogInfo("HackerDXGIFactory2::UnregisterOcclusionStatus(%s@%p) called \n", typeid(*this).name(), this);
	mOrigFactory2->UnregisterOcclusionStatus(dwCookie);
}
        

// -----------------------------------------------------------------------------

STDMETHODIMP HackerDXGIAdapter::EnumOutputs(THIS_
            /* [in] */ UINT Output,
            /* [annotation][out][in] */ 
            __out IDXGIOutput **ppOutput)
{
	LogInfo("HackerDXGIAdapter::EnumOutputs(%s@%p) called: output #%d requested\n", typeid(*this).name(), this, Output);
	HRESULT hr = mOrigAdapter->EnumOutputs(Output, ppOutput);
	LogInfo("  returns result = %x, handle = %p\n", hr, *ppOutput);
	return hr;
}
        
STDMETHODIMP HackerDXGIAdapter::GetDesc(THIS_
            /* [annotation][out] */ 
            __out DXGI_ADAPTER_DESC *pDesc)
{
	LogInfo("HackerDXGIAdapter::GetDesc(%s@%p) called \n", typeid(*this).name(), this);
	
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
	LogInfo("HackerDXGIAdapter::CheckInterfaceSupport(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(InterfaceName).c_str());

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
//
// Can also ask for Factory1 or Factory2.
// More details: https://msdn.microsoft.com/en-us/library/windows/apps/hh465096.aspx

STDMETHODIMP HackerDXGIAdapter::GetParent(THIS_
	/* [annotation][in] */
	__in  REFIID riid,
	/* [annotation][retval][out] */
	__out  void **ppParent)
{
	HRESULT hr;

	LogInfo("HackerDXGIAdapter::GetParent(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(riid).c_str());

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
	LogInfo("HackerDXGIAdapter1::GetDesc1(%s@%p) called \n", typeid(*this).name(), this);

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

STDMETHODIMP HackerDXGIOutput::GetDesc(THIS_ 
        /* [annotation][out] */ 
        __out  DXGI_OUTPUT_DESC *pDesc)
{
	LogInfo("HackerDXGIOutput::GetDesc(%s@%p) called \n", typeid(*this).name(), this);
	
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
	if (!G->FILTER_REFRESH[0]) return false;
	int i = 0;
	while (G->FILTER_REFRESH[i] && G->FILTER_REFRESH[i] != rate)
		++i;
	return G->FILTER_REFRESH[i] == 0;
}

STDMETHODIMP HackerDXGIOutput::GetDisplayModeList(THIS_ 
        /* [in] */ DXGI_FORMAT EnumFormat,
        /* [in] */ UINT Flags,
        /* [annotation][out][in] */ 
        __inout  UINT *pNumModes,
        /* [annotation][out] */ 
        __out_ecount_part_opt(*pNumModes,*pNumModes)  DXGI_MODE_DESC *pDesc)
{
	LogInfo("HackerDXGIOutput::GetDisplayModeList(%s@%p) called \n", typeid(*this).name(), this);
	
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
				// ToDo: Does this work?  I have no idea why setting width and height to 8 would matter.
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
	if (pModeToMatch) LogInfo("HackerDXGIOutput::FindClosestMatchingMode(%s@%p) called: width=%d, height=%d, refresh rate=%f\n", typeid(*this).name(), this,
		pModeToMatch->Width, pModeToMatch->Height, (float) pModeToMatch->RefreshRate.Numerator / (float) pModeToMatch->RefreshRate.Denominator);
	
	HRESULT hr = mOrigOutput->FindClosestMatchingMode(pModeToMatch, pClosestMatch, pConcernedDevice);

	if (pClosestMatch && G->SCREEN_REFRESH >= 0)
	{
		pClosestMatch->RefreshRate.Numerator = G->SCREEN_REFRESH;
		pClosestMatch->RefreshRate.Denominator = 1;
	}
	if (pClosestMatch && G->SCREEN_WIDTH >= 0) pClosestMatch->Width = G->SCREEN_WIDTH;
	if (pClosestMatch && G->SCREEN_HEIGHT >= 0) pClosestMatch->Height = G->SCREEN_HEIGHT;
	if (pClosestMatch) LogInfo("  returning width=%d, height=%d, refresh rate=%f\n", 
		pClosestMatch->Width, pClosestMatch->Height, (float) pClosestMatch->RefreshRate.Numerator / (float) pClosestMatch->RefreshRate.Denominator);
	
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::WaitForVBlank(THIS_ )
{
	LogInfo("HackerDXGIOutput::WaitForVBlank(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigOutput->WaitForVBlank();
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::TakeOwnership(THIS_  
        /* [annotation][in] */ 
        __in  IUnknown *pDevice,
        BOOL Exclusive)
{
	LogInfo("HackerDXGIOutput::TakeOwnership(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigOutput->TakeOwnership(pDevice, Exclusive);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
void STDMETHODCALLTYPE HackerDXGIOutput::ReleaseOwnership(void)
{
	LogInfo("HackerDXGIOutput::ReleaseOwnership(%s@%p) called \n", typeid(*this).name(), this);
	return mOrigOutput->ReleaseOwnership();
}
        
STDMETHODIMP HackerDXGIOutput::GetGammaControlCapabilities(THIS_  
        /* [annotation][out] */ 
        __out  DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps)
{
	LogInfo("HackerDXGIOutput::GetGammaControlCapabilities(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigOutput->GetGammaControlCapabilities(pGammaCaps);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::SetGammaControl(THIS_ 
        /* [annotation][in] */ 
        __in  const DXGI_GAMMA_CONTROL *pArray)
{
	LogInfo("HackerDXGIOutput::SetGammaControl(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigOutput->SetGammaControl(pArray);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::GetGammaControl(THIS_ 
        /* [annotation][out] */ 
        __out  DXGI_GAMMA_CONTROL *pArray)
{
	LogInfo("HackerDXGIOutput::GetGammaControl(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigOutput->GetGammaControl(pArray);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::SetDisplaySurface(THIS_ 
        /* [annotation][in] */ 
        __in  IDXGISurface *pScanoutSurface)
{
	LogInfo("HackerDXGIOutput::SetDisplaySurface(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigOutput->SetDisplaySurface(pScanoutSurface);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::GetDisplaySurfaceData(THIS_ 
        /* [annotation][in] */ 
        __in  IDXGISurface *pDestination)
{
	LogInfo("HackerDXGIOutput::GetDisplaySurfaceData(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigOutput->GetDisplaySurfaceData(pDestination);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::GetFrameStatistics(THIS_ 
        /* [annotation][out] */ 
        __out  DXGI_FRAME_STATISTICS *pStats)
{
	LogInfo("HackerDXGIOutput::GetFrameStatistics(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigOutput->GetFrameStatistics(pStats);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
               


// -----------------------------------------------------------------------------

STDMETHODIMP HackerDXGIDeviceSubObject::GetDevice(
	/* [annotation][in] */
	_In_  REFIID riid,
	/* [annotation][retval][out] */
	_Out_  void **ppDevice)
{
	LogInfo("HackerDXGIDeviceSubObject::GetDevice(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(riid).c_str());

	HRESULT hr = mOrigDeviceSubObject->GetDevice(riid, ppDevice);
	LogInfo("  returns result = %x, handle = %p\n", hr, *ppDevice);
	return hr;
}


// -----------------------------------------------------------------------------

IDXGISwapChain* HackerDXGISwapChain::GetOrigSwapChain()
{
	LogInfo("HackerDXGISwapChain::GetOrigSwapChain returns %p", mOrigSwapChain);
	return mOrigSwapChain;
}


// Called at each DXGI::Present() to give us reliable time to execute user
// input and hunting commands.

void HackerDXGISwapChain::RunFrameActions()
{
	static ULONGLONG last_ticks = 0;
	ULONGLONG ticks = GetTickCount64();

	// Prevent excessive input processing. XInput added an extreme
	// performance hit when processing four controllers on every draw call,
	// so only process input if at least 8ms has passed (approx 125Hz - may
	// be less depending on timer resolution)
	if (ticks - last_ticks < 8)
		return;
	last_ticks = ticks;

	LogDebug("Running frame actions.  Device: %p\n", mHackerDevice);

	// Regardless of log settings, since this runs every frame, let's flush the log
	// so that the most lost will be one frame worth.  Tradeoff of performance to accuracy
	if (LogFile) fflush(LogFile);

	bool newEvent = DispatchInputEvents(mHackerDevice);

	CurrentTransition.UpdateTransitions(mHackerDevice);

	// The config file is not safe to reload from within the input handler
	// since it needs to change the key bindings, so it sets this flag
	// instead and we handle it now.
	if (G->gReloadConfigPending)
		ReloadConfig(mHackerDevice);

	// When not hunting most keybindings won't have been registered, but
	// still skip the below logic that only applies while hunting.
	if (!G->hunting)
		return;

	// Update the huntTime whenever we get fresh user input.
	if (newEvent)
		G->huntTime = time(NULL);

	// Clear buffers after some user idle time.  This allows the buffers to be
	// stable during a hunt, and cleared after one minute of idle time.  The idea
	// is to make the arrays of shaders stable so that hunting up and down the arrays
	// is consistent, while the user is engaged.  After 1 minute, they are likely onto
	// some other spot, and we should start with a fresh set, to keep the arrays and
	// active shader list small for easier hunting.  Until the first keypress, the arrays
	// are cleared at each thread wake, just like before. 
	// The arrays will be continually filled by the SetShader sections, but should 
	// rapidly converge upon all active shaders.

	if (difftime(time(NULL), G->huntTime) > 60) {
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		TimeoutHuntingBuffers();
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}
}


STDMETHODIMP HackerDXGISwapChain::Present(THIS_
            /* [in] */ UINT SyncInterval,
            /* [in] */ UINT Flags)
{
	LogDebug("HackerDXGISwapChain::Present(%s@%p) called \n", typeid(*this).name(), this);
	LogDebug("  SyncInterval = %d\n", SyncInterval);
	LogDebug("  Flags = %d\n", Flags);

	// Every presented frame, we want to take some CPU time to run our actions,
	// which enables hunting, and snapshots, and aiming overrides and other inputs
	RunFrameActions();

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
	LogDebug("HackerDXGISwapChain::GetBuffer(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(riid).c_str());

	HRESULT hr = mOrigSwapChain->GetBuffer(Buffer, riid, ppSurface);
	LogInfo("  returns %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::SetFullscreenState(THIS_
            /* [in] */ BOOL Fullscreen,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pTarget)
{
	LogInfo("HackerDXGISwapChain::SetFullscreenState(%s@%p) called with\n", typeid(*this).name(), this);
	LogInfo("  Fullscreen = %d\n", Fullscreen);
	LogInfo("  Target = %p\n", pTarget);

	if (G->SCREEN_FULLSCREEN)
	{
		Fullscreen = true;
		LogInfo("->Fullscreen forced = %d \n", Fullscreen);
	}

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
	LogDebug("HackerDXGISwapChain::GetFullscreenState(%s@%p) called \n", typeid(*this).name(), this);
	
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
	LogInfo("HackerDXGISwapChain::GetDesc(%s@%p) called \n", typeid(*this).name(), this);
	
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
	LogInfo("HackerDXGISwapChain::ResizeBuffers(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigSwapChain->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);
	LogInfo("  returns result = %x\n", hr); 
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::ResizeTarget(THIS_
            /* [annotation][in] */ 
            _In_  const DXGI_MODE_DESC *pNewTargetParameters)
{
	LogInfo("HackerDXGISwapChain::ResizeTarget(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigSwapChain->ResizeTarget(pNewTargetParameters);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::GetContainingOutput(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppOutput)
{
	LogInfo("HackerDXGISwapChain::GetContainingOutput(%s@%p) called \n", typeid(*this).name(), this);
	
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
	LogInfo("HackerDXGISwapChain::GetFrameStatistics(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigSwapChain->GetFrameStatistics(pStats);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::GetLastPresentCount(THIS_
            /* [annotation][out] */ 
            _Out_  UINT *pLastPresentCount)
{
	LogInfo("HackerDXGISwapChain::GetLastPresentCount(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigSwapChain->GetLastPresentCount(pLastPresentCount);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}



// -----------------------------------------------------------------------------

STDMETHODIMP HackerDXGISwapChain1::GetDesc1(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_SWAP_CHAIN_DESC1 *pDesc)
{
	LogInfo("HackerDXGISwapChain1::GetDesc1(%s@%p) called \n", typeid(*this).name(), this);
	
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
	LogInfo("HackerDXGISwapChain1::GetFullscreenDesc(%s@%p) called \n", typeid(*this).name(), this);
	
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
	LogInfo("HackerDXGISwapChain1::GetHwnd(%s@%p) called \n", typeid(*this).name(), this);
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
	LogInfo("HackerDXGISwapChain1::GetCoreWindow(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(refiid).c_str());

	HRESULT hr = mOrigSwapChain1->GetCoreWindow(refiid, ppUnk);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
// Not presently used because of our philosophy of only requiring code
// to work for non-platform update computers.  SwapChain1 requires the 
// platform update.

STDMETHODIMP HackerDXGISwapChain1::Present1(THIS_
            /* [in] */ UINT SyncInterval,
            /* [in] */ UINT PresentFlags,
            /* [annotation][in] */ 
            _In_  const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
	LogInfo("HackerDXGISwapChain1::Present1(%s@%p) called \n", typeid(*this).name(), this);
	LogInfo("  SyncInterval = %d\n", SyncInterval);
	LogInfo("  PresentFlags = %d\n", PresentFlags);
	
	// Every presented frame, we want to take some CPU time to run our actions,
	// which enables hunting, and snapshots, and aiming overrides and other inputs
//	RunFrameActions();

	// Draw the on-screen overlay text with hunting info, before final Present.
	// But only when hunting is enabled, this will also make it obvious when
	// hunting is on.
//	if (G->hunting)
//		mOverlay->DrawOverlay();

	HRESULT hr = mOrigSwapChain1->Present1(SyncInterval, PresentFlags, pPresentParameters);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP_(BOOL) HackerDXGISwapChain1::IsTemporaryMonoSupported(THIS)
{
	LogInfo("HackerDXGISwapChain1::IsTemporaryMonoSupported(%s@%p) called \n", typeid(*this).name(), this);
	BOOL ret = mOrigSwapChain1->IsTemporaryMonoSupported();
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP HackerDXGISwapChain1::GetRestrictToOutput(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppRestrictToOutput)
{
	LogInfo("HackerDXGISwapChain1::GetRestrictToOutput(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigSwapChain1->GetRestrictToOutput(ppRestrictToOutput);
	LogInfo("  returns result = %x, handle = %p \n", hr, *ppRestrictToOutput);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::SetBackgroundColor(THIS_
            /* [annotation][in] */ 
            _In_  const DXGI_RGBA *pColor)
{
	LogInfo("HackerDXGISwapChain1::SetBackgroundColor(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigSwapChain1->SetBackgroundColor(pColor);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::GetBackgroundColor(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_RGBA *pColor)
{
	LogInfo("HackerDXGISwapChain1::GetBackgroundColor(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigSwapChain1->GetBackgroundColor(pColor);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::SetRotation(THIS_
            /* [annotation][in] */ 
            _In_  DXGI_MODE_ROTATION Rotation)
{
	LogInfo("HackerDXGISwapChain1::SetRotation(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigSwapChain1->SetRotation(Rotation);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::GetRotation(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_MODE_ROTATION *pRotation)
{
	LogInfo("HackerDXGISwapChain1::GetRotation(%s@%p) called \n", typeid(*this).name(), this);
	HRESULT hr = mOrigSwapChain1->GetRotation(pRotation);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

