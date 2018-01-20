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


// 1-15-18: New approach is keep a strict single-layer policy when wrapping
// objects like IDXGISwapChain1.  Only the top level object we are interested
// in can successfully wrapped, because otherwise the vtable is altered from
// the DX11 definition, which led to crashes.
// 
// Because of this, we are now creating only the HackerSwapChain and no
// other objects.  IDXGIFactory does not need to be wrapped, because it must
// be hooked in order to create the swap chains correctly. Device, Object,
// Adapter, Unknown were only wrapped in order to get us to the swap chain,
// and the new approach is to directly hook the CreateSwapChain and
// CreateSwapChainForHwnd, which saves a lot of complexity.
//
// We are making an IDXGISwapChain1, as the obvious descendant of SwapChain,
// and valid in platform_update scenarios.  It's only created by the
// CreateSwapChainForHwnd, but all our functionality is the same regardless.
// In the situation where the evil platform update is not installed, we will
// only create an IDXGISwapChain, but still reference it via composition in
// the HackerSwapChain.  The model is the same as that used in HackerDevice
// and HackerContext.


#include "HackerDXGI.h"
#include "HookedDevice.h"
#include "HookedDXGI.h"

#include "log.h"
#include "util.h"
#include "globals.h"
#include "Hunting.h"
#include "Override.h"
#include "IniHandler.h"


// -----------------------------------------------------------------------------
// SetWindowPos hook, activated by full_screen=2 in d3dx.ini

static BOOL(WINAPI *fnOrigSetWindowPos)(_In_ HWND hWnd, _In_opt_ HWND hWndInsertAfter,
	_In_ int X, _In_ int Y, _In_ int cx, _In_ int cy, _In_ UINT uFlags) = nullptr;

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

	return fnOrigSetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}


void InstallSetWindowPosHook()
{
	HINSTANCE hUser32;
	int fail = 0;

	// Only attempt to hook it once:
	if (fnOrigSetWindowPos != nullptr)
		return;

	hUser32 = NktHookLibHelpers::GetModuleBaseAddress(L"User32.dll");
	fail |= InstallHook(hUser32, "SetWindowPos", (void**)&fnOrigSetWindowPos, Hooked_SetWindowPos, true);

	if (fail) {
		LogInfo("Failed to hook SetWindowPos for full_screen=2\n");
		BeepFailure2();
		return;
	}

	LogInfo("Successfully hooked SetWindowPos for full_screen=2\n");
	return;
}

// -----------------------------------------------------------------------------

// In the Elite Dangerous case, they Release the HackerContext objects before creating the 
// swap chain.  That causes problems, because we are not expecting anyone to get here without
// having a valid context.  They later call GetImmediateContext, which will generate a wrapped
// context.  So, since we need the context for our Overlay, let's do that a litte early in
// this case, which will save the reference for their GetImmediateContext call.

HackerSwapChain::HackerSwapChain(IDXGISwapChain1 *pSwapChain, HackerDevice *pDevice, HackerContext *pContext)
{
	mOrigSwapChain1 = pSwapChain;

	if (pContext == NULL)
	{
		pDevice->GetImmediateContext(reinterpret_cast<ID3D11DeviceContext**>(&pContext));
	}
	mHackerDevice = pDevice;
	mHackerContext = pContext;

	try {
		// Create Overlay class that will be responsible for drawing any text
		// info over the game. Using the Hacker Device and Context we gave the game.
		mOverlay = new Overlay(pDevice, pContext, this);
	} catch (...) {
		LogInfo("  *** Failed to create Overlay. Exception caught.\n");
		mOverlay = NULL;
	}
}


IDXGISwapChain1* HackerSwapChain::GetOrigSwapChain1()
{
	LogDebug("HackerSwapChain::GetOrigSwapChain returns %p\n", mOrigSwapChain1);
	return mOrigSwapChain1;
}


// -----------------------------------------------------------------------------

void HackerSwapChain::UpdateStereoParams()
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

// Called at each DXGI::Present() to give us reliable time to execute user
// input and hunting commands.

void HackerSwapChain::RunFrameActions()
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
/** IUnknown **/

// In the game Elex, we see them call do the unusual SwapChain->QueryInterface(SwapChain).  
// We need to return This when that happens, because otherwise they disconnect us and
// we never get calls to Present.  Rather than do just this one-off, let's always 
// return This for any time this might happen, as we've seen it happen in HackerContext
// too, for Mafia 3.  So any future instances cannot leak.
//
// From: https://msdn.microsoft.com/en-us/library/windows/desktop/ms682521(v=vs.85).aspx
// And: https://blogs.msdn.microsoft.com/oldnewthing/20040326-00/?p=40033
//
//  For any one object, a specific query for the IUnknown interface on any of the object's 
//	interfaces must always return the same pointer value. This enables a client to determine 
//	whether two pointers point to the same component by calling QueryInterface with 
//	IID_IUnknown and comparing the results. 
//	It is specifically not the case that queries for interfaces other than IUnknown (even 
//	the same interface through the same pointer) must return the same pointer value.
//
STDMETHODIMP HackerSwapChain::QueryInterface(THIS_
	/* [in] */ REFIID riid,
	/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	LogInfo("HackerSwapChain::QueryInterface(%s@%p) called with IID: %s\n", type_name(this), this, NameFromIID(riid).c_str());

	HRESULT hr = mOrigSwapChain1->QueryInterface(riid, ppvObject);
	if (FAILED(hr) || !*ppvObject)
	{
		LogInfo("  failed result = %x for %p\n", hr, ppvObject);
		return hr;
	}

	// For TheDivision, only upon Win10, it will request these.  Even though the object
	// we would return is the exact same pointer in memory, it still calls into the object
	// with a vtable entry that does not match what they expected. Somehow they decide
	// they are on Win10, and know these APIs ought to exist.  Does not crash on Win7.
	//
	// Returning an E_NOINTERFACE here seems to work, but this does call into question our 
	// entire wrapping strategy.  If the object we've wrapped is a superclass of the
	// object they desire, the vtable is not going to match.

	if (riid == __uuidof(IDXGISwapChain2))
	{
		LogInfo("***  returns E_NOINTERFACE as error for IDXGISwapChain2.\n");
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	if (riid == __uuidof(IDXGISwapChain3))
	{
		LogInfo("***  returns E_NOINTERFACE as error for IDXGISwapChain3.\n");
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}

	IUnknown* unk_this;
	HRESULT hr_this = mOrigSwapChain1->QueryInterface(__uuidof(IUnknown), (void**)&unk_this);

	IUnknown* unk_ppvObject;
	HRESULT hr_ppvObject = static_cast<IUnknown*>(*ppvObject)->QueryInterface(__uuidof(IUnknown), (void**)&unk_ppvObject);

	if (SUCCEEDED(hr_this) && SUCCEEDED(hr_ppvObject))
	{
		// For an actual case of this->QueryInterface(this), just return our HackerSwapChain object.
		if (unk_this == unk_ppvObject)
			*ppvObject = this;

		unk_this->Release();
		unk_ppvObject->Release();

		LogInfo("  return HackerUnknown(%s@%p) wrapper of %p\n", type_name(this), this, mOrigSwapChain1);
		return hr;
	}

	LogInfo("  returns result = %x for %p\n", hr, ppvObject);
	return hr;
}

STDMETHODIMP_(ULONG) HackerSwapChain::AddRef(THIS)
{
	ULONG ulRef = mOrigSwapChain1->AddRef();
	LogInfo("HackerUnknown::AddRef(%s@%p), counter=%d, this=%p\n", type_name(this), this, ulRef, this);
	return ulRef;
}

STDMETHODIMP_(ULONG) HackerSwapChain::Release(THIS)
{
	ULONG ulRef = mOrigSwapChain1->Release();
	LogInfo("HackerUnknown::Release(%s@%p), counter=%d, this=%p\n", type_name(this), this, ulRef, this);

	if (ulRef <= 0)
	{
		LogInfo("  counter=%d, this=%p, deleting self.\n", ulRef, this);

		delete this;
		return 0L;
	}
	return ulRef;
}

// -----------------------------------------------------------------------------
/** IDXGIObject **/

STDMETHODIMP HackerSwapChain::SetPrivateData(THIS_
	/* [annotation][in] */
	__in  REFGUID Name,
	/* [in] */ UINT DataSize,
	/* [annotation][in] */
	__in_bcount(DataSize)  const void *pData)
{
	LogInfo("HackerSwapChain::SetPrivateData(%s@%p) called with GUID: %s\n", type_name(this), this, NameFromIID(Name).c_str());
	LogInfo("  DataSize = %d\n", DataSize);

	HRESULT hr = mOrigSwapChain1->SetPrivateData(Name, DataSize, pData);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerSwapChain::SetPrivateDataInterface(THIS_
	/* [annotation][in] */
	__in  REFGUID Name,
	/* [annotation][in] */
	__in  const IUnknown *pUnknown)
{
	LogInfo("HackerSwapChain::SetPrivateDataInterface(%s@%p) called with GUID: %s\n", type_name(this), this, NameFromIID(Name).c_str());

	HRESULT hr = mOrigSwapChain1->SetPrivateDataInterface(Name, pUnknown);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerSwapChain::GetPrivateData(THIS_
	/* [annotation][in] */
	__in  REFGUID Name,
	/* [annotation][out][in] */
	__inout  UINT *pDataSize,
	/* [annotation][out] */
	__out_bcount(*pDataSize)  void *pData)
{
	LogInfo("HackerSwapChain::GetPrivateData(%s@%p) called with GUID: %s\n", type_name(this), this, NameFromIID(Name).c_str());

	HRESULT hr = mOrigSwapChain1->GetPrivateData(Name, pDataSize, pData);
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
// we are taking the secret path for getting the swap chain. 
//
// We no longer return wrapped objects here, because our CreateSwapChain hooks 
// will correctly catch creation.

STDMETHODIMP HackerSwapChain::GetParent(THIS_
	/* [annotation][in] */
	__in  REFIID riid,
	/* [annotation][retval][out] */
	__out  void **ppParent)
{
	LogInfo("HackerSwapChain::GetParent(%s@%p) called with IID: %s\n", type_name(this), this, NameFromIID(riid).c_str());

	HRESULT hr = mOrigSwapChain1->GetParent(riid, ppParent);
	if (FAILED(hr))
	{
		LogInfo("  failed result = %x for %p\n", hr, ppParent);
		return hr;
	}

	LogInfo("  returns result = %#x\n", hr);
	return hr;
}

// -----------------------------------------------------------------------------
/** IDXGIDeviceSubObject **/

STDMETHODIMP HackerSwapChain::GetDevice(
	/* [annotation][in] */
	_In_  REFIID riid,
	/* [annotation][retval][out] */
	_Out_  void **ppDevice)
{
	LogDebug("HackerSwapChain::GetDevice(%s@%p) called with IID: %s\n", type_name(this), this, NameFromIID(riid).c_str());

	HRESULT hr = mOrigSwapChain1->GetDevice(riid, ppDevice);
	LogDebug("  returns result = %x, handle = %p\n", hr, *ppDevice);
	return hr;
}


// -----------------------------------------------------------------------------
/** IDXGISwapChain **/

STDMETHODIMP HackerSwapChain::Present(THIS_
            /* [in] */ UINT SyncInterval,
            /* [in] */ UINT Flags)
{
	LogDebug("HackerSwapChain::Present(%s@%p) called with\n", type_name(this), this);
	LogDebug("  SyncInterval = %d\n", SyncInterval);
	LogDebug("  Flags = %d\n", Flags);

	if (!(Flags & DXGI_PRESENT_TEST)) {
		// Every presented frame, we want to take some CPU time to run our actions,
		// which enables hunting, and snapshots, and aiming overrides and other inputs
		RunFrameActions();
	}

	HRESULT hr = mOrigSwapChain1->Present(SyncInterval, Flags);

	if (!(Flags & DXGI_PRESENT_TEST)) {
		// Update the stereo params texture just after the present so that 
		// shaders get the new values for the current frame:
		UpdateStereoParams();

		// Run the post present command list now, which can be used to restore
		// state changed in the pre-present command list, or to perform some
		// action at the start of a frame:
		RunCommandList(mHackerDevice, mHackerContext, &G->post_present_command_list, NULL, true);
	}

	LogDebug("  returns %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerSwapChain::GetBuffer(THIS_
            /* [in] */ UINT Buffer,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][out][in] */ 
            _Out_  void **ppSurface)
{
	LogDebug("HackerSwapChain::GetBuffer(%s@%p) called with IID: %s\n", type_name(this), this, NameFromIID(riid).c_str());

	HRESULT hr = mOrigSwapChain1->GetBuffer(Buffer, riid, ppSurface);
	LogDebug("  returns %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerSwapChain::SetFullscreenState(THIS_
            /* [in] */ BOOL Fullscreen,
            /* [annotation][in] */ 
            _In_opt_  IDXGIOutput *pTarget)
{
	LogInfo("HackerSwapChain::SetFullscreenState(%s@%p) called with\n", type_name(this), this);
	LogInfo("  Fullscreen = %d\n", Fullscreen);
	LogInfo("  Target = %p\n", pTarget);

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
		LogInfo("->Fullscreen forced = %d\n", Fullscreen);
	}

	//if (pTarget)	
	//	hr = mOrigSwapChain1->SetFullscreenState(Fullscreen, pTarget->m_pOutput);
	//else
	//	hr = mOrigSwapChain1->SetFullscreenState(Fullscreen, 0);

	HRESULT hr = mOrigSwapChain1->SetFullscreenState(Fullscreen, pTarget);
	LogInfo("  returns %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerSwapChain::GetFullscreenState(THIS_
            /* [annotation][out] */ 
            _Out_opt_  BOOL *pFullscreen,
            /* [annotation][out] */ 
            _Out_opt_  IDXGIOutput **ppTarget)
{
	LogDebug("HackerSwapChain::GetFullscreenState(%s@%p) called\n", type_name(this), this);
	
	//IDXGIOutput *origOutput;
	//HRESULT hr = mOrigSwapChain1->GetFullscreenState(pFullscreen, &origOutput);
	//if (hr == S_OK)
	//{
	//	*ppTarget = IDXGIOutput::GetDirectOutput(origOutput);
	//	if (pFullscreen) LogInfo("  returns Fullscreen = %d\n", *pFullscreen);
	//	if (ppTarget) LogInfo("  returns target IDXGIOutput = %x, wrapper = %x\n", origOutput, *ppTarget);
	//}

	HRESULT hr = mOrigSwapChain1->GetFullscreenState(pFullscreen, ppTarget);
	LogDebug("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerSwapChain::GetDesc(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_SWAP_CHAIN_DESC *pDesc)
{
	LogDebug("HackerSwapChain::GetDesc(%s@%p) called\n", type_name(this), this);
	
	HRESULT hr = mOrigSwapChain1->GetDesc(pDesc);
	
	if (hr == S_OK)
	{		
		if (pDesc) LogDebug("  returns Windowed = %d\n", pDesc->Windowed);
		if (pDesc) LogDebug("  returns Width = %d\n", pDesc->BufferDesc.Width);
		if (pDesc) LogDebug("  returns Height = %d\n", pDesc->BufferDesc.Height);
		if (pDesc) LogDebug("  returns Refresh rate = %f\n", 
			(float) pDesc->BufferDesc.RefreshRate.Numerator / (float) pDesc->BufferDesc.RefreshRate.Denominator);
	}

	LogDebug("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerSwapChain::ResizeBuffers(THIS_
            /* [in] */ UINT BufferCount,
            /* [in] */ UINT Width,
            /* [in] */ UINT Height,
            /* [in] */ DXGI_FORMAT NewFormat,
            /* [in] */ UINT SwapChainFlags)
{
	LogInfo("HackerSwapChain::ResizeBuffers(%s@%p) called\n", type_name(this), this);

	if (G->mResolutionInfo.from == GetResolutionFrom::SWAP_CHAIN)
	{
		G->mResolutionInfo.width = Width;
		G->mResolutionInfo.height = Height;
		LogInfo("  Got resolution from swap chain: %ix%i\n",
			G->mResolutionInfo.width, G->mResolutionInfo.height);
	}

	// In Direct Mode, we need to ensure that we are keeping our 2x width backbuffer.
	// We are specifically modifying the value passed to the call, but saving the desired
	// resolution before this.
	if (G->gForceStereo == 2)
	{
		Width *= 2;
		LogInfo("-> forced 2x width for Direct Mode: %d\n", Width);
	}

	HRESULT hr = mOrigSwapChain1->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);

	if (SUCCEEDED(hr)) 
	{
		mOverlay->Resize(Width, Height);
	}

	LogInfo("  returns result = %x\n", hr); 
	return hr;
}
        
STDMETHODIMP HackerSwapChain::ResizeTarget(THIS_
            /* [annotation][in] */ 
            _In_  const DXGI_MODE_DESC *pNewTargetParameters)
{
	LogInfo("HackerSwapChain::ResizeTarget(%s@%p) called\n", type_name(this), this);
	LogInfo("  Width: %d, Height: %d\n", pNewTargetParameters->Width, pNewTargetParameters->Height);

	// In Direct Mode, we need to ensure that we are keeping our 2x width target.
	if ((G->gForceStereo == 2) && (pNewTargetParameters->Width == G->mResolutionInfo.width))
	{
		const_cast<DXGI_MODE_DESC*>(pNewTargetParameters)->Width *= 2;
		LogInfo("-> forced 2x width for Direct Mode: %d\n", pNewTargetParameters->Width);
	}
	
	HRESULT hr = mOrigSwapChain1->ResizeTarget(pNewTargetParameters);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerSwapChain::GetContainingOutput(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppOutput)
{
	LogInfo("HackerSwapChain::GetContainingOutput(%s@%p) called\n", type_name(this), this);
	HRESULT hr = mOrigSwapChain1->GetContainingOutput(ppOutput);
	LogInfo("  returns result = %#x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerSwapChain::GetFrameStatistics(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_FRAME_STATISTICS *pStats)
{
	LogInfo("HackerSwapChain::GetFrameStatistics(%s@%p) called\n", type_name(this), this);
	HRESULT hr = mOrigSwapChain1->GetFrameStatistics(pStats);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerSwapChain::GetLastPresentCount(THIS_
            /* [annotation][out] */ 
            _Out_  UINT *pLastPresentCount)
{
	LogInfo("HackerSwapChain::GetLastPresentCount(%s@%p) called\n", type_name(this), this);
	HRESULT hr = mOrigSwapChain1->GetLastPresentCount(pLastPresentCount);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}


// -----------------------------------------------------------------------------
/** IDXGISwapChain1 **/

// IDXGISwapChain1 requires platform update
// IDXGISwapChain2 requires Win8.1
// IDXGISwapChain3 requires Win10

STDMETHODIMP HackerSwapChain::GetDesc1(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_SWAP_CHAIN_DESC1 *pDesc)
{
	LogInfo("HackerSwapChain::GetDesc1(%s@%p) called\n", type_name(this), this);
	
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
        
STDMETHODIMP HackerSwapChain::GetFullscreenDesc(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc)
{
	LogInfo("HackerSwapChain::GetFullscreenDesc(%s@%p) called\n", type_name(this), this);
	
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
        
STDMETHODIMP HackerSwapChain::GetHwnd(THIS_
            /* [annotation][out] */ 
            _Out_  HWND *pHwnd)
{
	LogInfo("HackerSwapChain::GetHwnd(%s@%p) called\n", type_name(this), this);
	HRESULT hr = mOrigSwapChain1->GetHwnd(pHwnd);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerSwapChain::GetCoreWindow(THIS_
            /* [annotation][in] */ 
            _In_  REFIID refiid,
            /* [annotation][out] */ 
            _Out_  void **ppUnk)
{
	LogInfo("HackerSwapChain::GetCoreWindow(%s@%p) called with IID: %s\n", type_name(this), this, NameFromIID(refiid).c_str());

	HRESULT hr = mOrigSwapChain1->GetCoreWindow(refiid, ppUnk);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        

// IDXGISwapChain1 requires the platform update, but will be the default
// swap chain we build whenever possible.
//
// ToDo: never seen this in action.  Setting to always log.  Once we see
// it in action and works OK, remove the gLogDebug sets, because debug log
// is too chatty for Present calls.

STDMETHODIMP HackerSwapChain::Present1(THIS_
            /* [in] */ UINT SyncInterval,
            /* [in] */ UINT PresentFlags,
            /* [annotation][in] */ 
            _In_  const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
	gLogDebug = true;

	LogDebug("HackerSwapChain::Present1(%s@%p) called\n", type_name(this), this);
	LogDebug("  SyncInterval = %d\n", SyncInterval);
	LogDebug("  Flags = %d\n", PresentFlags);

	if (!(PresentFlags & DXGI_PRESENT_TEST)) {
		// Every presented frame, we want to take some CPU time to run our actions,
		// which enables hunting, and snapshots, and aiming overrides and other inputs
		RunFrameActions();
	}

	HRESULT hr = mOrigSwapChain1->Present1(SyncInterval, PresentFlags, pPresentParameters);

	if (!(PresentFlags & DXGI_PRESENT_TEST)) {
		// Update the stereo params texture just after the present so that we
		// get the new values for the current frame:
		UpdateStereoParams();

		// Run the post present command list now, which can be used to restore
		// state changed in the pre-present command list, or to perform some
		// action at the start of a frame:
		RunCommandList(mHackerDevice, mHackerContext, &G->post_present_command_list, NULL, true);
	}

	LogDebug("  returns %x\n", hr);

	gLogDebug = false;
	return hr;
}
        
STDMETHODIMP_(BOOL) HackerSwapChain::IsTemporaryMonoSupported(THIS)
{
	LogInfo("HackerSwapChain::IsTemporaryMonoSupported(%s@%p) called\n", type_name(this), this);
	BOOL ret = mOrigSwapChain1->IsTemporaryMonoSupported();
	LogInfo("  returns %d\n", ret);
	return ret;
}
        
STDMETHODIMP HackerSwapChain::GetRestrictToOutput(THIS_
            /* [annotation][out] */ 
            _Out_  IDXGIOutput **ppRestrictToOutput)
{
	LogInfo("HackerSwapChain::GetRestrictToOutput(%s@%p) called\n", type_name(this), this);
	HRESULT hr = mOrigSwapChain1->GetRestrictToOutput(ppRestrictToOutput);
	LogInfo("  returns result = %x, handle = %p\n", hr, *ppRestrictToOutput);
	return hr;
}
        
STDMETHODIMP HackerSwapChain::SetBackgroundColor(THIS_
            /* [annotation][in] */ 
            _In_  const DXGI_RGBA *pColor)
{
	LogInfo("HackerSwapChain::SetBackgroundColor(%s@%p) called\n", type_name(this), this);
	HRESULT hr = mOrigSwapChain1->SetBackgroundColor(pColor);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerSwapChain::GetBackgroundColor(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_RGBA *pColor)
{
	LogInfo("HackerSwapChain::GetBackgroundColor(%s@%p) called\n", type_name(this), this);
	HRESULT hr = mOrigSwapChain1->GetBackgroundColor(pColor);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerSwapChain::SetRotation(THIS_
            /* [annotation][in] */ 
            _In_  DXGI_MODE_ROTATION Rotation)
{
	LogInfo("HackerSwapChain::SetRotation(%s@%p) called\n", type_name(this), this);
	HRESULT hr = mOrigSwapChain1->SetRotation(Rotation);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}
        
STDMETHODIMP HackerSwapChain::GetRotation(THIS_
            /* [annotation][out] */ 
            _Out_  DXGI_MODE_ROTATION *pRotation)
{
	LogInfo("HackerSwapChain::GetRotation(%s@%p) called\n", type_name(this), this);
	HRESULT hr = mOrigSwapChain1->GetRotation(pRotation);
	LogInfo("  returns result = %x\n", hr);
	return hr;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// HackerUpscalingSwapChain, to provide post-process upscaling to arbitrary
// resolutions.  Particularly good for 4K passive 3D.

HackerUpscalingSwapChain::HackerUpscalingSwapChain(IDXGISwapChain1 *pSwapChain, HackerDevice *pDevice, HackerContext *pContext, 
	const DXGI_SWAP_CHAIN_DESC* FakeSwapChainDesc, UINT NewWidth, UINT NewHeight, IDXGIFactory* Factory)
	: HackerSwapChain(pSwapChain, pDevice, pContext), mFakeBackBuffer(nullptr), mFakeSwapChain1(nullptr), mWidth(0), mHeight(0)
{

	CreateRenderTarget(FakeSwapChainDesc, Factory);

	mWidth = NewWidth;
	mHeight = NewHeight;

	if (mOverlay)
		mOverlay->Resize(NewWidth, NewWidth);
}


HackerUpscalingSwapChain::~HackerUpscalingSwapChain()
{
	if (mFakeSwapChain1)
		mFakeSwapChain1->Release();
	if (mFakeBackBuffer)
		mFakeBackBuffer->Release();
}


// CreateRenderTarget 

void HackerUpscalingSwapChain::CreateRenderTarget(const DXGI_SWAP_CHAIN_DESC* FakeSwapChainDesc, IDXGIFactory* Factory)
{
	HRESULT hr;

	switch (G->UPSCALE_MODE)
	{
	case 0:
	{
		// TODO: multisampled swap chain
		// TODO: multiple buffers within one spaw chain
		// ==> in this case upscale_mode = 1 should be used at the moment
		D3D11_TEXTURE2D_DESC fake_buffer_desc;
		std::memset(&fake_buffer_desc, 0, sizeof(D3D11_TEXTURE2D_DESC));
		fake_buffer_desc.ArraySize = 1;
		fake_buffer_desc.MipLevels = 1;
		fake_buffer_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		fake_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		fake_buffer_desc.SampleDesc.Count = 1;
		fake_buffer_desc.Format = FakeSwapChainDesc->BufferDesc.Format;
		fake_buffer_desc.MiscFlags = 0;
		fake_buffer_desc.Width = FakeSwapChainDesc->BufferDesc.Width;
		fake_buffer_desc.Height = FakeSwapChainDesc->BufferDesc.Height;
		fake_buffer_desc.CPUAccessFlags = 0;

		hr = mHackerDevice->CreateTexture2D(&fake_buffer_desc, nullptr, &mFakeBackBuffer);
	}
	break;
	case 1:
	{
		if (Factory == nullptr)
		{
			LogInfo("HackerUpscalingSwapChain::createRenderTarget failed provided factory pointer is invalid\n!");
			BeepFailure2();
		}
		const UINT flagBackup = FakeSwapChainDesc->Flags;
		const_cast<DXGI_SWAP_CHAIN_DESC*>(FakeSwapChainDesc)->Flags &= ~DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // fake swap chain should have no influence on window
		IDXGISwapChain* swapChain;
		hr = Factory->CreateSwapChain(mHackerDevice->GetOrigDevice1(), const_cast<DXGI_SWAP_CHAIN_DESC*>(FakeSwapChainDesc), &swapChain);

		HRESULT res = swapChain->QueryInterface(IID_PPV_ARGS(&mFakeSwapChain1));
		if (FAILED(res))
			mFakeSwapChain1 = static_cast<IDXGISwapChain1*>(swapChain);
		const_cast<DXGI_SWAP_CHAIN_DESC*>(FakeSwapChainDesc)->Flags = flagBackup; // restore old state in case fall back is required
	}
	break;
	default:
		LogInfo("HackerUpscalingSwapChain::HackerUpscalingSwapChain() failed ==> provided upscaling mode is not valid!\n");
		BeepFailure2();
	}

	LogInfo("HackerUpscalingSwapChain::HackerUpscalingSwapChain(): result %d\n", hr);

	if (FAILED(hr))
	{
		LogInfo("HackerUpscalingSwapChain::HackerUpscalingSwapChain() failed!\n");
		BeepFailure2();
	}
}

STDMETHODIMP HackerUpscalingSwapChain::GetBuffer(THIS_
	/* [in] */ UINT Buffer,
	/* [annotation][in] */
	_In_  REFIID riid,
	/* [annotation][out][in] */
	_Out_  void **ppSurface)
{
	LogDebug("HackerUpscalingSwapChain::GetBuffer(%s@%p) called with IID: %s\n", type_name(this), this, NameFromIID(riid).c_str());

	HRESULT hr = S_OK;

	// if upscaling is on give the game fake back buffer
	if (mFakeBackBuffer)
		*ppSurface = mFakeBackBuffer;
	else if (mFakeSwapChain1)
		hr = mFakeSwapChain1->GetBuffer(Buffer, riid, ppSurface);
	else
		assert(hr); // should never be triggered (class hierarchy)

	LogDebug("  returns %x\n", hr);
	return hr;
}

STDMETHODIMP HackerUpscalingSwapChain::SetFullscreenState(THIS_
	/* [in] */ BOOL Fullscreen,
	/* [annotation][in] */
	_In_opt_  IDXGIOutput *pTarget)
{
	LogInfo("HackerUpscalingSwapChain::SetFullscreenState(%s@%p) called with\n", type_name(this), this);
	LogInfo("  Fullscreen = %d\n", Fullscreen);
	LogInfo("  Target = %p\n", pTarget);

	HRESULT hr;

	BOOL fullscreen_state = FALSE;
	IDXGIOutput *target = nullptr;
	mOrigSwapChain1->GetFullscreenState(&fullscreen_state, &target);

	if (target)
		target->Release();

	// dont call setfullscreenstate again to avoid starting mode switching and flooding winproc with unnecessary messages
	// can disable fullscreen mode somehow
	if (fullscreen_state && Fullscreen)
	{
		hr = S_OK;
	}
	else
	{
		if (G->SCREEN_UPSCALING == 2)
		{
			hr = mOrigSwapChain1->SetFullscreenState(TRUE, pTarget); // Witcher seems to require forcing the fullscreen
		}
		else
		{
			hr = mOrigSwapChain1->SetFullscreenState(Fullscreen, pTarget);
		}
	}

	LogInfo("  returns %x\n", hr);
	return hr;
}

STDMETHODIMP HackerUpscalingSwapChain::GetDesc(THIS_
	/* [annotation][out] */
	_Out_  DXGI_SWAP_CHAIN_DESC *pDesc)
{
	LogDebug("HackerUpscalingSwapChain::GetDesc(%s@%p) called\n", type_name(this), this);

	HRESULT hr = mOrigSwapChain1->GetDesc(pDesc);

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
				LogDebug("->Using fake SwapChain Sizes.\n");
			}

			if (mFakeSwapChain1)
			{
				hr = mFakeSwapChain1->GetDesc(pDesc);
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

STDMETHODIMP HackerUpscalingSwapChain::ResizeBuffers(THIS_
	/* [in] */ UINT BufferCount,
	/* [in] */ UINT Width,
	/* [in] */ UINT Height,
	/* [in] */ DXGI_FORMAT NewFormat,
	/* [in] */ UINT SwapChainFlags)
{
	LogInfo("HackerSwapChain::ResizeBuffers(%s@%p) called\n", type_name(this), this);

	// TODO: not sure if it belongs here, in the resize target function or in both
	// or maybe it is better to put it in the getviewport function?
	// Require in case the software mouse and upscaling are on at the same time
	G->GAME_INTERNAL_WIDTH = Width;
	G->GAME_INTERNAL_HEIGHT = Height;

	if (G->mResolutionInfo.from == GetResolutionFrom::SWAP_CHAIN)
	{
		G->mResolutionInfo.width = Width;
		G->mResolutionInfo.height = Height;
		LogInfo("Got resolution from swap chain: %ix%i\n",
			G->mResolutionInfo.width, G->mResolutionInfo.height);
	}

	// In Direct Mode, we need to ensure that we are keeping our 2x width backbuffer.
	// We are specifically modifying the value passed to the call, but saving the desired
	// resolution before this.
	if (G->gForceStereo == 2)
	{
		Width *= 2;
		LogInfo("-> forced 2x width for Direct Mode: %d\n", Width);
	}

	HRESULT hr;

	if (mFakeBackBuffer) // UPSCALE_MODE 0
	{
		// TODO: need to consider the new code (G->gForceStereo == 2)
		// would my stuff work this way? i guess yes. What is with the games that are not calling resize buffer
		// just try to recreate texture with new game resolution
		// should be possible without any issues (texture just like the swap chain should not be used at this time point)

		D3D11_TEXTURE2D_DESC fd;
		mFakeBackBuffer->GetDesc(&fd);

		if (!(fd.Width == Width && fd.Height == Height))
		{
			mFakeBackBuffer->Release();

			fd.Width = Width;
			fd.Height = Height;
			fd.Format = NewFormat;
			// just recreate texture with new width and height
			hr = mHackerDevice->CreateTexture2D(&fd, nullptr, &mFakeBackBuffer);
		}
		else  // nothing to resize
			hr = S_OK;
	}
	else if (mFakeSwapChain1) // UPSCALE_MODE 1
	{
		// the last parameter have to be zero to avoid the influence of the faked swap chain on the resize target function 
		hr = mFakeSwapChain1->ResizeBuffers(BufferCount, Width, Height, NewFormat, 0);
	}
	else
	{
		assert(false); // should never be triggered (class hierarchy)
	}

	if (SUCCEEDED(hr))
	{
		mOverlay->Resize(Width, Height);
	}

	LogInfo("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerUpscalingSwapChain::ResizeTarget(THIS_
	/* [annotation][in] */
	_In_  const DXGI_MODE_DESC *pNewTargetParameters)
{
	LogInfo("HackerUpscalingSwapChain::ResizeTarget(%s@%p) called\n", type_name(this), this);

	if (pNewTargetParameters != nullptr)
	{
		// TODO: not sure if it belongs here, in the resize buffers function or in both
		// or maybe it is better to put it in the getviewport function?
		// Require in case the software mouse and upscaling are on at the same time
		G->GAME_INTERNAL_WIDTH = pNewTargetParameters->Width;
		G->GAME_INTERNAL_HEIGHT = pNewTargetParameters->Height;
	}

	// In Direct Mode, we need to ensure that we are keeping our 2x width target.
	if ((G->gForceStereo == 2) && (pNewTargetParameters->Width == G->mResolutionInfo.width))
	{
		const_cast<DXGI_MODE_DESC*>(pNewTargetParameters)->Width *= 2;
		LogInfo("-> forced 2x width for Direct Mode: %d\n", pNewTargetParameters->Width);
	}

	/*
	TODO: what about 3dv direct mode? why the resize target need to use double width?
	this would resize target window and have some other drawbacks, but again
	im not very familiar with directx 11 maybe it will cause no problems:
	Anyway need to consider new code for the upscaling later
	*/

	// Some games like Witcher seems to drop fullscreen everytime the resizetarget is called (original one)
	// Some other games seems to require the function 
	// I did it the way the faked texture mode (upscale_mode == 1) dont call resize target
	// the other mode does

	HRESULT hr;

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
		LONG displ_chainge_res = ChangeDisplaySettingsEx(NULL, &dmScreenSettings, nullptr, CDS_FULLSCREEN, 0);
		hr = displ_chainge_res == 0 ? S_OK : DXGI_ERROR_INVALID_CALL;
	}
	else if (G->SCREEN_UPSCALING == 1)
	{
		DXGI_MODE_DESC md = *pNewTargetParameters;

		// force upscaled resolution
		md.Width = mWidth;
		md.Height = mHeight;

		// Temporarily disable the GetClientRect() hook since DirectX
		// itself will call that and we want it to get the real
		// resolution. Fixes upscaling in ARK: Survival Evolved
		G->upscaling_hooks_armed = false;
		hr = mOrigSwapChain1->ResizeTarget(&md);
		G->upscaling_hooks_armed = true;
	}

	LogInfo("  returns result = %x\n", hr);
	return hr;
}



// Leftover functionality. From before refactoring.


//
//
//

//
//static bool FilterRate(int rate)
//{
//	if (!G->FILTER_REFRESH[0]) return false;
//	int i = 0;
//	while (G->FILTER_REFRESH[i] && G->FILTER_REFRESH[i] != rate)
//		++i;
//	return G->FILTER_REFRESH[i] == 0;
//}
//
//STDMETHODIMP HackerDXGIOutput::GetDisplayModeList(THIS_
//	/* [in] */ DXGI_FORMAT EnumFormat,
//	/* [in] */ UINT Flags,
//	/* [annotation][out][in] */
//	__inout  UINT *pNumModes,
//	/* [annotation][out] */
//	__out_ecount_part_opt(*pNumModes, *pNumModes)  DXGI_MODE_DESC *pDesc)
//{
//	LogInfo("HackerDXGIOutput::GetDisplayModeList(%s@%p) called\n", type_name(this), this);
//
//	HRESULT ret = mOrigOutput->GetDisplayModeList(EnumFormat, Flags, pNumModes, pDesc);
//	if (ret == S_OK && pDesc)
//	{
//		for (UINT j = 0; j < *pNumModes; ++j)
//		{
//			int rate = pDesc[j].RefreshRate.Numerator / pDesc[j].RefreshRate.Denominator;
//			if (FilterRate(rate))
//			{
//				LogInfo("  Skipping mode: width=%d, height=%d, refresh rate=%f\n", pDesc[j].Width, pDesc[j].Height,
//					(float)pDesc[j].RefreshRate.Numerator / (float)pDesc[j].RefreshRate.Denominator);
//				// ToDo: Does this work?  I have no idea why setting width and height to 8 would matter.
//				pDesc[j].Width = 8; pDesc[j].Height = 8;
//			}
//			else
//			{
//				LogInfo("  Mode detected: width=%d, height=%d, refresh rate=%f\n", pDesc[j].Width, pDesc[j].Height,
//					(float)pDesc[j].RefreshRate.Numerator / (float)pDesc[j].RefreshRate.Denominator);
//			}
//		}
//	}
//
//	return ret;
//}
//
//STDMETHODIMP HackerDXGIOutput::FindClosestMatchingMode(THIS_
//	/* [annotation][in] */
//	__in  const DXGI_MODE_DESC *pModeToMatch,
//	/* [annotation][out] */
//	__out  DXGI_MODE_DESC *pClosestMatch,
//	/* [annotation][in] */
//	__in_opt  IUnknown *pConcernedDevice)
//{
//	if (pModeToMatch) LogInfo("HackerDXGIOutput::FindClosestMatchingMode(%s@%p) called: width=%d, height=%d, refresh rate=%f\n", type_name(this), this,
//		pModeToMatch->Width, pModeToMatch->Height, (float)pModeToMatch->RefreshRate.Numerator / (float)pModeToMatch->RefreshRate.Denominator);
//
//	HRESULT hr = mOrigOutput->FindClosestMatchingMode(pModeToMatch, pClosestMatch, pConcernedDevice);
//
//	if (pClosestMatch && G->SCREEN_REFRESH >= 0)
//	{
//		pClosestMatch->RefreshRate.Numerator = G->SCREEN_REFRESH;
//		pClosestMatch->RefreshRate.Denominator = 1;
//	}
//	if (pClosestMatch && G->SCREEN_WIDTH >= 0) pClosestMatch->Width = G->SCREEN_WIDTH;
//	if (pClosestMatch && G->SCREEN_HEIGHT >= 0) pClosestMatch->Height = G->SCREEN_HEIGHT;
//	if (pClosestMatch) LogInfo("  returning width=%d, height=%d, refresh rate=%f\n",
//		pClosestMatch->Width, pClosestMatch->Height, (float)pClosestMatch->RefreshRate.Numerator / (float)pClosestMatch->RefreshRate.Denominator);
//
//	LogInfo("  returns hr=%x\n", hr);
//	return hr;
//}
//
//

/*
ToDo: 
use reintepret_cast  instead of static_cast?
revive the missing skip_dxgi to avoid beeps at launch
filter junk at bottom here to avoid lost functionality
Factory2 hook out of dxgi.dll at DLLMainHook? Only needed in Win10 case.
Restore hooking of device/context.
Remove QueryInterface hook in HookedDXGI.
*/