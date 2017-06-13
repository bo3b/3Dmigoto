// Object			OS				DXGI version	Feature level
// IDXGIDevice		Win7			1.0				11.0
// IDXGIDevice1		Win7			1.0				11.0
// IDXGIDevice2		Platform update	1.2				11.1
// IDXGIDevice3		Win8.1			1.3
// IDXGIDevice4						1.5
// 
// IDXGIAdapter		Win7			1.0				11.0
// IDXGIAdapter1	Win7			1.0				11.0
// IDXGIAdapter2	Platform update	1.2				11.1
// IDXGIAdapter3					1.3
// 
// IDXGIFactory		Win7			1.0				11.0
// IDXGIFactory1	Win7			1.0				11.0
// IDXGIFactory2	Platform update	1.2				11.1
// IDXGIFactory3	Win8.1			1.3
// IDXGIFactory4					1.4
// IDXGIFactory5					1.5
// 
// IDXGIOutput		Win7			1.0				11.0
// IDXGIOutput1		Platform update	1.2				11.1
// IDXGIOutput2		Win8.1			1.3
// IDXGIOutput3		Win8.1			1.3
// IDXGIOutput4		Win10			1.4
// IDXGIOutput5		Win10			1.5
// 
// IDXGIResource	Win7			1.0				11.0
// IDXGIResource1	Platform update	1.2				11.1
// 
// IDXGISwapChain	Win7			1.0				11.0
// IDXGISwapChain1	Platform update	1.2				11.1
// IDXGISwapChain2	Win8.1			1.3
// IDXGISwapChain3	Win10			1.4
// IDXGISwapChain4					1.5

#include "HackerDXGI.h"
#include "HookedDevice.h"

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
HackerDXGIDevice::HackerDXGIDevice(IDXGIDevice *pDXGIDevice, HackerDevice *pDevice)
	: HackerDXGIObject(pDXGIDevice)
{
	mOrigDXGIDevice = pDXGIDevice;
	mHackerDevice = pDevice;
}

HackerDXGIDevice1::HackerDXGIDevice1(IDXGIDevice1 *pDXGIDevice1, HackerDevice *pDevice)
	: HackerDXGIDevice(pDXGIDevice1, pDevice)
{
	mOrigDXGIDevice1 = pDXGIDevice1;
	mHackerDevice = pDevice;
}

HackerDXGIAdapter::HackerDXGIAdapter(IDXGIAdapter *pAdapter)
	: HackerDXGIObject(pAdapter)
{
	mOrigAdapter = pAdapter;
}

HackerDXGIAdapter1::HackerDXGIAdapter1(IDXGIAdapter1 *pAdapter1)
	: HackerDXGIAdapter(pAdapter1)
{
	mOrigAdapter1 = pAdapter1;
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


// In the Elite Dangerous case, they Release the HackerContext objects before creating the 
// swap chain.  That causes problems, because we are not expecting anyone to get here without
// having a valid context.  They later call GetImmediateContext, which will generate a wrapped
// context.  So, since we need the context for our Overlay, let's do that a litte early in
// this case, which will save the reference for their GetImmediateContext call.

HackerDXGISwapChain::HackerDXGISwapChain(IDXGISwapChain *pSwapChain, HackerDevice *pDevice, HackerContext *pContext)
	: HackerDXGIDeviceSubObject(pSwapChain), mFakeBackBuffer(nullptr), mFakeSwapChain(nullptr), mWidth(0), mHeight(0)
{
	mOrigSwapChain = pSwapChain;

	if (pContext == NULL)
	{
		pDevice->GetImmediateContext(reinterpret_cast<ID3D11DeviceContext**>(&pContext));
	}
	mHackerDevice = pDevice;
	mHackerContext = pContext;
	pDevice->SetHackerSwapChain(this);

	try {
		// Create Overlay class that will be responsible for drawing any text
		// info over the game. Using the Hacker Device and Context we gave the game.
		mOverlay = new Overlay(pDevice, pContext, this);
	}
	catch (...) {
		LogInfo("  *** Failed to create Overlay. Exception caught. \n");
		mOverlay = NULL;
	}
}

HackerDXGISwapChain::HackerDXGISwapChain(IDXGISwapChain *pSwapChain, HackerDevice *pDevice, HackerContext *pContext, const DXGI_SWAP_CHAIN_DESC* fake_swap_chain_desc, UINT new_width, UINT new_height)
	: HackerDXGIDeviceSubObject(pSwapChain), mFakeBackBuffer(nullptr), mFakeSwapChain(nullptr), mWidth(0), mHeight(0)
{
	mOrigSwapChain = pSwapChain;

	if (pContext == NULL)
	{
		pDevice->GetImmediateContext(reinterpret_cast<ID3D11DeviceContext**>(&pContext));
	}

	mHackerDevice = pDevice;
	mHackerContext = pContext;
	pDevice->SetHackerSwapChain(this);

	// TODO: multisampled swap chain
	// TODO: swap chain with more than one back buffer
	D3D11_TEXTURE2D_DESC fake_buffer_desc;
	std::memset(&fake_buffer_desc, 0, sizeof(D3D11_TEXTURE2D_DESC));
	fake_buffer_desc.ArraySize = 1;
	fake_buffer_desc.MipLevels = 1;
	fake_buffer_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	fake_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	fake_buffer_desc.SampleDesc.Count = 1;
	fake_buffer_desc.Format = fake_swap_chain_desc->BufferDesc.Format;
	fake_buffer_desc.MiscFlags = 0;
	fake_buffer_desc.Width = fake_swap_chain_desc->BufferDesc.Width;
	fake_buffer_desc.Height = fake_swap_chain_desc->BufferDesc.Height;
	fake_buffer_desc.CPUAccessFlags = 0;

	HRESULT res = pDevice->CreateTexture2D(&fake_buffer_desc, nullptr, &mFakeBackBuffer);

	mWidth = new_width;
	mHeight = new_height;

	if (res != S_OK)
	{
		LogInfo(" *** Failed to create faked swap chain buffer! \n");
		mFakeBackBuffer = nullptr;
		throw;
	}

	try {
		// Create Overlay class that will be responsible for drawing any text
		// info over the game. Using the Hacker Device and Context we gave the game.
		mOverlay = new Overlay(pDevice, pContext, this);
	}
	catch (...) {
		LogInfo("  *** Failed to create Overlay. Exception caught. \n");
		mOverlay = NULL;
	}
}

HackerDXGISwapChain::HackerDXGISwapChain(IDXGISwapChain *pSwapChain, HackerDevice *pDevice, HackerContext *pContext, IDXGISwapChain* fake_swap_chain, UINT new_width, UINT new_height)
	: HackerDXGIDeviceSubObject(pSwapChain), mFakeBackBuffer(nullptr), mFakeSwapChain(nullptr), mWidth(0), mHeight(0)
{

	mOrigSwapChain = pSwapChain;

	if (pContext == NULL)
	{
		pDevice->GetImmediateContext(reinterpret_cast<ID3D11DeviceContext**>(&pContext));
	}

	mHackerDevice = pDevice;
	mHackerContext = pContext;
	pDevice->SetHackerSwapChain(this);

	mFakeSwapChain = fake_swap_chain;

	if (mFakeSwapChain == nullptr)
	{
		LogInfo(" *** Failed to create hacker swap chain: provided faked swap chain is invalid! \n");
		throw;
	}

	mWidth = new_width;
	mHeight = new_height;

	try {
		// Create Overlay class that will be responsible for drawing any text
		// info over the game. Using the Hacker Device and Context we gave the game.
		mOverlay = new Overlay(pDevice, pContext, this);
	}
	catch (...) {
		LogInfo("  *** Failed to create Overlay. Exception caught. \n");
		mOverlay = NULL;
	}
}

HackerDXGISwapChain::~HackerDXGISwapChain()
{
	if (mFakeSwapChain)
		mFakeSwapChain->Release();
	if (mFakeBackBuffer)
		mFakeBackBuffer->Release();
}

HackerDXGISwapChain1::HackerDXGISwapChain1(IDXGISwapChain1 *pSwapChain1, HackerDevice *pDevice, HackerContext *pContext)
	: HackerDXGISwapChain(pSwapChain1, pDevice, pContext)
{
	mOrigSwapChain1 = pSwapChain1;
}


HackerDXGIFactory::HackerDXGIFactory(IDXGIFactory *pFactory)
	: HackerDXGIObject(pFactory)
{
	mOrigFactory = pFactory;
}

HackerDXGIFactory1::HackerDXGIFactory1(IDXGIFactory1 *pFactory1)
	: HackerDXGIFactory(pFactory1)
{
	mOrigFactory1 = pFactory1;
}

HackerDXGIFactory2::HackerDXGIFactory2(IDXGIFactory2 *pFactory2)
	: HackerDXGIFactory1(pFactory2)
{
	mOrigFactory2 = pFactory2;
}


// -----------------------------------------------------------------------------

STDMETHODIMP_(ULONG) HackerUnknown::AddRef(THIS)
{
	ULONG ulRef = mOrigUnknown->AddRef();
	LogInfo("HackerUnknown::AddRef(%s@%p), counter=%d, this=%p \n", type_name(this), this, ulRef, this);
	return ulRef;
}

STDMETHODIMP_(ULONG) HackerUnknown::Release(THIS)
{
	ULONG ulRef = mOrigUnknown->Release();
	LogInfo("HackerUnknown::Release(%s@%p), counter=%d, this=%p \n", type_name(this), this, ulRef, this);

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
	LogDebug("HackerUnknown::QueryInterface(%s@%p) called with IID: %s \n", type_name(this), this, NameFromIID(riid).c_str());
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
	LogInfo("HackerDXGIObject::SetPrivateData(%s@%p) called with GUID: %s \n", type_name(this), this, NameFromIID(Name).c_str());
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
	LogInfo("HackerDXGIObject::SetPrivateDataInterface(%s@%p) called with GUID: %s \n", type_name(this), this, NameFromIID(Name).c_str());

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
	LogInfo("HackerDXGIObject::GetPrivateData(%s@%p) called with GUID: %s \n", type_name(this), this, NameFromIID(Name).c_str());

	HRESULT hr = mOrigObject->GetPrivateData(Name, pDataSize, pData);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}


// More details: https://msdn.microsoft.com/en-us/library/windows/apps/hh465096.aspx
//
// This is the root class object, expected to be used for HackerDXGIAdapter, and
// HackerDXGIDevice GetParent() calls.  It would be legitimate for a caller to
// QueryInterface their objects to get the DXGIObject, and call GetParent, so
// this should be more robust.
//
// If the parent request is for the IDXGIAdapter or IDXGIFactory, that must mean 
// we are taking the secret path for getting the swap chain.  Return a wrapped version 
// whenever this happens, so we can get access later.

STDMETHODIMP HackerDXGIObject::GetParent(THIS_
	/* [annotation][in] */
	__in  REFIID riid,
	/* [annotation][retval][out] */
	__out  void **ppParent)
{
	LogInfo("HackerDXGIObject::GetParent(%s@%p) called with IID: %s \n", type_name(this), this, NameFromIID(riid).c_str());

	HRESULT hr = mOrigObject->GetParent(riid, ppParent);
	if (FAILED(hr))
	{
		LogInfo("  failed result = %x for %p \n", hr, ppParent);
		return hr;
	}

	// No need to check for null states from here, it would have thrown an error.

	if (riid == __uuidof(IDXGIAdapter) || riid == __uuidof(IDXGIAdapter1))
	{
		// Always return the IDXGIAdapter1 for these parents, as the superset on Win7.
		IDXGIAdapter1 *origAdapter1;
		static_cast<IDXGIAdapter*>(*ppParent)->QueryInterface(IID_PPV_ARGS(&origAdapter1));

		HackerDXGIAdapter1 *adapterWrap1 = new HackerDXGIAdapter1(origAdapter1);
		LogInfo("  created HackerDXGIAdapter1 wrapper = %p of %p \n", adapterWrap1, *ppParent);
		*ppParent = adapterWrap1;
	}
	else if (riid == __uuidof(IDXGIAdapter2))
	{
		if (!G->enable_platform_update) 
		{
			LogInfo("  returns E_NOINTERFACE as error for IDXGIAdapter2. \n");
			*ppParent = NULL;
			return E_NOINTERFACE;
		}
		// TODO: for platform update, return IDXGIAdapter2
		LogInfo("  returns E_NOINTERFACE as error for IDXGIAdapter2. \n");
		*ppParent = NULL;
		return E_NOINTERFACE;
	}
	else if (riid == __uuidof(IDXGIFactory))
	{
		// This is a specific hack for MGSV on Windows 10. If we wrap the DXGIFactory the game will reject it and
		// the game will quit. We still get the swap chain however, as the game creates it from a DXGIFactory1,
		// which it does allow us to wrap. If this turns out to be insufficient, we should be able to get it working
		// by using the same style of hooking we use for the context & device.
		if (!(G->enable_hooks & EnableHooks::SKIP_DXGI_FACTORY)) {
			HackerDXGIFactory *factoryWrap = new HackerDXGIFactory(static_cast<IDXGIFactory*>(*ppParent));
			LogInfo("  created HackerDXGIFactory wrapper = %p of %p \n", factoryWrap, *ppParent);
			*ppParent = factoryWrap;
		}
	}
	else if (riid == __uuidof(IDXGIFactory1))
	{
		HackerDXGIFactory1 *factoryWrap1 = new HackerDXGIFactory1(static_cast<IDXGIFactory1*>(*ppParent));
		LogInfo("  created HackerDXGIFactory1 wrapper = %p of %p \n", factoryWrap1, *ppParent);
		*ppParent = factoryWrap1;
	}
	else if (riid == __uuidof(IDXGIFactory2))
	{
		if (!G->enable_platform_update) 
		{
			LogInfo("  returns E_NOINTERFACE as error for IDXGIFactory2. \n");
			*ppParent = NULL;
			return E_NOINTERFACE;
		}
		HackerDXGIFactory2 *factoryWrap2 = new HackerDXGIFactory2(static_cast<IDXGIFactory2*>(*ppParent));
		LogInfo("  created HackerDXGIFactory2 wrapper = %p of %p \n", factoryWrap2, *ppParent);
		*ppParent = factoryWrap2;
	}

	LogInfo("  returns result = %#x \n", hr);
	return hr;
}


// -----------------------------------------------------------------------------

IDXGIDevice *HackerDXGIDevice::GetOrigDXGIDevice()
{
	return mOrigDXGIDevice;
}

HackerDevice *HackerDXGIDevice::GetHackerDevice()
{
	return mHackerDevice;
}


// Let's wrap GetAdapter as well, it appears that Ori calls this instead of 
// GetParent, which is their bug, but we need to work around it.

STDMETHODIMP HackerDXGIDevice::GetAdapter(
	/* [annotation][out] */
	_Out_  IDXGIAdapter **pAdapter)
{
	LogInfo("HackerDXGIDevice::GetAdapter(%s@%p) called with: %p \n", type_name(this), this, pAdapter);

	HRESULT hr = mOrigDXGIDevice->GetAdapter(pAdapter);

	// Check return value before wrapping - don't create wrappers for error states.
	if (SUCCEEDED(hr) && pAdapter)
	{
		HackerDXGIAdapter *adapterWrap = new HackerDXGIAdapter(*pAdapter);

		LogInfo("  created HackerDXGIAdapter wrapper = %p of %p \n", adapterWrap, *pAdapter);

		// Return the wrapped version which the game will use for follow on calls.
		*pAdapter = reinterpret_cast<IDXGIAdapter*>(adapterWrap);
	}

	LogInfo("  returns result = %#x \n", hr);
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
	LogInfo("HackerDXGIDevice::CreateSurface(%s@%p) called \n", type_name(this), this);
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
	LogInfo("HackerDXGIDevice::QueryResourceResidency(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigDXGIDevice->QueryResourceResidency(ppResources, pResidencyStatus, NumResources);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIDevice::SetGPUThreadPriority(
	/* [in] */ INT Priority)
{
	LogInfo("HackerDXGIDevice::SetGPUThreadPriority(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigDXGIDevice->SetGPUThreadPriority(Priority);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIDevice::GetGPUThreadPriority(
	/* [annotation][retval][out] */
	_Out_  INT *pPriority)
{
	LogInfo("HackerDXGIDevice::GetGPUThreadPriority(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigDXGIDevice->GetGPUThreadPriority(pPriority);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}


// -----------------------------------------------------------------------------

IDXGIDevice1 *HackerDXGIDevice1::GetOrigDXGIDevice1()
{
	return mOrigDXGIDevice1;
}

HackerDevice *HackerDXGIDevice1::GetHackerDevice()
{
	return mHackerDevice;
}



STDMETHODIMP HackerDXGIDevice1::SetMaximumFrameLatency(
	/* [in] */ UINT MaxLatency)
{
	LogInfo("HackerDXGIDevice1::SetMaximumFrameLatency(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigDXGIDevice1->SetMaximumFrameLatency(MaxLatency);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIDevice1::GetMaximumFrameLatency(
	/* [annotation][out] */
	_Out_  UINT *pMaxLatency)
{
	LogInfo("HackerDXGIDevice1::GetMaximumFrameLatency(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigDXGIDevice1->GetMaximumFrameLatency(pMaxLatency);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

// -----------------------------------------------------------------------------

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
	LogInfo("HackerDXGIFactory::QueryInterface(%s@%p) called with IID: %s \n", type_name(this), this, NameFromIID(riid).c_str());

	HRESULT	hr = mOrigFactory->QueryInterface(riid, ppvObject);

	if (SUCCEEDED(hr) && ppvObject)
	{
		if (riid == __uuidof(IDXGIFactory2))
		{
			// If we are being requested to create a DXGIFactory2, lie and say it's not possible.
			// Unless we are overriding default behavior from ini file.
			if ((G->enable_dxgi1_2 == 0) && (!G->enable_platform_update))
			{
				LogInfo("  returns E_NOINTERFACE as error for IDXGIFactory2. \n");
				*ppvObject = NULL;
				return E_NOINTERFACE;
			} else if ((G->enable_dxgi1_2 == 1) || (G->enable_platform_update)) {
				// For when we need to return a legit Factory2.
				HackerDXGIFactory2 *factory2Wrap = new HackerDXGIFactory2(static_cast<IDXGIFactory2*>(*ppvObject));
				LogInfo("  created HackerDXGIFactory2 wrapper = %p of %p \n", factory2Wrap, *ppvObject);
				*ppvObject = factory2Wrap;
			}
			// Wrapping DXGIFactory2 here or failing this call
			// seems to disable the Steam overlay on Winddows 7 and
			// Windows 8 in some games such as The Witcher 3, but
			// the logs do not show any calls being made on the
			// wrapped object, just that it is always released
			// immediately. It is also noteworthy that the overlay
			// is also disabled when using API Monitor. The
			// explanation is not immediately apparent, but perhaps
			// Steam is checking the type of the returned object or
			// something similar?
			//
			// This can be worked around by setting allow_dxgi1_2=2
			// to allow the unwrapped DXGIFactory2 object through,
			// but this may cause problems if the game itself is
			// using the Factory2 object, not just the overlay.
		}
	}

	LogInfo("  returns result = %x for %p \n", hr, ppvObject);
	return hr;
}

STDMETHODIMP HackerDXGIFactory::EnumAdapters(THIS_
	/* [in] */ UINT Adapter,
	/* [annotation][out] */
	_Out_  IDXGIAdapter **ppAdapter)
{
	LogInfo("HackerDXGIFactory::EnumAdapters(%s@%p) adapter %d requested\n", type_name(this), this, Adapter);

	HRESULT hr = mOrigFactory->EnumAdapters(Adapter, ppAdapter);

	// Check return value before wrapping - don't create a wrapper for
	// DXGI_ERROR_NOT_FOUND, as that will crash UE4 games.
	if (SUCCEEDED(hr) && ppAdapter) 
	{
		HackerDXGIAdapter *adapterWrap = new HackerDXGIAdapter(*ppAdapter);

		LogInfo("  created HackerDXGIAdapter wrapper = %p of %p \n", adapterWrap, *ppAdapter);

		// Return the wrapped version which the game will use for follow on calls.
		*ppAdapter = reinterpret_cast<IDXGIAdapter*>(adapterWrap);
	}

	LogInfo("  returns result = %#x \n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIFactory::MakeWindowAssociation(THIS_
            HWND WindowHandle,
            UINT Flags)
{
	if (LogFile)
	{
		LogInfo("HackerDXGIFactory::MakeWindowAssociation(%s@%p) called with WindowHandle = %p, Flags = %x\n", type_name(this), this, WindowHandle, Flags);
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
	LogInfo("HackerDXGIFactory::GetWindowAssociation(%s@%p) called\n", type_name(this), this);
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

	if (G->SCREEN_FULLSCREEN > 0)
	{
		if (G->SCREEN_FULLSCREEN == 2) {
			// We install this hook on demand to avoid any possible
			// issues with hooking the call when we don't need it:
			// Unconfirmed, but possibly related to:
			// https://forums.geforce.com/default/topic/685657/3d-vision/3dmigoto-now-open-source-/post/4801159/#4801159
			InstallSetWindowPosHook();
		}

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
// But.. it looks like it's legitimate to pass an IDXGIDevice as the pDevice
// here.  So, we need to handle either case.  If the input pDevice is not a
// wrapped device, then the typeid(*pDevice) will throw an RTTI exception.
//
// When creating the new swap chain, we need to pass the original device, not
// the wrapped version. For some reason, passing the wrapped version actually
// succeeds if the "evil" update is installed, which I would not expect.  Without
// the platform update, it would crash here.
//
// It's not clear if we should try to handle null inputs for pDevice, even knowing
// that there is a lot of terrible code out there calling this.
// Also if we get a non-wrapped pDevice here, the typid(*pdevice) will crash with
// an RTTI exception, which we could catch.  Not sure how heroic we want to be here.
// After some thought, current operating philosophy for this routine will be to
// not wrap these with an exception handler, as we want to know when games do
// something crazy, and a crash will let us know.  If we were to just catch and
// release some crazy stuff, it's not likely to work anyway, and a hard/fragile
// failure is superior in that case.

STDMETHODIMP HackerDXGIFactory::CreateSwapChain(THIS_
	/* [annotation][in] */
	_In_  IUnknown *pDevice,
	/* [annotation][in] */
	_In_  DXGI_SWAP_CHAIN_DESC *pDesc,
	/* [annotation][out] */
	_Out_  IDXGISwapChain **ppSwapChain)
{
	LogInfo("\n *** HackerDXGIFactory::CreateSwapChain(%s@%p) called with parameters \n", type_name(this), this);
	LogInfo("  Device = %s@%p \n", type_name(pDevice), pDevice);
	LogInfo("  SwapChain = %p \n", ppSwapChain);
	LogInfo("  Description = %p \n", pDesc);

	// CreateSwapChain could be called with a IDXGIDevice or ID3D11Device
	HackerDevice *hackerDevice = NULL;
	IUnknown *origDevice = NULL;

	hackerDevice = (HackerDevice*)lookup_hooked_device((ID3D11Device*)pDevice);
	if (hackerDevice) 
	{
		origDevice = pDevice;
	}
	else if (typeid(*pDevice) == typeid(HackerDevice))
	{
		hackerDevice = static_cast<HackerDevice*>(pDevice);
		origDevice = hackerDevice->GetOrigDevice();
	}
	else if (typeid(*pDevice) == typeid(HackerDevice1))
	{
		// Needed for Batman:Telltale games
		hackerDevice = static_cast<HackerDevice1*>(pDevice);
		origDevice = hackerDevice->GetOrigDevice();
	}
	else if (typeid(*pDevice) == typeid(HackerDXGIDevice))
	{
		hackerDevice = static_cast<HackerDXGIDevice*>(pDevice)->GetHackerDevice();
		origDevice = static_cast<HackerDXGIDevice*>(pDevice)->GetOrigDXGIDevice();
	}
	else if (typeid(*pDevice) == typeid(HackerDXGIDevice1))
	{
		hackerDevice = static_cast<HackerDXGIDevice1*>(pDevice)->GetHackerDevice();
		origDevice = static_cast<HackerDXGIDevice1*>(pDevice)->GetOrigDXGIDevice1();
	}
	else {
		LogInfo("FIXME: CreateSwapChain called with device of unknown type!\n");
		return E_FAIL;
	}

	HRESULT hr;

	HackerDXGISwapChain *swapchainWrap = nullptr;
	bool set_fullscreen_required = false;
	if (G->SCREEN_UPSCALING == 0) // no upscaling at all use the usual "old" path
	{
		ForceDisplayParams(pDesc);
		hr = mOrigFactory->CreateSwapChain(origDevice, pDesc, ppSwapChain);
		swapchainWrap = new HackerDXGISwapChain(*ppSwapChain, hackerDevice, hackerDevice->GetHackerContext());

	}
	else
	{
		set_fullscreen_required = !pDesc->Windowed; // to create two swap chains in row we need the swap chains to be in windowed mode
		pDesc->Windowed = 1; // so store has to be stored to call setFullscreen after and changed to windowed
							 // NOTE: no ForceDisplayResolution required here
		InstallSetWindowPosHook(); // This hook is very important in case of upscaling
								   //TODO: what about the hook and the warning in forceDisplayResolution (Testing required)
								   //SetWindowPos(pDesc->OutputWindow, nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW);
		DXGI_SWAP_CHAIN_DESC olddesc = *pDesc; // need old description to create fake swap chain (fake texture)

		pDesc->BufferDesc.Width = G->SCREEN_WIDTH;
		pDesc->BufferDesc.Height = G->SCREEN_HEIGHT;

		switch (G->UPSCALE_MODE) // maybe additional modes will come thats why switch 
		{
		case 0:  // create texture in game resolution and use it as the buffer for the game (e.g. GetBuffer returns the texture instead of swap chain buffer)
		{
			hr = mOrigFactory->CreateSwapChain(origDevice, pDesc, ppSwapChain);
			swapchainWrap = new HackerDXGISwapChain(*ppSwapChain, hackerDevice, hackerDevice->GetHackerContext(), &olddesc, G->SCREEN_WIDTH, G->SCREEN_HEIGHT);
		}
		// TODO: this mode dont consider the case if there are more than one buffer (bufferCount in REsizeBuffers)
		// When can this be the case???? // It seems most of the game does not support this way (mostly crashes on CreateRenderTargetView))
		// cause of invalid parameters. However Witcher seems to run a bit faster and smoother with this mode. Maybe there is a way to make it work with 
		// all games???
		break;
		case 1: // creates second swap chain in game resolution and provide it to the game
		{
			IDXGISwapChain* fake_sw_ch = nullptr;
			hr = mOrigFactory->CreateSwapChain(origDevice, &olddesc, &fake_sw_ch); // The first one should work anyway 
			hr = mOrigFactory->CreateSwapChain(origDevice, pDesc, ppSwapChain); // if the second swap chain is not possible to create then error will be returned
			swapchainWrap = new HackerDXGISwapChain(*ppSwapChain, hackerDevice, hackerDevice->GetHackerContext(), fake_sw_ch, G->SCREEN_WIDTH, G->SCREEN_HEIGHT);
		}
		break;
		default: // Fall back disable everything
			G->SCREEN_UPSCALING = 0;
			G->UPSCALE_MODE = 0;
			ForceDisplayParams(pDesc);
			hr = mOrigFactory->CreateSwapChain(origDevice, pDesc, ppSwapChain);
			swapchainWrap = new HackerDXGISwapChain(*ppSwapChain, hackerDevice, hackerDevice->GetHackerContext(), &olddesc, G->SCREEN_WIDTH, G->SCREEN_HEIGHT);
			break;
		}
	}

	if (FAILED(hr))
	{
		LogInfo("  failed result = %#x for device:%p, swapchain:%p \n", hr, pDevice, ppSwapChain);
		return hr;
	}

	if (ppSwapChain)
	{
		LogInfo("->HackerDXGISwapChain %p created to wrap %p \n", swapchainWrap, *ppSwapChain);
		*ppSwapChain = reinterpret_cast<IDXGISwapChain*>(swapchainWrap);

		if (G->SCREEN_UPSCALING == 2 || set_fullscreen_required)
		{
			/*	some games seems to react very strange (like render nothing) if set full screen state is called here)
			Other games like The Witcher 3 need the call to ensure entering the full screen on start (seems to be game internal stuff)
			*/
			(*ppSwapChain)->SetFullscreenState(TRUE, nullptr);
		}
	}

	// TODO: What resolution should be used for the migoto internal resolution: game resolution or upscaled resolution?
	// Let it be upscaled (did not see any issues with that but maybe it should be reconsidered
	if (pDesc && G->mResolutionInfo.from == GetResolutionFrom::SWAP_CHAIN) {
		G->mResolutionInfo.width = pDesc->BufferDesc.Width;
		G->mResolutionInfo.height = pDesc->BufferDesc.Height;
		LogInfo("Got resolution from swap chain: %ix%i\n",
			G->mResolutionInfo.width, G->mResolutionInfo.height);
	}

	LogInfo("->return value = %#x \n\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGIFactory::CreateSoftwareAdapter(THIS_
	/* [in] */ HMODULE Module,
	/* [annotation][out] */
	__out  IDXGIAdapter **ppAdapter)
{
	LogInfo("HackerDXGIFactory::CreateSoftwareAdapter(%s@%p) called\n", type_name(this), this);
	HRESULT hr = mOrigFactory->CreateSoftwareAdapter(Module, ppAdapter);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}


// -----------------------------------------------------------------------------

// We do not need to override the QueryInterface here, in the case the caller uses
// IDXGIFactory1::QueryInterface to create a IDXGIFactory2.
// The superclass of HackerDXGIFactory::QueryInterface will be called, as long as
// we successfully wrapped the factory in the first place.

// Factory1 IS required for Win7 with no platform update installed.

STDMETHODIMP HackerDXGIFactory1::EnumAdapters1(THIS_
	/* [in] */ UINT Adapter,
	/* [annotation][out] */
	__out  IDXGIAdapter1 **ppAdapter)

{
	LogInfo("HackerDXGIFactory1::EnumAdapters1(%s@%p) adapter %d requested\n", type_name(this), this, Adapter);

	HRESULT hr = mOrigFactory1->EnumAdapters1(Adapter, ppAdapter);

	// Check return value before wrapping - don't create a wrapper for
	// DXGI_ERROR_NOT_FOUND, as that will crash UE4 games.
	if (SUCCEEDED(hr) && ppAdapter)
	{
		HackerDXGIAdapter1 *adapterWrap1 = new HackerDXGIAdapter1(*ppAdapter);

		LogInfo("  created HackerDXGIAdapter1 wrapper = %p of %p \n", adapterWrap1, *ppAdapter);

		// Return the wrapped version which the game will use for follow on calls.
		*ppAdapter = reinterpret_cast<IDXGIAdapter1*>(adapterWrap1);
	}

	LogInfo("  returns result = %#x \n", hr);
	return hr;
}
		/*  Earlier code for reference.
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

STDMETHODIMP_(BOOL) HackerDXGIFactory1::IsCurrent(THIS)
{
	LogInfo("HackerDXGIFactory1::IsCurrent(%s@%p) called\n", type_name(this), this);
	BOOL ret = mOrigFactory1->IsCurrent();
	LogInfo("  returns result = %d\n", ret);
	return ret;
}


// -----------------------------------------------------------------------------

// We are no presently doing anything with these create calls, because we don't
// expect to use them until we are forced to.  They are not available on Win7,
// so no game dev is likely to target these for awhile.
// When we do need to update these, use the normal CreateSwapChain as a refernce.
//
// No support for IDXGIFactory2, IDXGIFactory3, IDXGIFactory4 on our target platform
// of Win7 with no evil update installed.  This is the optional update, and as of
// 11-15-15, about 50% of gamers are still using Win7. So, it's not unreasonable 
// to force an older code path that the game has to support anyway.  No game dev
// can afford to skip 50% of the market.  
//
// Ignoring these, and returning errors simplifies our job.
//
// IDXGIFactory2 requires Platform Update
// IDXGIFactory3 requires Win8.1
// IDXGIFactory4 requires Win10
//
// Adding Factory2 for Batman-Telltale Series, and Dishonored2, which require the
// platform update.

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
	// TODO: do we need fake swap chain here? is there games that use this function?
	LogInfo("HackerDXGIFactory2::CreateSwapChainForHwnd(%s@%p) called with parameters\n", type_name(this), this);
	LogInfo("  Device = %p\n", pDevice);
	LogInfo("  HWND = %p\n", hWnd);
	if (pDesc) LogInfo("  Stereo = %d\n", pDesc->Stereo);
	if (pDesc) LogInfo("  Width = %d\n", pDesc->Width);
	if (pDesc) LogInfo("  Height = %d\n", pDesc->Height);
	if (pFullscreenDesc) LogInfo("  Refresh rate = %f\n", 
		(float) pFullscreenDesc->RefreshRate.Numerator / (float) pFullscreenDesc->RefreshRate.Denominator);
	if (pFullscreenDesc) LogInfo("  Windowed = %d\n", pFullscreenDesc->Windowed);

	// Not at all sure this can be called with DXGIDevice, but following CreateSwapChain model.
	HackerDevice *hackerDevice = NULL;
	IUnknown *origDevice = NULL;
	if (typeid(*pDevice) == typeid(HackerDevice))
	{
		hackerDevice = static_cast<HackerDevice*>(pDevice);
		origDevice = hackerDevice->GetOrigDevice();
	}
	else if (typeid(*pDevice) == typeid(HackerDevice1))
	{
		// Needed for Batman:Telltale games
		hackerDevice = static_cast<HackerDevice1*>(pDevice);
		origDevice = hackerDevice->GetOrigDevice();
	}
	else if (typeid(*pDevice) == typeid(HackerDXGIDevice))
	{
		hackerDevice = static_cast<HackerDXGIDevice*>(pDevice)->GetHackerDevice();
		origDevice = static_cast<HackerDXGIDevice*>(pDevice)->GetOrigDXGIDevice();
	}
	else if (typeid(*pDevice) == typeid(HackerDXGIDevice1))
	{
		hackerDevice = static_cast<HackerDXGIDevice1*>(pDevice)->GetHackerDevice();
		origDevice = static_cast<HackerDXGIDevice1*>(pDevice)->GetOrigDXGIDevice1();
	}
	else {
		LogInfo("FIXME: CreateSwapChain called with device of unknown type!\n");
		return E_FAIL;
	}

	//if (pDesc && SCREEN_WIDTH >= 0) pDesc->Width = SCREEN_WIDTH;
	//if (pDesc && SCREEN_HEIGHT >= 0) pDesc->Height = SCREEN_HEIGHT;
	//if (pFullscreenDesc && SCREEN_REFRESH >= 0)
	//{
	//	pFullscreenDesc->RefreshRate.Numerator = SCREEN_REFRESH;
	//	pFullscreenDesc->RefreshRate.Denominator = 1;
	//}
	//if (pFullscreenDesc && SCREEN_FULLSCREEN >= 0) pFullscreenDesc->Windowed = !SCREEN_FULLSCREEN; SCREEN_FULLSCREEN has different valid values now, if you enable this code you need to rework it

	//HRESULT hr = -1;
	//if (pRestrictToOutput)
	//hr = mOrigFactory2->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput->m_pOutput, &origSwapChain);

	HRESULT hr = mOrigFactory2->CreateSwapChainForHwnd(origDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
	if (FAILED(hr))
	{
		LogInfo("  failed result = %x for %p \n", hr, pDevice);
		return hr;
	}

	if (ppSwapChain)
	{
		HackerDXGISwapChain1 *swapchainWrap1 = new HackerDXGISwapChain1(*ppSwapChain, hackerDevice, hackerDevice->GetHackerContext());
		LogInfo("->HackerDXGISwapChain1 %p created to wrap %p \n", swapchainWrap1, *ppSwapChain);
		*ppSwapChain = reinterpret_cast<IDXGISwapChain1*>(swapchainWrap1);
	}

	if (pDesc && G->mResolutionInfo.from == GetResolutionFrom::SWAP_CHAIN) {
		G->mResolutionInfo.width = pDesc->Width;
		G->mResolutionInfo.height = pDesc->Height;
		LogInfo("Got resolution from swap chain: %ix%i\n",
			G->mResolutionInfo.width, G->mResolutionInfo.height);
	}

	LogInfo("->return value = %#x \n\n", hr);
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
	// TODO: do we need fake swap chain here? is there games that use this function?
	LogInfo("HackerDXGIFactory2::CreateSwapChainForCoreWindow(%s@%p) called with parameters\n", type_name(this), this);
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

	if (SUCCEEDED(hr)) {
	//	*ppSwapChain = HackerDXGISwapChain1::GetDirectSwapChain(origSwapChain);
	//	if ((*ppSwapChain)->m_WrappedDevice) (*ppSwapChain)->m_WrappedDevice->Release();
	//	(*ppSwapChain)->m_WrappedDevice = pDevice; pDevice->AddRef();
	//	(*ppSwapChain)->m_RealDevice = realDevice;
		if (pDesc && G->mResolutionInfo.from == GetResolutionFrom::SWAP_CHAIN) {
			G->mResolutionInfo.width = pDesc->Width;
			G->mResolutionInfo.height = pDesc->Height;
			LogInfo("Got resolution from swap chain: %ix%i\n",
				G->mResolutionInfo.width, G->mResolutionInfo.height);
		}
	}

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
	// TODO: do we need fake swap chain here? is there games that use this function?
	LogInfo("HackerDXGIFactory2::CreateSwapChainForComposition(%s@%p) called with parameters\n", type_name(this), this);
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

	HRESULT	hr = mOrigFactory2->CreateSwapChainForComposition(pDevice, pDesc, pRestrictToOutput, ppSwapChain);

	if (SUCCEEDED(hr)) {
	//	*ppSwapChain = HackerDXGISwapChain1::GetDirectSwapChain(origSwapChain);
	//	if ((*ppSwapChain)->m_WrappedDevice) (*ppSwapChain)->m_WrappedDevice->Release();
	//	(*ppSwapChain)->m_WrappedDevice = pDevice; pDevice->AddRef();
	//	(*ppSwapChain)->m_RealDevice = realDevice;
		if (pDesc && G->mResolutionInfo.from == GetResolutionFrom::SWAP_CHAIN) {
			G->mResolutionInfo.width = pDesc->Width;
			G->mResolutionInfo.height = pDesc->Height;
			LogInfo("Got resolution from swap chain: %ix%i\n",
				G->mResolutionInfo.width, G->mResolutionInfo.height);
		}
	}

	LogInfo("  return value = %x\n", hr);
	return hr;
}

STDMETHODIMP_(BOOL) HackerDXGIFactory2::IsWindowedStereoEnabled(THIS)
{
	LogInfo("HackerDXGIFactory2::IsWindowedStereoEnabled(%s@%p) called \n", type_name(this), this);
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
	LogInfo("HackerDXGIFactory2::GetSharedResourceAdapterLuid(%s@%p) called \n", type_name(this), this);
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
	LogInfo("HackerDXGIFactory2::RegisterStereoStatusWindow(%s@%p) called \n", type_name(this), this);
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
	LogInfo("HackerDXGIFactory2::RegisterStereoStatusEvent(%s@%p) called \n", type_name(this), this);
	HRESULT ret = mOrigFactory2->RegisterStereoStatusEvent(hEvent, pdwCookie);
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP_(void) HackerDXGIFactory2::UnregisterStereoStatus(THIS_
            /* [annotation][in] */ 
            _In_  DWORD dwCookie)
{
	LogInfo("HackerDXGIFactory2::UnregisterStereoStatus(%s@%p) called \n", type_name(this), this);
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
	LogInfo("HackerDXGIFactory2::RegisterOcclusionStatusWindow(%s@%p) called \n", type_name(this), this);
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
	LogInfo("HackerDXGIFactory2::RegisterOcclusionStatusEvent(%s@%p) called \n", type_name(this), this);
	HRESULT ret = mOrigFactory2->RegisterOcclusionStatusEvent(hEvent, pdwCookie);
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP_(void) HackerDXGIFactory2::UnregisterOcclusionStatus(THIS_
            /* [annotation][in] */ 
            _In_  DWORD dwCookie)
{
	LogInfo("HackerDXGIFactory2::UnregisterOcclusionStatus(%s@%p) called \n", type_name(this), this);
	mOrigFactory2->UnregisterOcclusionStatus(dwCookie);
}
        

// -----------------------------------------------------------------------------

// Handle the upcasting/type coercion from a IDXGIAdapter to IDXGIAdapter1.  
// If it's a request for IDXGIAdapter2, return an error as that requires the
// evil update, and we return errors for that (at present)

STDMETHODIMP HackerDXGIAdapter::QueryInterface(THIS_
	/* [in] */ REFIID riid,
	/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	LogDebug("HackerDXGIAdapter::QueryInterface(%s@%p) called with IID: %s \n", type_name(this), this, NameFromIID(riid).c_str());

	HRESULT hr = mOrigAdapter->QueryInterface(riid, ppvObject);
	if (FAILED(hr))
	{
		LogInfo("  failed result = %x for %p \n", hr, ppvObject);
		return hr;
	}

	// No need for further checks of null ppvObject, as it could not have successfully
	// called the original in that case.

	if (riid == __uuidof(IDXGIAdapter1))
	{
		IDXGIAdapter1 *origAdapter1 = static_cast<IDXGIAdapter1*>(*ppvObject);
		HackerDXGIAdapter1 *dxgiAdapterWrap1 = new HackerDXGIAdapter1(origAdapter1);
		*ppvObject = dxgiAdapterWrap1;
		LogDebug("  created HackerDXGIAdapter1(%s@%p) wrapper of %p \n", type_name(dxgiAdapterWrap1), dxgiAdapterWrap1, origAdapter1);
	}
	else if (riid == __uuidof(IDXGIAdapter2))
	{
		// Well, bizarrely, this approach to upcasting to a IDXGIAdapter1 is supported on Win7, 
		// but only if you have the 'evil update', the platform update installed.  Since that
		// is an optional update, that certainly means that numerous people do not have it 
		// installed. Ergo, a game developer cannot in good faith just assume that it's there,
		// and it's very unlikely they would require it. No performance advantage on Win8.
		// So, that means that a game developer must support a fallback path, even if they
		// actually want Device1 for some reason.
		//
		// Sooo... Current plan is to return an error here, and pretend that the platform
		// update is not installed, or missing feature on Win8.1.  This will force the game
		// to use a more compatible path and make our job easier.

		if (!G->enable_platform_update) 
		{
			LogInfo("  returns E_NOINTERFACE as error for IDXGIAdapter2. \n");
			*ppvObject = NULL;
			return E_NOINTERFACE;
		}
		// TODO: for platform update, return IDXGIAdapter2
		LogInfo("  returns E_NOINTERFACE as error for IDXGIAdapter2. \n");
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	else if (riid == __uuidof(IDXGIFactory2))
	{
		// Called from Batman: Telltale games. 

		if (!G->enable_platform_update)
		{
			LogInfo("  returns E_NOINTERFACE as error for IDXGIFactory2. \n");
			*ppvObject = NULL;
			return E_NOINTERFACE;
		}
		LogInfo("  returns E_NOINTERFACE as error for IDXGIFactory2. \n");
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}

	LogDebug("  returns result = %x for %p \n", hr, *ppvObject);
	return hr;
}

STDMETHODIMP HackerDXGIAdapter::EnumOutputs(THIS_
            /* [in] */ UINT Output,
            /* [annotation][out][in] */ 
            __out IDXGIOutput **ppOutput)
{
	LogInfo("HackerDXGIAdapter::EnumOutputs(%s@%p) called: output #%d requested\n", type_name(this), this, Output);
	HRESULT hr = mOrigAdapter->EnumOutputs(Output, ppOutput);
	LogInfo("  returns result = %x, handle = %p\n", hr, *ppOutput);
	return hr;
}
        
STDMETHODIMP HackerDXGIAdapter::GetDesc(THIS_
            /* [annotation][out] */ 
            __out DXGI_ADAPTER_DESC *pDesc)
{
	LogInfo("HackerDXGIAdapter::GetDesc(%s@%p) called \n", type_name(this), this);
	
	HRESULT hr = mOrigAdapter->GetDesc(pDesc);
	if (LogFile && hr == S_OK)
	{
		char s[MAX_PATH];
		wcstombs(s, pDesc->Description, MAX_PATH);
		LogInfo("  returns adapter: %s, sysmem=%Iu, vidmem=%Iu\n", s, pDesc->DedicatedSystemMemory, pDesc->DedicatedVideoMemory);
	}
	return hr;
}

STDMETHODIMP HackerDXGIAdapter::CheckInterfaceSupport(THIS_
            /* [annotation][in] */ 
            __in  REFGUID InterfaceName,
            /* [annotation][out] */ 
            __out  LARGE_INTEGER *pUMDVersion)
{
	LogInfo("HackerDXGIAdapter::CheckInterfaceSupport(%s@%p) called with IID: %s \n", type_name(this), this, NameFromIID(InterfaceName).c_str());

	HRESULT hr = mOrigAdapter->CheckInterfaceSupport(InterfaceName, pUMDVersion);

	// Force error response for anything other than ID3D11Device.  This fixes a crash in FC4
	// when no evil update is installed, and matches our CreateDevice strategy.
	// Because this call is only ever supposed to be used for ID3D10 lookups, this
	// probably means it will always return the error.
	// Some games fail at launch, so a d3dx.ini setting can allow them skip this.
	if (!G->enable_check_interface)
	{
		if (InterfaceName != __uuidof(ID3D11Device))
			hr = DXGI_ERROR_UNSUPPORTED;
	}

	if (hr == S_OK && pUMDVersion) LogInfo("  UMDVersion high=%x, low=%x\n", pUMDVersion->HighPart, pUMDVersion->LowPart);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}


// -----------------------------------------------------------------------------

STDMETHODIMP HackerDXGIAdapter1::GetDesc1(THIS_
	/* [annotation][out] */
	__out  DXGI_ADAPTER_DESC1 *pDesc)
{
	LogInfo("HackerDXGIAdapter1::GetDesc1(%s@%p) called \n", type_name(this), this);

	HRESULT hr = mOrigAdapter1->GetDesc1(pDesc);
	if (LogFile)
	{
		char s[MAX_PATH];
		if (hr == S_OK)
		{
			wcstombs(s, pDesc->Description, MAX_PATH);
			LogInfo("  returns adapter: %s, sysmem=%Iu, vidmem=%Iu, flags=%x\n", s, pDesc->DedicatedSystemMemory, pDesc->DedicatedVideoMemory, pDesc->Flags);
		}
	}
	return hr;
}


// -----------------------------------------------------------------------------

STDMETHODIMP HackerDXGIOutput::GetDesc(THIS_ 
        /* [annotation][out] */ 
        __out  DXGI_OUTPUT_DESC *pDesc)
{
	LogInfo("HackerDXGIOutput::GetDesc(%s@%p) called \n", type_name(this), this);
	
	HRESULT ret = mOrigOutput->GetDesc(pDesc);
	if (SUCCEEDED(ret))
	{
		LogInfo("  returned %S, desktop=%d\n", pDesc->DeviceName, pDesc->AttachedToDesktop);
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
	LogInfo("HackerDXGIOutput::GetDisplayModeList(%s@%p) called \n", type_name(this), this);
	
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
	if (pModeToMatch) LogInfo("HackerDXGIOutput::FindClosestMatchingMode(%s@%p) called: width=%d, height=%d, refresh rate=%f\n", type_name(this), this,
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
	LogInfo("HackerDXGIOutput::WaitForVBlank(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigOutput->WaitForVBlank();
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::TakeOwnership(THIS_  
        /* [annotation][in] */ 
        __in  IUnknown *pDevice,
        BOOL Exclusive)
{
	LogInfo("HackerDXGIOutput::TakeOwnership(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigOutput->TakeOwnership(pDevice, Exclusive);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
void STDMETHODCALLTYPE HackerDXGIOutput::ReleaseOwnership(void)
{
	LogInfo("HackerDXGIOutput::ReleaseOwnership(%s@%p) called \n", type_name(this), this);
	return mOrigOutput->ReleaseOwnership();
}
        
STDMETHODIMP HackerDXGIOutput::GetGammaControlCapabilities(THIS_  
        /* [annotation][out] */ 
        __out  DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps)
{
	LogInfo("HackerDXGIOutput::GetGammaControlCapabilities(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigOutput->GetGammaControlCapabilities(pGammaCaps);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::SetGammaControl(THIS_ 
        /* [annotation][in] */ 
        __in  const DXGI_GAMMA_CONTROL *pArray)
{
	LogInfo("HackerDXGIOutput::SetGammaControl(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigOutput->SetGammaControl(pArray);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::GetGammaControl(THIS_ 
        /* [annotation][out] */ 
        __out  DXGI_GAMMA_CONTROL *pArray)
{
	LogInfo("HackerDXGIOutput::GetGammaControl(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigOutput->GetGammaControl(pArray);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::SetDisplaySurface(THIS_ 
        /* [annotation][in] */ 
        __in  IDXGISurface *pScanoutSurface)
{
	LogInfo("HackerDXGIOutput::SetDisplaySurface(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigOutput->SetDisplaySurface(pScanoutSurface);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::GetDisplaySurfaceData(THIS_ 
        /* [annotation][in] */ 
        __in  IDXGISurface *pDestination)
{
	LogInfo("HackerDXGIOutput::GetDisplaySurfaceData(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigOutput->GetDisplaySurfaceData(pDestination);
	LogInfo("  returns hr=%x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGIOutput::GetFrameStatistics(THIS_ 
        /* [annotation][out] */ 
        __out  DXGI_FRAME_STATISTICS *pStats)
{
	LogInfo("HackerDXGIOutput::GetFrameStatistics(%s@%p) called \n", type_name(this), this);
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
	LogDebug("HackerDXGIDeviceSubObject::GetDevice(%s@%p) called with IID: %s \n", type_name(this), this, NameFromIID(riid).c_str());

	HRESULT hr = mOrigDeviceSubObject->GetDevice(riid, ppDevice);
	LogDebug("  returns result = %x, handle = %p\n", hr, *ppDevice);
	return hr;
}


// -----------------------------------------------------------------------------

IDXGISwapChain* HackerDXGISwapChain::GetOrigSwapChain()
{
	LogDebug("HackerDXGISwapChain::GetOrigSwapChain returns %p \n", mOrigSwapChain);
	return mOrigSwapChain;
}


static void UpdateStereoParams(HackerDevice *mHackerDevice, HackerContext *mHackerContext)
{
	if (G->ENABLE_TUNE)
	{
		//device->mParamTextureManager.mSeparationModifier = gTuneValue;
		mHackerDevice->mParamTextureManager.mTuneVariable1 = G->gTuneValue[0];
		mHackerDevice->mParamTextureManager.mTuneVariable2 = G->gTuneValue[1];
		mHackerDevice->mParamTextureManager.mTuneVariable3 = G->gTuneValue[2];
		mHackerDevice->mParamTextureManager.mTuneVariable4 = G->gTuneValue[3];
		int counter = 0;
		if (counter-- < 0)
		{
			counter = 30;
			mHackerDevice->mParamTextureManager.mForceUpdate = true;
		}
	}

	// Update stereo parameter texture. It's possible to arrive here with no texture available though,
	// so we need to check first.
	if (mHackerDevice->mStereoTexture)
	{
		LogDebug("  updating stereo parameter texture.\n");
		mHackerDevice->mParamTextureManager.UpdateStereoTexture(mHackerDevice, mHackerContext, mHackerDevice->mStereoTexture, false);
	}
	else
	{
		LogDebug("  stereo parameter texture missing.\n");
	}
}

IDXGISwapChain* HackerDXGISwapChain::GetFakeSwapChain()
{
	return mFakeSwapChain;
}

ID3D11Texture2D* HackerDXGISwapChain::getFakeSwapChainBufferPtr()
{
	return mFakeBackBuffer;
}

BOOL HackerDXGISwapChain::getFakeSwapChainDesc(DXGI_SWAP_CHAIN_DESC* pDesc) const
{
	if (mFakeSwapChain)
	{
		mFakeSwapChain->GetDesc(pDesc);
		return TRUE;
	}

	return FALSE;
}

BOOL HackerDXGISwapChain::getFakeSwapChainBufferDesc(D3D11_TEXTURE2D_DESC* pDesc) const
{
	if (mFakeBackBuffer)
	{
		mFakeBackBuffer->GetDesc(pDesc);
		return TRUE;
	}

	return FALSE;
}

// Called at each DXGI::Present() to give us reliable time to execute user
// input and hunting commands.

void HackerDXGISwapChain::RunFrameActions()
{
	LogDebug("Running frame actions.  Device: %p\n", mHackerDevice);

	// Regardless of log settings, since this runs every frame, let's flush the log
	// so that the most lost will be one frame worth.  Tradeoff of performance to accuracy
	if (LogFile) fflush(LogFile);

	// Run the command list here, before drawing the overlay so that a
	// custom shader on the present call won't remove the overlay. Also,
	// run this before most frame actions so that this can be considered as
	// a pre-present command list. We have a separate post-present command
	// list after the present call in case we need to restore state or
	// affect something at the start of the frame.
	RunCommandList(mHackerDevice, mHackerContext, &G->present_command_list, NULL, false);

	// Draw the on-screen overlay text with hunting info, before final Present.
	// But only when hunting is enabled, this will also make it obvious when
	// hunting is on.
	if ((G->hunting == HUNTING_MODE_ENABLED) && mOverlay)
		mOverlay->DrawOverlay();

	if (G->analyse_frame) {
		// We don't allow hold to be changed mid-frame due to potential
		// for filename conflicts, so use def_analyse_options:
		if (G->def_analyse_options & FrameAnalysisOptions::HOLD) {
			// If using analyse_options=hold we don't stop the
			// analysis at the frame boundary (it will be stopped
			// at the key up event instead), but we do increment
			// the frame count and reset the draw count:
			G->analyse_frame_no++;
			G->analyse_frame = 1;
		} else {
			G->analyse_frame = 0;
			if (G->DumpUsage)
				DumpUsage(G->ANALYSIS_PATH);
		}
	}

	// NOTE: Now that key overrides can check an ini param, the ordering of
	// this and the present_command_list is significant. We might set an
	// ini param during a frame for scene detection, which is checked on
	// override activation, then cleared from the command list run on
	// present. If we ever needed to run the command list before this
	// point, we should consider making an explicit "pre" command list for
	// that purpose rather than breaking the existing behaviour.
	bool newEvent = DispatchInputEvents(mHackerDevice);

	CurrentTransition.UpdateTransitions(mHackerDevice);

	G->frame_no++;

	// The config file is not safe to reload from within the input handler
	// since it needs to change the key bindings, so it sets this flag
	// instead and we handle it now.
	if (G->gReloadConfigPending)
		ReloadConfig(mHackerDevice);

	// When not hunting most keybindings won't have been registered, but
	// still skip the below logic that only applies while hunting.
	if (G->hunting != HUNTING_MODE_ENABLED)
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
	LogDebug("HackerDXGISwapChain::Present(%s@%p) called \n", type_name(this), this);
	LogDebug("  SyncInterval = %d\n", SyncInterval);
	LogDebug("  Flags = %d\n", Flags);

	if (!(Flags & DXGI_PRESENT_TEST)) {
		// Every presented frame, we want to take some CPU time to run our actions,
		// which enables hunting, and snapshots, and aiming overrides and other inputs
		RunFrameActions();
	}

	HRESULT hr = mOrigSwapChain->Present(SyncInterval, Flags);

	if (!(Flags & DXGI_PRESENT_TEST)) {
		// Update the stereo params texture just after the present so that we
		// get the new values for the current frame:
		UpdateStereoParams(mHackerDevice, mHackerContext);

		// Run the post present command list now, which can be used to restore
		// state changed in the pre-present command list, or to perform some
		// action at the start of a frame:
		RunCommandList(mHackerDevice, mHackerContext, &G->post_present_command_list, NULL, true);
	}

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
	LogDebug("HackerDXGISwapChain::GetBuffer(%s@%p) called with IID: %s \n", type_name(this), this, NameFromIID(riid).c_str());

	HRESULT hr = S_OK;

	if (mFakeBackBuffer)
		*ppSurface = mFakeBackBuffer;
	else if (mFakeSwapChain)
		hr = mFakeSwapChain->GetBuffer(Buffer, riid, ppSurface);
	else
		hr = mOrigSwapChain->GetBuffer(Buffer, riid, ppSurface);

	LogDebug("  returns %x \n", hr);
	return hr;
}

STDMETHODIMP HackerDXGISwapChain::SetFullscreenState(THIS_
	/* [in] */ BOOL Fullscreen,
	/* [annotation][in] */
	_In_opt_  IDXGIOutput *pTarget)
{
	LogInfo("HackerDXGISwapChain::SetFullscreenState(%s@%p) called with\n", type_name(this), this);
	LogInfo("  Fullscreen = %d\n", Fullscreen);
	LogInfo("  Target = %p\n", pTarget);

	HRESULT hr;

	if (mFakeBackBuffer || mFakeSwapChain) // we need fullscreen anyway
	{
		BOOL fst = FALSE;
		IDXGIOutput *target = nullptr;
		mOrigSwapChain->GetFullscreenState(&fst, &target);

		if (target)
			target->Release();

		// dont call setfullscreenstate again to avoid starting mode switching and flooding winproc with unnecessary messages
		// can disable fullscreen mode somehow
		if (fst && Fullscreen)
		{
			return S_OK;
		}
		else
		{
			if (G->SCREEN_UPSCALING == 2)
			{
				hr = mOrigSwapChain->SetFullscreenState(TRUE, pTarget); // Witcher seems to require forcing the fullscreen
			}
			else
			{
				hr = mOrigSwapChain->SetFullscreenState(Fullscreen, pTarget);
			}
		}

		return hr;
	}

	if (G->SCREEN_FULLSCREEN > 0)
	{
		if (G->SCREEN_FULLSCREEN == 2) {
			// We install this hook on demand to avoid any possible
			// issues with hooking the call when we don't need it.
			// Unconfirmed, but possibly related to:
			// https://forums.geforce.com/default/topic/685657/3d-vision/3dmigoto-now-open-source-/post/4801159/#4801159
			InstallSetWindowPosHook();
		}

		Fullscreen = true;
		LogInfo("->Fullscreen forced = %d \n", Fullscreen);
	}

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
	LogDebug("HackerDXGISwapChain::GetFullscreenState(%s@%p) called \n", type_name(this), this);

	//IDXGIOutput *origOutput;
	//HRESULT hr = mOrigSwapChain->GetFullscreenState(pFullscreen, &origOutput);
	//if (hr == S_OK)
	//{
	//	*ppTarget = IDXGIOutput::GetDirectOutput(origOutput);
	//	if (pFullscreen) LogInfo("  returns Fullscreen = %d\n", *pFullscreen);
	//	if (ppTarget) LogInfo("  returns target IDXGIOutput = %x, wrapper = %x\n", origOutput, *ppTarget);
	//}

	HRESULT hr = mOrigSwapChain->GetFullscreenState(pFullscreen, ppTarget);
	//std::cout << "GetFullScreenState called " << *pFullscreen << std::endl;
	LogDebug("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGISwapChain::GetDesc(THIS_
	/* [annotation][out] */
	_Out_  DXGI_SWAP_CHAIN_DESC *pDesc)
{
	LogDebug("HackerDXGISwapChain::GetDesc(%s@%p) called \n", type_name(this), this);

	HRESULT hr = mOrigSwapChain->GetDesc(pDesc);

	if (hr == S_OK)
	{
		if (pDesc)
		{
			//TODO: not sure whether the upscaled resolution or game resolution should be returned
			// all tested games did not use this function only migoto does
			// I let them be the game resolution at the moment
			if (mFakeBackBuffer)
			{
				D3D11_TEXTURE2D_DESC fd;
				mFakeBackBuffer->GetDesc(&fd);
				pDesc->BufferDesc.Width = fd.Width;
				pDesc->BufferDesc.Height = fd.Height;
				LogDebug("	Fake Swap Chain Sizes Provided!");
			}

			if (mFakeSwapChain)
			{
				hr = mFakeSwapChain->GetDesc(pDesc);
			}
		}

		if (pDesc) LogDebug("  returns Windowed = %d\n", pDesc->Windowed);
		if (pDesc) LogDebug("  returns Width = %d\n", pDesc->BufferDesc.Width);
		if (pDesc) LogDebug("  returns Height = %d\n", pDesc->BufferDesc.Height);
		if (pDesc) LogDebug("  returns Refresh rate = %f\n",
			(float)pDesc->BufferDesc.RefreshRate.Numerator / (float)pDesc->BufferDesc.RefreshRate.Denominator);
	}
	LogDebug("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGISwapChain::ResizeBuffers(THIS_
	/* [in] */ UINT BufferCount,
	/* [in] */ UINT Width,
	/* [in] */ UINT Height,
	/* [in] */ DXGI_FORMAT NewFormat,
	/* [in] */ UINT SwapChainFlags)
{
	LogInfo("HackerDXGISwapChain::ResizeBuffers(%s@%p) called \n", type_name(this), this);

	HRESULT hr;

	if (mFakeBackBuffer) // UPSCALE_MODE 0
	{
		// just try to recreate texture with new game resolution
		// should be possible without any issues (texture just like the swap chain should not be used at this time point)
		//std::cout << "ResizeBuffer Called: " << Width << "  " << Height << std::endl;

		D3D11_TEXTURE2D_DESC fd;
		mFakeBackBuffer->GetDesc(&fd);

		if (!(fd.Width == Width && fd.Height == Height))
		{

			mFakeBackBuffer->Release();

			fd.Width = Width;
			fd.Height = Height;
			fd.Format = NewFormat;

			hr = mHackerDevice->CreateTexture2D(&fd, nullptr, &mFakeBackBuffer);
		}
		else  // nothing to resize
			hr = S_OK;

	}
	else if (mFakeSwapChain) // UPSCALE_MODE 1
	{
		hr = mFakeSwapChain->ResizeBuffers(BufferCount, Width, Height, NewFormat, 0);
	}
	else
		hr = mOrigSwapChain->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);

	if (SUCCEEDED(hr)) {
		mOverlay->Resize(Width, Height);

		if (G->mResolutionInfo.from == GetResolutionFrom::SWAP_CHAIN) {
			G->mResolutionInfo.width = Width;
			G->mResolutionInfo.height = Height;
			LogInfo("Got resolution from swap chain: %ix%i\n",
				G->mResolutionInfo.width, G->mResolutionInfo.height);
		}
	}

	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDXGISwapChain::ResizeTarget(THIS_
	/* [annotation][in] */
	_In_  const DXGI_MODE_DESC *pNewTargetParameters)
{
	LogInfo("HackerDXGISwapChain::ResizeTarget(%s@%p) called \n", type_name(this), this);

	// This function is very diffucult to recode
	// Some games like Witcher seems to drop fullscreen everytime the resizetarget is called (original one)
	// Some other games seems to require the function 
	// I did it the way the faked texture mode (upscale_mode == 1) dont call resize target
	// the other mode does
	if (G->SCREEN_UPSCALING == 2)
	{
		DEVMODE dmScreenSettings;
		memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
		dmScreenSettings.dmSize = sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsWidth = (unsigned long)mWidth;
		dmScreenSettings.dmPelsHeight = (unsigned long)mHeight;
		dmScreenSettings.dmBitsPerPel = 32;
		dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		// Change the display settings to full screen.
		LONG hr = ChangeDisplaySettingsEx(NULL, &dmScreenSettings, nullptr, CDS_FULLSCREEN, 0);
		LogInfo("  returns result = %x\n", hr);
		return hr == 0 ? S_OK : DXGI_ERROR_INVALID_CALL;
	}
	else if (G->SCREEN_UPSCALING == 1)
	{
		DXGI_MODE_DESC md = *pNewTargetParameters;

		// force upscaled resolution
		md.Width = mWidth;
		md.Height = mHeight;

		HRESULT hr = mOrigSwapChain->ResizeTarget(&md);
		LogInfo("  returns result = %x\n", hr);
		return hr;
	}

	DXGI_MODE_DESC mode = *pNewTargetParameters;
	HRESULT hr = mOrigSwapChain->ResizeTarget(&mode);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::GetContainingOutput(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppOutput)
{
	LogDebug("HackerDXGISwapChain::GetContainingOutput(%s@%p) called \n", type_name(this), this);
	
	//IDXGIOutput *origOutput;
	//HRESULT hr = mOrigSwapChain->GetContainingOutput(&origOutput);
	//if (hr == S_OK)
	//{
	//	*ppOutput = IDXGIOutput::GetDirectOutput(origOutput);
	//}

	HRESULT hr = mOrigSwapChain->GetContainingOutput(ppOutput);
	LogDebug("  returns result = %x, handle = %p \n", hr, *ppOutput);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::GetFrameStatistics(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_FRAME_STATISTICS *pStats)
{
	LogInfo("HackerDXGISwapChain::GetFrameStatistics(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigSwapChain->GetFrameStatistics(pStats);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain::GetLastPresentCount(THIS_
            /* [annotation][out] */ 
            _Out_  UINT *pLastPresentCount)
{
	LogInfo("HackerDXGISwapChain::GetLastPresentCount(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigSwapChain->GetLastPresentCount(pLastPresentCount);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}



// -----------------------------------------------------------------------------

// The IDXGISwapChain1 is an unsupported variant from our current design goals.
// IDXGISwapChain1, IDXGISwapChain2, IDXGISwapChain3 all require at least Win7 with
// the 'evil' update installed. That update is optional, hence not required by 
// game devs for their games. We push them back to older code paths for simplicity,
// because there is no apparent advantage (yet) to the new features.
//
// IDXGISwapChain2 requires Win8.1
// IDXGISwapChain3 requires Win10

STDMETHODIMP HackerDXGISwapChain1::GetDesc1(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_SWAP_CHAIN_DESC1 *pDesc)
{
	LogInfo("HackerDXGISwapChain1::GetDesc1(%s@%p) called \n", type_name(this), this);
	
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
	LogInfo("HackerDXGISwapChain1::GetFullscreenDesc(%s@%p) called \n", type_name(this), this);
	
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
	LogInfo("HackerDXGISwapChain1::GetHwnd(%s@%p) called \n", type_name(this), this);
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
	LogInfo("HackerDXGISwapChain1::GetCoreWindow(%s@%p) called with IID: %s \n", type_name(this), this, NameFromIID(refiid).c_str());

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
	LogInfo("HackerDXGISwapChain1::Present1(%s@%p) called \n", type_name(this), this);
	LogInfo("  SyncInterval = %d\n", SyncInterval);
	LogInfo("  PresentFlags = %d\n", PresentFlags);

	// TODO like regular Present call:
	// if (!(PresentFlags & DXGI_PRESENT_TEST)) {
	// 	// Every presented frame, we want to take some CPU time to run our actions,
	// 	// which enables hunting, and snapshots, and aiming overrides and other inputs
	// 	RunFrameActions();
	// }

	HRESULT hr = mOrigSwapChain1->Present1(SyncInterval, PresentFlags, pPresentParameters);

	// TODO like regular Present call:
	// if (!(PresentFlags & DXGI_PRESENT_TEST)) {
	// 	// Update the stereo params texture just after the present so that we
	// 	// get the new values for the current frame:
	// 	UpdateStereoParams(mHackerDevice, mHackerContext);

	// 	// Run the post present command list now, which can be used to restore
	// 	// state changed in the pre-present command list, or to perform some
	// 	// action at the start of a frame:
	// 	RunCommandList(mHackerDevice, mHackerContext, &G->post_present_command_list, NULL, true);
	// }

	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP_(BOOL) HackerDXGISwapChain1::IsTemporaryMonoSupported(THIS)
{
	LogInfo("HackerDXGISwapChain1::IsTemporaryMonoSupported(%s@%p) called \n", type_name(this), this);
	BOOL ret = mOrigSwapChain1->IsTemporaryMonoSupported();
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP HackerDXGISwapChain1::GetRestrictToOutput(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppRestrictToOutput)
{
	LogInfo("HackerDXGISwapChain1::GetRestrictToOutput(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigSwapChain1->GetRestrictToOutput(ppRestrictToOutput);
	LogInfo("  returns result = %x, handle = %p \n", hr, *ppRestrictToOutput);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::SetBackgroundColor(THIS_
            /* [annotation][in] */ 
            _In_  const DXGI_RGBA *pColor)
{
	LogInfo("HackerDXGISwapChain1::SetBackgroundColor(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigSwapChain1->SetBackgroundColor(pColor);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::GetBackgroundColor(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_RGBA *pColor)
{
	LogInfo("HackerDXGISwapChain1::GetBackgroundColor(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigSwapChain1->GetBackgroundColor(pColor);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::SetRotation(THIS_
            /* [annotation][in] */ 
            _In_  DXGI_MODE_ROTATION Rotation)
{
	LogInfo("HackerDXGISwapChain1::SetRotation(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigSwapChain1->SetRotation(Rotation);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerDXGISwapChain1::GetRotation(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_MODE_ROTATION *pRotation)
{
	LogInfo("HackerDXGISwapChain1::GetRotation(%s@%p) called \n", type_name(this), this);
	HRESULT hr = mOrigSwapChain1->GetRotation(pRotation);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

