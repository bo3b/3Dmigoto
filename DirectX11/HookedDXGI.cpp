// Object			OS				DXGI version	Feature level
// IDXGIFactory		Win7			1.0				11.0
// IDXGIFactory1	Win7			1.0				11.0
// IDXGIFactory2	Platform update	1.2				11.1
// IDXGIFactory3	Win8.1			1.3
// IDXGIFactory4					1.4
// IDXGIFactory5					1.5
//
// IDXGISwapChain	Win7			1.0				11.0
// IDXGISwapChain1	Platform update	1.2				11.1
// IDXGISwapChain2	Win8.1			1.3
// IDXGISwapChain3	Win10			1.4
// IDXGISwapChain4					1.5

#include <d3d11_1.h>

#include "HookedDXGI.h"
#include "HackerDXGI.h"

#include "DLLMainHook.h"
#include "log.h"
#include "util.h"
#include "D3D11Wrapper.h"


// This class is for a different approach than the wrapping of the system objects
// like we do with ID3D11Device for example.  When we wrap a COM object like that,
// it's not a real C++ object, and consequently cannot use the superclass normally,
// and requires boilerplate call-throughs for every interface to the object.  We
// may only care about a 5 calls, but we have to wrap all 150 calls. 
//
// Rather than do that with DXGI, this approach will be to singly hook the calls we
// are interested in, using the Nektra In-Proc hooking.  We'll still create
// objects for encapsulation where necessary, by returning HackerDXGIFactory1
// and HackerDXGIFactory2 when platform_update is set.  We won't ever return
// HackerDXGIFactory because the minimum on Win7 is IDXGIFactory1.



// -----------------------------------------------------------------------------
// This tweaks the parameters passed to the real CreateSwapChain, to change behavior.
// These global parameters come originally from the d3dx.ini, so the user can
// change them.
//
// There is now also ForceDisplayParams1 which has some overlap.

void ForceDisplayParams(DXGI_SWAP_CHAIN_DESC *pDesc)
{
	if (pDesc == NULL)
		return;

	LogInfo("     Windowed = %d\n", pDesc->Windowed);
	LogInfo("     Width = %d\n", pDesc->BufferDesc.Width);
	LogInfo("     Height = %d\n", pDesc->BufferDesc.Height);
	LogInfo("     Refresh rate = %f\n",
		(float)pDesc->BufferDesc.RefreshRate.Numerator / (float)pDesc->BufferDesc.RefreshRate.Denominator);

	if (G->SCREEN_UPSCALING == 0 && G->SCREEN_FULLSCREEN > 0)
	{
		pDesc->Windowed = false;
		LogInfo("->Forcing Windowed to = %d\n", pDesc->Windowed);
	}

	if (G->SCREEN_FULLSCREEN == 2 || G->SCREEN_UPSCALING > 0)
	{
		// We install this hook on demand to avoid any possible
		// issues with hooking the call when we don't need it:
		// Unconfirmed, but possibly related to:
		// https://forums.geforce.com/default/topic/685657/3d-vision/3dmigoto-now-open-source-/post/4801159/#4801159
		//
		// This hook is also very important in case of Upscaling
		InstallSetWindowPosHook();
	}

	if (G->SCREEN_REFRESH >= 0 && !pDesc->Windowed)
	{
		pDesc->BufferDesc.RefreshRate.Numerator = G->SCREEN_REFRESH;
		pDesc->BufferDesc.RefreshRate.Denominator = 1;
		LogInfo("->Forcing refresh rate to = %f\n",
			(float)pDesc->BufferDesc.RefreshRate.Numerator / (float)pDesc->BufferDesc.RefreshRate.Denominator);
	}
	if (G->SCREEN_WIDTH >= 0)
	{
		pDesc->BufferDesc.Width = G->SCREEN_WIDTH;
		LogInfo("->Forcing Width to = %d\n", pDesc->BufferDesc.Width);
	}
	if (G->SCREEN_HEIGHT >= 0)
	{
		pDesc->BufferDesc.Height = G->SCREEN_HEIGHT;
		LogInfo("->Forcing Height to = %d\n", pDesc->BufferDesc.Height);
	}

	// To support 3D Vision Direct Mode, we need to force the backbuffer from the
	// swapchain to be 2x its normal width.  
	if (G->gForceStereo == 2)
	{
		pDesc->BufferDesc.Width *= 2;
		LogInfo("->Direct Mode: Forcing Width to = %d\n", pDesc->BufferDesc.Width);
	}
}

// Different variant for the CreateSwapChainForHwnd.
//
// We absolutely need the force full screen in order to enable 3D.  
// Batman Telltale needs this.
// The rest of the variants are less clear.

void ForceDisplayParams1(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc1)
{
	if (pDesc1 == NULL)
		return;

	LogInfo("     Windowed = %d\n", pDesc1->Windowed);

	if (G->SCREEN_FULLSCREEN > 0)
	{
		pDesc1->Windowed = false;
		LogInfo("->Forcing Windowed to = %d\n", pDesc1->Windowed);
	}

	if (G->SCREEN_FULLSCREEN == 2)
	{
		// We install this hook on demand to avoid any possible
		// issues with hooking the call when we don't need it:
		// Unconfirmed, but possibly related to:
		// https://forums.geforce.com/default/topic/685657/3d-vision/3dmigoto-now-open-source-/post/4801159/#4801159

		InstallSetWindowPosHook();
	}

	// These input parameters are not clear how to implement for CreateSwapChainForHwnd,
	// and are stubbed out with error reporting. Can be implemented when cases arise.
	if (G->SCREEN_REFRESH >= 0 && !pDesc1->Windowed)
	{
		LogInfo("*** Unimplemented feature for refresh_rate in CreateSwapChainForHwnd\n");
		BeepFailure();
	}
	if (G->SCREEN_WIDTH >= 0)
	{
		LogInfo("*** Unimplemented feature to force screen width in CreateSwapChainForHwnd\n");
		BeepFailure();
	}
	if (G->SCREEN_HEIGHT >= 0)
	{
		LogInfo("*** Unimplemented feature to force screen height in CreateSwapChainForHwnd\n");
		BeepFailure();
	}

	// To support 3D Vision Direct Mode, we need to force the backbuffer from the
	// swapchain to be 2x its normal width.  
	if (G->gForceStereo == 2)
	{
		LogInfo("*** Unimplemented feature for Direct Mode in CreateSwapChainForHwnd\n");
		BeepFailure();
	}
}


// -----------------------------------------------------------------------------
// Actual hook for any IDXGICreateSwapChainForHwnd calls the game makes.
// This can only be called with Win7+platform_update or greater, using
// the IDXGIFactory2.
// 
// This type of SwapChain cannot be made through the CreateDeviceAndSwapChain,
// so there is only one logical path to create this, which is 
// IDXGIFactory2->CreateSwapChainForHwnd.  That means that the Device has
// already been created with CreateDevice, and dereferenced through the 
// chain of QueryInterface calls to get the IDXGIFactory2.

HRESULT(__stdcall *fnOrigCreateSwapChainForHwnd)(
	IDXGIFactory2 * This,
	/* [annotation][in] */
	_In_  IUnknown *pDevice,
	/* [annotation][in] */
	_In_  HWND hWnd,
	/* [annotation][in] */
	_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
	/* [annotation][in] */
	_In_opt_  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
	/* [annotation][in] */
	_In_opt_  IDXGIOutput *pRestrictToOutput,
	/* [annotation][out] */
	_Out_  IDXGISwapChain1 **ppSwapChain) = nullptr;


HRESULT __stdcall Hooked_CreateSwapChainForHwnd(
	IDXGIFactory2 * This,
	/* [annotation][in] */
	_In_  IUnknown *pDevice,
	/* [annotation][in] */
	_In_  HWND hWnd,
	/* [annotation][in] */
	_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
	/* [annotation][in] */
	_In_opt_  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
	/* [annotation][in] */
	_In_opt_  IDXGIOutput *pRestrictToOutput,
	/* [annotation][out] */
	_Out_  IDXGISwapChain1 **ppSwapChain)
{
	LogInfo("Hooked IDXGIFactory2::CreateSwapChainForHwnd(%p) called\n", This);
	LogInfo("  Device = %p\n", pDevice);
	LogInfo("  SwapChain = %p\n", ppSwapChain);
	LogInfo("  Description1 = %p\n", pDesc);
	LogInfo("  FullScreenDescription = %p\n", pFullscreenDesc);

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullScreenDesc = { 0 };
	if (!pFullscreenDesc)
		pFullscreenDesc = &fullScreenDesc;
	ForceDisplayParams1(&fullScreenDesc);

	HRESULT hr = fnOrigCreateSwapChainForHwnd(This, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
	if (FAILED(hr))
	{
		LogInfo("->Failed result %#x\n\n", hr);
		return hr;
	}

	IDXGISwapChain1* origSwapChain = *ppSwapChain;
	HackerDevice* hackerDevice = reinterpret_cast<HackerDevice*>(pDevice);
	HackerContext* hackerContext = hackerDevice->GetHackerContext();

	HackerSwapChain* hackerSwapChain;
	hackerSwapChain = new HackerSwapChain(origSwapChain, hackerDevice, hackerContext);


	// When creating a new swapchain, we can assume this is the game creating 
	// the most important object, and return the wrapped swapchain to the game 
	// so it will call our Present.
	*ppSwapChain = hackerSwapChain;

	LogInfo("->return result %#x, HackerSwapChain = %p wrapper of ppSwapChain = %p\n\n", hr, hackerSwapChain, origSwapChain);
	return hr;
}

// -----------------------------------------------------------------------------
// This hook should work in all variants, including the CreateSwapChain1
// and CreateSwapChainForHwnd

void HookCreateSwapChainForHwnd(void* factory2)
{
	LogInfo("*** IDXGIFactory2 creating hook for CreateSwapChainForHwnd. \n");

	IDXGIFactory2* dxgiFactory = reinterpret_cast<IDXGIFactory2*>(factory2);

	SIZE_T hook_id;
	DWORD dwOsErr = cHookMgr.Hook(&hook_id, (void**)&fnOrigCreateSwapChainForHwnd,
		lpvtbl_CreateSwapChainForHwnd(dxgiFactory), Hooked_CreateSwapChainForHwnd, 0);

	if (dwOsErr == ERROR_SUCCESS)
		LogInfo("  Successfully installed IDXGIFactory2->CreateSwapChainForHwnd hook.\n");
	else
		LogInfo("  *** Failed install IDXGIFactory2->CreateSwapChainForHwnd hook.\n");
}

// -----------------------------------------------------------------------------
// Actual hook for any IDXGICreateSwapChain calls the game makes.
//
// There are two primary paths that can arrive here.
// ---1. d3d11->CreateDeviceAndSwapChain
//	This path arrives here with a normal ID3D11Device1 device, not a HackerDevice.
//	This is called implictly from the middle of CreateDeviceAndSwapChain.---
//	No longer necessary, with CreateDeviceAndSwapChain broken into two direct calls.
// 2. IDXGIFactory->CreateSwapChain after CreateDevice
//	This path requires a pDevice passed in, which is a HackerDevice.  This is the
//	secret path, where they take the Device and QueryInterface to get IDXGIDevice
//	up to getting Factory, where they call CreateSwapChain. In this path, we can
//	expect the input pDevice to have already been setup as a HackerDevice.
//
//
// In prior code, we were looking for possible IDXGIDevice's as the pDevice input.
// That should not be a problem now, because we are specifically trying to cast
// that input into an ID3D11Device1 using QueryInterface.  Leaving the original
// code commented out at the bottom of the file, for reference.

HRESULT(__stdcall *fnOrigCreateSwapChain)(
	IDXGIFactory * This,
	/* [annotation][in] */
	_In_  IUnknown *pDevice,
	/* [annotation][in] */
	_In_  DXGI_SWAP_CHAIN_DESC *pDesc,
	/* [annotation][out] */
	_Out_  IDXGISwapChain **ppSwapChain) = nullptr;


HRESULT __stdcall Hooked_CreateSwapChain(
	IDXGIFactory * This,
	/* [annotation][in] */
	_In_  IUnknown *pDevice,
	/* [annotation][in] */
	_In_  DXGI_SWAP_CHAIN_DESC *pDesc,
	/* [annotation][out] */
	_Out_  IDXGISwapChain **ppSwapChain)
{
	LogInfo("\nHooked IDXGIFactory::CreateSwapChain(%p) called\n", This);
	LogInfo("  Device = %p\n", pDevice);
	LogInfo("  SwapChain = %p\n", ppSwapChain);
	LogInfo("  Description = %p\n", pDesc);

	//if (pDesc != nullptr) 
	//{
	//	// Save window handle so we can translate mouse coordinates to the window:
	//	G->hWnd = pDesc->OutputWindow;
	//
	//	if (G->SCREEN_UPSCALING > 0)
	//	{
	//		// For the upscaling case, fullscreen has to be set after swap chain is created
	//		setFullscreenRequired = !pDesc->Windowed;
	//		pDesc->Windowed = true;
	//		// Copy input swap chain desc for case the upscaling is on
	//		memcpy(&originalSwapChainDesc, pDesc, sizeof(DXGI_SWAP_CHAIN_DESC));
	//	}
	//
	//	// Require in case the software mouse and upscaling are on at the same time
	//	// TODO: Use a helper class to track *all* different resolutions
	//	G->GAME_INTERNAL_WIDTH = pDesc->BufferDesc.Width;
	//	G->GAME_INTERNAL_HEIGHT = pDesc->BufferDesc.Height;
	//
	//	if (G->mResolutionInfo.from == GetResolutionFrom::SWAP_CHAIN) {
	//		// TODO: Use a helper class to track *all* different resolutions
	//		G->mResolutionInfo.width = pDesc->BufferDesc.Width;
	//		G->mResolutionInfo.height = pDesc->BufferDesc.Height;
	//		LogInfo("Got resolution from swap chain: %ix%i\n",
	//			G->mResolutionInfo.width, G->mResolutionInfo.height);
	//	}
	//}

	ForceDisplayParams(pDesc);

	HRESULT hr = fnOrigCreateSwapChain(This, pDevice, pDesc, ppSwapChain);
	if (FAILED(hr))
	{
		LogInfo("->Failed result %#x\n\n", hr);
		return hr;
	}

	// Always upcast to IDXGISwapChain1 whenever possible.
	// If the upcast fails, that means we have a normal IDXGISwapChain,
	// but we'll still store it as an IDXGISwapChain1.  It's a little
	// weird to reinterpret this way, but should cause no problems in
	// the Win7 no platform_udpate case.
	IDXGISwapChain1* origSwapChain;
	(*ppSwapChain)->QueryInterface(IID_PPV_ARGS(&origSwapChain));
	if (origSwapChain == nullptr)
		origSwapChain = reinterpret_cast<IDXGISwapChain1*>(*ppSwapChain);

	HackerDevice* hackerDevice = reinterpret_cast<HackerDevice*>(pDevice);
	HackerContext* hackerContext = hackerDevice->GetHackerContext();


	// Original swapchain has been successfully created. Now we want to 
	// wrap the returned swapchain as either HackerSwapChain or HackerUpscalingSwapChain.  
	HackerSwapChain* swapchainWrap;
	
	if (G->SCREEN_UPSCALING == 0)		// Normal case
	{
		swapchainWrap = new HackerSwapChain(origSwapChain, hackerDevice, hackerContext);
		LogInfo("->HackerSwapChain %p created to wrap %p\n", swapchainWrap, *ppSwapChain);
	}
	//else								// Upscaling case
	//{
	//	swapchainWrap = new HackerUpscalingSwapChain(origSwapChain, origDevice, origContext,
	//		&originalSwapChainDesc, pDesc->BufferDesc.Width, pDesc->BufferDesc.Height, mOrigFactory);
	//	LogInfo("  HackerUpscalingSwapChain %p created to wrap %p.\n", swapchainWrap, *ppSwapChain);
	//}

	//if (G->SCREEN_UPSCALING == 2 || setFullscreenRequired)
	//{
	//	// Some games seems to react very strange (like render nothing) if set full screen state is called here)
	//	// Other games like The Witcher 3 need the call to ensure entering the full screen on start (seems to be game internal stuff)
	//	// If something would go wrong we would not get here
	//	(*ppSwapChain)->SetFullscreenState(TRUE, nullptr);
	//}

	// When creating a new swapchain, we can assume this is the game creating 
	// the most important object. Return the wrapped swapchain to the game so it 
	// will call our Present.
	*ppSwapChain = swapchainWrap;

	LogInfo("->return result %#x, HackerSwapChain = %p wrapper of ppSwapChain = %p\n\n", hr, swapchainWrap, origSwapChain);
	return hr;
}

// -----------------------------------------------------------------------------
// This hook should work in all variants, including the CreateSwapChain1
// and CreateSwapChainForHwnd

void HookCreateSwapChain(void* factory)
{
	LogInfo("*** IDXGIFactory creating hook for CreateSwapChain. \n");

	IDXGIFactory* dxgiFactory = reinterpret_cast<IDXGIFactory*>(factory);

	SIZE_T hook_id;
	DWORD dwOsErr = cHookMgr.Hook(&hook_id, (void**)&fnOrigCreateSwapChain,
		lpvtbl_CreateSwapChain(dxgiFactory), Hooked_CreateSwapChain, 0);

	if (dwOsErr == ERROR_SUCCESS)
		LogInfo("  Successfully installed IDXGIFactory->CreateSwapChain hook.\n");
	else
		LogInfo("  *** Failed install IDXGIFactory->CreateSwapChain hook.\n");
}


// -----------------------------------------------------------------------------
// Actual hook for any Factory->QueryInterface calls the game makes.
// ToDo: Presently commented out in activation, because it crashes Dishonored2
// if this is enabled.  Probably we no longer need this at all, because
// we are hooking the actual Create* calls, and don't need to worry about the
// QueryInterface sequence. Even if they upcast from Factory to Factory2.

HRESULT(__stdcall *fnOrigQueryInterface)(
	IDXGIObject * This,
	/* [in] */ REFIID riid,
	/* [annotation][iid_is][out] */
	_COM_Outptr_  void **ppvObject) = nullptr;


HRESULT __stdcall Hooked_QueryInterface(
	IDXGIObject * This,
	/* [in] */ REFIID riid,
	/* [annotation][iid_is][out] */
	_COM_Outptr_  void **ppvObject)
{
	LogInfo("DXGIFactory::QueryInterface(%p) called with IID: %s\n", This, NameFromIID(riid).c_str());

	HRESULT	hr = fnOrigQueryInterface(This, riid, ppvObject);

	if (SUCCEEDED(hr) && ppvObject)
	{
		if (riid == __uuidof(IDXGIFactory2))
		{
			// If we are being requested to create a DXGIFactory2, lie and say it's not possible.
			// Unless we are overriding default behavior from ini file.
			if (!G->enable_platform_update)
			{
				LogInfo("***  returns E_NOINTERFACE as error for IDXGIFactory2.\n");
				*ppvObject = NULL;
				return E_NOINTERFACE;
			}

			if (!fnOrigCreateSwapChainForHwnd)
				HookCreateSwapChainForHwnd(*ppvObject);
		}
	}

	LogInfo("  returns result = %x for %p\n", hr, ppvObject);
	return hr;
}

// -----------------------------------------------------------------------------
// QueryInterface can be used to upcast to an IDXGIFactory2, so we need to hook it.

void HookQueryInterface(void* factory)
{
	LogInfo("*** IDXGIFactory creating hook for QueryInterface. \n");

	IDXGIFactory* dxgiFactory = reinterpret_cast<IDXGIFactory*>(factory);

	SIZE_T hook_id;
	DWORD dwOsErr = cHookMgr.Hook(&hook_id, (void**)&fnOrigQueryInterface,
		lpvtbl_CreateSwapChain(dxgiFactory), Hooked_QueryInterface, 0);

	if (dwOsErr == ERROR_SUCCESS)
		LogInfo("  Successfully installed IDXGIFactory->QueryInterface hook.\n");
	else
		LogInfo("  *** Failed install IDXGIFactory->QueryInterface hook.\n");
}


// -----------------------------------------------------------------------------
// Actual function called by the game for every CreateDXGIFactory they make.
// This is only called for the in-process game, not system wide.
//
// This is our replacement, so that we can return a wrapped factory, which
// will allow us access to the SwapChain.

// It's legal to request a DXGIFactory2 here, so if platform_update is 
// enabled we'll go ahead and return that.  We are also going to always
// return at least a DXGIFactory1 now, because that is the baseline
// expected object for Win7, and allows us to better handle QueryInterface
// and GetParent calls.

HRESULT (__stdcall *fnOrigCreateDXGIFactory)(
	REFIID riid,
	_Out_ void   **ppFactory
	) = nullptr;

HRESULT __stdcall Hooked_CreateDXGIFactory(REFIID riid, void **ppFactory)
{
	LogInfo("*** Hooked_CreateDXGIFactory called with riid: %s\n", NameFromIID(riid).c_str());

	// If this happens to be first call from the game, let's make sure to load
	// up our d3d11.dll and the .ini file.
	InitD311();

	// If we are being requested to create a DXGIFactory2, lie and say it's not possible.
	if (riid == __uuidof(IDXGIFactory2) && !G->enable_platform_update)
	{
		LogInfo("  returns E_NOINTERFACE as error for IDXGIFactory2.\n");
		*ppFactory = NULL;
		return E_NOINTERFACE;
	}

	HRESULT hr = fnOrigCreateDXGIFactory(riid, ppFactory);
	if (FAILED(hr))
	{
		LogInfo("->failed with HRESULT=%x\n", hr);
		return hr;
	}

	//if (!fnOrigQueryInterface)
	//	HookQueryInterface(*ppFactory);

	if (!fnOrigCreateSwapChain)
		HookCreateSwapChain(*ppFactory);

	// With the addition of the platform_update, we need to allow for specifically
	// creating a DXGIFactory2 instead of DXGIFactory1.  We want to always upcast
	// the highest supported object for each scenario, to properly suppport
	// QueryInterface and GetParent upcasts.

	IDXGIFactory2* dxgiFactory = reinterpret_cast<IDXGIFactory2*>(*ppFactory);
	HRESULT res = dxgiFactory->QueryInterface(IID_PPV_ARGS(&dxgiFactory));
	if (SUCCEEDED(res))
	{
		*ppFactory = (void*)dxgiFactory;
		LogInfo("  Upcast QueryInterface(IDXGIFactory2) returned result = %x, factory = %p\n", res, dxgiFactory);

		if (!fnOrigCreateSwapChainForHwnd)
			HookCreateSwapChainForHwnd(*ppFactory);
	}

	LogInfo("  CreateDXGIFactory returned factory = %p, result = %x\n", *ppFactory, hr);
	return hr;
}


// -----------------------------------------------------------------------------

// It's not legal to mix Factory1 and Factory in the same app, so we'll not
// look for Factory here.  Bizarrely though, Factory2 is expected.
// Except that, Dragon Age makes the mistake of calling Factory1 for Factory. D'oh!

// Dishonored2 requires platform_update=1.  In this case, let's make sure to always
// create a DXGIFactory2, as the highest level object available.  Because they can
// always upcast at any time using QueryInterface.  We have previously been rewrapping
// and returning different objects, which seems wrong, especially if they ever do
// pointer comparisons.
// If we return DXGIFactory2 and they only ever need DXGIFactory1, that should cause
// no problems, as the interfaces are the same.
//
// Another factor is the use of GetParent, to get back to this DXGIFactory1 object.
// If we have wrapped extra times, then we'll return a different object.  We need
// to maintain the chain of objects so that GetParent from the Device will return
// the correct Adapter, which can then return the correct Factory.

HRESULT (__stdcall *fnOrigCreateDXGIFactory1)(
	REFIID riid,
	_Out_ void   **ppFactory
	) = nullptr;

HRESULT __stdcall Hooked_CreateDXGIFactory1(REFIID riid, void **ppFactory1)
{
	LogInfo("*** Hooked_CreateDXGIFactory1 called with riid: %s\n", NameFromIID(riid).c_str());

	// If this happens to be first call from the game, let's make sure to load
	// up our d3d11.dll and the .ini file.
	InitD311();

	// If we are being requested to create a DXGIFactory2, lie and say it's not possible.
	if (riid == __uuidof(IDXGIFactory2) && !G->enable_platform_update)
	{
		LogInfo("  returns E_NOINTERFACE as error for IDXGIFactory2.\n");
		*ppFactory1 = NULL;
		return E_NOINTERFACE;
	}

	// Call original factory, regardless of what they requested, to keep the
	// same expected sequence from their perspective.  (Which includes refcounts)
	HRESULT hr = fnOrigCreateDXGIFactory1(riid, ppFactory1);
	if (FAILED(hr))
	{
		LogInfo("->failed with HRESULT=%x\n", hr);
		return hr;
	}

	//if (!fnOrigQueryInterface)
	//	HookQueryInterface(*ppFactory1);

	if (!fnOrigCreateSwapChain)
		HookCreateSwapChain(*ppFactory1);

	// With the addition of the platform_update, we need to allow for specifically
	// creating a DXGIFactory2 instead of DXGIFactory1.  We want to always upcast
	// the highest supported object for each scenario, to properly suppport
	// QueryInterface and GetParent upcasts.

	IDXGIFactory2* dxgiFactory = reinterpret_cast<IDXGIFactory2*>(*ppFactory1);
	HRESULT res = dxgiFactory->QueryInterface(IID_PPV_ARGS(&dxgiFactory));
	if (SUCCEEDED(res))
	{
		*ppFactory1 = (void*)dxgiFactory;
		LogInfo("  Upcast QueryInterface(IDXGIFactory2) returned result = %x, factory = %p\n", res, dxgiFactory);

		if (!fnOrigCreateSwapChainForHwnd)
			HookCreateSwapChainForHwnd(*ppFactory1);
	}

	LogInfo("  CreateDXGIFactory1 returned factory = %p, result = %x\n", *ppFactory1, hr);
	return hr;
}


// -----------------------------------------------------------------------------

// Some partly obsolete comments, but still maybe worthwhile as thoughts on DXGI.

// The hooks need to be installed late, and cannot be installed during DLLMain, because
// they need to be installed in the COM object vtable itself, and the order cannot be
// defined that early.  Because the documentation says it's not viable at DLLMain time,
// we'll install these hooks at InitD311() time, essentially the first call of D3D11.
// 
// The piece we care about in DXGI is the swap chain, and we don't otherwise have a
// good way to access it.  It can be created directly via DXGI, and not through 
// CreateDeviceAndSwapChain.
//
// Not certain, but it seems likely that we only need to hook a given instance of the
// calls we want, because they are not true objects with attached vtables, they have
// a non-standard vtable/indexing system, and the main differentiator is the object
// passed in as 'this'.  
//
// After much experimentation and study, it seems clear that we should use the in-proc
// version of Deviare. I tried to see if Deviare2 would be a match, but they have a 
// funny event callback mechanism that requires an ATL object connection, and is not
// really suited for same-process operations.  It's really built with separate
// processes in mind.

// For this object, we want to use the CINTERFACE, not the C++ interface.
// The reason is that it allows us easy access to the COM object vtable, which
// we need to hook in order to override the functions.  Once we include dxgi.h
// it will be defined with C headers instead.
//
// This is a little odd, but it's the way that Detours hooks COM objects, and
// thus it seems superior to the Nektra approach of groping the vtable directly
// using constants and void* pointers.
// 
// This is only used for .cpp file here, not the .h file, because otherwise other
// units get compiled with this CINTERFACE, which wrecks their calling out.

// -----------------------------------------------------------------------------

// Obsolete code that is useful for reference only.
// This is a technique I created, doesn't exist on the internet.

//#define CINTERFACE
//#include <dxgi.h>
//#undef CINTERFACE
//
//#include <d3d11_1.h>
//
//#include "log.h"
//#include "DLLMainHook.h"
////#include "Overlay.h"

//bool InstallDXGIHooks(void)
//{
//	// Seems like we need to do this in order to init any use of COM here.
//	CoInitializeEx(NULL, COINIT_MULTITHREADED);
//
//	HRESULT result;
//	IDXGIFactory* factory;
//		
//	// We need a DXGI interface factory in order to get access to the CreateSwapChain
//	// call. This CreateDXGIFactory call is defined as an export in dxgi.dll and is
//	// called normally.  It will return an interface to a COM object.
//	// We will have access to the COM routines available after we have one of these.
//
//	result = CreateDXGIFactory(IID_IDXGIFactory, (void**)(&factory));
//	if (FAILED(result))
//	{
//		LogInfo("*** InstallDXGIHooks CreateDXGIFactory failed: %d\n", result);
//		return false;
//	}
//
//	// Hook the factory call for CreateSwapChain, as the game may use that to create
//	// its swapchain instead of CreateDeviceAndSwapChain.
//
//	cHookMgr.SetEnableDebugOutput(TRUE);
//
//	// Routine address fetched from the COM vtable.  This address will be patched by
//	// deviare to jump to HookedCreateSwapChain, which will then need the original
//	// address to call the unmodified function, found in OrigCreateSwapChain.
//
//	LPVOID CreateSwapChain = factory->lpVtbl->CreateSwapChain;
//
//	DWORD dwOsErr;
//	dwOsErr = cHookMgr.Hook(&nCSCHookId, &CreateSwapChain, &OrigCreateSwapChain, HookedCreateSwapChain);
//	if (FAILED(dwOsErr))
//	{
//		LogInfo("*** InstallDXGIHooks Hook failed: %d\n", dwOsErr);
//		return false;
//	}
//
//	LogInfo("InstallDXGIHooks CreateSwapChain result: %d, at: %p\n", dwOsErr, OrigCreateSwapChain);
//
//
//	// Create a SwapChain, just so we can get access to its vtable, and thus hook
//	// the Present() call to ours.
//
//	// not sure where to get device from.
//	//IDXGIFactory_CreateSwapChain(factory, pDevice, pDesc, ppSwapChain);
//
//	//HackerDevice *pDevice;
//	//DXGI_SWAP_CHAIN_DESC *pDesc;
//	//IDXGISwapChain *ppSwapChain;
//	//IDXGIFactory2_CreateSwapChain(factory, pDevice, pDesc, &ppSwapChain);
//
//	return true;
//}

//void UninstallDXGIHooks()
//{
//	DWORD dwOsErr; 
//	dwOsErr = cHookMgr.Unhook(nCSCHookId);
//}

// -----------------------------------------------------------------------------
//HookedSwapChain::HookedSwapChain(IDXGISwapChain* pOrigSwapChain)
//{
//	DWORD dwOsErr;
//
//	mOrigSwapChain = pOrigSwapChain;
//
//	if (pOrigPresent == NULL)
//	{
//		LPVOID dxgiSwapChain = pOrigSwapChain->lpVtbl->Present;
//
//		cHookMgr.SetEnableDebugOutput(TRUE);
//		dwOsErr = cHookMgr.Hook(&nCSCHookId, (LPVOID*)&pOrigPresent, &dxgiSwapChain, HookedPresent);
//		if (dwOsErr)
//		{
//			LogInfo("*** HookedSwapChain::HookedSwapChain Hook failed: %d\n", dwOsErr);
//			return;
//		}
//	}
//	LogInfo("HookedSwapChain::HookedSwapChain hooked Present result: %d, at: %p\n", dwOsErr, pOrigPresent);
//}
//
//
//HookedSwapChain::~HookedSwapChain()
//{
//	
//}


// Hook the Present call in the DXGI COM interface.
// Will only hook once, because there is only one instance
// of the actual underlying code.
//
// The cHookMgr is assumed to already be created and initialized by the
// C++ runtime, even if we are not hooking in DLLMain.

//void HookSwapChain(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
//{
//	DWORD dwOsErr;
//	HRESULT hr;
//
//	if (pOrigPresent != NULL)
//	{
//		LogInfo("*** HookSwapChain called again. SwapChain: %p, Device: %p, Context: %p\n", pSwapChain, pDevice, pContext);
//		return;
//	}
//
//	// Seems like we need to do this in order to init any use of COM here.
//	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
//	if (hr != NOERROR)
//		LogInfo("HookSwapChain CoInitialize return error: %d\n", hr);
//
//	// The tricky part- fetching the actual address of the original
//	// DXGI::Present call.
//	LPVOID dxgiSwapChain = pSwapChain->lpVtbl->Present;
//
//	cHookMgr.SetEnableDebugOutput(bLog);
//
//	dwOsErr = cHookMgr.Hook(&nHookId, (LPVOID*)&pOrigPresent, dxgiSwapChain, HookedPresent, 0);
//	if (dwOsErr)
//	{
//		LogInfo("*** HookSwapChain Hook failed: %d\n", dwOsErr);
//		return;
//	}
//
//
//	// Create Overlay class that will be responsible for drawing any text
//	// info over the game. Using the original Device and Context.
//	//	overlay = new Overlay(pDevice, pContext);
//
//	LogInfo("HookSwapChain hooked Present result: %d, at: %p\n", dwOsErr, pOrigPresent);
//}

// -----------------------------------------------------------------------------

// The hooked static version of Present.
//
// It is worth noting, since it took me 3 days to figure it out, than even though
// this is defined C style, that it must use STDMETHODCALLTYPE (or__stdcall) 
// because otherwise the stack is broken by the different calling conventions.
//
// In normal method calls, the 'this' parameter is implicitly added.  Since we are
// using the C style dxgi interface though, we are declaring this routine differently.
//
// Since we want to allow reentrancy for this call, we need to use the returned
// lpfnPresent to call the original, instead of the alternate approach offered by
// Deviare.

//static HRESULT STDMETHODCALLTYPE HookedPresent(
//	IDXGISwapChain * This,
//	/* [in] */ UINT SyncInterval,
//	/* [in] */ UINT Flags)
//{
//	HRESULT hr;
//
//	// Draw the on-screen overlay text with hunting info, before final Present.
//	//overlay->DrawOverlay();
//
//	hr = pOrigPresent(This, SyncInterval, Flags);
//
//	LogDebug("HookedPresent result: %d\n", hr);
//
//	return hr;
//}



// Functionality removed during refactoring.

//
//STDMETHODIMP HackerDXGIFactory::MakeWindowAssociation(THIS_
//	HWND WindowHandle,
//	UINT Flags)
//{
//	if (LogFile)
//	{
//		LogInfo("HackerDXGIFactory::MakeWindowAssociation(%s@%p) called with WindowHandle = %p, Flags = %x\n", type_name(this), this, WindowHandle, Flags);
//		if (Flags) LogInfoNoNL("  Flags =");
//		if (Flags & DXGI_MWA_NO_WINDOW_CHANGES) LogInfoNoNL(" DXGI_MWA_NO_WINDOW_CHANGES(no monitoring)");
//		if (Flags & DXGI_MWA_NO_ALT_ENTER) LogInfoNoNL(" DXGI_MWA_NO_ALT_ENTER");
//		if (Flags & DXGI_MWA_NO_PRINT_SCREEN) LogInfoNoNL(" DXGI_MWA_NO_PRINT_SCREEN");
//		if (Flags) LogInfo("\n");
//	}
//
//	if (G->SCREEN_ALLOW_COMMANDS && Flags)
//	{
//		LogInfo("  overriding Flags to allow all window commands\n");
//
//		Flags = 0;
//	}
//	HRESULT hr = mOrigFactory->MakeWindowAssociation(WindowHandle, Flags);
//	LogInfo("  returns result = %x\n", hr);
//
//	return hr;
//}
//
//
//
//// For any given SwapChain created by the factory here, we want to wrap the 
//// SwapChain so that we can get called when Present() is called.
////
//// When this is called by a game that creates their swap chain directly from the 
//// Factory object, it's possible that mHackerDevice and mHackerContext are null,
//// because they don't exist when the factory is instantiated.
//// Because of this, we want to check for null, and if so, set those up here, 
//// because we have the input pDevice that we can use to wrap.
////
//// We always expect the pDevice passed in here to be a HackerDevice. If we
//// get one that is not, we have an object leak/bug. It shouldn't be possible to
//// create a ID3D11Device without us wrapping it. 
//// But.. it looks like it's legitimate to pass an IDXGIDevice as the pDevice
//// here.  So, we need to handle either case.  If the input pDevice is not a
//// wrapped device, then the typeid(*pDevice) will throw an RTTI exception.
////
//// When creating the new swap chain, we need to pass the original device, not
//// the wrapped version. For some reason, passing the wrapped version actually
//// succeeds if the "evil" update is installed, which I would not expect.  Without
//// the platform update, it would crash here.
////
//// It's not clear if we should try to handle null inputs for pDevice, even knowing
//// that there is a lot of terrible code out there calling this.
//// Also if we get a non-wrapped pDevice here, the typid(*pdevice) will crash with
//// an RTTI exception, which we could catch.  Not sure how heroic we want to be here.
//// After some thought, current operating philosophy for this routine will be to
//// not wrap these with an exception handler, as we want to know when games do
//// something crazy, and a crash will let us know.  If we were to just catch and
//// release some crazy stuff, it's not likely to work anyway, and a hard/fragile
//// failure is superior in that case.
//
//STDMETHODIMP HackerDXGIFactory::CreateSwapChain(THIS_
//	/* [annotation][in] */
//	_In_  IUnknown *pDevice,
//	/* [annotation][in] */
//	_In_  DXGI_SWAP_CHAIN_DESC *pDesc,
//	/* [annotation][out] */
//	_Out_  IDXGISwapChain **ppSwapChain)
//{
//	LogInfo("\n *** HackerDXGIFactory::CreateSwapChain(%s@%p) called with parameters\n", type_name(this), this);
//	LogInfo("  Device = %s@%p\n", type_name(pDevice), pDevice);
//	LogInfo("  SwapChain = %p\n", ppSwapChain);
//	LogInfo("  Description = %p\n", pDesc);
//
//	// CreateSwapChain could be called with a IDXGIDevice or ID3D11Device
//	HackerDevice *hackerDevice = NULL;
//	IUnknown *origDevice = NULL;
//
//	hackerDevice = (HackerDevice*)lookup_hooked_device((ID3D11Device*)pDevice);
//	if (hackerDevice)
//	{
//		origDevice = pDevice;
//	}
//	else if (typeid(*pDevice) == typeid(HackerDevice))
//	{
//		hackerDevice = static_cast<HackerDevice*>(pDevice);
//		origDevice = hackerDevice->GetOrigDevice();
//	}
//	else if (typeid(*pDevice) == typeid(HackerDevice1))
//	{
//		// Needed for Batman:Telltale games
//		hackerDevice = static_cast<HackerDevice1*>(pDevice);
//		origDevice = hackerDevice->GetOrigDevice();
//	}
//	else if (typeid(*pDevice) == typeid(HackerDXGIDevice))
//	{
//		hackerDevice = static_cast<HackerDXGIDevice*>(pDevice)->GetHackerDevice();
//		origDevice = static_cast<HackerDXGIDevice*>(pDevice)->GetOrigDXGIDevice();
//	}
//	else if (typeid(*pDevice) == typeid(HackerDXGIDevice1))
//	{
//		hackerDevice = static_cast<HackerDXGIDevice1*>(pDevice)->GetHackerDevice();
//		origDevice = static_cast<HackerDXGIDevice1*>(pDevice)->GetOrigDXGIDevice1();
//	}
//	else if (typeid(*pDevice) == typeid(HackerDXGIDevice2))
//	{
//		hackerDevice = static_cast<HackerDXGIDevice2*>(pDevice)->GetHackerDevice();
//		origDevice = static_cast<HackerDXGIDevice2*>(pDevice)->GetOrigDXGIDevice2();
//	}
//	else {
//		LogInfo("FIXME: CreateSwapChain called with device of unknown type!\n");
//		return E_FAIL;
//	}
//
//	HRESULT hr;
//
//	HackerSwapChain *swapchainWrap = nullptr;
//	bool setFullscreenRequired = false;
//	DXGI_SWAP_CHAIN_DESC originalSwapChainDesc;
//
//	if (pDesc != nullptr) {
//		// Save off the window handle so we can translate mouse cursor
//		// coordinates to the window:
//		G->hWnd = pDesc->OutputWindow;
//
//		if (G->SCREEN_UPSCALING > 0)
//		{
//			// For the case the upscaling is on the information if the fullscreen have to be set after swap chain is created
//			setFullscreenRequired = !pDesc->Windowed;
//			pDesc->Windowed = true;
//			// Copy input swap chain desc for case the upscaling is on
//			memcpy(&originalSwapChainDesc, pDesc, sizeof(DXGI_SWAP_CHAIN_DESC));
//		}
//
//		// Require in case the software mouse and upscaling are on at the same time
//		// TODO: Use a helper class to track *all* different resolutions
//		G->GAME_INTERNAL_WIDTH = pDesc->BufferDesc.Width;
//		G->GAME_INTERNAL_HEIGHT = pDesc->BufferDesc.Height;
//
//		if (G->mResolutionInfo.from == GetResolutionFrom::SWAP_CHAIN) {
//			// TODO: Use a helper class to track *all* different resolutions
//			G->mResolutionInfo.width = pDesc->BufferDesc.Width;
//			G->mResolutionInfo.height = pDesc->BufferDesc.Height;
//			LogInfo("Got resolution from swap chain: %ix%i\n",
//				G->mResolutionInfo.width, G->mResolutionInfo.height);
//		}
//	}
//
//	ForceDisplayParams(pDesc);
//
//	hr = mOrigFactory->CreateSwapChain(origDevice, pDesc, ppSwapChain);
//
//	if (SUCCEEDED(hr)) // First swap chain was successfully created and upscaling is on
//	{
//		if (G->SCREEN_UPSCALING > 0)
//		{
//			try
//			{
//				// Do not need to check pDesc == null because if this the case previos call of the CreateSwapChain would fail
//				swapchainWrap = new HackerUpscalingSwapChain(*ppSwapChain, hackerDevice, hackerDevice->GetHackerContext(), &originalSwapChainDesc, pDesc->BufferDesc.Width, pDesc->BufferDesc.Height, mOrigFactory);
//				LogInfo("  HackerUpscalingSwapChain %p created to wrap %p.\n", swapchainWrap, ppSwapChain);
//			}
//			catch (const Exception3DMigoto& e)
//			{
//				LogInfo("HackerDXGIFactory::CreateSwapChain(): Creation of Upscaling Swapchain failed. Error: %s\n", e.what().c_str());
//				// Something went wrong inform the user with double beep and end!;
//				DoubleBeepExit();
//			}
//		}
//		else
//		{
//			swapchainWrap = new HackerSwapChain(*ppSwapChain, hackerDevice, hackerDevice->GetHackerContext());
//			LogInfo("->HackerSwapChain %p created to wrap %p\n", swapchainWrap, *ppSwapChain);
//		}
//	}
//	else
//	{
//		LogInfo("  failed result = %#x for device:%p, swapchain:%p\n", hr, pDevice, ppSwapChain);
//		return hr;
//	}
//
//	if (G->SCREEN_UPSCALING == 2 || setFullscreenRequired)
//	{
//		// Some games seems to react very strange (like render nothing) if set full screen state is called here)
//		// Other games like The Witcher 3 need the call to ensure entering the full screen on start (seems to be game internal stuff)
//		// If something would go wrong we would not get here
//		(*ppSwapChain)->SetFullscreenState(TRUE, nullptr);
//	}
//
//	// And again if something would go wrong we would not get here
//	*ppSwapChain = reinterpret_cast<IDXGISwapChain*>(swapchainWrap);
//
//	LogInfo("->return value = %#x\n\n", hr);
//	return hr;
//}
//