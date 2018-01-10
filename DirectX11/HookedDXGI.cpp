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

#include "DLLMainHook.h"
#include "log.h"
#include "util.h"
#include "D3D11Wrapper.h"

#include "Hunting.h"
#include "input.h"
#include "Override.h"
#include "IniHandler.h"
#include "Overlay.h"


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

static BOOL(WINAPI *trampoline_SetWindowPos)(_In_ HWND hWnd, _In_opt_ HWND hWndInsertAfter,
	_In_ int X, _In_ int Y, _In_ int cx, _In_ int cy, _In_ UINT uFlags)
	= SetWindowPos;

static BOOL WINAPI Hooked_SetWindowPos(
	_In_ HWND hWnd,
	_In_opt_ HWND hWndInsertAfter,
	_In_ int X,
	_In_ int Y,
	_In_ int cx,
	_In_ int cy,
	_In_ UINT uFlags)
{
	if (G->SCREEN_UPSCALING != 0) {
		// Force desired upscaled resolution (only when desired resolution is provided!)
		if (cx != 0 && cy != 0) {
			cx = G->SCREEN_WIDTH;
			cy = G->SCREEN_HEIGHT;
			X = 0;
			Y = 0;
		}
	}
	else if (G->SCREEN_FULLSCREEN == 2) {
		// Do nothing - passing this call through could change the game
		// to a borderless window. Needed for The Witness.
		return true;
	}

	return trampoline_SetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

void InstallSetWindowPosHook()
{
	HINSTANCE hUser32;
	static bool hook_installed = false;
	int fail = 0;

	// Only attempt to hook it once:
	if (hook_installed)
		return;
	hook_installed = true;

	hUser32 = NktHookLibHelpers::GetModuleBaseAddress(L"User32.dll");
	fail |= InstallHook(hUser32, "SetWindowPos", (void**)&trampoline_SetWindowPos, Hooked_SetWindowPos, true);

	if (fail) {
		LogInfo("Failed to hook SetWindowPos for full_screen=2\n");
		BeepFailure2();
		return;
	}

	LogInfo("Successfully hooked SetWindowPos for full_screen=2\n");
	return;
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
		// This hook is very important in case of upscaling
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

// -----------------------------------------------------------------------------

// Called at each DXGI::Present() to give us reliable time to execute user
// input and hunting commands.

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


static void RunFrameActions(HackerDevice *mHackerDevice, HackerContext *mHackerContext, Overlay *mOverlay)
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
		}
		else {
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

	CurrentTransition.UpdatePresets(mHackerDevice);
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
		EnterCriticalSection(&G->mCriticalSection);
		TimeoutHuntingBuffers();
		LeaveCriticalSection(&G->mCriticalSection);
	}
}

// -----------------------------------------------------------------------------
// This serves a dual purpose of defining the interface routine as required by
// DXGI, and also is the storage for the original call, returned by cHookMgr.Hook.

HRESULT(__stdcall *fnOrigPresent)(
	IDXGISwapChain * This,
	/* [in] */ UINT SyncInterval,
	/* [in] */ UINT Flags) = nullptr;


HRESULT __stdcall Hooked_Present(
	IDXGISwapChain * This,
	/* [in] */ UINT SyncInterval,
	/* [in] */ UINT Flags)
{
	LogDebug("Hooked DXGISwapChain::Present(%p) called\n", This);
	LogDebug("  SyncInterval = %d\n", SyncInterval);
	LogDebug("  Flags = %d\n", Flags);

	// For late binding, if mOverlay is null then we need to create it.
	// Since we are hooking DXGISwapChain, there is no object to attach to.
	if (G->gOverlay == nullptr)
		G->gOverlay = new Overlay(G->gHackerDevice, G->gHackerContext, This);
	if (G->gSwapChain == nullptr)
		G->gSwapChain = This;

	if (!(Flags & DXGI_PRESENT_TEST)) {
		// Every presented frame, we want to take some CPU time to run our actions,
		// which enables hunting, and snapshots, and aiming overrides and other inputs
		RunFrameActions(G->gHackerDevice, G->gHackerContext, G->gOverlay);
	}

	HRESULT hr = fnOrigPresent(This, SyncInterval, Flags);

	if (!(Flags & DXGI_PRESENT_TEST)) {
		// Update the stereo params texture just after the present so that we
		// get the new values for the current frame:
		UpdateStereoParams(G->gHackerDevice, G->gHackerContext);

		// Run the post present command list now, which can be used to restore
		// state changed in the pre-present command list, or to perform some
		// action at the start of a frame:
		RunCommandList(G->gHackerDevice, G->gHackerContext, &G->post_present_command_list, NULL, true);
	}

	LogDebug("  returns %x\n", hr);
	return hr;
}


// Present1 for IDXGIFactory2 created CreateSwapChainforHwnd, swap chains.

HRESULT(__stdcall *fnOrigPresent1)(
	IDXGISwapChain1 * This,
	/* [in] */ UINT SyncInterval,
	/* [in] */ UINT PresentFlags,
	/* [annotation][in] */
	_In_  const DXGI_PRESENT_PARAMETERS *pPresentParameters) = nullptr;


HRESULT __stdcall Hooked_Present1(
	IDXGISwapChain1 * This,
	/* [in] */ UINT SyncInterval,
	/* [in] */ UINT PresentFlags,
	/* [annotation][in] */
	_In_  const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
	LogDebug("Hooked DXGISwapChain1::Present1(%p) called\n", This);
	LogDebug("  SyncInterval = %d\n", SyncInterval);
	LogDebug("  Flags = %d\n", PresentFlags);

	// For late binding, if mOverlay is null then we need to create it.
	// Since we are hooking DXGISwapChain, there is no object to attach to.
	if (G->gOverlay == nullptr)
		G->gOverlay = new Overlay(G->gHackerDevice, G->gHackerContext, This);
	if (G->gSwapChain == nullptr)
		G->gSwapChain = This;

	if (!(PresentFlags & DXGI_PRESENT_TEST)) {
		// Every presented frame, we want to take some CPU time to run our actions,
		// which enables hunting, and snapshots, and aiming overrides and other inputs
		RunFrameActions(G->gHackerDevice, G->gHackerContext, G->gOverlay);
	}

	HRESULT hr = fnOrigPresent(This, SyncInterval, PresentFlags);

	if (!(PresentFlags & DXGI_PRESENT_TEST)) {
		// Update the stereo params texture just after the present so that we
		// get the new values for the current frame:
		UpdateStereoParams(G->gHackerDevice, G->gHackerContext);

		// Run the post present command list now, which can be used to restore
		// state changed in the pre-present command list, or to perform some
		// action at the start of a frame:
		RunCommandList(G->gHackerDevice, G->gHackerContext, &G->post_present_command_list, NULL, true);
	}

	LogDebug("  returns %x\n", hr);
	return hr;
}


// -----------------------------------------------------------------------------
// Hook the current game's Present call.
// 
// This takes multiple steps to get there, but when CreateSwapChain
// is called, we want to use that swapchain to find the Present call
// so we can hook it. That Present call drives all our UI.

void HookPresent(IDXGISwapChain* swapChain)
{
	LogInfo("*** CreateSwapChain creating hook for Present. \n");

	SIZE_T hook_id;
	DWORD dwOsErr = cHookMgr.Hook(&hook_id, (void**)&fnOrigPresent,
		lpvtbl_Present(swapChain), Hooked_Present, 0);

	if (dwOsErr == ERROR_SUCCESS)
		LogInfo("  Successfully installed IDXGISwapChain->Present hook.\n");
	else
		LogInfo("  *** Failed install IDXGISwapChain->Present hook.\n");
}

// Present1 call is slightly different, and needs to be hooked for
// the platform_update variant of CreateSwapChainForHwnd.

void HookPresent1(IDXGISwapChain1* swapChain1)
{
	LogInfo("*** CreateSwapChain creating hook for Present1. \n");

	SIZE_T hook_id;
	DWORD dwOsErr = cHookMgr.Hook(&hook_id, (void**)&fnOrigPresent1,
		lpvtbl_Present1(swapChain1), Hooked_Present1, 0);

	if (dwOsErr == ERROR_SUCCESS)
		LogInfo("  Successfully installed IDXGISwapChain->Present hook.\n");
	else
		LogInfo("  *** Failed install IDXGISwapChain->Present hook.\n");
}


// -----------------------------------------------------------------------------
// Actual hook for any IDXGICreateSwapChainForHwnd calls the game makes.
// This can only be called with Win7+platform_update or greater, using
// the IDXGIFactory2.

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
	LogInfo("  Description = %p\n", pDesc);
	LogInfo("  FullScreenDescription = %p\n", pFullscreenDesc);

//	ForceDisplayParams(pDesc);
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullScreen = { 0 };
	fullScreen.Windowed = false;

	HRESULT hr = fnOrigCreateSwapChainForHwnd(This, pDevice, hWnd, pDesc, &fullScreen, pRestrictToOutput, ppSwapChain);
	if (SUCCEEDED(hr))
	{
		if (!fnOrigPresent)
			HookPresent(*ppSwapChain);
		if (!fnOrigPresent1)
			HookPresent1(*ppSwapChain);
	}

	LogInfo("->return result %#x, ppSwapChain = %p\n\n", hr, *ppSwapChain);
	return hr;
}

// -----------------------------------------------------------------------------
// This hook should work in all variants, including the CreateSwapChain1
// and CreateSwapChainForHwnd

void HookCreateSwapChainForHwnd(void* factory2)
{
	LogInfo("*** IDXGIFactory2 creating hook for CreateSwapChainForHwnd. \n");

	IDXGIFactory2* dxgiFactory = static_cast<IDXGIFactory2*>(factory2);

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
	LogInfo("Hooked IDXGIFactory::CreateSwapChain(%p) called\n", This);
	LogInfo("  Device = %p\n", pDevice);
	LogInfo("  SwapChain = %p\n", ppSwapChain);
	LogInfo("  Description = %p\n", pDesc);

	ForceDisplayParams(pDesc);

	HRESULT hr = fnOrigCreateSwapChain(This, pDevice, pDesc, ppSwapChain);
	if (SUCCEEDED(hr) && !fnOrigPresent)
		HookPresent(*ppSwapChain);

	LogInfo("->return result %#x, ppSwapChain = %p\n\n", hr, *ppSwapChain);
	return hr;
}

// -----------------------------------------------------------------------------
// This hook should work in all variants, including the CreateSwapChain1
// and CreateSwapChainForHwnd

void HookCreateSwapChain(void* factory)
{
	LogInfo("*** IDXGIFactory creating hook for CreateSwapChain. \n");

	IDXGIFactory* dxgiFactory = static_cast<IDXGIFactory*>(factory);

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

HRESULT(__stdcall *fnOrigQueryInterface)(
	IDXGIObject * This,
	/* [in] */ REFIID riid,
	/* [annotation][iid_is][out] */
	_COM_Outptr_  void **ppvObject)= nullptr;
 

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

	IDXGIFactory* dxgiFactory = static_cast<IDXGIFactory*>(factory);

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

	if (!fnOrigQueryInterface)
		HookQueryInterface(*ppFactory);

	if (!fnOrigCreateSwapChain)
		HookCreateSwapChain(*ppFactory);

	// With the addition of the platform_update, we need to allow for specifically
	// creating a DXGIFactory2 instead of DXGIFactory1.  We want to always upcast
	// the highest supported object for each scenario, to properly suppport
	// QueryInterface and GetParent upcasts.

	IDXGIFactory2* dxgiFactory = static_cast<IDXGIFactory2*>(*ppFactory);
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

	if (!fnOrigQueryInterface)
		HookQueryInterface(*ppFactory1);

	if (!fnOrigCreateSwapChain)
		HookCreateSwapChain(*ppFactory1);

	// With the addition of the platform_update, we need to allow for specifically
	// creating a DXGIFactory2 instead of DXGIFactory1.  We want to always upcast
	// the highest supported object for each scenario, to properly suppport
	// QueryInterface and GetParent upcasts.

	IDXGIFactory2* dxgiFactory = static_cast<IDXGIFactory2*>(*ppFactory1);
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

