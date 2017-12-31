// Object			OS				DXGI version	Feature level
// IDXGIFactory		Win7			1.0				11.0
// IDXGIFactory1	Win7			1.0				11.0
// IDXGIFactory2	Platform update	1.2				11.1
// IDXGIFactory3	Win8.1			1.3
// IDXGIFactory4					1.4
// IDXGIFactory5					1.5

#include <d3d11.h>

#include "DLLMainHook.h"
#include "log.h"
#include "util.h"
#include "D3D11Wrapper.h"

#include "HookedDXGI.h"
#include "HackerDXGI.h"


// This class is for a different approach than the wrapping of the system objects
// like we do with ID3D11Device for example.  When we wrap a COM object like that,
// it's not a real C++ object, and consequently cannot use the superclass normally,
// and requires boilerplate call-throughs for every interface to the object.  We
// may only care about a 5 calls, but we have to wrap all 150 calls. 
//
// Rather than do that with DXGI, this approach will be to singly hook the calls we
// are interested in, using the Deviare in-proc hooking.  We'll still create
// objects for encapsulation where necessary, by returning HackerDXGIFactory1
// and HackerDXGIFactory2 when platform_update is set.  We won't ever return
// HackerDXGIFactory because the minimum on Win7 is IDXGIFactory1.


// -----------------------------------------------------------------------------
// The signature copied from dxgi.h, in C section.
// This unusual format also provides storage for the original pointer to the 
// routine.

HRESULT(__stdcall *pOrigCreateSwapChain)(IDXGIFactory* This,
	/* [in] */  IUnknown             *pDevice,
	/* [in] */  DXGI_SWAP_CHAIN_DESC *pDesc,
	/* [out] */ IDXGISwapChain       **ppSwapChain
	) = nullptr;


// Our override method for any callers to CreateSwapChain.

HRESULT __stdcall Hooked_CreateSwapChain(IDXGIFactory* This,
	/* [in] */  IUnknown             *pDevice,
	/* [in] */  DXGI_SWAP_CHAIN_DESC *pDesc,
	/* [out] */ IDXGISwapChain       **ppSwapChain)
{
	HRESULT hr;
	IDXGISwapChain *mOrigSwapChain;

	hr = pOrigCreateSwapChain(This, pDevice, pDesc, ppSwapChain);
	if (SUCCEEDED(hr))
		mOrigSwapChain = *ppSwapChain;

	LogInfo("HookedSwapChain::HookedCreateSwapChain mOrigSwapChain: %p, pDevice: %p, result: %d\n", mOrigSwapChain, pDevice, hr);

	return hr;
}


// -----------------------------------------------------------------------------

static HRESULT WrapFactory1(void **ppFactory1)
{
	LogInfo("Calling original CreateDXGIFactory1 API\n");

//	IDXGIFactory1 *origFactory1;
	HRESULT hr = fnOrigCreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)ppFactory1);
	if (FAILED(hr))
	{
		LogInfo("  failed with HRESULT=%x\n", hr);
		return hr;
	}
	LogInfo("  CreateDXGIFactory1 returned factory = %p, result = %x\n", *ppFactory1, hr);

	//HackerDXGIFactory1 *factory1Wrap;
	//factory1Wrap = new HackerDXGIFactory1(origFactory1);

	//if (ppFactory1)
	//	*ppFactory1 = factory1Wrap;
	//LogInfo("->new HackerDXGIFactory1(%s@%p) wrapped %p\n", type_name(factory1Wrap), factory1Wrap, origFactory1);

	return hr;
}

// -----------------------------------------------------------------------------

// No wrap here, just hooking.  platform_update=1 is only hook for DXGI

static HRESULT WrapFactory2(void **ppFactory2)
{
	// To create an original Factory2, this is not hooked out of the DXGI.dll, as
	// it's not defined there.  But, we can call into fnOrigCreateFactory1 for it.

	//IDXGIFactory2 *origFactory2;
	HRESULT hr = fnOrigCreateDXGIFactory1(__uuidof(IDXGIFactory2), (void **)ppFactory2);
	if (FAILED(hr))
	{
		LogInfo("  failed with HRESULT=%x\n", hr);
		return hr;
	}
	LogInfo("->CreateDXGIFactory2 returned factory = %p, result = %x\n", ppFactory2, hr);

	//HackerDXGIFactory2 *factory2Wrap;
	//factory2Wrap = new HackerDXGIFactory2(origFactory2);

	//if (ppFactory2)
	//	*ppFactory2 = factory2Wrap;

	//LogInfo("  new HackerDXGIFactory2(%s@%p) wrapped %p\n", type_name(factory2Wrap), factory2Wrap, origFactory2);

	// ToDo: Skipped null checks as they would throw exceptions- but
	// we should handle exceptions.

	// With the factory created, hook CreateSwapChain now.
	//SIZE_T hook_id;
	//DWORD dwOsErr = cHookMgr.Hook(&hook_id, (void**)&pOrigCreateSwapChain,
	//	lpvtbl_CreateSwapChain((IDXGIFactory*)*ppFactory2), Hooked_CreateSwapChain, 0);

	return hr;
}

// -----------------------------------------------------------------------------

// This serves a dual purpose of defining the interface routine as required by
// DXGI, and also is the storage for the original call, returned by cHookMgr.Hook.

HRESULT(__stdcall *fnOrigPresent)(
	IDXGISwapChain * This,
	/* [in] */ UINT SyncInterval,
	/* [in] */ UINT Flags) = nullptr;


static int frames = 0;

HRESULT __stdcall Hooked_Present(
	IDXGISwapChain * This,
	/* [in] */ UINT SyncInterval,
	/* [in] */ UINT Flags)
{
	frames++;
	if (frames % 30)
		LogInfo("Hooked_Present\n");

	return fnOrigPresent(This, SyncInterval, Flags);
}


// -----------------------------------------------------------------------------
// Hook the current game's Present call.
// 
// This takes a bunch of junk setup to get there, but we want to create a
// SwapChain, so that we can access its Present call.  To do that, we also
// need a Device, so we'll use CreateDeviceAndSwapChain, because at this 
// moment we have no idea what the Device would be, or whether there 
// actually is one.  We'll just make one and dispose if it when done.
// In order to CreateDeviceAndSwapChain, we also need a Window to pass
// to the SwapChain description. We'll make that and dispose of it when done too.

void HookPresent()
{
	LogInfo("*** Creating hook for Present. \n");

	// Create a window, that will be invisible to start, and disposed here.
	HWND hWnd = CreateWindow(
		L"STATIC",    // name of the window class
		NULL,   // title of the window
		WS_OVERLAPPED,    // window style
		300,    // x-position of the window
		300,    // y-position of the window
		500,    // width of the window
		400,    // height of the window
		NULL,    // we have no parent window, NULL
		NULL,    // we aren't using menus, NULL
		NULL,    // application handle
		NULL);    // used with multiple windows, NULL
	if (hWnd == NULL)
	{
		DWORD last = GetLastError();
		LogInfo("*** Fail to HookPresent, CreateWindowEx err: %d", last);
		return;
	}

	DXGI_SWAP_CHAIN_DESC scd = { 0 };
	scd.BufferCount = 1;                                    // one back buffer
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // use 32-bit color
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // how swap chain is to be used
	scd.OutputWindow = hWnd;                                // the window to be used
	scd.SampleDesc.Count = 4;                               // how many multisamples
	scd.Windowed = TRUE;                                    // windowed/full-screen mode

	IDXGISwapChain *swapChain;     
	ID3D11Device *device;          

	HRESULT hr = (*_D3D11CreateDeviceAndSwapChain)(NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		NULL,
		NULL,
		NULL,
		D3D11_SDK_VERSION,
		&scd,
		&swapChain,
		&device,
		NULL,
		nullptr);
	if (FAILED(hr))
	{
		LogInfo("*** Fail to HookPresent, _D3D11CreateDeviceAndSwapChain err: %x", hr);
		return;
	}
	
	// Now with all that jazz out of the way, we have the swapChain, and can use it
	// to find the address of the Present call. Let's hook that call, because we need
	// it to drive our Overlay, Hunting, and override functions.
	SIZE_T hook_id;
	DWORD dwOsErr = cHookMgr.Hook(&hook_id, (void**)&fnOrigPresent,
		lpvtbl_Present(swapChain), Hooked_Present, 0);

	// With the routine hooked now, so time to cleanup.
	swapChain->Release();
	device->Release();
	DestroyWindow(hWnd);

	LogInfo("  Successfully installed IDXGISwapChain->Present hook.\n");
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
	//if (riid == __uuidof(IDXGIFactory2) && !G->enable_platform_update)
	//{
	//	LogInfo("  returns E_NOINTERFACE as error for IDXGIFactory2.\n");
	//	*ppFactory = NULL;
	//	return E_NOINTERFACE;
	//}

	//HRESULT hr;
	//if (G->enable_platform_update)
	//	hr = WrapFactory2(ppFactory);
	//else
	//	hr = WrapFactory1(ppFactory);


	// For hooking only, no wrapping, we want to just create a swap chain, then hook
	// the Present call.  We need to return their factory regardless.
	HRESULT hr = fnOrigCreateDXGIFactory(riid, ppFactory);
	if (SUCCEEDED(hr))
	{
		HookPresent();
	}

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

	// With the addition of the platform_update, we need to allow for specifically
	// creating a DXGIFactory2 instead of DXGIFactory1.  We want to always upcast
	// the highest supported object for each scenario, to properly suppport
	// QueryInterface and GetParent upcasts.

	// Minimal Factory supported for base Win7 is IDXGIFactory1, so let's always
	// return at least that.

	HRESULT hr;
	if (G->enable_platform_update)
		hr = WrapFactory2(ppFactory1);
	else
		hr = WrapFactory1(ppFactory1);

	// This sequence makes Witcher3 crash.  They also send in uuid=IDXGIFactory to this
	// Factory1 object.  Not supposed to be legal, but apparently the factory will still 
	// make a Factory1 object.  
	// If we were requested to create a DXGIFactory, go ahead and make our wrapper.
	//if (riid == __uuidof(IDXGIFactory))
	//{
	//	HackerDXGIFactory *factoryWrap;
	//	factoryWrap = new HackerDXGIFactory(static_cast<IDXGIFactory*>(origFactory1));
	//	if (ppFactory1)
	//		*ppFactory1 = factoryWrap;
	//	LogInfo("  new HackerDXGIFactory(%s@%p) wrapped %p\n", type_name(factoryWrap), factoryWrap, origFactory1);
	//}
	//else
	//   Seems like we really need to return the highest level object supported at runtime,
	//   in order to more closely match how DXGI works.  It looks to me like the DXGI will
	//   always create a higher level object, and return it as a downcast, and then return
	//   the high level object when QueryInterface upcast is used.  Sucky hacky mechanism,
	//   but I'm pretty sure that's how it works.


	// ToDo: Skipped null checks as they would throw exceptions- but
	// we should handle exceptions.

	// We are returning a "IDXGIFactory1" here, but it will actually be wrapped as a
	// Hacker object, and be either HackerDXGIFactory1 or HackerDXGIFactory2;

	return hr;
}


// -----------------------------------------------------------------------------



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
//#include <d3d11.h>
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

