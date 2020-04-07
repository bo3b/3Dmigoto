#include "HookedSwapChain.h"
#include "d3d9Wrapper.h"

inline void D3D9Wrapper::IDirect3DSwapChain9::Delete()
{
	LogInfo("IDirect3DSwapChain9::Delete\n");
	LogInfo("  deleting self\n");
	for (auto it = m_backBuffers.begin(); it != m_backBuffers.end();)
	{
		(*it)->Delete();
		++it;
	}
	if (m_pRealUnk) m_List.DeleteMember(m_pRealUnk);
	m_pUnk = 0;
	m_pRealUnk = 0;
	delete this;
}

inline void D3D9Wrapper::IDirect3DSwapChain9::HookSwapChain()
{
	if (hackerDevice->_ex)
		m_pUnk = hook_swapchain(GetSwapChain9Ex(), reinterpret_cast<::IDirect3DSwapChain9Ex*>(this));
	else
		m_pUnk = hook_swapchain(GetSwapChain9(), reinterpret_cast<::IDirect3DSwapChain9*>(this));
}
D3D9Wrapper::IDirect3DSwapChain9::IDirect3DSwapChain9( ::LPDIRECT3DSWAPCHAIN9 pSwapChain, D3D9Wrapper::IDirect3DDevice9* pDevice)
    : IDirect3DUnknown((IUnknown*) pSwapChain),
	pendingGetSwapChain(false),
	hackerDevice(pDevice),
	mFakeSwapChain(NULL),
	_SwapChain(0),
	shared_ref_count(1),
	bound(false),
	zero_d3d_ref_count(false)
{

	if ((G->enable_hooks >= EnableHooksDX9::ALL) && pSwapChain) {
		this->HookSwapChain();
	}
}

D3D9Wrapper::IDirect3DSwapChain9* D3D9Wrapper::IDirect3DSwapChain9::GetSwapChain(::LPDIRECT3DSWAPCHAIN9 pSwapChain, D3D9Wrapper::IDirect3DDevice9* pDevice)
{
	D3D9Wrapper::IDirect3DSwapChain9* p = new D3D9Wrapper::IDirect3DSwapChain9(pSwapChain, pDevice);
    if (pSwapChain) m_List.AddMember(pSwapChain, p);
    return p;
}
STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::QueryInterface(THIS_ REFIID riid, void ** ppvObj)
{
	LogDebug("D3D9Wrapper::IDirect3DSwapChain9::QueryInterface called\n");// at 'this': %s\n", type_name_dx9((IUnknown*)this));
	HRESULT hr = NULL;
	if (QueryInterface_DXGI_Callback(riid, ppvObj, &hr))
		return hr;
	LogInfo("QueryInterface request for %s on %p\n", NameFromIID(riid), this);
	hr = m_pUnk->QueryInterface(riid, ppvObj);
	if (hr == S_OK) {
		if ((*ppvObj) == GetRealOrig()) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppvObj = this;
				++m_ulRef;
				++shared_ref_count;
				zero_d3d_ref_count = false;
				LogInfo("  interface replaced with IDirect3DSwapChain9 wrapper.\n");
				LogInfo("  result = %x, handle = %p\n", hr, *ppvObj);
				return hr;
			}
		}
		D3D9Wrapper::IDirect3DUnknown *unk = QueryInterface_Find_Wrapper(*ppvObj);
		if (unk)
			*ppvObj = unk;
	}
	LogInfo("  result = %x, handle = %p\n", hr, *ppvObj);
	return hr;
}
STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DSwapChain9::AddRef(THIS)
{
	++m_ulRef;
	++shared_ref_count;
	zero_d3d_ref_count = false;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DSwapChain9::Release(THIS)
{
	LogDebug("IDirect3DSwapChain9::Release handle=%p, counter=%d, this=%p\n", m_pUnk, m_ulRef, this);

    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);

	--m_ulRef;
	bool prev_non_zero = !zero_d3d_ref_count;
    if (ulRef == 0)
    {
		if (!gLogDebug) LogInfo("IDirect3DSwapChain9::Release handle=%p, counter=%d, internal counter = %d\n", m_pUnk, m_ulRef, ulRef);
		zero_d3d_ref_count = true;
    }
	if (prev_non_zero) {
		--shared_ref_count;
		if (shared_ref_count == 0) {
			if (!bound)
				Delete();
		}
	}
    return ulRef;
}

static void CheckSwapChain(D3D9Wrapper::IDirect3DSwapChain9 *me)
{
	if (!me->pendingGetSwapChain)
		return;
	me->pendingGetSwapChain = false;
	CheckDevice(me->pendingDevice);

	LogInfo("  calling postponed GetSwapChain.\n");
	::LPDIRECT3DSWAPCHAIN9 baseSwapChain = NULL;
	HRESULT hr = me->pendingDevice->GetD3D9Device()->GetSwapChain(me->_SwapChain, &baseSwapChain);
	if (FAILED(hr))
	{
		LogInfo("    failed getting swap chain with result = %x\n", hr);

		return;
	}
	const IID IID_IDirect3DSwapChain9Ex = { 0x91886caf, 0x1c3d, 0x4d2e, { 0xa0, 0xab, 0x3e, 0x4c, 0x7d, 0x8d, 0x33, 0x3 }};
	hr = baseSwapChain->QueryInterface(IID_IDirect3DSwapChain9Ex, (void **)&me->m_pUnk);
	baseSwapChain->Release();
	if (FAILED(hr))
	{
		LogInfo("    failed casting swap chain to IDirect3DSwapChain9 with result = %x\n", hr);

		return;
	}
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::Present(THIS_ CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion,DWORD dwFlags)
{
	LogDebug("IDirect3DSwapChain9::Present called\n");

	CheckSwapChain(this);
	Profiling::State profiling_state = { 0 };
	bool profiling = false;
	// Profiling::mode may change below, so make a copy
	profiling = Profiling::mode == Profiling::Mode::SUMMARY;
	if (profiling)
		Profiling::start(&profiling_state);

	// Every presented frame, we want to take some CPU time to run our actions,
	// which enables hunting, and snapshots, and aiming overrides and other inputs
	CachedStereoValues prePresentCachedStereoValues;
	RunFrameActions(hackerDevice, &prePresentCachedStereoValues);

	if (profiling)
		Profiling::end(&profiling_state, &Profiling::present_overhead);

	HRESULT hr = GetSwapChain9()->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);

	if (profiling)
		Profiling::start(&profiling_state);

	// Update the stereo params texture just after the present so that
	// shaders get the new values for the current frame:
	if (G->gTrackNvAPIStereoActive && !G->gTrackNvAPIStereoActiveDisableReset)
		NvAPIResetStereoActiveTracking();
	if (G->gTrackNvAPIConvergence && !G->gTrackNvAPIConvergenceDisableReset)
		NvAPIResetConvergenceTracking();
	if (G->gTrackNvAPISeparation && !G->gTrackNvAPISeparationDisableReset)
		NvAPIResetSeparationTracking();
	if (G->gTrackNvAPIEyeSeparation && !G->gTrackNvAPIEyeSeparationDisableReset)
		NvAPIResetEyeSeparationTracking();
	hackerDevice->stereo_params_updated_this_frame = false;
	CachedStereoValues postPresentCachedStereoValues;
	hackerDevice->UpdateStereoParams(false, &postPresentCachedStereoValues);
	G->bb_is_upscaling_bb = !!G->SCREEN_UPSCALING && G->upscaling_command_list_using_explicit_bb_flip;

	// Run the post present command list now, which can be used to restore
	// state changed in the pre-present command list, or to perform some
	// action at the start of a frame:
	hackerDevice->GetD3D9Device()->BeginScene();
	RunCommandList(hackerDevice, &G->post_present_command_list, NULL, true, &postPresentCachedStereoValues);
	hackerDevice->GetD3D9Device()->EndScene();

	if (G->gAutoDetectDepthBuffer) {
		hackerDevice->DetectDepthSource();
		hackerDevice->drawCalls = hackerDevice->vertices = 0;
	}
	if (profiling)
		Profiling::end(&profiling_state, &Profiling::present_overhead);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetFrontBufferData(THIS_ IDirect3DSurface9 *pDestSurface)
{
	LogDebug("IDirect3DSwapChain9::GetFrontBufferData called\n");

	CheckSwapChain(this);
	HRESULT hr = GetSwapChain9()->GetFrontBufferData(baseSurface9(pDestSurface));
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetBackBuffer(THIS_ UINT iBackBuffer,::D3DBACKBUFFER_TYPE Type, D3D9Wrapper::IDirect3DSurface9 **ppBackBuffer)
{
	LogDebug("IDirect3DSwapChain9::GetBackBuffer called\n");

	CheckSwapChain(this);
	::LPDIRECT3DSURFACE9 baseSurface = 0;
	HRESULT hr;
	D3D9Wrapper::IDirect3DSurface9 * wrappedSurface = NULL;
	if (G->SCREEN_UPSCALING > 0 || G->gForceStereo == 2) {
		LogDebug("HackerFakeDeviceSwapChain::GetBuffer(%s@%p)\n", type_name_dx9((IUnknown*)this), this);
		if (mFakeSwapChain && mFakeSwapChain->mFakeBackBuffers.size() > iBackBuffer) {
			wrappedSurface = mFakeSwapChain->mFakeBackBuffers.at(iBackBuffer);
			wrappedSurface->AddRef();
			hr = S_OK;
		}
		else {
			hr = D3DERR_INVALIDCALL;
			LogInfo("BUG: HackerFakeSwapChain::GetBuffer(): Missing fake back buffer\n");
			if (ppBackBuffer) *ppBackBuffer = 0;
			return hr;
		}
		if (ppBackBuffer) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrappedSurface) {
				*ppBackBuffer = wrappedSurface;
			}
			else {
				*ppBackBuffer = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(wrappedSurface->GetRealOrig());
			}
		}
	}
	else {

		wrappedSurface = m_backBuffers[iBackBuffer];
		if (wrappedSurface) {
			wrappedSurface->AddRef();
			hr = S_OK;
		}
		else {
			hr = D3DERR_INVALIDCALL;
			LogInfo("  failed with hr=%x\n", hr);
			if (ppBackBuffer) *ppBackBuffer = 0;
			return hr;
		}

		if (ppBackBuffer) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppBackBuffer = wrappedSurface;
			}
			else {
				*ppBackBuffer = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(wrappedSurface->GetRealOrig());
			}
		}
	}
	if (ppBackBuffer) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseSurface, *ppBackBuffer);

    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetRasterStatus(THIS_ ::D3DRASTER_STATUS* pRasterStatus)
{
	LogDebug("IDirect3DSwapChain9::GetRasterStatus called\n");

	CheckSwapChain(this);
	return GetSwapChain9()->GetRasterStatus(pRasterStatus);
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetDisplayMode(THIS_ ::D3DDISPLAYMODE* pMode)
{
	LogDebug("IDirect3DSwapChain9::GetDisplayMode called\n");

	CheckSwapChain(this);
	HRESULT hr = GetSwapChain9()->GetDisplayMode(pMode);
	if (hr == S_OK && pMode)
	{
		//if (G->UPSCALE_MODE > 0) {
		//	if (G->GAME_INTERNAL_WIDTH() > 1)
		//		pMode->Width = G->GAME_INTERNAL_WIDTH();
		//	if (G->GAME_INTERNAL_HEIGHT() > 1)
		//		pMode->Height = G->GAME_INTERNAL_HEIGHT();
		//}
		//if (G->SCREEN_REFRESH != 1 && pMode->RefreshRate != origPresentationParameters.FullScreen_RefreshRateInHz)
		//	pMode->RefreshRate = origPresentationParameters.FullScreen_RefreshRateInHz;
		//if (G->SCREEN_REFRESH != -1 && pMode->RefreshRate != G->SCREEN_REFRESH)
		//{
		//	LogInfo("  overriding refresh rate %d with %d\n", pMode->RefreshRate, G->SCREEN_REFRESH);

		//	pMode->RefreshRate = G->SCREEN_REFRESH;
		//}
		//if (G->GAME_INTERNAL_WIDTH() > 1 && G->FORCE_REPORT_GAME_RES) {
		//	pMode->Width = G->GAME_INTERNAL_WIDTH();
		//}
		////else if (G->FORCE_REPORT_WIDTH > 0) {
		////	pMode->Width = G->FORCE_REPORT_WIDTH;
		////}
		//if (G->GAME_INTERNAL_HEIGHT() > 1 && G->FORCE_REPORT_GAME_RES) {
		//	pMode->Height = G->GAME_INTERNAL_HEIGHT();
		//}
		////else if (G->FORCE_REPORT_HEIGHT > 0) {
		////	pMode->Height = G->FORCE_REPORT_HEIGHT;
		////}
	}
	if (!pMode) LogInfo("  returns result=%x\n", hr);
	if (pMode) LogInfo("  returns result=%x, Width=%d, Height=%d, RefreshRate=%d, Format=%d\n", hr,
		pMode->Width, pMode->Height, pMode->RefreshRate, pMode->Format);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetDevice(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice)
{
	LogDebug("IDirect3DSwapChain9::GetDevice called\n");

	CheckSwapChain(this);
	HRESULT hr;
	if (hackerDevice) {
		hackerDevice->AddRef();
		hr = S_OK;
	}
	else {
		return D3DERR_NOTFOUND;
	}
	if (!(G->enable_hooks & EnableHooksDX9::DEVICE)) {
		*ppDevice = hackerDevice;
	}
	else {
		*ppDevice = reinterpret_cast<D3D9Wrapper::IDirect3DDevice9*>(hackerDevice->GetRealOrig());
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetPresentParameters(THIS_ ::D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	LogDebug("IDirect3DSwapChain9::GetPresentParameters called.\n");

	CheckSwapChain(this);
	HRESULT hr = GetSwapChain9()->GetPresentParameters(pPresentationParameters);

	if (hr == S_OK)
	{
		if (G->SCREEN_UPSCALING > 0 || G->gForceStereo == 2) {
			if (mFakeSwapChain != NULL && mFakeSwapChain->mFakeBackBuffers.size() > 0) {
				pPresentationParameters->BackBufferWidth = mFakeSwapChain->mOrignalWidth;
				pPresentationParameters->BackBufferHeight = mFakeSwapChain->mOrignalHeight;
				LogDebug("->Using fake SwapChain Back Buffer Sizes.\n");
			}
		}

		if (pPresentationParameters) LogDebug("  returns Windowed = %d\n", pPresentationParameters->Windowed);
		if (pPresentationParameters) LogDebug("  returns Width = %d\n", pPresentationParameters->BackBufferWidth);
		if (pPresentationParameters) LogDebug("  returns Height = %d\n", pPresentationParameters->BackBufferHeight);
		if (pPresentationParameters) LogDebug("  returns Refresh rate = %f\n",(float)pPresentationParameters->FullScreen_RefreshRateInHz);

	}

	LogDebug("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetDisplayModeEx(THIS_ ::D3DDISPLAYMODEEX* pMode,::D3DDISPLAYROTATION* pRotation)
{
	LogDebug("IDirect3DSwapChain9::GetDisplayModeEx called.\n");

	CheckSwapChain(this);
	HRESULT hr = GetSwapChain9Ex()->GetDisplayModeEx(pMode, pRotation);
	if (hr == S_OK && pMode)
	{
		//if (G->UPSCALE_MODE > 0) {
		//	if (G->GAME_INTERNAL_WIDTH() > 1)
		//		pMode->Width = G->GAME_INTERNAL_WIDTH();
		//	if (G->GAME_INTERNAL_HEIGHT() > 1)
		//		pMode->Height = G->GAME_INTERNAL_HEIGHT();
		//}
		//if (G->SCREEN_REFRESH != 1 && pMode->RefreshRate != origPresentationParameters.FullScreen_RefreshRateInHz)
		//	pMode->RefreshRate = origPresentationParameters.FullScreen_RefreshRateInHz;
		//if (G->SCREEN_REFRESH != -1 && pMode->RefreshRate != G->SCREEN_REFRESH)
		//{
		//	LogInfo("  overriding refresh rate %d with %d\n", pMode->RefreshRate, G->SCREEN_REFRESH);

		//	pMode->RefreshRate = G->SCREEN_REFRESH;
		//}
		//if (G->GAME_INTERNAL_WIDTH() > 1 && G->FORCE_REPORT_GAME_RES) {
		//	pMode->Width = G->GAME_INTERNAL_WIDTH();
		//}
		////else if (G->FORCE_REPORT_WIDTH > 0) {
		////	pMode->Width = G->FORCE_REPORT_WIDTH;
		////}
		//if (G->GAME_INTERNAL_HEIGHT() > 1 && G->FORCE_REPORT_GAME_RES) {
		//	pMode->Height = G->GAME_INTERNAL_HEIGHT();
		//}
		////else if (G->FORCE_REPORT_HEIGHT > 0) {
		////	pMode->Height = G->FORCE_REPORT_HEIGHT;
		////}
	}
	if (!pMode) LogInfo("  returns result=%x\n", hr);
	if (pMode) LogInfo("  returns result=%x, Width=%d, Height=%d, RefreshRate=%d, Format=%d\n", hr,
		pMode->Width, pMode->Height, pMode->RefreshRate, pMode->Format);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetLastPresentCount(UINT * pLastPresentCount)
{
	LogDebug("IDirect3DSwapChain9::GetLastPresentCount called.\n");

	CheckSwapChain(this);
	return GetSwapChain9Ex()->GetLastPresentCount(pLastPresentCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DSwapChain9::GetPresentStats(::D3DPRESENTSTATS * pPresentationStatistics)
{
	LogDebug("IDirect3DSwapChain9::GetPresentStatistics called.\n");

	CheckSwapChain(this);
	return GetSwapChain9Ex()->GetPresentStats(pPresentationStatistics);
}
