#include <unordered_map>
#include "HookedDeviceDX9.h"

#include "DLLMainHookDX9.h"

#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

// A map to look up the hacker device from the original device:
typedef std::unordered_map<IDirect3DDevice9 *, IDirect3DDevice9 *> DeviceMap;

static DeviceMap device_map;
static CRITICAL_SECTION device_map_lock;

// Holds all the function pointers that we need to call into the real original
// device:
static struct IDirect3DDevice9ExVtbl orig_vtable_ex;
static struct IDirect3DDevice9Vtbl orig_vtable;
static bool hooks_installed = false;
IDirect3DDevice9* lookup_hooked_device_dx9(IDirect3DDevice9 *orig_device)
{
	DeviceMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&device_map_lock);
	i = device_map.find(orig_device);
	if (i == device_map.end()) {
		LeaveCriticalSection(&device_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&device_map_lock);

	return i->second;
}

::IDirect3DDevice9Ex * lookup_hooked_device_dx9(::IDirect3DDevice9Ex * orig_device)
{
	DeviceMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&device_map_lock);
	i = device_map.find((::IDirect3DDevice9*)orig_device);
	if (i == device_map.end()) {
		LeaveCriticalSection(&device_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&device_map_lock);

	return(::IDirect3DDevice9Ex*)i->second;
}

// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3DDevice9 * This,
	/*  */ REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice:: QueryInterface()\n");

	if (device)
		return IDirect3DDevice9_QueryInterface(device, riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}

static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3DDevice9 * This)
{
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice:: AddRef()\n");

	if (device)
		return IDirect3DDevice9_AddRef(device);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3DDevice9 * This)
{
	DeviceMap::iterator i;
	ULONG ref;

	HookDebug("HookedDevice:: Release()\n");

	EnterCriticalSection(&device_map_lock);
	i = device_map.find((IDirect3DDevice9*)This);
	if (i != device_map.end()) {
		ref = IDirect3DDevice9_Release(i->second);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&device_map_lock);

	return ref;
}
//IDirect3DDevice9
static HRESULT STDMETHODCALLTYPE TestCooperativeLevel(IDirect3DDevice9 * This) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::TestCooperativeLevel()\n");

	if (device)
		return IDirect3DDevice9_TestCooperativeLevel(device);

	return orig_vtable.TestCooperativeLevel(This);

}
static UINT STDMETHODCALLTYPE GetAvailableTextureMem(IDirect3DDevice9 * This) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::GetAvailableTextureMem()\n");

	if (device)
		return IDirect3DDevice9_GetAvailableTextureMem(device);

	return orig_vtable.GetAvailableTextureMem(This);


}
static HRESULT STDMETHODCALLTYPE EvictManagedResources(IDirect3DDevice9 * This) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::EvictManagedResources()\n");

	if (device)
		return IDirect3DDevice9_EvictManagedResources(device);

	return orig_vtable.EvictManagedResources(This);



}
static HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3DDevice9 * This, IDirect3D9** ppD3D9) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::GetDirect3D()\n");

	if (device)
		return IDirect3DDevice9_GetDirect3D(device, ppD3D9);

	return orig_vtable.GetDirect3D(This, ppD3D9);
}

static HRESULT STDMETHODCALLTYPE _GetDeviceCaps(IDirect3DDevice9 * This, D3DCAPS9* pCaps) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::GetDeviceCaps()\n");

	if (device)
		return IDirect3DDevice9_GetDeviceCaps(device, pCaps);

	return orig_vtable.GetDeviceCaps(This, pCaps);

}

static HRESULT STDMETHODCALLTYPE GetDisplayMode(IDirect3DDevice9 * This, UINT iSwapChain, D3DDISPLAYMODE* pMode) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::GetDisplayMode()\n");

	if (device)
		return IDirect3DDevice9_GetDisplayMode(device, iSwapChain, pMode);

	return orig_vtable.GetDisplayMode(This, iSwapChain, pMode);



}

static HRESULT STDMETHODCALLTYPE GetCreationParameters(IDirect3DDevice9 * This, D3DDEVICE_CREATION_PARAMETERS *pParameters) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::GetCreationParameters()\n");

	if (device)
		return IDirect3DDevice9_GetCreationParameters(device, pParameters);

	return orig_vtable.GetCreationParameters(This, pParameters);

}

static HRESULT STDMETHODCALLTYPE SetCursorProperties(IDirect3DDevice9 * This, UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::SetCursorProperties()\n");

	if (device)
		return IDirect3DDevice9_SetCursorProperties(device, XHotSpot, YHotSpot, pCursorBitmap);

	return orig_vtable.SetCursorProperties(This, XHotSpot, YHotSpot, pCursorBitmap);


}

static void STDMETHODCALLTYPE SetCursorPosition(IDirect3DDevice9 * This, int X, int Y, DWORD Flags) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::SetCursorPosition()\n");

	if (device)
		return IDirect3DDevice9_SetCursorPosition(device, X, Y, Flags);

	return orig_vtable.SetCursorPosition(This, X, Y, Flags);

}

static BOOL STDMETHODCALLTYPE _ShowCursor(IDirect3DDevice9 * This, BOOL bShow) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::ShowCursor()\n");

	if (device)
		return IDirect3DDevice9_ShowCursor(device, bShow);

	return orig_vtable.ShowCursor(This, bShow);


}
static HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(IDirect3DDevice9 * This, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::CreateAdditionalSwapChain()\n");

	if (device)
		return IDirect3DDevice9_CreateAdditionalSwapChain(device, pPresentationParameters, pSwapChain);

	return orig_vtable.CreateAdditionalSwapChain(This, pPresentationParameters, pSwapChain);

}

static HRESULT STDMETHODCALLTYPE GetSwapChain(IDirect3DDevice9 * This, UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::GetSwapChain()\n");

	if (device)
		return IDirect3DDevice9_GetSwapChain(device, iSwapChain, pSwapChain);

	return orig_vtable.GetSwapChain(This, iSwapChain, pSwapChain);


}

static UINT STDMETHODCALLTYPE GetNumberOfSwapChains(IDirect3DDevice9 * This) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::GetNumberOfSwapChains()\n");

	if (device)
		return IDirect3DDevice9_GetNumberOfSwapChains(device);

	return orig_vtable.GetNumberOfSwapChains(This);

}

static HRESULT STDMETHODCALLTYPE Reset(IDirect3DDevice9 * This, D3DPRESENT_PARAMETERS* pPresentationParameters) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::Reset()\n");

	if (device)
		return IDirect3DDevice9_Reset(device, pPresentationParameters);

	return orig_vtable.Reset(This, pPresentationParameters);

}

static HRESULT STDMETHODCALLTYPE Present(IDirect3DDevice9 * This, const RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::Present()\n");

	if (device)
		return IDirect3DDevice9_Present(device, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

	return orig_vtable.Present(This, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);



}
static HRESULT STDMETHODCALLTYPE GetBackBuffer(IDirect3DDevice9 * This, UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::GetBackBuffer()\n");

	if (device)
		return IDirect3DDevice9_GetBackBuffer(device, iSwapChain, iBackBuffer, Type, ppBackBuffer);

	return orig_vtable.GetBackBuffer(This, iSwapChain, iBackBuffer, Type, ppBackBuffer);


}
static HRESULT STDMETHODCALLTYPE GetRasterStatus(IDirect3DDevice9 * This, UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::GetRasterStatus()\n");

	if (device)
		return IDirect3DDevice9_GetRasterStatus(device, iSwapChain, pRasterStatus);

	return orig_vtable.GetRasterStatus(This, iSwapChain, pRasterStatus);



}

static HRESULT STDMETHODCALLTYPE SetDialogBoxMode(IDirect3DDevice9 * This, BOOL bEnableDialogs) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::SetDialogBoxMode()\n");

	if (device)
		return IDirect3DDevice9_SetDialogBoxMode(device, bEnableDialogs);

	return orig_vtable.SetDialogBoxMode(This, bEnableDialogs);

}

static void STDMETHODCALLTYPE SetGammaRamp(IDirect3DDevice9 * This, UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP* pRamp) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::SetGammaRamp()\n");

	if (device)
		return IDirect3DDevice9_SetGammaRamp(device, iSwapChain, Flags, pRamp);

	return orig_vtable.SetGammaRamp(This, iSwapChain, Flags, pRamp);

}

static void STDMETHODCALLTYPE GetGammaRamp(IDirect3DDevice9 * This, UINT iSwapChain, D3DGAMMARAMP* pRamp) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::GetGammaRamp()\n");

	if (device)
		return IDirect3DDevice9_GetGammaRamp(device, iSwapChain, pRamp);

	return orig_vtable.GetGammaRamp(This, iSwapChain, pRamp);
}

static HRESULT STDMETHODCALLTYPE CreateTexture(IDirect3DDevice9 * This, UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::CreateTexture()\n");

	if (device)
		return IDirect3DDevice9_CreateTexture(device, Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);

	return orig_vtable.CreateTexture(This, Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);


}

static HRESULT STDMETHODCALLTYPE CreateVolumeTexture(IDirect3DDevice9 * This, UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);
	HookDebug("HookedDevice::CreateVolumeTexture()\n");

	if (device)
		return IDirect3DDevice9_CreateVolumeTexture(device, Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);

	return orig_vtable.CreateVolumeTexture(This, Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);


}

static HRESULT STDMETHODCALLTYPE CreateCubeTexture(IDirect3DDevice9 * This, UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::CreateCubeTexture()\n");

	if (device)
		return IDirect3DDevice9_CreateCubeTexture(device, EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);

	return orig_vtable.CreateCubeTexture(This, EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);


}
static HRESULT STDMETHODCALLTYPE CreateVertexBuffer(IDirect3DDevice9 * This, UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::CreateVertexBuffer()\n");

	if (device)
		return IDirect3DDevice9_CreateVertexBuffer(device, Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);

	return orig_vtable.CreateVertexBuffer(This, Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);

}

static HRESULT STDMETHODCALLTYPE CreateIndexBuffer(IDirect3DDevice9 * This, UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) {


	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::CreateIndexBuffer()\n");

	if (device)
		return IDirect3DDevice9_CreateIndexBuffer(device, Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);

	return orig_vtable.CreateIndexBuffer(This, Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);


}

static HRESULT STDMETHODCALLTYPE CreateRenderTarget(IDirect3DDevice9 * This, UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::CreateRenderTarget()\n");

	if (device)
		return IDirect3DDevice9_CreateRenderTarget(device, Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);

	return orig_vtable.CreateRenderTarget(This, Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);



}
static HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(IDirect3DDevice9 * This, UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::CreateDepthStencilSurface()\n");

	if (device)
		return IDirect3DDevice9_CreateDepthStencilSurface(device, Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);

	return orig_vtable.CreateDepthStencilSurface(This, Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);




}
static HRESULT STDMETHODCALLTYPE UpdateSurface(IDirect3DDevice9 * This, IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, CONST POINT* pDestPoint) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::UpdateSurface()\n");

	if (device)
		return IDirect3DDevice9_UpdateSurface(device, pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint);

	return orig_vtable.UpdateSurface(This, pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint);



}
static HRESULT STDMETHODCALLTYPE UpdateTexture(IDirect3DDevice9 * This, IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::UpdateTexture()\n");

	if (device)
		return IDirect3DDevice9_UpdateTexture(device, pSourceTexture, pDestinationTexture);

	return orig_vtable.UpdateTexture(This, pSourceTexture, pDestinationTexture);



}

static HRESULT STDMETHODCALLTYPE GetRenderTargetData(IDirect3DDevice9 * This, IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetRenderTargetData()\n");

	if (device)
		return IDirect3DDevice9_GetRenderTargetData(device, pRenderTarget, pDestSurface);

	return orig_vtable.GetRenderTargetData(This, pRenderTarget, pDestSurface);




}

static HRESULT STDMETHODCALLTYPE GetFrontBufferData(IDirect3DDevice9 * This, UINT iSwapChain, IDirect3DSurface9* pDestSurface) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetFrontBufferData()\n");

	if (device)
		return IDirect3DDevice9_GetFrontBufferData(device, iSwapChain, pDestSurface);

	return orig_vtable.GetFrontBufferData(This, iSwapChain, pDestSurface);

}

static HRESULT STDMETHODCALLTYPE StretchRect(IDirect3DDevice9 * This, IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::StretchRect()\n");

	if (device)
		return IDirect3DDevice9_StretchRect(device, pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);

	return orig_vtable.StretchRect(This, pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);


}

static HRESULT STDMETHODCALLTYPE ColorFill(IDirect3DDevice9 * This, IDirect3DSurface9* pSurface, CONST RECT* pRect, D3DCOLOR color) {


	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::ColorFill()\n");

	if (device)
		return IDirect3DDevice9_ColorFill(device, pSurface, pRect, color);

	return orig_vtable.ColorFill(This, pSurface, pRect, color);


}

static HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurface(IDirect3DDevice9 * This, UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::CreateOffscreenPlainSurface()\n");

	if (device)
		return IDirect3DDevice9_CreateOffscreenPlainSurface(device, Width, Height, Format, Pool, ppSurface, pSharedHandle);

	return orig_vtable.CreateOffscreenPlainSurface(This, Width, Height, Format, Pool, ppSurface, pSharedHandle);

}

static HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirect3DDevice9 * This, DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetRenderTarget()\n");

	if (device)
		return IDirect3DDevice9_SetRenderTarget(device, RenderTargetIndex, pRenderTarget);

	return orig_vtable.SetRenderTarget(This, RenderTargetIndex, pRenderTarget);



}
static HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirect3DDevice9 * This, DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetRenderTarget()\n");

	if (device)
		return IDirect3DDevice9_GetRenderTarget(device, RenderTargetIndex, ppRenderTarget);

	return orig_vtable.GetRenderTarget(This, RenderTargetIndex, ppRenderTarget);



}

static HRESULT STDMETHODCALLTYPE SetDepthStencilSurface(IDirect3DDevice9 * This, IDirect3DSurface9* pNewZStencil) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetDepthStencilSurface()\n");

	if (device)
		return IDirect3DDevice9_SetDepthStencilSurface(device, pNewZStencil);

	return orig_vtable.SetDepthStencilSurface(This, pNewZStencil);


}

static HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DDevice9 * This, IDirect3DSurface9** ppZStencilSurface) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetDepthStencilSurface()\n");

	if (device)
		return IDirect3DDevice9_GetDepthStencilSurface(device, ppZStencilSurface);

	return orig_vtable.GetDepthStencilSurface(This, ppZStencilSurface);
}

static HRESULT STDMETHODCALLTYPE BeginScene(IDirect3DDevice9 * This) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::BeginScene()\n");

	if (device)
		return IDirect3DDevice9_BeginScene(device);

	return orig_vtable.BeginScene(This);

}

static HRESULT STDMETHODCALLTYPE EndScene(IDirect3DDevice9 * This) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::EndScene()\n");

	if (device)
		return IDirect3DDevice9_EndScene(device);

	return orig_vtable.EndScene(This);

}

static HRESULT STDMETHODCALLTYPE Clear(IDirect3DDevice9 * This, DWORD Count, CONST D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::Clear()\n");

	if (device)
		return IDirect3DDevice9_Clear(device, Count, pRects, Flags, Color, Z, Stencil);

	return orig_vtable.Clear(This, Count, pRects, Flags, Color, Z, Stencil);

}

static HRESULT STDMETHODCALLTYPE SetTransform(IDirect3DDevice9 * This, D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetTransform()\n");

	if (device)
		return IDirect3DDevice9_SetTransform(device, State, pMatrix);

	return orig_vtable.SetTransform(This, State, pMatrix);

}

static HRESULT STDMETHODCALLTYPE GetTransform(IDirect3DDevice9 * This, D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetTransform()\n");

	if (device)
		return IDirect3DDevice9_GetTransform(device, State, pMatrix);

	return orig_vtable.GetTransform(This, State, pMatrix);
}

static HRESULT STDMETHODCALLTYPE MultiplyTransform(IDirect3DDevice9 * This, D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix) {

	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::MultiplyTransform()\n");

	if (device)
		return IDirect3DDevice9_MultiplyTransform(device, State, pMatrix);

	return orig_vtable.MultiplyTransform(This, State, pMatrix);
}

static HRESULT STDMETHODCALLTYPE SetViewport(IDirect3DDevice9 * This, CONST D3DVIEWPORT9* pViewport) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetViewport()\n");

	if (device)
		return IDirect3DDevice9_SetViewport(device, pViewport);

	return orig_vtable.SetViewport(This, pViewport);

}

static HRESULT STDMETHODCALLTYPE GetViewport(IDirect3DDevice9 * This, D3DVIEWPORT9* pViewport) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetViewport()\n");

	if (device)
		return IDirect3DDevice9_GetViewport(device, pViewport);

	return orig_vtable.GetViewport(This, pViewport);

}

static HRESULT STDMETHODCALLTYPE SetMaterial(IDirect3DDevice9 * This, CONST D3DMATERIAL9* pMaterial) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetMaterial()\n");

	if (device)
		return IDirect3DDevice9_SetMaterial(device, pMaterial);

	return orig_vtable.SetMaterial(This, pMaterial);

}

static HRESULT STDMETHODCALLTYPE GetMaterial(IDirect3DDevice9 * This, D3DMATERIAL9* pMaterial) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetMaterial()\n");

	if (device)
		return IDirect3DDevice9_GetMaterial(device, pMaterial);

	return orig_vtable.GetMaterial(This, pMaterial);

}

static HRESULT STDMETHODCALLTYPE SetLight(IDirect3DDevice9 * This, DWORD Index, CONST D3DLIGHT9 *pLight) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetLight()\n");

	if (device)
		return IDirect3DDevice9_SetLight(device, Index, pLight);

	return orig_vtable.SetLight(This, Index, pLight);

}
static HRESULT STDMETHODCALLTYPE GetLight(IDirect3DDevice9 * This, DWORD Index, D3DLIGHT9 *pLight) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetLight()\n");

	if (device)
		return IDirect3DDevice9_GetLight(device, Index, pLight);

	return orig_vtable.GetLight(This, Index, pLight);
}

static HRESULT STDMETHODCALLTYPE LightEnable(IDirect3DDevice9 * This, DWORD Index, BOOL Enable) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::LightEnable()\n");

	if (device)
		return IDirect3DDevice9_LightEnable(device, Index, Enable);

	return orig_vtable.LightEnable(This, Index, Enable);
}

static HRESULT STDMETHODCALLTYPE GetLightEnable(IDirect3DDevice9 * This, DWORD Index, BOOL* pEnable) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetLightEnable()\n");

	if (device)
		return IDirect3DDevice9_GetLightEnable(device, Index, pEnable);

	return orig_vtable.GetLightEnable(This, Index, pEnable);
}

static HRESULT STDMETHODCALLTYPE SetClipPlane(IDirect3DDevice9 * This, DWORD Index, CONST float* pPlane) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetClipPlane()\n");

	if (device)
		return IDirect3DDevice9_SetClipPlane(device, Index, pPlane);

	return orig_vtable.SetClipPlane(This, Index, pPlane);
}

static HRESULT STDMETHODCALLTYPE GetClipPlane(IDirect3DDevice9 * This, DWORD Index, float* pPlane) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetClipPlane()\n");

	if (device)
		return IDirect3DDevice9_GetClipPlane(device, Index, pPlane);

	return orig_vtable.GetClipPlane(This, Index, pPlane);
}

static HRESULT STDMETHODCALLTYPE SetRenderState(IDirect3DDevice9 * This, D3DRENDERSTATETYPE State, DWORD Value) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetRenderState()\n");

	if (device)
		return IDirect3DDevice9_SetRenderState(device, State, Value);

	return orig_vtable.SetRenderState(This, State, Value);
}
static HRESULT STDMETHODCALLTYPE GetRenderState(IDirect3DDevice9 * This, D3DRENDERSTATETYPE State, DWORD* pValue) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetRenderState()\n");

	if (device)
		return IDirect3DDevice9_GetRenderState(device, State, pValue);

	return orig_vtable.GetRenderState(This, State, pValue);
}
static HRESULT STDMETHODCALLTYPE CreateStateBlock(IDirect3DDevice9 * This, D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::CreateStateBlock()\n");

	if (device)
		return IDirect3DDevice9_CreateStateBlock(device, Type, ppSB);

	return orig_vtable.CreateStateBlock(This, Type, ppSB);
}

static HRESULT STDMETHODCALLTYPE BeginStateBlock(IDirect3DDevice9 * This) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::BeginStateBlock()\n");

	if (device)
		return IDirect3DDevice9_BeginStateBlock(device);

	return orig_vtable.BeginStateBlock(This);
}
static HRESULT STDMETHODCALLTYPE EndStateBlock(IDirect3DDevice9 * This, IDirect3DStateBlock9** ppSB) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::EndStateBlock()\n");

	if (device)
		return IDirect3DDevice9_EndStateBlock(device, ppSB);

	return orig_vtable.EndStateBlock(This, ppSB);
}

static HRESULT STDMETHODCALLTYPE SetClipStatus(IDirect3DDevice9 * This, CONST D3DCLIPSTATUS9* pClipStatus) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetClipStatus()\n");

	if (device)
		return IDirect3DDevice9_SetClipStatus(device, pClipStatus);

	return orig_vtable.SetClipStatus(This, pClipStatus);
}

static HRESULT STDMETHODCALLTYPE GetClipStatus(IDirect3DDevice9 * This, D3DCLIPSTATUS9* pClipStatus) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetClipStatus()\n");

	if (device)
		return IDirect3DDevice9_GetClipStatus(device, pClipStatus);

	return orig_vtable.GetClipStatus(This, pClipStatus);
}

static HRESULT STDMETHODCALLTYPE GetTexture(IDirect3DDevice9 * This, DWORD Stage, IDirect3DBaseTexture9** ppTexture) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetTexture()\n");

	if (device)
		return IDirect3DDevice9_GetTexture(device, Stage, ppTexture);

	return orig_vtable.GetTexture(This, Stage, ppTexture);
}

static HRESULT STDMETHODCALLTYPE SetTexture(IDirect3DDevice9 * This, DWORD Stage, IDirect3DBaseTexture9* ppTexture) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetTexture()\n");

	if (device)
		return IDirect3DDevice9_SetTexture(device, Stage, ppTexture);

	return orig_vtable.SetTexture(This, Stage, ppTexture);
}
static HRESULT STDMETHODCALLTYPE GetTextureStageState(IDirect3DDevice9 * This, DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetTextureStageState()\n");

	if (device)
		return IDirect3DDevice9_GetTextureStageState(device, Stage, Type, pValue);

	return orig_vtable.GetTextureStageState(This, Stage, Type, pValue);
}

static HRESULT STDMETHODCALLTYPE SetTextureStageState(IDirect3DDevice9 * This, DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetTextureStageState()\n");

	if (device)
		return IDirect3DDevice9_SetTextureStageState(device, Stage, Type, Value);

	return orig_vtable.SetTextureStageState(This, Stage, Type, Value);
}

static HRESULT STDMETHODCALLTYPE GetSamplerState(IDirect3DDevice9 * This, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetSamplerState()\n");

	if (device)
		return IDirect3DDevice9_GetSamplerState(device, Sampler, Type, pValue);

	return orig_vtable.GetSamplerState(This, Sampler, Type, pValue);
}

static HRESULT STDMETHODCALLTYPE SetSamplerState(IDirect3DDevice9 * This, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetSamplerState()\n");

	if (device)
		return IDirect3DDevice9_SetSamplerState(device, Sampler, Type, Value);

	return orig_vtable.SetSamplerState(This, Sampler, Type, Value);
}

static HRESULT STDMETHODCALLTYPE ValidateDevice(IDirect3DDevice9 * This, DWORD* pNumPasses) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::ValidateDevice()\n");

	if (device)
		return IDirect3DDevice9_ValidateDevice(device, pNumPasses);

	return orig_vtable.ValidateDevice(This, pNumPasses);
}

static HRESULT STDMETHODCALLTYPE _SetPaletteEntries(IDirect3DDevice9 * This, UINT PaletteNumber, CONST PALETTEENTRY* pEntries) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetPaletteEntries()\n");

	if (device)
		return IDirect3DDevice9_SetPaletteEntries(device, PaletteNumber, pEntries);

	return orig_vtable.SetPaletteEntries(This, PaletteNumber, pEntries);
}
static HRESULT STDMETHODCALLTYPE _GetPaletteEntries(IDirect3DDevice9 * This, UINT PaletteNumber, PALETTEENTRY* pEntries) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetPaletteEntries()\n");

	if (device)
		return IDirect3DDevice9_GetPaletteEntries(device, PaletteNumber, pEntries);

	return orig_vtable.GetPaletteEntries(This, PaletteNumber, pEntries);
}

static HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(IDirect3DDevice9 * This, UINT PaletteNumber) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetCurrentTexturePalette()\n");

	if (device)
		return IDirect3DDevice9_SetCurrentTexturePalette(device, PaletteNumber);

	return orig_vtable.SetCurrentTexturePalette(This, PaletteNumber);
}
static HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(IDirect3DDevice9 * This, UINT *PaletteNumber) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetCurrentTexturePalette()\n");

	if (device)
		return IDirect3DDevice9_GetCurrentTexturePalette(device, PaletteNumber);

	return orig_vtable.GetCurrentTexturePalette(This, PaletteNumber);
}

static HRESULT STDMETHODCALLTYPE SetScissorRect(IDirect3DDevice9 * This, CONST RECT* pRect) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetScissorRect()\n");

	if (device)
		return IDirect3DDevice9_SetScissorRect(device, pRect);

	return orig_vtable.SetScissorRect(This, pRect);
}
static HRESULT STDMETHODCALLTYPE GetScissorRect(IDirect3DDevice9 * This, RECT* pRect) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetScissorRect()\n");

	if (device)
		return IDirect3DDevice9_GetScissorRect(device, pRect);

	return orig_vtable.GetScissorRect(This, pRect);
}
static HRESULT STDMETHODCALLTYPE SetSoftwareVertexProcessing(IDirect3DDevice9 * This, BOOL bSoftware) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetSoftwareVertexProcessing()\n");

	if (device)
		return IDirect3DDevice9_SetSoftwareVertexProcessing(device, bSoftware);

	return orig_vtable.SetSoftwareVertexProcessing(This, bSoftware);
}

static BOOL STDMETHODCALLTYPE GetSoftwareVertexProcessing(IDirect3DDevice9 * This) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetSoftwareVertexProcessing()\n");

	if (device)
		return IDirect3DDevice9_GetSoftwareVertexProcessing(device);

	return orig_vtable.GetSoftwareVertexProcessing(This);
}
static HRESULT STDMETHODCALLTYPE SetNPatchMode(IDirect3DDevice9 * This, float nSegments) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetNPatchMode()\n");

	if (device)
		return IDirect3DDevice9_SetNPatchMode(device, nSegments);

	return orig_vtable.SetNPatchMode(This, nSegments);
}
static float STDMETHODCALLTYPE GetNPatchMode(IDirect3DDevice9 * This) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetNPatchMode()\n");

	if (device)
		return IDirect3DDevice9_GetNPatchMode(device);

	return orig_vtable.GetNPatchMode(This);

}

static HRESULT STDMETHODCALLTYPE DrawPrimitive(IDirect3DDevice9 * This, D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::DrawPrimitive()\n");

	if (device)
		return IDirect3DDevice9_DrawPrimitive(device, PrimitiveType, StartVertex, PrimitiveCount);

	return orig_vtable.DrawPrimitive(This, PrimitiveType, StartVertex, PrimitiveCount);

}

static HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(IDirect3DDevice9 * This, D3DPRIMITIVETYPE Type, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::DrawIndexedPrimitive()\n");

	if (device)
		return IDirect3DDevice9_DrawIndexedPrimitive(device, Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);

	return orig_vtable.DrawIndexedPrimitive(This, Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);

}

static HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(IDirect3DDevice9 * This, D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::DrawPrimitiveUP()\n");

	if (device)
		return IDirect3DDevice9_DrawPrimitiveUP(device, PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);

	return orig_vtable.DrawPrimitiveUP(This, PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);

}
static HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(IDirect3DDevice9 * This, D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::DrawIndexedPrimitiveUP()\n");

	if (device)
		return IDirect3DDevice9_DrawIndexedPrimitiveUP(device, PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);

	return orig_vtable.DrawIndexedPrimitiveUP(This, PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);

}

static HRESULT STDMETHODCALLTYPE ProcessVertices(IDirect3DDevice9 * This, UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::ProcessVertices()\n");

	if (device)
		return IDirect3DDevice9_ProcessVertices(device, SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags);

	return orig_vtable.ProcessVertices(This, SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags);

}
static HRESULT STDMETHODCALLTYPE CreateVertexDeclaration(IDirect3DDevice9 * This, CONST D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::CreateVertexDeclaration()\n");

	if (device)
		return IDirect3DDevice9_CreateVertexDeclaration(device, pVertexElements, ppDecl);

	return orig_vtable.CreateVertexDeclaration(This, pVertexElements, ppDecl);

}

static HRESULT STDMETHODCALLTYPE SetVertexDeclaration(IDirect3DDevice9 * This, IDirect3DVertexDeclaration9* pDecl) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetVertexDeclaration()\n");

	if (device)
		return IDirect3DDevice9_SetVertexDeclaration(device, pDecl);

	return orig_vtable.SetVertexDeclaration(This, pDecl);

}
static HRESULT STDMETHODCALLTYPE GetVertexDeclaration(IDirect3DDevice9 * This, IDirect3DVertexDeclaration9** ppDecl) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetVertexDeclaration()\n");

	if (device)
		return IDirect3DDevice9_GetVertexDeclaration(device, ppDecl);

	return orig_vtable.GetVertexDeclaration(This, ppDecl);

}
static HRESULT STDMETHODCALLTYPE SetFVF(IDirect3DDevice9 * This, DWORD FVF) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetFVF()\n");

	if (device)
		return IDirect3DDevice9_SetFVF(device, FVF);

	return orig_vtable.SetFVF(This, FVF);

}
static HRESULT STDMETHODCALLTYPE GetFVF(IDirect3DDevice9 * This, DWORD* pFVF) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetFVF()\n");

	if (device)
		return IDirect3DDevice9_GetFVF(device, pFVF);

	return orig_vtable.GetFVF(This, pFVF);

}
static HRESULT STDMETHODCALLTYPE CreateVertexShader(IDirect3DDevice9 * This, CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::CreateVertexShader()\n");

	if (device)
		return IDirect3DDevice9_CreateVertexShader(device, pFunction, ppShader);

	return orig_vtable.CreateVertexShader(This, pFunction, ppShader);

}
static HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DDevice9 * This, IDirect3DVertexShader9* pShader) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetVertexShader()\n");

	if (device)
		return IDirect3DDevice9_SetVertexShader(device, pShader);

	return orig_vtable.SetVertexShader(This, pShader);

}

static HRESULT STDMETHODCALLTYPE GetVertexShader(IDirect3DDevice9 * This, IDirect3DVertexShader9** ppShader) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetVertexShader()\n");

	if (device)
		return IDirect3DDevice9_GetVertexShader(device, ppShader);

	return orig_vtable.GetVertexShader(This, ppShader);

}
static HRESULT STDMETHODCALLTYPE SetVertexShaderConstantF(IDirect3DDevice9 * This, UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetVertexShaderConstantF()\n");

	if (device)
		return IDirect3DDevice9_SetVertexShaderConstantF(device, StartRegister, pConstantData, Vector4fCount);

	return orig_vtable.SetVertexShaderConstantF(This, StartRegister, pConstantData, Vector4fCount);

}

static HRESULT STDMETHODCALLTYPE GetVertexShaderConstantF(IDirect3DDevice9 * This, UINT StartRegister, float* pConstantData, UINT Vector4fCount) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetVertexShaderConstantF()\n");

	if (device)
		return IDirect3DDevice9_GetVertexShaderConstantF(device, StartRegister, pConstantData, Vector4fCount);

	return orig_vtable.GetVertexShaderConstantF(This, StartRegister, pConstantData, Vector4fCount);

}

static HRESULT STDMETHODCALLTYPE SetVertexShaderConstantI(IDirect3DDevice9 * This, UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetVertexShaderConstantI()\n");

	if (device)
		return IDirect3DDevice9_SetVertexShaderConstantI(device, StartRegister, pConstantData, Vector4iCount);

	return orig_vtable.SetVertexShaderConstantI(This, StartRegister, pConstantData, Vector4iCount);

}
static HRESULT STDMETHODCALLTYPE GetVertexShaderConstantI(IDirect3DDevice9 * This, UINT StartRegister, int* pConstantData, UINT Vector4iCount) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetVertexShaderConstantI()\n");

	if (device)
		return IDirect3DDevice9_GetVertexShaderConstantI(device, StartRegister, pConstantData, Vector4iCount);

	return orig_vtable.GetVertexShaderConstantI(This, StartRegister, pConstantData, Vector4iCount);

}

static HRESULT STDMETHODCALLTYPE SetVertexShaderConstantB(IDirect3DDevice9 * This, UINT StartRegister, CONST BOOL* pConstantData, UINT  BoolCount) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetVertexShaderConstantB()\n");

	if (device)
		return IDirect3DDevice9_SetVertexShaderConstantB(device, StartRegister, pConstantData, BoolCount);

	return orig_vtable.SetVertexShaderConstantB(This, StartRegister, pConstantData, BoolCount);

}

static HRESULT STDMETHODCALLTYPE GetVertexShaderConstantB(IDirect3DDevice9 * This, UINT StartRegister, BOOL* pConstantData, UINT BoolCount) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetVertexShaderConstantB()\n");

	if (device)
		return IDirect3DDevice9_GetVertexShaderConstantB(device, StartRegister, pConstantData, BoolCount);

	return orig_vtable.GetVertexShaderConstantB(This, StartRegister, pConstantData, BoolCount);

}

static HRESULT STDMETHODCALLTYPE SetStreamSource(IDirect3DDevice9 * This, UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetStreamSource()\n");

	if (device)
		return IDirect3DDevice9_SetStreamSource(device, StreamNumber, pStreamData, OffsetInBytes, Stride);

	return orig_vtable.SetStreamSource(This, StreamNumber, pStreamData, OffsetInBytes, Stride);

}

static HRESULT STDMETHODCALLTYPE GetStreamSource(IDirect3DDevice9 * This, UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* pOffsetInBytes, UINT* pStride) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetStreamSource()\n");

	if (device)
		return IDirect3DDevice9_GetStreamSource(device, StreamNumber, ppStreamData, pOffsetInBytes, pStride);

	return orig_vtable.GetStreamSource(This, StreamNumber, ppStreamData, pOffsetInBytes, pStride);

}

static HRESULT STDMETHODCALLTYPE SetStreamSourceFreq(IDirect3DDevice9 * This, UINT StreamNumber, UINT Setting) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetStreamSourceFreq()\n");

	if (device)
		return IDirect3DDevice9_SetStreamSourceFreq(device, StreamNumber, Setting);

	return orig_vtable.SetStreamSourceFreq(This, StreamNumber, Setting);

}

static HRESULT STDMETHODCALLTYPE GetStreamSourceFreq(IDirect3DDevice9 * This, UINT StreamNumber, UINT* pSetting) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetStreamSourceFreq()\n");

	if (device)
		return IDirect3DDevice9_GetStreamSourceFreq(device, StreamNumber, pSetting);

	return orig_vtable.GetStreamSourceFreq(This, StreamNumber, pSetting);

}

static HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DDevice9 * This, IDirect3DIndexBuffer9* pIndexData) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetIndices()\n");

	if (device)
		return IDirect3DDevice9_SetIndices(device, pIndexData);

	return orig_vtable.SetIndices(This, pIndexData);

}
static HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DDevice9 * This, IDirect3DIndexBuffer9** ppIndexData) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetIndices()\n");

	if (device)
		return IDirect3DDevice9_GetIndices(device, ppIndexData);

	return orig_vtable.GetIndices(This, ppIndexData);

}

static HRESULT STDMETHODCALLTYPE CreatePixelShader(IDirect3DDevice9 * This, CONST DWORD* pFunction, IDirect3DPixelShader9** ppShader) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::CreatePixelShader()\n");

	if (device)
		return IDirect3DDevice9_CreatePixelShader(device, pFunction, ppShader);

	return orig_vtable.CreatePixelShader(This, pFunction, ppShader);

}

static HRESULT STDMETHODCALLTYPE SetPixelShader(IDirect3DDevice9 * This, IDirect3DPixelShader9* pShader) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetPixelShader()\n");

	if (device)
		return IDirect3DDevice9_SetPixelShader(device, pShader);

	return orig_vtable.SetPixelShader(This, pShader);

}

static HRESULT STDMETHODCALLTYPE GetPixelShader(IDirect3DDevice9 * This, IDirect3DPixelShader9** ppShader) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetPixelShader()\n");

	if (device)
		return IDirect3DDevice9_GetPixelShader(device, ppShader);

	return orig_vtable.GetPixelShader(This, ppShader);

}

static HRESULT STDMETHODCALLTYPE SetPixelShaderConstantF(IDirect3DDevice9 * This, UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetPixelShaderConstantF()\n");

	if (device)
		return IDirect3DDevice9_SetPixelShaderConstantF(device, StartRegister, pConstantData, Vector4fCount);

	return orig_vtable.SetPixelShaderConstantF(This, StartRegister, pConstantData, Vector4fCount);

}

static HRESULT STDMETHODCALLTYPE GetPixelShaderConstantF(IDirect3DDevice9 * This, UINT StartRegister, float* pConstantData, UINT Vector4fCount) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetPixelShaderConstantF()\n");

	if (device)
		return IDirect3DDevice9_GetPixelShaderConstantF(device, StartRegister, pConstantData, Vector4fCount);

	return orig_vtable.GetPixelShaderConstantF(This, StartRegister, pConstantData, Vector4fCount);

}

static HRESULT STDMETHODCALLTYPE SetPixelShaderConstantI(IDirect3DDevice9 * This, UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetPixelShaderConstantI()\n");

	if (device)
		return IDirect3DDevice9_SetPixelShaderConstantI(device, StartRegister, pConstantData, Vector4iCount);

	return orig_vtable.SetPixelShaderConstantI(This, StartRegister, pConstantData, Vector4iCount);

}

static HRESULT STDMETHODCALLTYPE GetPixelShaderConstantI(IDirect3DDevice9 * This, UINT StartRegister, int* pConstantData, UINT Vector4iCount) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetPixelShaderConstantI()\n");

	if (device)
		return IDirect3DDevice9_GetPixelShaderConstantI(device, StartRegister, pConstantData, Vector4iCount);

	return orig_vtable.GetPixelShaderConstantI(This, StartRegister, pConstantData, Vector4iCount);

}

static HRESULT STDMETHODCALLTYPE SetPixelShaderConstantB(IDirect3DDevice9 * This, UINT StartRegister, CONST BOOL* pConstantData, UINT BoolCount) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::SetPixelShaderConstantB()\n");

	if (device)
		return IDirect3DDevice9_SetPixelShaderConstantB(device, StartRegister, pConstantData, BoolCount);

	return orig_vtable.SetPixelShaderConstantB(This, StartRegister, pConstantData, BoolCount);

}

static HRESULT STDMETHODCALLTYPE GetPixelShaderConstantB(IDirect3DDevice9 * This, UINT StartRegister, BOOL* pConstantData, UINT BoolCount) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::GetPixelShaderConstantB()\n");

	if (device)
		return IDirect3DDevice9_GetPixelShaderConstantB(device, StartRegister, pConstantData, BoolCount);

	return orig_vtable.GetPixelShaderConstantB(This, StartRegister, pConstantData, BoolCount);

}

static HRESULT STDMETHODCALLTYPE DrawRectPatch(IDirect3DDevice9 * This, UINT Handle, CONST float* pNumSegs, CONST D3DRECTPATCH_INFO* pRectPatchInfo) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::DrawRectPatch()\n");

	if (device)
		return IDirect3DDevice9_DrawRectPatch(device, Handle, pNumSegs, pRectPatchInfo);

	return orig_vtable.DrawRectPatch(This, Handle, pNumSegs, pRectPatchInfo);

}

static HRESULT STDMETHODCALLTYPE DrawTriPatch(IDirect3DDevice9 * This, UINT Handle, CONST float* pNumSegs, CONST D3DTRIPATCH_INFO* pTriPatchInfo) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::DrawTriPatch()\n");

	if (device)
		return IDirect3DDevice9_DrawTriPatch(device, Handle, pNumSegs, pTriPatchInfo);

	return orig_vtable.DrawTriPatch(This, Handle, pNumSegs, pTriPatchInfo);

}

static HRESULT STDMETHODCALLTYPE DeletePatch(IDirect3DDevice9 * This, UINT Handle) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::DeletePatch()\n");

	if (device)
		return IDirect3DDevice9_DeletePatch(device, Handle);

	return orig_vtable.DeletePatch(This, Handle);

}

static HRESULT STDMETHODCALLTYPE CreateQuery(IDirect3DDevice9 * This, D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) {
	IDirect3DDevice9 *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDevice::CreateQuery()\n");

	if (device)
		return IDirect3DDevice9_CreateQuery(device, Type, ppQuery);

	return orig_vtable.CreateQuery(This, Type, ppQuery);

}
static HRESULT STDMETHODCALLTYPE CheckDeviceState(IDirect3DDevice9Ex * This, HWND hWindow) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::CheckDeviceState()\n");

	if (device)
		return IDirect3DDevice9Ex_CheckDeviceState(device, hWindow);

	return orig_vtable_ex.CheckDeviceState(This, hWindow);
}

static HRESULT STDMETHODCALLTYPE CheckResourceResidency(IDirect3DDevice9Ex * This, IDirect3DResource9 **pResourceArray, UINT32 NumResources) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::CheckResourceResidency()\n");

	if (device)
		return IDirect3DDevice9Ex_CheckResourceResidency(device, pResourceArray, NumResources);

	return orig_vtable_ex.CheckResourceResidency(This, pResourceArray, NumResources);
}
static HRESULT STDMETHODCALLTYPE ComposeRects(IDirect3DDevice9Ex * This, IDirect3DSurface9 *pSource, IDirect3DSurface9 *pDestination, IDirect3DVertexBuffer9 *pSrcRectDescriptors, UINT NumRects, IDirect3DVertexBuffer9 *pDstRectDescriptors, D3DCOMPOSERECTSOP Operation, INT XOffset, INT YOffset) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::ComposeRects()\n");

	if (device)
		return IDirect3DDevice9Ex_ComposeRects(device, pSource, pDestination, pSrcRectDescriptors, NumRects, pDstRectDescriptors, Operation, XOffset, YOffset);

	return orig_vtable_ex.ComposeRects(This, pSource, pDestination, pSrcRectDescriptors, NumRects, pDstRectDescriptors, Operation, XOffset, YOffset);
}
static HRESULT STDMETHODCALLTYPE CreateDepthStencilSurfaceEx(IDirect3DDevice9Ex * This, UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::CreateDepthStencilSurfaceEx()\n");

	if (device)
		return IDirect3DDevice9Ex_CreateDepthStencilSurfaceEx(device, Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle, Usage);

	return orig_vtable_ex.CreateDepthStencilSurfaceEx(This, Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle, Usage);
}
static HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurfaceEx(IDirect3DDevice9Ex * This, UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::CreateOffscreenPlainSurfaceEx()\n");

	if (device)
		return IDirect3DDevice9Ex_CreateOffscreenPlainSurfaceEx(device, Width, Height, Format, Pool, ppSurface, pSharedHandle, Usage);

	return orig_vtable_ex.CreateOffscreenPlainSurfaceEx(This, Width, Height, Format, Pool, ppSurface, pSharedHandle, Usage);
}

static HRESULT STDMETHODCALLTYPE CreateRenderTargetEx(IDirect3DDevice9Ex * This, UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::CreateRenderTargetEx ()\n");

	if (device)
		return IDirect3DDevice9Ex_CreateRenderTargetEx(device, Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle, Usage);

	return orig_vtable_ex.CreateRenderTargetEx(This, Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle, Usage);
}

static HRESULT STDMETHODCALLTYPE GetDisplayModeEx(IDirect3DDevice9Ex * This, UINT iSwapChain, D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::GetDisplayModeEx ()\n");

	if (device)
		return IDirect3DDevice9Ex_GetDisplayModeEx(device, iSwapChain, pMode, pRotation);

	return orig_vtable_ex.GetDisplayModeEx(This, iSwapChain, pMode, pRotation);
}

static HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(IDirect3DDevice9Ex * This, INT *pPriority) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::GetGPUThreadPriority ()\n");

	if (device)
		return IDirect3DDevice9Ex_GetGPUThreadPriority(device, pPriority);

	return orig_vtable_ex.GetGPUThreadPriority(This, pPriority);
}

static HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(IDirect3DDevice9Ex * This, UINT *pMaxLatency) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::GetMaximumFrameLatency ()\n");

	if (device)
		return IDirect3DDevice9Ex_GetMaximumFrameLatency(device, pMaxLatency);

	return orig_vtable_ex.GetMaximumFrameLatency(This, pMaxLatency);
}

static HRESULT STDMETHODCALLTYPE PresentEx(IDirect3DDevice9Ex * This, const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::PresentEx ()\n");

	if (device)
		return IDirect3DDevice9Ex_PresentEx(device, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);

	return orig_vtable_ex.PresentEx(This, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

static HRESULT STDMETHODCALLTYPE ResetEx(IDirect3DDevice9Ex * This, D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::ResetEx ()\n");

	if (device)
		return IDirect3DDevice9Ex_ResetEx(device, pPresentationParameters, pFullscreenDisplayMode);

	return orig_vtable_ex.ResetEx(This, pPresentationParameters, pFullscreenDisplayMode);
}

static HRESULT STDMETHODCALLTYPE SetConvolutionMonoKernel(IDirect3DDevice9Ex * This, UINT Width, UINT Height, float *RowWeights, float *ColumnWeights) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::SetConvolutionMonoKernel ()\n");

	if (device)
		return IDirect3DDevice9Ex_SetConvolutionMonoKernel(device, Width, Height, RowWeights, ColumnWeights);

	return orig_vtable_ex.SetConvolutionMonoKernel(This, Width, Height, RowWeights, ColumnWeights);
}

static HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(IDirect3DDevice9Ex * This, INT pPriority) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::SetGPUThreadPriority ()\n");

	if (device)
		return IDirect3DDevice9Ex_SetGPUThreadPriority(device, pPriority);

	return orig_vtable_ex.SetGPUThreadPriority(This, pPriority);
}

static HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(IDirect3DDevice9Ex * This, UINT pMaxLatency) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::SetMaximumFrameLatency ()\n");

	if (device)
		return IDirect3DDevice9Ex_SetMaximumFrameLatency(device, pMaxLatency);

	return orig_vtable_ex.SetMaximumFrameLatency(This, pMaxLatency);
}

static HRESULT STDMETHODCALLTYPE WaitForVBlank(IDirect3DDevice9Ex * This, UINT SwapChainIndex) {
	IDirect3DDevice9Ex *device = lookup_hooked_device_dx9(This);

	HookDebug("HookedDeviceEx::WaitForVBlank  ()\n");

	if (device)
		return IDirect3DDevice9Ex_WaitForVBlank(device, SwapChainIndex);

	return orig_vtable_ex.WaitForVBlank(This, SwapChainIndex);
}
static void install_hooks(IDirect3DDevice9 *device, EnableHooksDX9 enable_hooks)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSection(&device_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, device->lpVtbl, sizeof(struct IDirect3DDevice9ExVtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, device->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, device->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, device->lpVtbl->Release, Release);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.BeginScene, device->lpVtbl->BeginScene, BeginScene);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.BeginStateBlock, device->lpVtbl->BeginStateBlock, BeginStateBlock);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Clear, device->lpVtbl->Clear, Clear);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.ColorFill, device->lpVtbl->ColorFill, ColorFill);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateAdditionalSwapChain, device->lpVtbl->CreateAdditionalSwapChain, CreateAdditionalSwapChain);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateCubeTexture, device->lpVtbl->CreateCubeTexture, CreateCubeTexture);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateDepthStencilSurface, device->lpVtbl->CreateDepthStencilSurface, CreateDepthStencilSurface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateIndexBuffer, device->lpVtbl->CreateIndexBuffer, CreateIndexBuffer);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateOffscreenPlainSurface, device->lpVtbl->CreateOffscreenPlainSurface, CreateOffscreenPlainSurface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreatePixelShader, device->lpVtbl->CreatePixelShader, CreatePixelShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateQuery, device->lpVtbl->CreateQuery, CreateQuery);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateRenderTarget, device->lpVtbl->CreateRenderTarget, CreateRenderTarget);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateStateBlock, device->lpVtbl->CreateStateBlock, CreateStateBlock);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateTexture, device->lpVtbl->CreateTexture, CreateTexture);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateVertexBuffer, device->lpVtbl->CreateVertexBuffer, CreateVertexBuffer);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateVertexDeclaration, device->lpVtbl->CreateVertexDeclaration, CreateVertexDeclaration);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateVertexShader, device->lpVtbl->CreateVertexShader, CreateVertexShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateVolumeTexture, device->lpVtbl->CreateVolumeTexture, CreateVolumeTexture);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DeletePatch, device->lpVtbl->DeletePatch, DeletePatch);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DrawIndexedPrimitive, device->lpVtbl->DrawIndexedPrimitive, DrawIndexedPrimitive);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DrawIndexedPrimitiveUP, device->lpVtbl->DrawIndexedPrimitiveUP, DrawIndexedPrimitiveUP);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DrawPrimitive, device->lpVtbl->DrawPrimitive, DrawPrimitive);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DrawPrimitiveUP, device->lpVtbl->DrawPrimitiveUP, DrawPrimitiveUP);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DrawRectPatch, device->lpVtbl->DrawRectPatch, DrawRectPatch);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.DrawTriPatch, device->lpVtbl->DrawTriPatch, DrawTriPatch);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.EndScene, device->lpVtbl->EndScene, EndScene);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.EndStateBlock, device->lpVtbl->EndStateBlock, EndStateBlock);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.EvictManagedResources, device->lpVtbl->EvictManagedResources, EvictManagedResources);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetAvailableTextureMem, device->lpVtbl->GetAvailableTextureMem, GetAvailableTextureMem);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetBackBuffer, device->lpVtbl->GetBackBuffer, GetBackBuffer);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetClipPlane, device->lpVtbl->GetClipPlane, GetClipPlane);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetClipStatus, device->lpVtbl->GetClipStatus, GetClipStatus);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetCreationParameters, device->lpVtbl->GetCreationParameters, GetCreationParameters);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetCurrentTexturePalette, device->lpVtbl->GetCurrentTexturePalette, GetCurrentTexturePalette);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDepthStencilSurface, device->lpVtbl->GetDepthStencilSurface, GetDepthStencilSurface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDeviceCaps, device->lpVtbl->GetDeviceCaps, _GetDeviceCaps);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDirect3D, device->lpVtbl->GetDirect3D, GetDirect3D);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDisplayMode, device->lpVtbl->GetDisplayMode, GetDisplayMode);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetFrontBufferData, device->lpVtbl->GetFrontBufferData, GetFrontBufferData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetFVF, device->lpVtbl->GetFVF, GetFVF);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetGammaRamp, device->lpVtbl->GetGammaRamp, GetGammaRamp);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetIndices, device->lpVtbl->GetIndices, GetIndices);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetLight, device->lpVtbl->GetLight, GetLight);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetLightEnable, device->lpVtbl->GetLightEnable, GetLightEnable);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetMaterial, device->lpVtbl->GetMaterial, GetMaterial);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetNPatchMode, device->lpVtbl->GetNPatchMode, GetNPatchMode);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetNumberOfSwapChains, device->lpVtbl->GetNumberOfSwapChains, GetNumberOfSwapChains);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPaletteEntries, device->lpVtbl->GetPaletteEntries, _GetPaletteEntries);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPixelShader, device->lpVtbl->GetPixelShader, GetPixelShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPixelShaderConstantB, device->lpVtbl->GetPixelShaderConstantB, GetPixelShaderConstantB);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPixelShaderConstantF, device->lpVtbl->GetPixelShaderConstantF, GetPixelShaderConstantF);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPixelShaderConstantI, device->lpVtbl->GetPixelShaderConstantI, GetPixelShaderConstantI);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetRasterStatus, device->lpVtbl->GetRasterStatus, GetRasterStatus);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetRenderState, device->lpVtbl->GetRenderState, GetRenderState);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetRenderTarget, device->lpVtbl->GetRenderTarget, GetRenderTarget);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetRenderTargetData, device->lpVtbl->GetRenderTargetData, GetRenderTargetData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetSamplerState, device->lpVtbl->GetSamplerState, GetSamplerState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetScissorRect, device->lpVtbl->GetScissorRect, GetScissorRect);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetSoftwareVertexProcessing, device->lpVtbl->GetSoftwareVertexProcessing, GetSoftwareVertexProcessing);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetStreamSource, device->lpVtbl->GetStreamSource, GetStreamSource);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetStreamSourceFreq, device->lpVtbl->GetStreamSourceFreq, GetStreamSourceFreq);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetSwapChain, device->lpVtbl->GetSwapChain, GetSwapChain);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetTexture, device->lpVtbl->GetTexture, GetTexture);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetTextureStageState, device->lpVtbl->GetTextureStageState, GetTextureStageState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetTransform, device->lpVtbl->GetTransform, GetTransform);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetVertexDeclaration, device->lpVtbl->GetVertexDeclaration, GetVertexDeclaration);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetVertexShader, device->lpVtbl->GetVertexShader, GetVertexShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetVertexShaderConstantB, device->lpVtbl->GetVertexShaderConstantB, GetVertexShaderConstantB);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetVertexShaderConstantF, device->lpVtbl->GetVertexShaderConstantF, GetVertexShaderConstantF);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetVertexShaderConstantI, device->lpVtbl->GetVertexShaderConstantI, GetVertexShaderConstantI);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetViewport, device->lpVtbl->GetViewport, GetViewport);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.LightEnable, device->lpVtbl->LightEnable, LightEnable);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.MultiplyTransform, device->lpVtbl->MultiplyTransform, MultiplyTransform);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Present, device->lpVtbl->Present, Present);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.ProcessVertices, device->lpVtbl->ProcessVertices, ProcessVertices);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Reset, device->lpVtbl->Reset, Reset);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetClipPlane, device->lpVtbl->SetClipPlane, SetClipPlane);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetClipStatus, device->lpVtbl->SetClipStatus, SetClipStatus);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetCurrentTexturePalette, device->lpVtbl->SetCurrentTexturePalette, SetCurrentTexturePalette);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetCursorPosition, device->lpVtbl->SetCursorPosition, SetCursorPosition);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetCursorProperties, device->lpVtbl->SetCursorProperties, SetCursorProperties);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetDepthStencilSurface, device->lpVtbl->SetDepthStencilSurface, SetDepthStencilSurface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetDialogBoxMode, device->lpVtbl->SetDialogBoxMode, SetDialogBoxMode);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetFVF, device->lpVtbl->SetFVF, SetFVF);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetGammaRamp, device->lpVtbl->SetGammaRamp, SetGammaRamp);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetIndices, device->lpVtbl->SetIndices, SetIndices);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetLight, device->lpVtbl->SetLight, SetLight);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetMaterial, device->lpVtbl->SetMaterial, SetMaterial);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetNPatchMode, device->lpVtbl->SetNPatchMode, SetNPatchMode);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPaletteEntries, device->lpVtbl->SetPaletteEntries, _SetPaletteEntries);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPixelShader, device->lpVtbl->SetPixelShader, SetPixelShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetRenderState, device->lpVtbl->SetRenderState, SetRenderState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetRenderTarget, device->lpVtbl->SetRenderTarget, SetRenderTarget);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetSamplerState, device->lpVtbl->SetSamplerState, SetSamplerState);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPixelShaderConstantB, device->lpVtbl->SetPixelShaderConstantB, SetPixelShaderConstantB);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPixelShaderConstantF, device->lpVtbl->SetPixelShaderConstantF, SetPixelShaderConstantF);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPixelShaderConstantI, device->lpVtbl->SetPixelShaderConstantI, SetPixelShaderConstantI);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetVertexShaderConstantB, device->lpVtbl->SetVertexShaderConstantB, SetVertexShaderConstantB);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetVertexShaderConstantF, device->lpVtbl->SetVertexShaderConstantF, SetVertexShaderConstantF);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetVertexShaderConstantI, device->lpVtbl->SetVertexShaderConstantI, SetVertexShaderConstantI);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetTexture, device->lpVtbl->SetTexture, SetTexture);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetScissorRect, device->lpVtbl->SetScissorRect, SetScissorRect);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetSoftwareVertexProcessing, device->lpVtbl->SetSoftwareVertexProcessing, SetSoftwareVertexProcessing);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetStreamSource, device->lpVtbl->SetStreamSource, SetStreamSource);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetStreamSourceFreq, device->lpVtbl->SetStreamSourceFreq, SetStreamSourceFreq);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetTextureStageState, device->lpVtbl->SetTextureStageState, SetTextureStageState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetTransform, device->lpVtbl->SetTransform, SetTransform);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetVertexDeclaration, device->lpVtbl->SetVertexDeclaration, SetVertexDeclaration);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetVertexShader, device->lpVtbl->SetVertexShader, SetVertexShader);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetViewport, device->lpVtbl->SetViewport, SetViewport);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.ShowCursor, device->lpVtbl->ShowCursor, _ShowCursor);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.StretchRect, device->lpVtbl->StretchRect, StretchRect);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.TestCooperativeLevel, device->lpVtbl->TestCooperativeLevel, TestCooperativeLevel);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.UpdateSurface, device->lpVtbl->UpdateSurface, UpdateSurface);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.UpdateTexture, device->lpVtbl->UpdateTexture, UpdateTexture);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.ValidateDevice, device->lpVtbl->ValidateDevice, ValidateDevice);

}
static void install_hooks(IDirect3DDevice9Ex *device, EnableHooksDX9 enable_hooks)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;

	install_hooks((IDirect3DDevice9*)device, enable_hooks);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.CheckDeviceState, device->lpVtbl->CheckDeviceState, CheckDeviceState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.CheckResourceResidency, device->lpVtbl->CheckResourceResidency, CheckResourceResidency);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.ComposeRects, device->lpVtbl->ComposeRects, ComposeRects);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.CreateDepthStencilSurfaceEx, device->lpVtbl->CreateDepthStencilSurfaceEx, CreateDepthStencilSurfaceEx);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.CreateOffscreenPlainSurfaceEx, device->lpVtbl->CreateOffscreenPlainSurfaceEx, CreateOffscreenPlainSurfaceEx);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.CreateRenderTargetEx, device->lpVtbl->CreateRenderTargetEx, CreateRenderTargetEx);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.GetDisplayModeEx, device->lpVtbl->GetDisplayModeEx, GetDisplayModeEx);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.GetGPUThreadPriority, device->lpVtbl->GetGPUThreadPriority, GetGPUThreadPriority);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.GetMaximumFrameLatency, device->lpVtbl->GetMaximumFrameLatency, GetMaximumFrameLatency);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.PresentEx, device->lpVtbl->PresentEx, PresentEx);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.ResetEx, device->lpVtbl->ResetEx, ResetEx);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.SetConvolutionMonoKernel, device->lpVtbl->SetConvolutionMonoKernel, SetConvolutionMonoKernel);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.SetGPUThreadPriority, device->lpVtbl->SetGPUThreadPriority, SetGPUThreadPriority);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.SetMaximumFrameLatency, device->lpVtbl->SetMaximumFrameLatency, SetMaximumFrameLatency);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.WaitForVBlank, device->lpVtbl->WaitForVBlank, WaitForVBlank);
}

typedef struct IDirect3DDevice9Trampoline {
	CONST_VTBL struct IDirect3DDevice9Vtbl *lpVtbl;
	IDirect3DDevice9 *orig_this;
} IDirect3DDevice9Trampoline;

typedef struct IDirect3DDevice9ExTrampoline {
	CONST_VTBL struct IDirect3DDevice9ExVtbl *lpVtbl;
	IDirect3DDevice9Ex *orig_this;
} IDirect3DDevice9ExTrampoline;


// IUnknown
template <typename Device>
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	Device * This,
	 REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineDevice:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3DDevice9Trampoline*)This)->orig_this, riid, ppvObject);
}
template <typename Device>
static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	Device * This)
{
	HookDebug("TrampolineDevice:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3DDevice9Trampoline*)This)->orig_this);
}
template <typename Device>
 static ULONG STDMETHODCALLTYPE TrampolineRelease(
	 Device * This)
{
	HookDebug("TrampolineDevice:: Release()\n");

	return orig_vtable.Release(((IDirect3DDevice9Trampoline*)This)->orig_this);
}
//IDirect3DDevice9
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineTestCooperativeLevel(Device * This) {

	HookDebug("TrampolineDevice:: TestCooperativeLevel()\n");

	return orig_vtable.TestCooperativeLevel(((IDirect3DDevice9Trampoline*)This)->orig_this);

}
 template <typename Device>
 static UINT STDMETHODCALLTYPE TrampolineGetAvailableTextureMem(Device * This) {
	HookDebug("TrampolineDevice:: TrampolineGetAvailableTextureMem()\n");

	return orig_vtable.GetAvailableTextureMem(((IDirect3DDevice9Trampoline*)This)->orig_this);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineEvictManagedResources(Device * This) {

	HookDebug("TrampolineDevice:: EvictManagedResources()\n");

	return orig_vtable.EvictManagedResources(((IDirect3DDevice9Trampoline*)This)->orig_this);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetDirect3D(Device * This, IDirect3D9** ppD3D9) {

	HookDebug("TrampolineDevice:: GetDirect3D()\n");

	return orig_vtable.GetDirect3D(((IDirect3DDevice9Trampoline*)This)->orig_this, ppD3D9);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetDeviceCaps(Device * This, D3DCAPS9* pCaps) {

	HookDebug("TrampolineDevice:: GetDeviceCaps()\n");

	return orig_vtable.GetDeviceCaps(((IDirect3DDevice9Trampoline*)This)->orig_this, pCaps);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetDisplayMode(Device * This, UINT iSwapChain, D3DDISPLAYMODE* pMode) {

	HookDebug("TrampolineDevice:: GetDisplayMode()\n");

	return orig_vtable.GetDisplayMode(((IDirect3DDevice9Trampoline*)This)->orig_this, iSwapChain, pMode);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetCreationParameters(Device * This, D3DDEVICE_CREATION_PARAMETERS *pParameters) {

	HookDebug("TrampolineDevice:: GetCreationParameters()\n");

	return orig_vtable.GetCreationParameters(((IDirect3DDevice9Trampoline*)This)->orig_this, pParameters);




}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetCursorProperties(Device * This, UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) {

	HookDebug("TrampolineDevice:: SetCursorProperties()\n");

	return orig_vtable.SetCursorProperties(((IDirect3DDevice9Trampoline*)This)->orig_this, XHotSpot, YHotSpot, pCursorBitmap);




}
 template <typename Device>
 static void STDMETHODCALLTYPE TrampolineSetCursorPosition(Device * This, int X, int Y, DWORD Flags) {

	HookDebug("TrampolineDevice:: SetCursorPosition()\n");

	return orig_vtable.SetCursorPosition(((IDirect3DDevice9Trampoline*)This)->orig_this, X, Y, Flags);

}
 template <typename Device>
 static BOOL STDMETHODCALLTYPE TrampolineShowCursor(Device * This, BOOL bShow) {

	HookDebug("TrampolineDevice:: ShowCursor()\n");

	return orig_vtable.ShowCursor(((IDirect3DDevice9Trampoline*)This)->orig_this, bShow);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineCreateAdditionalSwapChain(Device * This, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain) {
	HookDebug("TrampolineDevice:: CreateAdditionalSwapChain()\n");

	return orig_vtable.CreateAdditionalSwapChain(((IDirect3DDevice9Trampoline*)This)->orig_this, pPresentationParameters, pSwapChain);
}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetSwapChain(Device * This, UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) {

	HookDebug("TrampolineDevice:: GetSwapChain()\n");

	return orig_vtable.GetSwapChain(((IDirect3DDevice9Trampoline*)This)->orig_this, iSwapChain, pSwapChain);


}
 template <typename Device>
 static UINT STDMETHODCALLTYPE TrampolineGetNumberOfSwapChains(Device * This) {
	HookDebug("TrampolineDevice:: GetNumberOfSwapChains()\n");

	return orig_vtable.GetNumberOfSwapChains(((IDirect3DDevice9Trampoline*)This)->orig_this);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineReset(Device * This, D3DPRESENT_PARAMETERS* pPresentationParameters) {

	HookDebug("TrampolineDevice:: Reset()\n");

	return orig_vtable.Reset(((IDirect3DDevice9Trampoline*)This)->orig_this, pPresentationParameters);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolinePresent(Device * This, const RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion) {

	HookDebug("TrampolineDevice:: Present()\n");

	return orig_vtable.Present(((IDirect3DDevice9Trampoline*)This)->orig_this, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetBackBuffer(Device * This, UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer) {

	HookDebug("TrampolineDevice:: GetBackBuffer()\n");

	return orig_vtable.GetBackBuffer(((IDirect3DDevice9Trampoline*)This)->orig_this, iSwapChain, iBackBuffer, Type, ppBackBuffer);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetRasterStatus(Device * This, UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) {
	HookDebug("TrampolineDevice:: GetRasterStatus()\n");

	return orig_vtable.GetRasterStatus(((IDirect3DDevice9Trampoline*)This)->orig_this, iSwapChain, pRasterStatus);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetDialogBoxMode(Device * This, BOOL bEnableDialogs) {
	HookDebug("TrampolineDevice:: SetDialogBoxMode()\n");

	return orig_vtable.SetDialogBoxMode(((IDirect3DDevice9Trampoline*)This)->orig_this, bEnableDialogs);



}
 template <typename Device>
 static void STDMETHODCALLTYPE TrampolineSetGammaRamp(Device * This, UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP* pRamp) {
	HookDebug("TrampolineDevice:: SetGammaRamp()\n");

	return orig_vtable.SetGammaRamp(((IDirect3DDevice9Trampoline*)This)->orig_this, iSwapChain, Flags, pRamp);



}
 template <typename Device>
 static void STDMETHODCALLTYPE TrampolineGetGammaRamp(Device * This, UINT iSwapChain, D3DGAMMARAMP* pRamp) {

	HookDebug("TrampolineDevice:: GetGammaRamp()\n");

	return orig_vtable.GetGammaRamp(((IDirect3DDevice9Trampoline*)This)->orig_this, iSwapChain, pRamp);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineCreateTexture(Device * This, UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) {

	HookDebug("TrampolineDevice:: CreateTexture()\n");

	return orig_vtable.CreateTexture(((IDirect3DDevice9Trampoline*)This)->orig_this, Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineCreateVolumeTexture(Device * This, UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) {

	HookDebug("TrampolineDevice:: CreateVolumeTexture()\n");

	return orig_vtable.CreateVolumeTexture(((IDirect3DDevice9Trampoline*)This)->orig_this, Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineCreateCubeTexture(Device * This, UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) {

	HookDebug("TrampolineDevice:: CreateCubeTexture()\n");

	return orig_vtable.CreateCubeTexture(((IDirect3DDevice9Trampoline*)This)->orig_this, EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineCreateVertexBuffer(Device * This, UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) {

	HookDebug("TrampolineDevice:: CreateVertexBuffer()\n");

	return orig_vtable.CreateVertexBuffer(((IDirect3DDevice9Trampoline*)This)->orig_this, Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineCreateIndexBuffer(Device * This, UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) {


	HookDebug("TrampolineDevice:: CreateIndexBuffer()\n");

	return orig_vtable.CreateIndexBuffer(((IDirect3DDevice9Trampoline*)This)->orig_this, Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineCreateRenderTarget(Device * This, UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {

	HookDebug("TrampolineDevice:: CreateRenderTarget()\n");

	return orig_vtable.CreateRenderTarget(((IDirect3DDevice9Trampoline*)This)->orig_this, Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineCreateDepthStencilSurface(Device * This, UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {

	HookDebug("TrampolineDevice:: CreateDepthStencilSurface()\n");

	return orig_vtable.CreateDepthStencilSurface(((IDirect3DDevice9Trampoline*)This)->orig_this, Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineUpdateSurface(Device * This, IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, CONST POINT* pDestPoint) {

	HookDebug("TrampolineDevice:: UpdateSurface()\n");

	return orig_vtable.UpdateSurface(((IDirect3DDevice9Trampoline*)This)->orig_this, pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineUpdateTexture(Device * This, IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) {

	HookDebug("TrampolineDevice:: UpdateTexture()\n");

	return orig_vtable.UpdateTexture(((IDirect3DDevice9Trampoline*)This)->orig_this, pSourceTexture, pDestinationTexture);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetRenderTargetData(Device * This, IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) {

	HookDebug("TrampolineDevice:: GetRenderTargetData()\n");

	return orig_vtable.GetRenderTargetData(((IDirect3DDevice9Trampoline*)This)->orig_this, pRenderTarget, pDestSurface);




}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetFrontBufferData(Device * This, UINT iSwapChain, IDirect3DSurface9* pDestSurface) {

	HookDebug("TrampolineDevice:: GetFrontBufferData()\n");

	return orig_vtable.GetFrontBufferData(((IDirect3DDevice9Trampoline*)This)->orig_this, iSwapChain, pDestSurface);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineStretchRect(Device * This, IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter) {

	HookDebug("TrampolineDevice:: StretchRect()\n");

	return orig_vtable.StretchRect(((IDirect3DDevice9Trampoline*)This)->orig_this, pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineColorFill(Device * This, IDirect3DSurface9* pSurface, CONST RECT* pRect, D3DCOLOR color) {


	HookDebug("TrampolineDevice:: ColorFill()\n");

	return orig_vtable.ColorFill(((IDirect3DDevice9Trampoline*)This)->orig_this, pSurface, pRect, color);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineCreateOffscreenPlainSurface(Device * This, UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {

	HookDebug("TrampolineDevice:: CreateOffscreenPlainSurface()\n");

	return orig_vtable.CreateOffscreenPlainSurface(((IDirect3DDevice9Trampoline*)This)->orig_this, Width, Height, Format, Pool, ppSurface, pSharedHandle);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetRenderTarget(Device * This, DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) {
	HookDebug("TrampolineDevice:: SetRenderTarget()\n");

	return orig_vtable.SetRenderTarget(((IDirect3DDevice9Trampoline*)This)->orig_this, RenderTargetIndex, pRenderTarget);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetRenderTarget(Device * This, DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) {
	HookDebug("TrampolineDevice:: GetRenderTarget()\n");

	return orig_vtable.GetRenderTarget(((IDirect3DDevice9Trampoline*)This)->orig_this, RenderTargetIndex, ppRenderTarget);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetDepthStencilSurface(Device * This, IDirect3DSurface9* pNewZStencil) {

	HookDebug("TrampolineDevice:: SetDepthStencilSurface()\n");

	return orig_vtable.SetDepthStencilSurface(((IDirect3DDevice9Trampoline*)This)->orig_this, pNewZStencil);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetDepthStencilSurface(Device * This, IDirect3DSurface9** ppZStencilSurface) {

	HookDebug("TrampolineDevice:: GetDepthStencilSurface()\n");

	return orig_vtable.GetDepthStencilSurface(((IDirect3DDevice9Trampoline*)This)->orig_this, ppZStencilSurface);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineBeginScene(Device * This) {
	HookDebug("TrampolineDevice:: BeginScene()\n");

	return orig_vtable.BeginScene(((IDirect3DDevice9Trampoline*)This)->orig_this);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineEndScene(Device * This) {
	HookDebug("TrampolineDevice:: EndScene()\n");

	return orig_vtable.EndScene(((IDirect3DDevice9Trampoline*)This)->orig_this);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineClear(Device * This, DWORD Count, CONST D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) {
	HookDebug("TrampolineDevice:: Clear()\n");

	return orig_vtable.Clear(((IDirect3DDevice9Trampoline*)This)->orig_this, Count, pRects, Flags, Color, Z, Stencil);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetTransform(Device * This, D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix) {
	HookDebug("TrampolineDevice:: SetTransform()\n");

	return orig_vtable.SetTransform(((IDirect3DDevice9Trampoline*)This)->orig_this, State, pMatrix);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetTransform(Device * This, D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) {

	HookDebug("TrampolineDevice:: GetTransform()\n");

	return orig_vtable.GetTransform(((IDirect3DDevice9Trampoline*)This)->orig_this, State, pMatrix);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineMultiplyTransform(Device * This, D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix) {

	HookDebug("TrampolineDevice:: MultiplyTransform()\n");

	return orig_vtable.MultiplyTransform(((IDirect3DDevice9Trampoline*)This)->orig_this, State, pMatrix);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetViewport(Device * This, CONST D3DVIEWPORT9* pViewport) {
	HookDebug("TrampolineDevice:: SetViewport()\n");

	return orig_vtable.SetViewport(((IDirect3DDevice9Trampoline*)This)->orig_this, pViewport);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetViewport(Device * This, D3DVIEWPORT9* pViewport) {
	HookDebug("TrampolineDevice:: GetViewport()\n");

	return orig_vtable.GetViewport(((IDirect3DDevice9Trampoline*)This)->orig_this, pViewport);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetMaterial(Device * This, CONST D3DMATERIAL9* pMaterial) {
	HookDebug("TrampolineDevice:: SetMaterial()\n");

	return orig_vtable.SetMaterial(((IDirect3DDevice9Trampoline*)This)->orig_this, pMaterial);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetMaterial(Device * This, D3DMATERIAL9* pMaterial) {
	HookDebug("TrampolineDevice:: GetMaterial()\n");

	return orig_vtable.GetMaterial(((IDirect3DDevice9Trampoline*)This)->orig_this, pMaterial);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetLight(Device * This, DWORD Index, CONST D3DLIGHT9 *pLight) {
	HookDebug("TrampolineDevice:: SetLight()\n");

	return orig_vtable.SetLight(((IDirect3DDevice9Trampoline*)This)->orig_this, Index, pLight);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetLight(Device * This, DWORD Index, D3DLIGHT9 *pLight) {
	HookDebug("TrampolineDevice:: GetLight()\n");

	return orig_vtable.GetLight(((IDirect3DDevice9Trampoline*)This)->orig_this, Index, pLight);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineLightEnable(Device * This, DWORD Index, BOOL Enable) {
	HookDebug("TrampolineDevice:: LightEnable()\n");

	return orig_vtable.LightEnable(((IDirect3DDevice9Trampoline*)This)->orig_this, Index, Enable);

}

 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetLightEnable(Device * This, DWORD Index, BOOL* pEnable) {
	HookDebug("TrampolineDevice:: GetLightEnable()\n");

	return orig_vtable.GetLightEnable(((IDirect3DDevice9Trampoline*)This)->orig_this, Index, pEnable);

}

 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetClipPlane(Device * This, DWORD Index, CONST float* pPlane) {
	HookDebug("TrampolineDevice:: SetClipPlane()\n");

	return orig_vtable.SetClipPlane(((IDirect3DDevice9Trampoline*)This)->orig_this, Index, pPlane);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetClipPlane(Device * This, DWORD Index, float* pPlane) {
	HookDebug("TrampolineDevice:: GetClipPlane()\n");

	return orig_vtable.GetClipPlane(((IDirect3DDevice9Trampoline*)This)->orig_this, Index, pPlane);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetRenderState(Device * This, D3DRENDERSTATETYPE State, DWORD Value) {
	HookDebug("TrampolineDevice:: SetRenderState()\n");

	return orig_vtable.SetRenderState(((IDirect3DDevice9Trampoline*)This)->orig_this, State, Value);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetRenderState(Device * This, D3DRENDERSTATETYPE State, DWORD* pValue) {
	HookDebug("TrampolineDevice:: GetRenderState()\n");

	return orig_vtable.GetRenderState(((IDirect3DDevice9Trampoline*)This)->orig_this, State, pValue);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineCreateStateBlock(Device * This, D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) {
	HookDebug("TrampolineDevice:: CreateStateBlock()\n");

	return orig_vtable.CreateStateBlock(((IDirect3DDevice9Trampoline*)This)->orig_this, Type, ppSB);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineBeginStateBlock(Device * This) {
	HookDebug("TrampolineDevice:: BeginStateBlock()\n");

	return orig_vtable.BeginStateBlock(((IDirect3DDevice9Trampoline*)This)->orig_this);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineEndStateBlock(Device * This, IDirect3DStateBlock9** ppSB) {
	HookDebug("TrampolineDevice:: EndStateBlock()\n");

	return orig_vtable.EndStateBlock(((IDirect3DDevice9Trampoline*)This)->orig_this, ppSB);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetClipStatus(Device * This, CONST D3DCLIPSTATUS9* pClipStatus) {
	HookDebug("TrampolineDevice:: SetClipStatus()\n");

	return orig_vtable.SetClipStatus(((IDirect3DDevice9Trampoline*)This)->orig_this, pClipStatus);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetClipStatus(Device * This, D3DCLIPSTATUS9* pClipStatus) {
	HookDebug("TrampolineDevice:: GetClipStatus()\n");

	return orig_vtable.GetClipStatus(((IDirect3DDevice9Trampoline*)This)->orig_this, pClipStatus);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetTexture(Device * This, DWORD Stage, IDirect3DBaseTexture9** ppTexture) {
	HookDebug("TrampolineDevice:: GetTexture()\n");

	return orig_vtable.GetTexture(((IDirect3DDevice9Trampoline*)This)->orig_this, Stage, ppTexture);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetTexture(Device * This, DWORD Stage, IDirect3DBaseTexture9* ppTexture) {
	HookDebug("TrampolineDevice:: SetTexture()\n");

	return orig_vtable.SetTexture(((IDirect3DDevice9Trampoline*)This)->orig_this, Stage, ppTexture);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetTextureStageState(Device * This, DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) {
	HookDebug("TrampolineDevice:: GetTextureStageState()\n");

	return orig_vtable.GetTextureStageState(((IDirect3DDevice9Trampoline*)This)->orig_this, Stage, Type, pValue);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetTextureStageState(Device * This, DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) {
	HookDebug("TrampolineDevice:: SetTextureStageState()\n");

	return orig_vtable.SetTextureStageState(((IDirect3DDevice9Trampoline*)This)->orig_this, Stage, Type, Value);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetSamplerState(Device * This, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) {
	HookDebug("TrampolineDevice:: GetSamplerState()\n");

	return orig_vtable.GetSamplerState(((IDirect3DDevice9Trampoline*)This)->orig_this, Sampler, Type, pValue);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetSamplerState(Device * This, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) {
	HookDebug("TrampolineDevice:: SetSamplerState()\n");

	return orig_vtable.SetSamplerState(((IDirect3DDevice9Trampoline*)This)->orig_this, Sampler, Type, Value);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineValidateDevice(Device * This, DWORD* pNumPasses) {
	HookDebug("TrampolineDevice:: ValidateDevice()\n");

	return orig_vtable.ValidateDevice(((IDirect3DDevice9Trampoline*)This)->orig_this, pNumPasses);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetPaletteEntries(Device * This, UINT PaletteNumber, CONST PALETTEENTRY* pEntries) {
	HookDebug("TrampolineDevice:: SetPaletteEntries()\n");

	return orig_vtable.SetPaletteEntries(((IDirect3DDevice9Trampoline*)This)->orig_this, PaletteNumber, pEntries);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetPaletteEntries(Device * This, UINT PaletteNumber, PALETTEENTRY* pEntries) {
	HookDebug("TrampolineDevice:: GetPaletteEntries()\n");

	return orig_vtable.GetPaletteEntries(((IDirect3DDevice9Trampoline*)This)->orig_this, PaletteNumber, pEntries);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetCurrentTexturePalette(Device * This, UINT PaletteNumber) {
	HookDebug("TrampolineDevice:: SetCurrentTexturePalette()\n");

	return orig_vtable.SetCurrentTexturePalette(((IDirect3DDevice9Trampoline*)This)->orig_this, PaletteNumber);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetCurrentTexturePalette(Device * This, UINT *PaletteNumber) {
	HookDebug("TrampolineDevice:: GetCurrentTexturePalette()\n");

	return orig_vtable.GetCurrentTexturePalette(((IDirect3DDevice9Trampoline*)This)->orig_this, PaletteNumber);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetScissorRect(Device * This, CONST RECT* pRect) {
	HookDebug("TrampolineDevice:: SetScissorRect()\n");

	return orig_vtable.SetScissorRect(((IDirect3DDevice9Trampoline*)This)->orig_this, pRect);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetScissorRect(Device * This, RECT* pRect) {
	HookDebug("TrampolineDevice:: GetScissorRect()\n");

	return orig_vtable.GetScissorRect(((IDirect3DDevice9Trampoline*)This)->orig_this, pRect);
}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetSoftwareVertexProcessing(Device * This, BOOL bSoftware) {
	HookDebug("TrampolineDevice:: SetSoftwareVertexProcessing()\n");

	return orig_vtable.SetSoftwareVertexProcessing(((IDirect3DDevice9Trampoline*)This)->orig_this, bSoftware);
}
 template <typename Device>
 static BOOL STDMETHODCALLTYPE TrampolineGetSoftwareVertexProcessing(Device * This) {
	HookDebug("TrampolineDevice:: GetSoftwareVertexProcessing()\n");

	return orig_vtable.GetSoftwareVertexProcessing(((IDirect3DDevice9Trampoline*)This)->orig_this);
}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetNPatchMode(Device * This, float nSegments) {
	HookDebug("TrampolineDevice:: SetNPatchMode()\n");

	return orig_vtable.SetNPatchMode(((IDirect3DDevice9Trampoline*)This)->orig_this, nSegments);

}
 template <typename Device>
 static float STDMETHODCALLTYPE TrampolineGetNPatchMode(Device * This) {
	HookDebug("TrampolineDevice:: GetNPatchMode()\n");

	return orig_vtable.GetNPatchMode(((IDirect3DDevice9Trampoline*)This)->orig_this);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineDrawPrimitive(Device * This, D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) {
	HookDebug("TrampolineDevice:: DrawPrimitive()\n");

	return orig_vtable.DrawPrimitive(((IDirect3DDevice9Trampoline*)This)->orig_this, PrimitiveType, StartVertex, PrimitiveCount);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineDrawIndexedPrimitive(Device * This, D3DPRIMITIVETYPE Type, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) {
	HookDebug("TrampolineDevice:: DrawIndexedPrimitive()\n");

	return orig_vtable.DrawIndexedPrimitive(((IDirect3DDevice9Trampoline*)This)->orig_this, Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineDrawPrimitiveUP(Device * This, D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
	HookDebug("TrampolineDevice:: DrawPrimitiveUP()\n");

	return orig_vtable.DrawPrimitiveUP(((IDirect3DDevice9Trampoline*)This)->orig_this, PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineDrawIndexedPrimitiveUP(Device * This, D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
	HookDebug("TrampolineDevice:: DrawIndexedPrimitiveUP()\n");

	return orig_vtable.DrawIndexedPrimitiveUP(((IDirect3DDevice9Trampoline*)This)->orig_this, PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineProcessVertices(Device * This, UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) {
	HookDebug("TrampolineDevice:: ProcessVertices()\n");

	return orig_vtable.ProcessVertices(((IDirect3DDevice9Trampoline*)This)->orig_this, SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineCreateVertexDeclaration(Device * This, CONST D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl) {
	HookDebug("TrampolineDevice:: CreateVertexDeclaration()\n");

	return orig_vtable.CreateVertexDeclaration(((IDirect3DDevice9Trampoline*)This)->orig_this, pVertexElements, ppDecl);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetVertexDeclaration(Device * This, IDirect3DVertexDeclaration9* pDecl) {
	HookDebug("TrampolineDevice:: SetVertexDeclaration()\n");

	return orig_vtable.SetVertexDeclaration(((IDirect3DDevice9Trampoline*)This)->orig_this, pDecl);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetVertexDeclaration(Device * This, IDirect3DVertexDeclaration9** ppDecl) {
	HookDebug("TrampolineDevice:: GetVertexDeclaration()\n");

	return orig_vtable.GetVertexDeclaration(((IDirect3DDevice9Trampoline*)This)->orig_this, ppDecl);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetFVF(Device * This, DWORD FVF) {
	HookDebug("TrampolineDevice:: SetFVF()\n");

	return orig_vtable.SetFVF(((IDirect3DDevice9Trampoline*)This)->orig_this, FVF);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetFVF(Device * This, DWORD* pFVF) {
	HookDebug("TrampolineDevice:: GetFVF()\n");

	return orig_vtable.GetFVF(((IDirect3DDevice9Trampoline*)This)->orig_this, pFVF);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineCreateVertexShader(Device * This, CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader) {
	HookDebug("TrampolineDevice:: CreateVertexShader()\n");

	return orig_vtable.CreateVertexShader(((IDirect3DDevice9Trampoline*)This)->orig_this, pFunction, ppShader);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetVertexShader(Device * This, IDirect3DVertexShader9* pShader) {
	HookDebug("TrampolineDevice:: SetVertexShader()\n");

	return orig_vtable.SetVertexShader(((IDirect3DDevice9Trampoline*)This)->orig_this, pShader);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetVertexShader(Device * This, IDirect3DVertexShader9** ppShader) {
	HookDebug("TrampolineDevice:: GetVertexShader()\n");

	return orig_vtable.GetVertexShader(((IDirect3DDevice9Trampoline*)This)->orig_this, ppShader);



}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetVertexShaderConstantF(Device * This, UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) {
	HookDebug("TrampolineDevice:: SetVertexShaderConstantF()\n");

	return orig_vtable.SetVertexShaderConstantF(((IDirect3DDevice9Trampoline*)This)->orig_this, StartRegister, pConstantData, Vector4fCount);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetVertexShaderConstantF(Device * This, UINT StartRegister, float* pConstantData, UINT Vector4fCount) {
	HookDebug("TrampolineDevice:: GetVertexShaderConstantF()\n");

	return orig_vtable.GetVertexShaderConstantF(((IDirect3DDevice9Trampoline*)This)->orig_this, StartRegister, pConstantData, Vector4fCount);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetVertexShaderConstantI(Device * This, UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) {
	HookDebug("TrampolineDevice:: SetVertexShaderConstantI()\n");

	return orig_vtable.SetVertexShaderConstantI(((IDirect3DDevice9Trampoline*)This)->orig_this, StartRegister, pConstantData, Vector4iCount);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetVertexShaderConstantI(Device * This, UINT StartRegister, int* pConstantData, UINT Vector4iCount) {
	HookDebug("TrampolineDevice:: GetVertexShaderConstantI()\n");

	return orig_vtable.GetVertexShaderConstantI(((IDirect3DDevice9Trampoline*)This)->orig_this, StartRegister, pConstantData, Vector4iCount);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetVertexShaderConstantB(Device * This, UINT StartRegister, CONST BOOL* pConstantData, UINT  BoolCount) {
	HookDebug("TrampolineDevice:: SetVertexShaderConstantB()\n");

	return orig_vtable.SetVertexShaderConstantB(((IDirect3DDevice9Trampoline*)This)->orig_this, StartRegister, pConstantData, BoolCount);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetVertexShaderConstantB(Device * This, UINT StartRegister, BOOL* pConstantData, UINT BoolCount) {
	HookDebug("TrampolineDevice:: GetVertexShaderConstantB()\n");

	return orig_vtable.GetVertexShaderConstantB(((IDirect3DDevice9Trampoline*)This)->orig_this, StartRegister, pConstantData, BoolCount);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetStreamSource(Device * This, UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) {
	HookDebug("TrampolineDevice:: SetStreamSource()\n");

	return orig_vtable.SetStreamSource(((IDirect3DDevice9Trampoline*)This)->orig_this, StreamNumber, pStreamData, OffsetInBytes, Stride);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetStreamSource(Device * This, UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* pOffsetInBytes, UINT* pStride) {
	HookDebug("TrampolineDevice:: GetStreamSource()\n");

	return orig_vtable.GetStreamSource(((IDirect3DDevice9Trampoline*)This)->orig_this, StreamNumber, ppStreamData, pOffsetInBytes, pStride);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetStreamSourceFreq(Device * This, UINT StreamNumber, UINT Setting) {
	HookDebug("TrampolineDevice:: SetStreamSourceFreq()\n");

	return orig_vtable.SetStreamSourceFreq(((IDirect3DDevice9Trampoline*)This)->orig_this, StreamNumber, Setting);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetStreamSourceFreq(Device * This, UINT StreamNumber, UINT* pSetting) {
	HookDebug("TrampolineDevice:: GetStreamSourceFreq()\n");

	return orig_vtable.GetStreamSourceFreq(((IDirect3DDevice9Trampoline*)This)->orig_this, StreamNumber, pSetting);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetIndices(Device * This, IDirect3DIndexBuffer9* pIndexData) {
	HookDebug("TrampolineDevice:: SetIndices()\n");

	return orig_vtable.SetIndices(((IDirect3DDevice9Trampoline*)This)->orig_this, pIndexData);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetIndices(Device * This, IDirect3DIndexBuffer9** ppIndexData) {
	HookDebug("TrampolineDevice:: GetIndices()\n");

	return orig_vtable.GetIndices(((IDirect3DDevice9Trampoline*)This)->orig_this, ppIndexData);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineCreatePixelShader(Device * This, CONST DWORD* pFunction, IDirect3DPixelShader9** ppShader) {
	HookDebug("TrampolineDevice:: CreatePixelShader()\n");

	return orig_vtable.CreatePixelShader(((IDirect3DDevice9Trampoline*)This)->orig_this, pFunction, ppShader);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetPixelShader(Device * This, IDirect3DPixelShader9* pShader) {
	HookDebug("TrampolineDevice:: SetPixelShader()\n");

	return orig_vtable.SetPixelShader(((IDirect3DDevice9Trampoline*)This)->orig_this, pShader);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetPixelShader(Device * This, IDirect3DPixelShader9** ppShader) {
	HookDebug("TrampolineDevice:: GetPixelShader()\n");

	return orig_vtable.GetPixelShader(((IDirect3DDevice9Trampoline*)This)->orig_this, ppShader);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetPixelShaderConstantF(Device * This, UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) {
	HookDebug("TrampolineDevice:: SetPixelShaderConstantF()\n");

	return orig_vtable.SetPixelShaderConstantF(((IDirect3DDevice9Trampoline*)This)->orig_this, StartRegister, pConstantData, Vector4fCount);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetPixelShaderConstantF(Device * This, UINT StartRegister, float* pConstantData, UINT Vector4fCount) {
	HookDebug("TrampolineDevice:: GetPixelShaderConstantF()\n");

	return orig_vtable.GetPixelShaderConstantF(((IDirect3DDevice9Trampoline*)This)->orig_this, StartRegister, pConstantData, Vector4fCount);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetPixelShaderConstantI(Device * This, UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) {
	HookDebug("TrampolineDevice:: SetPixelShaderConstantI()\n");

	return orig_vtable.SetPixelShaderConstantI(((IDirect3DDevice9Trampoline*)This)->orig_this, StartRegister, pConstantData, Vector4iCount);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetPixelShaderConstantI(Device * This, UINT StartRegister, int* pConstantData, UINT Vector4iCount) {
	HookDebug("TrampolineDevice:: GetPixelShaderConstantI()\n");

	return orig_vtable.GetPixelShaderConstantI(((IDirect3DDevice9Trampoline*)This)->orig_this, StartRegister, pConstantData, Vector4iCount);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineSetPixelShaderConstantB(Device * This, UINT StartRegister, CONST BOOL* pConstantData, UINT BoolCount) {
	HookDebug("TrampolineDevice:: SetPixelShaderConstantB()\n");

	return orig_vtable.SetPixelShaderConstantB(((IDirect3DDevice9Trampoline*)This)->orig_this, StartRegister, pConstantData, BoolCount);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineGetPixelShaderConstantB(Device * This, UINT StartRegister, BOOL* pConstantData, UINT BoolCount) {
	HookDebug("TrampolineDevice:: GetPixelShaderConstantB()\n");

	return orig_vtable.GetPixelShaderConstantB(((IDirect3DDevice9Trampoline*)This)->orig_this, StartRegister, pConstantData, BoolCount);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineDrawRectPatch(Device * This, UINT Handle, CONST float* pNumSegs, CONST D3DRECTPATCH_INFO* pRectPatchInfo) {
	HookDebug("TrampolineDevice:: DrawRectPatch()\n");

	return orig_vtable.DrawRectPatch(((IDirect3DDevice9Trampoline*)This)->orig_this, Handle, pNumSegs, pRectPatchInfo);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineDrawTriPatch(Device * This, UINT Handle, CONST float* pNumSegs, CONST D3DTRIPATCH_INFO* pTriPatchInfo) {
	HookDebug("TrampolineDevice:: DrawTriPatch()\n");

	return orig_vtable.DrawTriPatch(((IDirect3DDevice9Trampoline*)This)->orig_this, Handle, pNumSegs, pTriPatchInfo);

}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineDeletePatch(Device * This, UINT Handle) {
	HookDebug("TrampolineDevice:: DeletePatch()\n");

	return orig_vtable.DeletePatch(((IDirect3DDevice9Trampoline*)This)->orig_this, Handle);


}
 template <typename Device>
 static HRESULT STDMETHODCALLTYPE TrampolineCreateQuery(Device * This, D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) {

	HookDebug("TrampolineDevice:: CreateQuery()\n");

	return orig_vtable.CreateQuery(((IDirect3DDevice9Trampoline*)This)->orig_this, Type, ppQuery);


}
static HRESULT STDMETHODCALLTYPE TrampolineCheckDeviceState(IDirect3DDevice9Ex * This, HWND hWindow)
{
	HookDebug("TrampolineDeviceEx:: CheckDeviceState()\n");

	return orig_vtable_ex.CheckDeviceState(((IDirect3DDevice9ExTrampoline*)This)->orig_this, hWindow);
}

static HRESULT STDMETHODCALLTYPE TrampolineCheckResourceResidency(IDirect3DDevice9Ex * This, IDirect3DResource9 **pResourceArray, UINT32 NumResources)
{
	HookDebug("TrampolineDeviceEx:: CheckResourceResidency()\n");

	return orig_vtable_ex.CheckResourceResidency(((IDirect3DDevice9ExTrampoline*)This)->orig_this, pResourceArray, NumResources);
}

static HRESULT STDMETHODCALLTYPE TrampolineComposeRects(IDirect3DDevice9Ex * This, IDirect3DSurface9 *pSource, IDirect3DSurface9 *pDestination, IDirect3DVertexBuffer9 *pSrcRectDescriptors, UINT NumRects, IDirect3DVertexBuffer9 *pDstRectDescriptors, D3DCOMPOSERECTSOP Operation, INT XOffset, INT YOffset)
{
	HookDebug("TrampolineDeviceEx:: ComposeRects()\n");

	return orig_vtable_ex.ComposeRects(((IDirect3DDevice9ExTrampoline*)This)->orig_this, pSource, pDestination, pSrcRectDescriptors, NumRects, pDstRectDescriptors, Operation, XOffset, YOffset);
}

static HRESULT STDMETHODCALLTYPE TrampolineCreateDepthStencilSurfaceEx(IDirect3DDevice9Ex * This, UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	HookDebug("TrampolineDeviceEx:: CreateDepthStencilSurfaceEx()\n");

	return orig_vtable_ex.CreateDepthStencilSurfaceEx(((IDirect3DDevice9ExTrampoline*)This)->orig_this, Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle, Usage);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateOffscreenPlainSurfaceEx(IDirect3DDevice9Ex * This, UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	HookDebug("TrampolineDeviceEx:: CreateOffscreenPlainSurfaceEx()\n");

	return orig_vtable_ex.CreateOffscreenPlainSurfaceEx(((IDirect3DDevice9ExTrampoline*)This)->orig_this, Width, Height, Format, Pool, ppSurface, pSharedHandle, Usage);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateRenderTargetEx(IDirect3DDevice9Ex * This, UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	HookDebug("TrampolineDeviceEx:: CreateRenderTargetEx()\n");

	return orig_vtable_ex.CreateRenderTargetEx(((IDirect3DDevice9ExTrampoline*)This)->orig_this, Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle, Usage);
}
static HRESULT STDMETHODCALLTYPE TrampolineGetDisplayModeEx(IDirect3DDevice9Ex * This, UINT iSwapChain, D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation)
{
	HookDebug("TrampolineDeviceEx:: GetDisplayModeEx()\n");

	return orig_vtable_ex.GetDisplayModeEx(((IDirect3DDevice9ExTrampoline*)This)->orig_this, iSwapChain, pMode, pRotation);
}
static HRESULT STDMETHODCALLTYPE TrampolineGetGPUThreadPriority(IDirect3DDevice9Ex * This, INT *pPriority)
{
	HookDebug("TrampolineDeviceEx:: GetGPUThreadPriority()\n");

	return orig_vtable_ex.GetGPUThreadPriority(((IDirect3DDevice9ExTrampoline*)This)->orig_this, pPriority);
}
static HRESULT STDMETHODCALLTYPE TrampolineGetMaximumFrameLatency(IDirect3DDevice9Ex * This, UINT *pMaxLatency)
{
	HookDebug("TrampolineDeviceEx:: GetMaximumFrameLatency()\n");

	return orig_vtable_ex.GetMaximumFrameLatency(((IDirect3DDevice9ExTrampoline*)This)->orig_this, pMaxLatency);
}
static HRESULT STDMETHODCALLTYPE TrampolinePresentEx(IDirect3DDevice9Ex * This, const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags)
{
	HookDebug("TrampolineDeviceEx:: PresentEx()\n");

	return orig_vtable_ex.PresentEx(((IDirect3DDevice9ExTrampoline*)This)->orig_this, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}
static HRESULT STDMETHODCALLTYPE TrampolineResetEx(IDirect3DDevice9Ex * This, D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode)
{
	HookDebug("TrampolineDeviceEx:: ResetEx()\n");

	return orig_vtable_ex.ResetEx(((IDirect3DDevice9ExTrampoline*)This)->orig_this, pPresentationParameters, pFullscreenDisplayMode);
}
static HRESULT STDMETHODCALLTYPE TrampolineSetConvolutionMonoKernel(IDirect3DDevice9Ex * This, UINT Width, UINT Height, float *RowWeights, float *ColumnWeights)
{
	HookDebug("TrampolineDeviceEx:: SetConvolutionMonoKernel()\n");

	return orig_vtable_ex.SetConvolutionMonoKernel(((IDirect3DDevice9ExTrampoline*)This)->orig_this, Width, Height, RowWeights, ColumnWeights);
}
static HRESULT STDMETHODCALLTYPE TrampolineSetGPUThreadPriority(IDirect3DDevice9Ex * This, INT pPriority)
{
	HookDebug("TrampolineDeviceEx:: SetGPUThreadPriority()\n");

	return orig_vtable_ex.SetGPUThreadPriority(((IDirect3DDevice9ExTrampoline*)This)->orig_this, pPriority);
}
static HRESULT STDMETHODCALLTYPE TrampolineSetMaximumFrameLatency(IDirect3DDevice9Ex * This, UINT pMaxLatency)
{
	HookDebug("TrampolineDeviceEx:: SetMaximumFrameLatency()\n");

	return orig_vtable_ex.SetMaximumFrameLatency(((IDirect3DDevice9ExTrampoline*)This)->orig_this, pMaxLatency);
}
static HRESULT STDMETHODCALLTYPE TrampolineWaitForVBlank(IDirect3DDevice9Ex * This, UINT SwapChainIndex)
{
	HookDebug("TrampolineDeviceEx:: WaitForVBlank()\n");

	return orig_vtable_ex.WaitForVBlank(((IDirect3DDevice9ExTrampoline*)This)->orig_this, SwapChainIndex);
}
static CONST_VTBL struct IDirect3DDevice9Vtbl trampoline_vtable = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolineTestCooperativeLevel,
	TrampolineGetAvailableTextureMem,
	TrampolineEvictManagedResources,
	TrampolineGetDirect3D,
	TrampolineGetDeviceCaps,
	TrampolineGetDisplayMode,
	TrampolineGetCreationParameters,
	TrampolineSetCursorProperties,
	TrampolineSetCursorPosition,
	TrampolineShowCursor,
	TrampolineCreateAdditionalSwapChain,
	TrampolineGetSwapChain,
	TrampolineGetNumberOfSwapChains,
	TrampolineReset,
	TrampolinePresent,
	TrampolineGetBackBuffer,
	TrampolineGetRasterStatus,
	TrampolineSetDialogBoxMode,
	TrampolineSetGammaRamp,
	TrampolineGetGammaRamp,
	TrampolineCreateTexture,
	TrampolineCreateVolumeTexture,
	TrampolineCreateCubeTexture,
	TrampolineCreateVertexBuffer,
	TrampolineCreateIndexBuffer,
	TrampolineCreateRenderTarget,
	TrampolineCreateDepthStencilSurface,
	TrampolineUpdateSurface,
	TrampolineUpdateTexture,
	TrampolineGetRenderTargetData,
	TrampolineGetFrontBufferData,
	TrampolineStretchRect,
	TrampolineColorFill,
	TrampolineCreateOffscreenPlainSurface,
	TrampolineSetRenderTarget,
	TrampolineGetRenderTarget,
	TrampolineSetDepthStencilSurface,
	TrampolineGetDepthStencilSurface,
	TrampolineBeginScene,
	TrampolineEndScene,
	TrampolineClear,
	TrampolineSetTransform,
	TrampolineGetTransform,
	TrampolineMultiplyTransform,
	TrampolineSetViewport,
	TrampolineGetViewport,
	TrampolineSetMaterial,
	TrampolineGetMaterial,
	TrampolineSetLight,
	TrampolineGetLight,
	TrampolineLightEnable,
	TrampolineGetLightEnable,
	TrampolineSetClipPlane,
	TrampolineGetClipPlane,
	TrampolineSetRenderState,
	TrampolineGetRenderState,
	TrampolineCreateStateBlock,
	TrampolineBeginStateBlock,
	TrampolineEndStateBlock,
	TrampolineSetClipStatus,
	TrampolineGetClipStatus,
	TrampolineGetTexture,
	TrampolineSetTexture,
	TrampolineGetTextureStageState,
	TrampolineSetTextureStageState,
	TrampolineGetSamplerState,
	TrampolineSetSamplerState,
	TrampolineValidateDevice,
	TrampolineSetPaletteEntries,
	TrampolineGetPaletteEntries,
	TrampolineSetCurrentTexturePalette,
	TrampolineGetCurrentTexturePalette,
	TrampolineSetScissorRect,
	TrampolineGetScissorRect,
	TrampolineSetSoftwareVertexProcessing,
	TrampolineGetSoftwareVertexProcessing,
	TrampolineSetNPatchMode,
	TrampolineGetNPatchMode,
	TrampolineDrawPrimitive,
	TrampolineDrawIndexedPrimitive,
	TrampolineDrawPrimitiveUP,
	TrampolineDrawIndexedPrimitiveUP,
	TrampolineProcessVertices,
	TrampolineCreateVertexDeclaration,
	TrampolineSetVertexDeclaration,
	TrampolineGetVertexDeclaration,
	TrampolineSetFVF,
	TrampolineGetFVF,
	TrampolineCreateVertexShader,
	TrampolineSetVertexShader,
	TrampolineGetVertexShader,
	TrampolineSetVertexShaderConstantF,
	TrampolineGetVertexShaderConstantF,
	TrampolineSetVertexShaderConstantI,
	TrampolineGetVertexShaderConstantI,
	TrampolineSetVertexShaderConstantB,
	TrampolineGetVertexShaderConstantB,
	TrampolineSetStreamSource,
	TrampolineGetStreamSource,
	TrampolineSetStreamSourceFreq,
	TrampolineGetStreamSourceFreq,
	TrampolineSetIndices,
	TrampolineGetIndices,
	TrampolineCreatePixelShader,
	TrampolineSetPixelShader,
	TrampolineGetPixelShader,
	TrampolineSetPixelShaderConstantF,
	TrampolineGetPixelShaderConstantF,
	TrampolineSetPixelShaderConstantI,
	TrampolineGetPixelShaderConstantI,
	TrampolineSetPixelShaderConstantB,
	TrampolineGetPixelShaderConstantB,
	TrampolineDrawRectPatch,
	TrampolineDrawTriPatch,
	TrampolineDeletePatch,
	TrampolineCreateQuery
};

static CONST_VTBL struct IDirect3DDevice9ExVtbl trampoline_vtable_ex = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolineTestCooperativeLevel,
	TrampolineGetAvailableTextureMem,
	TrampolineEvictManagedResources,
	TrampolineGetDirect3D,
	TrampolineGetDeviceCaps,
	TrampolineGetDisplayMode,
	TrampolineGetCreationParameters,
	TrampolineSetCursorProperties,
	TrampolineSetCursorPosition,
	TrampolineShowCursor,
	TrampolineCreateAdditionalSwapChain,
	TrampolineGetSwapChain,
	TrampolineGetNumberOfSwapChains,
	TrampolineReset,
	TrampolinePresent,
	TrampolineGetBackBuffer,
	TrampolineGetRasterStatus,
	TrampolineSetDialogBoxMode,
	TrampolineSetGammaRamp,
	TrampolineGetGammaRamp,
	TrampolineCreateTexture,
	TrampolineCreateVolumeTexture,
	TrampolineCreateCubeTexture,
	TrampolineCreateVertexBuffer,
	TrampolineCreateIndexBuffer,
	TrampolineCreateRenderTarget,
	TrampolineCreateDepthStencilSurface,
	TrampolineUpdateSurface,
	TrampolineUpdateTexture,
	TrampolineGetRenderTargetData,
	TrampolineGetFrontBufferData,
	TrampolineStretchRect,
	TrampolineColorFill,
	TrampolineCreateOffscreenPlainSurface,
	TrampolineSetRenderTarget,
	TrampolineGetRenderTarget,
	TrampolineSetDepthStencilSurface,
	TrampolineGetDepthStencilSurface,
	TrampolineBeginScene,
	TrampolineEndScene,
	TrampolineClear,
	TrampolineSetTransform,
	TrampolineGetTransform,
	TrampolineMultiplyTransform,
	TrampolineSetViewport,
	TrampolineGetViewport,
	TrampolineSetMaterial,
	TrampolineGetMaterial,
	TrampolineSetLight,
	TrampolineGetLight,
	TrampolineLightEnable,
	TrampolineGetLightEnable,
	TrampolineSetClipPlane,
	TrampolineGetClipPlane,
	TrampolineSetRenderState,
	TrampolineGetRenderState,
	TrampolineCreateStateBlock,
	TrampolineBeginStateBlock,
	TrampolineEndStateBlock,
	TrampolineSetClipStatus,
	TrampolineGetClipStatus,
	TrampolineGetTexture,
	TrampolineSetTexture,
	TrampolineGetTextureStageState,
	TrampolineSetTextureStageState,
	TrampolineGetSamplerState,
	TrampolineSetSamplerState,
	TrampolineValidateDevice,
	TrampolineSetPaletteEntries,
	TrampolineGetPaletteEntries,
	TrampolineSetCurrentTexturePalette,
	TrampolineGetCurrentTexturePalette,
	TrampolineSetScissorRect,
	TrampolineGetScissorRect,
	TrampolineSetSoftwareVertexProcessing,
	TrampolineGetSoftwareVertexProcessing,
	TrampolineSetNPatchMode,
	TrampolineGetNPatchMode,
	TrampolineDrawPrimitive,
	TrampolineDrawIndexedPrimitive,
	TrampolineDrawPrimitiveUP,
	TrampolineDrawIndexedPrimitiveUP,
	TrampolineProcessVertices,
	TrampolineCreateVertexDeclaration,
	TrampolineSetVertexDeclaration,
	TrampolineGetVertexDeclaration,
	TrampolineSetFVF,
	TrampolineGetFVF,
	TrampolineCreateVertexShader,
	TrampolineSetVertexShader,
	TrampolineGetVertexShader,
	TrampolineSetVertexShaderConstantF,
	TrampolineGetVertexShaderConstantF,
	TrampolineSetVertexShaderConstantI,
	TrampolineGetVertexShaderConstantI,
	TrampolineSetVertexShaderConstantB,
	TrampolineGetVertexShaderConstantB,
	TrampolineSetStreamSource,
	TrampolineGetStreamSource,
	TrampolineSetStreamSourceFreq,
	TrampolineGetStreamSourceFreq,
	TrampolineSetIndices,
	TrampolineGetIndices,
	TrampolineCreatePixelShader,
	TrampolineSetPixelShader,
	TrampolineGetPixelShader,
	TrampolineSetPixelShaderConstantF,
	TrampolineGetPixelShaderConstantF,
	TrampolineSetPixelShaderConstantI,
	TrampolineGetPixelShaderConstantI,
	TrampolineSetPixelShaderConstantB,
	TrampolineGetPixelShaderConstantB,
	TrampolineDrawRectPatch,
	TrampolineDrawTriPatch,
	TrampolineDeletePatch,
	TrampolineCreateQuery,
	TrampolineSetConvolutionMonoKernel,
	TrampolineComposeRects,
	TrampolinePresentEx,
	TrampolineGetGPUThreadPriority,
	TrampolineSetGPUThreadPriority,
	TrampolineWaitForVBlank,
	TrampolineCheckResourceResidency,
	TrampolineSetMaximumFrameLatency,
	TrampolineGetMaximumFrameLatency,
	TrampolineCheckDeviceState,
	TrampolineCreateRenderTargetEx,
	TrampolineCreateOffscreenPlainSurfaceEx,
	TrampolineCreateDepthStencilSurfaceEx,
	TrampolineResetEx,
	TrampolineGetDisplayModeEx
};
IDirect3DDevice9Ex* hook_device(IDirect3DDevice9Ex *orig_device, IDirect3DDevice9Ex *hacker_device, EnableHooksDX9 enable_hooks)
{
	IDirect3DDevice9ExTrampoline *trampoline_device = new IDirect3DDevice9ExTrampoline();
	trampoline_device->lpVtbl = &trampoline_vtable_ex;
	trampoline_device->orig_this = orig_device;

	install_hooks(orig_device, enable_hooks);
	EnterCriticalSection(&device_map_lock);
	device_map[(IDirect3DDevice9*)orig_device] = (IDirect3DDevice9*)hacker_device;
	LeaveCriticalSection(&device_map_lock);

	return (IDirect3DDevice9Ex*)trampoline_device;
}
IDirect3DDevice9* hook_device(IDirect3DDevice9 *orig_device, IDirect3DDevice9 *hacker_device, EnableHooksDX9 enable_hooks)
{
	IDirect3DDevice9Trampoline *trampoline_device = new IDirect3DDevice9Trampoline();
	trampoline_device->lpVtbl = &trampoline_vtable;
	trampoline_device->orig_this = orig_device;

	install_hooks(orig_device, enable_hooks);
	EnterCriticalSection(&device_map_lock);
	device_map[orig_device] = hacker_device;
	LeaveCriticalSection(&device_map_lock);

	return (IDirect3DDevice9*)trampoline_device;
}
void remove_hooked_device(::IDirect3DDevice9 * orig_device)
{
	HookDebug("HookedDevice:: Remove Device()\n");
	DeviceMap::iterator i;
	EnterCriticalSection(&device_map_lock);
	i = device_map.find(orig_device);
	if (i != device_map.end()) {
		device_map.erase(i);
	}
	LeaveCriticalSection(&device_map_lock);
}
