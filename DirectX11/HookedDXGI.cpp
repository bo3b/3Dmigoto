// This class is for a different approach than the wrapping of the system objects
// like we do with ID3D11Device for example.  When we wrap a COM object like that,
// it's not a real C++ object, and consequently cannot use the superclass normally,
// and requires boilerplate call-throughs for every interface to the object.  We
// may only care about a 5 calls, but we have to wrap all 150 calls. 
//
// Rather than do that with DXGI, this approach will be to singly hook the calls we
// are interested in, using the Deviare in-proc hooking.  We'll still have an
// object just for encapsulation, but we won't deliver it back to the game, because
// it won't be of IDXGI* form.  
//
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

#include "HookedDXGI.h"

#include "log.h"
#include "DLLMainHook.h"



// ----------------------------------------------------------------------------

// The signature copied from dxgi.h, in C section.
// This unusual format also provides storage for the original pointer to the 
// routine.

HRESULT(STDMETHODCALLTYPE *OrigCreateSwapChain)(
	IDXGIFactory * This,
	/* [annotation][in] */
	_In_  IUnknown *pDevice,
	/* [annotation][in] */
	_In_  DXGI_SWAP_CHAIN_DESC *pDesc,
	/* [annotation][out] */
	_Out_  IDXGISwapChain **ppSwapChain);


// Our override method for any callers to CreateSwapChain.

HRESULT STDMETHODCALLTYPE HookedCreateSwapChain(
	IDXGIFactory * This,
	/* [annotation][in] */
	_In_  IUnknown *pDevice,
	/* [annotation][in] */
	_In_  DXGI_SWAP_CHAIN_DESC *pDesc,
	/* [annotation][out] */
	_Out_  IDXGISwapChain **ppSwapChain)
{
	HRESULT hr;
	IDXGISwapChain *mOrigSwapChain;

	hr = OrigCreateSwapChain(This, pDevice, pDesc, ppSwapChain);
	mOrigSwapChain = *ppSwapChain;

	LogInfo("HookedSwapChain::HookedCreateSwapChain mOrigSwapChain: %p, pDevice: %p, result: %d \n", mOrigSwapChain, pDevice, hr);

	return hr;
}

// ----------------------------------------------------------------------------

typedef HRESULT(STDMETHODCALLTYPE *lpfnPresent)(
	IDXGISwapChain * This,
	/* [in] */ UINT SyncInterval,
	/* [in] */ UINT Flags);

static HRESULT HookedPresent(
	IDXGISwapChain * This,
	/* [in] */ UINT SyncInterval,
	/* [in] */ UINT Flags);

static SIZE_T nHookId = 0;
static lpfnPresent pOrigPresent = NULL;

// ----------------------------------------------------------------------------

SIZE_T nCSCHookId;

bool InstallDXGIHooks(void)
{
	// Seems like we need to do this in order to init any use of COM here.
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	HRESULT result;
	IDXGIFactory* factory;
		
	// We need a DXGI interface factory in order to get access to the CreateSwapChain
	// call. This CreateDXGIFactory call is defined as an export in dxgi.dll and is
	// called normally.  It will return an interface to a COM object.
	// We will have access to the COM routines available after we have one of these.

	result = CreateDXGIFactory(IID_IDXGIFactory, (void**)(&factory));
	if (FAILED(result))
	{
		LogInfo("*** InstallDXGIHooks CreateDXGIFactory failed: %d \n", result);
		return false;
	}

	// Hook the factory call for CreateSwapChain, as the game may use that to create
	// its swapchain instead of CreateDeviceAndSwapChain.

	cHookMgr.SetEnableDebugOutput(TRUE);

	// Routine address fetched from the COM vtable.  This address will be patched by
	// deviare to jump to HookedCreateSwapChain, which will then need the original
	// address to call the unmodified function, found in OrigCreateSwapChain.

	LPVOID CreateSwapChain = factory->lpVtbl->CreateSwapChain;

	DWORD dwOsErr;
	dwOsErr = cHookMgr.Hook(&nCSCHookId, &CreateSwapChain, &OrigCreateSwapChain, HookedCreateSwapChain);
	if (FAILED(dwOsErr))
	{
		LogInfo("*** InstallDXGIHooks Hook failed: %d \n", dwOsErr);
		return false;
	}

	LogInfo("InstallDXGIHooks CreateSwapChain result: %d, at: %p \n", dwOsErr, OrigCreateSwapChain);


	// Create a SwapChain, just so we can get access to its vtable, and thus hook
	// the Present() call to ours.

	// not sure where to get device from.
	//IDXGIFactory_CreateSwapChain(factory, pDevice, pDesc, ppSwapChain);

	//HackerDevice *pDevice;
	//DXGI_SWAP_CHAIN_DESC *pDesc;
	//IDXGISwapChain *ppSwapChain;
	//IDXGIFactory2_CreateSwapChain(factory, pDevice, pDesc, &ppSwapChain);

	return true;
}

static void UninstallDXGIHooks()
{
	DWORD dwOsErr; 
	dwOsErr = cHookMgr.Unhook(nCSCHookId);
}

// -----------------------------------------------------------------------------------------------

HookedSwapChain::HookedSwapChain(IDXGISwapChain* pOrigSwapChain)
{
	DWORD dwOsErr;

	mOrigSwapChain = pOrigSwapChain;

	if (pOrigPresent == NULL)
	{
		LPVOID dxgiSwapChain = pOrigSwapChain->lpVtbl->Present;

		cHookMgr.SetEnableDebugOutput(TRUE);
		dwOsErr = cHookMgr.Hook(&nCSCHookId, (LPVOID*)&pOrigPresent, &dxgiSwapChain, HookedPresent);
		if (dwOsErr)
		{
			LogInfo("*** HookedSwapChain::HookedSwapChain Hook failed: %d \n", dwOsErr);
			return;
		}
	}
	LogInfo("HookedSwapChain::HookedSwapChain hooked Present result: %d, at: %p \n", dwOsErr, pOrigPresent);
}


HookedSwapChain::~HookedSwapChain()
{
	
}


// The hooked static version of Present.

HRESULT HookedPresent(
	IDXGISwapChain * This,
	/* [in] */ UINT SyncInterval,
	/* [in] */ UINT Flags)
{
	HRESULT hr;

	hr = pOrigPresent(This, SyncInterval, Flags);

	LogInfo("HookedPresent result: %d \n", hr);

	return hr;
}


