
#include <unordered_map>

#include "HookedSwapChain.h"


#include "DLLMainHookDX9.h"


#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

typedef std::unordered_map<IDirect3DSwapChain9 *, IDirect3DSwapChain9 *> ChainMap;
static ChainMap chain_map;
static CRITICAL_SECTION chain_map_lock;

static struct IDirect3DSwapChain9ExVtbl orig_vtable_ex;
static struct IDirect3DSwapChain9Vtbl orig_vtable;
static bool hooks_installed = false;

IDirect3DSwapChain9Ex* lookup_hooked_swapchain(IDirect3DSwapChain9Ex *orig_swapchain)
{
	ChainMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&chain_map_lock);
	i = chain_map.find((IDirect3DSwapChain9*)orig_swapchain);
	if (i == chain_map.end()) {
		LeaveCriticalSection(&chain_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&chain_map_lock);

	return (IDirect3DSwapChain9Ex*)i->second;
}
IDirect3DSwapChain9* lookup_hooked_swapchain(IDirect3DSwapChain9 *orig_swapchain)
{
	ChainMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&chain_map_lock);
	i = chain_map.find(orig_swapchain);
	if (i == chain_map.end()) {
		LeaveCriticalSection(&chain_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&chain_map_lock);

	return (IDirect3DSwapChain9*)i->second;
}
// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3DSwapChain9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3DSwapChain9 *chain = lookup_hooked_swapchain(This);

	HookDebug("HookedSwapChain:: QueryInterface()\n");

	if (chain)
		return IDirect3DSwapChain9_QueryInterface(chain, riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3DSwapChain9 * This)
{
	IDirect3DSwapChain9 *chain = lookup_hooked_swapchain(This);

	HookDebug("HookedSwapChain:: AddRef()\n");

	if (chain)
		return IDirect3DSwapChain9_AddRef(chain);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3DSwapChain9 * This)
{
	ChainMap::iterator i;
	ULONG ref;

	HookDebug("HookedChain:: Release()\n");

	EnterCriticalSection(&chain_map_lock);
	i = chain_map.find(This);
	if (i != chain_map.end()) {
		ref = IDirect3DSwapChain9_Release(i->second);
		if (!ref)
			chain_map.erase(i);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&chain_map_lock);

	return ref;
}

//IDirect3DSwapChain9
static HRESULT STDMETHODCALLTYPE Present(IDirect3DSwapChain9 * This, RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags) {
	IDirect3DSwapChain9 *chain = lookup_hooked_swapchain(This);

	HookDebug("HookedChain::Present()\n");

	if (chain)
		return IDirect3DSwapChain9_Present(chain, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);

	return orig_vtable.Present(This, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);

}

static HRESULT STDMETHODCALLTYPE GetFrontBufferData(IDirect3DSwapChain9 * This, IDirect3DSurface9 *pDestSurface) {
	IDirect3DSwapChain9 *chain = lookup_hooked_swapchain(This);

	HookDebug("HookedChain::GetFrontBufferData()\n");

	if (chain)
		return IDirect3DSwapChain9_GetFrontBufferData(chain, pDestSurface);

	return orig_vtable.GetFrontBufferData(This, pDestSurface);

}

static HRESULT STDMETHODCALLTYPE GetBackBuffer(IDirect3DSwapChain9 * This, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer) {
	IDirect3DSwapChain9 *chain = lookup_hooked_swapchain(This);

	HookDebug("HookedChain::GetBackBuffer()\n");

	if (chain)
		return IDirect3DSwapChain9_GetBackBuffer(chain, iBackBuffer, Type, ppBackBuffer);

	return orig_vtable.GetBackBuffer(This, iBackBuffer, Type, ppBackBuffer);

}

static HRESULT STDMETHODCALLTYPE GetRasterStatus(IDirect3DSwapChain9 * This, D3DRASTER_STATUS* pRasterStatus){
	IDirect3DSwapChain9 *chain = lookup_hooked_swapchain(This);

	HookDebug("HookedChain::GetRasterStatus()\n");

	if (chain)
		return IDirect3DSwapChain9_GetRasterStatus(chain, pRasterStatus);

	return orig_vtable.GetRasterStatus(This, pRasterStatus);

}

static HRESULT STDMETHODCALLTYPE GetDisplayMode(IDirect3DSwapChain9 * This, D3DDISPLAYMODE* pMode) {
	IDirect3DSwapChain9 *chain = lookup_hooked_swapchain(This);

	HookDebug("HookedChain::GetDisplayMode()\n");

	if (chain)
		return IDirect3DSwapChain9_GetDisplayMode(chain, pMode);

	return orig_vtable.GetDisplayMode(This, pMode);

}
static HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DSwapChain9 * This, IDirect3DDevice9** ppDevice) {
	IDirect3DSwapChain9 *chain = lookup_hooked_swapchain(This);

	HookDebug("HookedChain::GetDevice()\n");

	if (chain)
		return IDirect3DSwapChain9_GetDevice(chain, ppDevice);

	return orig_vtable.GetDevice(This, ppDevice);

}

static HRESULT STDMETHODCALLTYPE GetPresentParameters(IDirect3DSwapChain9 * This, D3DPRESENT_PARAMETERS* pPresentationParameters) {
	IDirect3DSwapChain9 *chain = lookup_hooked_swapchain(This);

	HookDebug("HookedChain::GetPresentParameters()\n");

	if (chain)
		return IDirect3DSwapChain9_GetPresentParameters(chain, pPresentationParameters);

	return orig_vtable.GetPresentParameters(This, pPresentationParameters);

}

static HRESULT STDMETHODCALLTYPE GetDisplayModeEx(IDirect3DSwapChain9Ex * This, D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation) {
	IDirect3DSwapChain9Ex *chain = lookup_hooked_swapchain(This);

	HookDebug("HookedChain::GetDisplayModeEx()\n");

	if (chain)
		return IDirect3DSwapChain9Ex_GetDisplayModeEx(chain, pMode, pRotation);

	return orig_vtable_ex.GetDisplayModeEx(This, pMode, pRotation);

}

static HRESULT STDMETHODCALLTYPE GetLastPresentCount(IDirect3DSwapChain9Ex * This, UINT *pLastPresentCount) {
	IDirect3DSwapChain9Ex *chain = lookup_hooked_swapchain(This);

	HookDebug("HookedChain::GetLastPresentCount()\n");

	if (chain)
		return IDirect3DSwapChain9Ex_GetLastPresentCount(chain, pLastPresentCount);

	return orig_vtable_ex.GetLastPresentCount(This, pLastPresentCount);

}

static HRESULT STDMETHODCALLTYPE GetPresentStats(IDirect3DSwapChain9Ex * This, D3DPRESENTSTATS *pPresentationStatistics) {
	IDirect3DSwapChain9Ex *chain = lookup_hooked_swapchain(This);

	HookDebug("HookedChain::GetPresentStatistics()\n");

	if (chain)
		return IDirect3DSwapChain9Ex_GetPresentStats(chain, pPresentationStatistics);

	return orig_vtable_ex.GetPresentStats(This, pPresentationStatistics);

}

static void install_hooks(IDirect3DSwapChain9 *chain)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSection(&chain_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, chain->lpVtbl, sizeof(struct IDirect3DSwapChain9ExVtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, chain->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, chain->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, chain->lpVtbl->Release, Release);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Present, chain->lpVtbl->Present, Present);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetFrontBufferData, chain->lpVtbl->GetFrontBufferData, GetFrontBufferData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetBackBuffer, chain->lpVtbl->GetBackBuffer, GetBackBuffer);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetRasterStatus, chain->lpVtbl->GetRasterStatus, GetRasterStatus);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDisplayMode, chain->lpVtbl->GetDisplayMode, GetDisplayMode);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDevice, chain->lpVtbl->GetDevice, GetDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPresentParameters, chain->lpVtbl->GetPresentParameters, GetPresentParameters);

}
static void install_hooks(IDirect3DSwapChain9Ex *chain)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;

	install_hooks((IDirect3DSwapChain9*)chain);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.GetDisplayModeEx, chain->lpVtbl->GetDisplayModeEx, GetDisplayModeEx);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.GetPresentStats, chain->lpVtbl->GetDisplayModeEx, GetPresentStats);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.GetLastPresentCount, chain->lpVtbl->GetLastPresentCount, GetLastPresentCount);
}

typedef struct IDirect3DSwapChain9ExTrampoline {
	CONST_VTBL struct IDirect3DSwapChain9ExVtbl *lpVtbl;
	IDirect3DSwapChain9Ex *orig_this;
} IDirect3DSwapChain9ExTrampoline;
typedef struct IDirect3DSwapChain9Trampoline {
	CONST_VTBL struct IDirect3DSwapChain9Vtbl *lpVtbl;
	IDirect3DSwapChain9 *orig_this;
} IDirect3DSwapChain9Trampoline;
// IUnknown
template<typename SwapChain>
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	SwapChain * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineSwapChain:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3DSwapChain9Trampoline*)This)->orig_this, riid, ppvObject);
}

template<typename SwapChain>
static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	SwapChain * This)
{
	HookDebug("TrampolineSwapChain:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3DSwapChain9Trampoline*)This)->orig_this);
}

template<typename SwapChain>
static ULONG STDMETHODCALLTYPE TrampolineRelease(
	SwapChain * This)
{
	HookDebug("TrampolineSwapChain:: Release()\n");

	return orig_vtable.Release(((IDirect3DSwapChain9Trampoline*)This)->orig_this);
}

template<typename SwapChain>
static HRESULT STDMETHODCALLTYPE TrampolinePresent(SwapChain * This, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags) {

	HookDebug("TrampolineSwapChain:: Present()\n");

	return orig_vtable.Present(((IDirect3DSwapChain9Trampoline*)This)->orig_this, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);

}
template<typename SwapChain>
static HRESULT STDMETHODCALLTYPE TrampolineGetFrontBufferData(SwapChain * This, IDirect3DSurface9 *pDestSurface) {

	HookDebug("TrampolineSwapChain:: GetFrontBufferData()\n");

	return orig_vtable.GetFrontBufferData(((IDirect3DSwapChain9Trampoline*)This)->orig_this, pDestSurface);

}
template<typename SwapChain>
static HRESULT STDMETHODCALLTYPE TrampolineGetBackBuffer(SwapChain * This, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer) {

	HookDebug("TrampolineSwapChain:: GetBackBuffer(()\n");

	return orig_vtable.GetBackBuffer(((IDirect3DSwapChain9Trampoline*)This)->orig_this, iBackBuffer, Type, ppBackBuffer);

}

template<typename SwapChain>
static HRESULT STDMETHODCALLTYPE TrampolineGetRasterStatus(SwapChain * This, D3DRASTER_STATUS* pRasterStatus) {

	HookDebug("TrampolineSwapChain:: GetRasterStatus()\n");

	return orig_vtable.GetRasterStatus(((IDirect3DSwapChain9Trampoline*)This)->orig_this, pRasterStatus);

}
template<typename SwapChain>
static HRESULT STDMETHODCALLTYPE TrampolineGetDisplayMode(SwapChain * This, D3DDISPLAYMODE* pMode) {
	HookDebug("TrampolineSwapChain:: GetDisplayMode()\n");

	return orig_vtable.GetDisplayMode(((IDirect3DSwapChain9Trampoline*)This)->orig_this, pMode);


}
template<typename SwapChain>
static HRESULT STDMETHODCALLTYPE TrampolineGetDevice(SwapChain * This, IDirect3DDevice9** ppDevice) {

	HookDebug("TrampolineSwapChain:: GetDevice()\n");

	return orig_vtable.GetDevice(((IDirect3DSwapChain9Trampoline*)This)->orig_this, ppDevice);

}
template<typename SwapChain>
static HRESULT STDMETHODCALLTYPE TrampolineGetPresentParameters(SwapChain * This, D3DPRESENT_PARAMETERS* pPresentationParameters) {

	HookDebug("TrampolineSwapChain:: GetPresentParameters()\n");

	return orig_vtable.GetPresentParameters(((IDirect3DSwapChain9Trampoline*)This)->orig_this, pPresentationParameters);

}

static HRESULT STDMETHODCALLTYPE TrampolineGetPresentStats(IDirect3DSwapChain9Ex *This, D3DPRESENTSTATS *pPresentationStatistics) {

	HookDebug("TrampolineSwapChain:: GetPresentStats()\n");

	return orig_vtable_ex.GetPresentStats(((IDirect3DSwapChain9ExTrampoline*)This)->orig_this, pPresentationStatistics);

}

static HRESULT STDMETHODCALLTYPE TrampolineGetLastPresentCount(IDirect3DSwapChain9Ex *This, UINT *pLastPresentCount) {

	HookDebug("TrampolineSwapChain:: GetLastPresentCount()\n");

	return orig_vtable_ex.GetLastPresentCount(((IDirect3DSwapChain9ExTrampoline*)This)->orig_this, pLastPresentCount);

}

static HRESULT STDMETHODCALLTYPE TrampolineGetDisplayModeEx(IDirect3DSwapChain9Ex *This, D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation) {

	HookDebug("TrampolineSwapChain:: GetDisplayModeEx()\n");

	return orig_vtable_ex.GetDisplayModeEx(((IDirect3DSwapChain9ExTrampoline*)This)->orig_this, pMode, pRotation);

}
static CONST_VTBL struct IDirect3DSwapChain9Vtbl trampoline_vtable = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolinePresent,
	TrampolineGetFrontBufferData,
	TrampolineGetBackBuffer,
	TrampolineGetRasterStatus,
	TrampolineGetDisplayMode,
	TrampolineGetDevice,
	TrampolineGetPresentParameters
};

static CONST_VTBL struct IDirect3DSwapChain9ExVtbl trampoline_vtable_ex = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolinePresent,
	TrampolineGetFrontBufferData,
	TrampolineGetBackBuffer,
	TrampolineGetRasterStatus,
	TrampolineGetDisplayMode,
	TrampolineGetDevice,
	TrampolineGetPresentParameters,
	TrampolineGetLastPresentCount,
	TrampolineGetPresentStats,
	TrampolineGetDisplayModeEx
};
IDirect3DSwapChain9Ex* hook_swapchain(IDirect3DSwapChain9Ex *orig_chain, IDirect3DSwapChain9Ex *hacker_chain)
{
	IDirect3DSwapChain9ExTrampoline *trampoline_chain = new IDirect3DSwapChain9ExTrampoline();
	trampoline_chain->lpVtbl = &trampoline_vtable_ex;
	trampoline_chain->orig_this = orig_chain;

	install_hooks(orig_chain);
	EnterCriticalSection(&chain_map_lock);
	chain_map[(IDirect3DSwapChain9*)orig_chain] = (IDirect3DSwapChain9*)hacker_chain;
	LeaveCriticalSection(&chain_map_lock);

	return (IDirect3DSwapChain9Ex*)trampoline_chain;
}

IDirect3DSwapChain9* hook_swapchain(IDirect3DSwapChain9 *orig_chain, IDirect3DSwapChain9 *hacker_chain)
{
	IDirect3DSwapChain9Trampoline *trampoline_chain = new IDirect3DSwapChain9Trampoline();
	trampoline_chain->lpVtbl = &trampoline_vtable;
	trampoline_chain->orig_this = orig_chain;

	install_hooks(orig_chain);
	EnterCriticalSection(&chain_map_lock);
	chain_map[orig_chain] = hacker_chain;
	LeaveCriticalSection(&chain_map_lock);

	return (IDirect3DSwapChain9*)trampoline_chain;
}
