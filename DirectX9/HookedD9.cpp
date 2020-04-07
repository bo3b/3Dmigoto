
#include <unordered_map>
#include "HookedD9.h"

#include "DLLMainHookDX9.h"

#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

#undef  INTERFACE
#define INTERFACE IDirect3D9Ex2

DECLARE_INTERFACE_(IDirect3D9Ex2, IDirect3D9Ex)
{
	BEGIN_INTERFACE
		/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	/*** IDirect3D9 methods ***/
	STDMETHOD (RegisterSoftwareDevice)(THIS_ void* pInitializeFunction) PURE;
	STDMETHOD_(UINT, GetAdapterCount)(THIS) PURE;
	STDMETHOD(GetAdapterIdentifier)(THIS_ UINT Adapter, DWORD Flags, ::D3DADAPTER_IDENTIFIER9* pIdentifier) PURE;
	STDMETHOD_(UINT, GetAdapterModeCount)(THIS_ UINT Adapter, ::D3DFORMAT Format) PURE;
	STDMETHOD(EnumAdapterModes)(THIS_ UINT Adapter, ::D3DFORMAT Format, UINT Mode, ::D3DDISPLAYMODE* pMode) PURE;
	STDMETHOD(GetAdapterDisplayMode)(THIS_ UINT Adapter, ::D3DDISPLAYMODE* pMode) PURE;
	STDMETHOD(CheckDeviceType)(THIS_ UINT Adapter, ::D3DDEVTYPE DevType, ::D3DFORMAT AdapterFormat, ::D3DFORMAT BackBufferFormat, BOOL bWindowed) PURE;
	STDMETHOD(CheckDeviceFormat)(THIS_ UINT Adapter, ::D3DDEVTYPE DeviceType, ::D3DFORMAT AdapterFormat, DWORD Usage, ::D3DRESOURCETYPE RType, ::D3DFORMAT CheckFormat) PURE;
	STDMETHOD(CheckDeviceMultiSampleType)(THIS_ UINT Adapter, ::D3DDEVTYPE DeviceType, ::D3DFORMAT SurfaceFormat, BOOL Windowed, ::D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels) PURE;
	STDMETHOD(CheckDepthStencilMatch)(THIS_ UINT Adapter, ::D3DDEVTYPE DeviceType, ::D3DFORMAT AdapterFormat, ::D3DFORMAT RenderTargetFormat, ::D3DFORMAT DepthStencilFormat) PURE;
	STDMETHOD(CheckDeviceFormatConversion)(THIS_ UINT Adapter, ::D3DDEVTYPE DeviceType, ::D3DFORMAT SourceFormat, ::D3DFORMAT TargetFormat) PURE;
	STDMETHOD(GetDeviceCaps)(THIS_ UINT Adapter, ::D3DDEVTYPE DeviceType, ::D3DCAPS9* pCaps) PURE;
	STDMETHOD_(HMONITOR, GetAdapterMonitor)(THIS_ UINT Adapter) PURE;
	STDMETHOD(CreateDevice)(THIS_ UINT Adapter, ::D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, ::D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface) PURE;
	STDMETHOD_(UINT, GetAdapterModeCountEx)(THIS_ UINT Adapter, CONST ::D3DDISPLAYMODEFILTER* pFilter) PURE;
	STDMETHOD(EnumAdapterModesEx)(THIS_ UINT Adapter, CONST ::D3DDISPLAYMODEFILTER* pFilter, UINT Mode, ::D3DDISPLAYMODEEX* pMode) PURE;
	STDMETHOD(GetAdapterDisplayModeEx)(THIS_ UINT Adapter, ::D3DDISPLAYMODEEX* pMode, ::D3DDISPLAYROTATION* pRotation) PURE;
	STDMETHOD(CreateDeviceEx)(THIS_ UINT Adapter, ::D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, ::D3DPRESENT_PARAMETERS* pPresentationParameters, ::D3DDISPLAYMODEEX* pFullscreenDisplayMode, IDirect3DDevice9Ex** ppReturnedDeviceInterface) PURE;
	STDMETHOD(GetAdapterLUID)(THIS_ UINT Adapter, LUID * pLUID) PURE;
	END_INTERFACE
};

typedef std::unordered_map<IDirect3D9 *, IDirect3D9 *> D9Map;
static D9Map D9_map;
static CRITICAL_SECTION D9_map_lock;

static struct IDirect3D9Vtbl orig_vtable;
static struct IDirect3D9Ex2Vtbl orig_vtable_ex;

static bool hooks_installed = false;

IDirect3D9Ex* lookup_hooked_D9(IDirect3D9Ex *orig_D9)
{
	D9Map::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&D9_map_lock);
	i = D9_map.find((IDirect3D9*)orig_D9);
	if (i == D9_map.end()) {
		LeaveCriticalSection(&D9_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&D9_map_lock);

	return (IDirect3D9Ex*)i->second;
}
IDirect3D9* lookup_hooked_D9(IDirect3D9 *orig_D9)
{
	D9Map::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&D9_map_lock);
	i = D9_map.find(orig_D9);
	if (i == D9_map.end()) {
		LeaveCriticalSection(&D9_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&D9_map_lock);

	return i->second;
}
// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3D9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9:: QueryInterface()\n");

	if (D9)
		return IDirect3D9Ex_QueryInterface((IDirect3D9*)D9,riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3D9 * This)
{
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9:: AddRef()\n");

	if (D9)
		return IDirect3D9Ex_AddRef(D9);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3D9 * This)
{
	D9Map::iterator i;
	ULONG ref;

	HookDebug("HookedD9:: Release()\n");

	EnterCriticalSection(&D9_map_lock);
	i = D9_map.find(This);
	if (i != D9_map.end()) {
		ref = IDirect3D9Ex_Release(i->second);
		if (!ref)
			D9_map.erase(i);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&D9_map_lock);

	return ref;
}

static HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(IDirect3D9 * This, void* pInitializeFunction) {
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::RegisterSoftwareDevice()\n");

	if (D9)
		return IDirect3D9_RegisterSoftwareDevice((IDirect3D9*)D9, pInitializeFunction);

	return orig_vtable.RegisterSoftwareDevice(This, pInitializeFunction);

}

static UINT STDMETHODCALLTYPE GetAdapterCount(IDirect3D9 * This) {
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::GetAdapterCount()\n");

	if (D9)
		return IDirect3D9Ex_GetAdapterCount(D9);

	return orig_vtable.GetAdapterCount(This);

}

static HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(IDirect3D9 * This, UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier){
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::GetAdapterIdentifier()\n");

	if (D9)
		return IDirect3D9Ex_GetAdapterIdentifier((IDirect3D9*)D9,Adapter, Flags, pIdentifier);

	return orig_vtable.GetAdapterIdentifier(This, Adapter, Flags, pIdentifier);

}

static UINT STDMETHODCALLTYPE GetAdapterModeCount(IDirect3D9 * This, UINT Adapter, D3DFORMAT Format) {
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::GetAdapterModeCount()\n");

	if (D9)
		return IDirect3D9Ex_GetAdapterModeCount((IDirect3D9*)D9,Adapter, Format);

	return orig_vtable.GetAdapterModeCount(This, Adapter, Format);

}

static HRESULT STDMETHODCALLTYPE EnumAdapterModes(IDirect3D9 * This, UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode){
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::EnumAdapterModes()\n");

	if (D9)
		return IDirect3D9Ex_EnumAdapterModes((IDirect3D9*)D9,Adapter, Format, Mode, pMode);

	return orig_vtable.EnumAdapterModes(This, Adapter, Format, Mode, pMode);

}

static HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(IDirect3D9 * This, UINT Adapter, D3DDISPLAYMODE* pMode) {
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::GetAdapterDisplayMode()\n");

	if (D9)
		return IDirect3D9Ex_GetAdapterDisplayMode((IDirect3D9*)D9,Adapter, pMode);

	return orig_vtable.GetAdapterDisplayMode(This, Adapter, pMode);

}

static HRESULT STDMETHODCALLTYPE CheckDeviceType(IDirect3D9 * This, UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed){
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::CheckDeviceType()\n");

	if (D9)
		return IDirect3D9Ex_CheckDeviceType((IDirect3D9*)D9,Adapter, DevType, AdapterFormat, BackBufferFormat, bWindowed);

	return orig_vtable.CheckDeviceType(This, Adapter, DevType, AdapterFormat, BackBufferFormat, bWindowed);

}

static HRESULT STDMETHODCALLTYPE CheckDeviceFormat(IDirect3D9 * This, UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat){
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::CheckDeviceFormat()\n");

	if (D9)
		return IDirect3D9Ex_CheckDeviceFormat((IDirect3D9*)D9,Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);

	return orig_vtable.CheckDeviceFormat(This, Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);

}

static HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(IDirect3D9 * This, UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels){
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::CheckDeviceMultiSampleType()\n");

	if (D9)
		return IDirect3D9Ex_CheckDeviceMultiSampleType((IDirect3D9*)D9,Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels);

	return orig_vtable.CheckDeviceMultiSampleType(This, Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels);

}

static HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(IDirect3D9 * This, UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat){
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::CheckDepthStencilMatch()\n");

	if (D9)
		return IDirect3D9Ex_CheckDepthStencilMatch((IDirect3D9*)D9,Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat);

	return orig_vtable.CheckDepthStencilMatch(This, Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat);

}

static HRESULT STDMETHODCALLTYPE CheckDeviceFormatConversion(IDirect3D9 * This, UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) {
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::CheckDeviceFormatConversion()\n");

	if (D9)
		return IDirect3D9Ex_CheckDeviceFormatConversion((IDirect3D9*)D9,Adapter, DeviceType, SourceFormat, TargetFormat);

	return orig_vtable.CheckDeviceFormatConversion(This, Adapter, DeviceType, SourceFormat, TargetFormat);

}

static HRESULT STDMETHODCALLTYPE _GetDeviceCaps(IDirect3D9 * This, UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps) {
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::GetDeviceCaps()\n");

	if (D9)
		return IDirect3D9Ex_GetDeviceCaps((IDirect3D9*)D9, Adapter, DeviceType, pCaps);

	return orig_vtable.GetDeviceCaps(This, Adapter, DeviceType, pCaps);

}

static HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(IDirect3D9 * This, UINT Adapter) {
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::GetAdapterMonitor()\n");

	if (D9)
		return IDirect3D9Ex_GetAdapterMonitor((IDirect3D9*)D9,Adapter);

	return orig_vtable.GetAdapterMonitor(This, Adapter);

}

static HRESULT STDMETHODCALLTYPE CreateDevice(IDirect3D9 * This, UINT Adapter, ::D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface){
	IDirect3D9 *D9 = (IDirect3D9*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::CreateDevice()\n");

	if (D9)
		return IDirect3D9Ex_CreateDevice((IDirect3D9*)D9,Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

	return orig_vtable.CreateDevice(This, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

}

static HRESULT STDMETHODCALLTYPE CreateDeviceEx(IDirect3D9Ex2 * This,
	          UINT                  Adapter,
	          D3DDEVTYPE            DeviceType,
	          HWND                  hFocusWindow,
	          DWORD                 BehaviorFlags,
	     D3DPRESENT_PARAMETERS *pPresentationParameters,
	     D3DDISPLAYMODEEX      *pFullscreenDisplayMode,
	 IDirect3DDevice9Ex    **ppReturnedDeviceInterface)
{
	IDirect3D9Ex2 *D9 = (IDirect3D9Ex2*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::CreateDeviceEx()\n");

	if (D9)
		return IDirect3D9Ex_CreateDeviceEx((IDirect3D9Ex2*)D9,Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);

	return orig_vtable_ex.CreateDeviceEx(This, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);

}

static HRESULT STDMETHODCALLTYPE EnumAdapterModesEx(IDirect3D9Ex2 * This,
	                UINT                 Adapter,
	          const D3DDISPLAYMODEFILTER *pFilter,
	                UINT                 Mode,
	       D3DDISPLAYMODEEX     *pMode)
{
	IDirect3D9Ex2 *D9 = (IDirect3D9Ex2*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::EnumAdapterModesEx()\n");

	if (D9)
		return IDirect3D9Ex_EnumAdapterModesEx((IDirect3D9Ex2*)D9,Adapter, pFilter, Mode, pMode);

	return orig_vtable_ex.EnumAdapterModesEx(This, Adapter, pFilter, Mode, pMode);

}

static HRESULT STDMETHODCALLTYPE GetAdapterDisplayModeEx(IDirect3D9Ex2 * This,
	      UINT               Adapter,
	 D3DDISPLAYMODEEX   *pMode,
	 D3DDISPLAYROTATION *pRotation)
{
	IDirect3D9Ex2 *D9 = (IDirect3D9Ex2*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::GetAdapterDisplayModeEx()\n");

	if (D9)
		return IDirect3D9Ex_GetAdapterDisplayModeEx((IDirect3D9Ex2*)D9,Adapter, pMode, pRotation);

	return orig_vtable_ex.GetAdapterDisplayModeEx(This, Adapter, pMode, pRotation);

}

static HRESULT STDMETHODCALLTYPE GetAdapterLUID(IDirect3D9Ex2 * This,
	 UINT Adapter,
	 LUID *pLUID)
{
	IDirect3D9Ex2 *D9 = (IDirect3D9Ex2*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::GetAdapterLUID()\n");

	if (D9)
		return IDirect3D9Ex_GetAdapterLUID((IDirect3D9Ex2*)D9,Adapter, pLUID);

	return orig_vtable_ex.GetAdapterLUID(This, Adapter, pLUID);

}

static HRESULT STDMETHODCALLTYPE GetAdapterModeCountEx(IDirect3D9Ex2 * This,
	       UINT                 Adapter,
	 const D3DDISPLAYMODEFILTER *pFilter)
{
	IDirect3D9Ex2 *D9 = (IDirect3D9Ex2*)lookup_hooked_D9((IDirect3D9Ex*)This);

	HookDebug("HookedD9::GetAdapterModeCountEx()\n");

	if (D9)
		return IDirect3D9Ex_GetAdapterModeCountEx((IDirect3D9Ex2*)D9,Adapter, pFilter);

	return orig_vtable_ex.GetAdapterModeCountEx(This, Adapter, pFilter);

}
static void install_hooks(IDirect3D9 *D9)
{
	SIZE_T hook_id;
	if (hooks_installed)
		return;


	InitializeCriticalSection(&D9_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, D9->lpVtbl, sizeof(struct IDirect3D9Ex2Vtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, D9->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, D9->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, D9->lpVtbl->Release, Release);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CheckDepthStencilMatch, D9->lpVtbl->CheckDepthStencilMatch, CheckDepthStencilMatch);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CheckDeviceFormat, D9->lpVtbl->CheckDeviceFormat, CheckDeviceFormat);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CheckDeviceFormatConversion, D9->lpVtbl->CheckDeviceFormatConversion, CheckDeviceFormatConversion);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CheckDeviceMultiSampleType, D9->lpVtbl->CheckDeviceMultiSampleType, CheckDeviceMultiSampleType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CheckDeviceType, D9->lpVtbl->CheckDeviceType, CheckDeviceType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateDevice, D9->lpVtbl->CreateDevice, CreateDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.EnumAdapterModes, D9->lpVtbl->EnumAdapterModes, EnumAdapterModes);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetAdapterCount, D9->lpVtbl->GetAdapterCount, GetAdapterCount);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetAdapterDisplayMode, D9->lpVtbl->GetAdapterDisplayMode, GetAdapterDisplayMode);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetAdapterIdentifier, D9->lpVtbl->GetAdapterIdentifier, GetAdapterIdentifier);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetAdapterModeCount, D9->lpVtbl->GetAdapterModeCount, GetAdapterModeCount);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetAdapterMonitor, D9->lpVtbl->GetAdapterMonitor, GetAdapterMonitor);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDeviceCaps, D9->lpVtbl->GetDeviceCaps, _GetDeviceCaps);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.RegisterSoftwareDevice, D9->lpVtbl->RegisterSoftwareDevice, RegisterSoftwareDevice);

}
static void install_hooks(IDirect3D9Ex2 *D9)
{
	SIZE_T hook_id;
	if (hooks_installed)
		return;

	install_hooks((IDirect3D9*)D9);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.CreateDeviceEx, D9->lpVtbl->CreateDeviceEx, CreateDeviceEx);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.EnumAdapterModesEx, D9->lpVtbl->EnumAdapterModesEx, EnumAdapterModesEx);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.GetAdapterDisplayModeEx, D9->lpVtbl->GetAdapterDisplayModeEx, GetAdapterDisplayModeEx);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.GetAdapterLUID, D9->lpVtbl->GetAdapterLUID, GetAdapterLUID);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable_ex.GetAdapterModeCountEx, D9->lpVtbl->GetAdapterModeCountEx, GetAdapterModeCountEx);

}
typedef struct IDirect3D9ExTrampoline {
	CONST_VTBL struct IDirect3D9Ex2Vtbl *lpVtbl;
	IDirect3D9Ex2 *orig_this;
} IDirect3D9ExTrampoline;
typedef struct IDirect3D9Trampoline {
	CONST_VTBL struct IDirect3D9Vtbl *lpVtbl;
	IDirect3D9 *orig_this;
} IDirect3D9Trampoline;

// IUnknown
template<typename D3D9>
 static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	 D3D9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineIDirect3D9:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3D9Trampoline*)This)->orig_this, riid, ppvObject);
}

 template<typename D3D9>
 static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	 D3D9 * This)
{
	HookDebug("TrampolineIDirect3D9:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3D9Trampoline*)This)->orig_this);
}
 template<typename D3D9>
 static ULONG STDMETHODCALLTYPE TrampolineRelease(
	 D3D9 * This)
{
	HookDebug("TrampolineIDirect3D9:: Release()\n");

	return orig_vtable.Release(((IDirect3D9Trampoline*)This)->orig_this);
}
 template<typename D3D9>
 static HRESULT STDMETHODCALLTYPE TrampolineCheckDepthStencilMatch(
	 D3D9 * This, UINT Adapter,
	 D3DDEVTYPE DeviceType,
	 D3DFORMAT  AdapterFormat,
	 D3DFORMAT  RenderTargetFormat,
	 D3DFORMAT  DepthStencilFormat)
{
	HookDebug("TrampolineIDirect3D9:: CheckDepthStencilMatch()\n");

	return orig_vtable.CheckDepthStencilMatch(((IDirect3D9Trampoline*)This)->orig_this, Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat);
}
 template<typename D3D9>
 static HRESULT STDMETHODCALLTYPE TrampolineCheckDeviceFormat(
	 D3D9 * This, UINT Adapter,
	 D3DDEVTYPE      DeviceType,
	 D3DFORMAT       AdapterFormat,
	 DWORD           Usage,
	 D3DRESOURCETYPE RType,
	 D3DFORMAT       CheckFormat)
{
	HookDebug("TrampolineIDirect3D9:: CheckDeviceFormat()\n");

	return orig_vtable.CheckDeviceFormat(((IDirect3D9Trampoline*)This)->orig_this, Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);
}
 template<typename D3D9>
 static HRESULT STDMETHODCALLTYPE TrampolineCheckDeviceFormatConversion(
	 D3D9 * This, UINT Adapter,
	 D3DDEVTYPE DeviceType,
	 D3DFORMAT  SourceFormat,
	 D3DFORMAT  TargetFormat)
{
	HookDebug("TrampolineIDirect3D9:: CheckDeviceFormatConversion()\n");

	return orig_vtable.CheckDeviceFormatConversion(((IDirect3D9Trampoline*)This)->orig_this, Adapter, DeviceType, SourceFormat, TargetFormat);
}
 template<typename D3D9>
 static HRESULT STDMETHODCALLTYPE TrampolineCheckDeviceMultiSampleType(
	 D3D9 * This, UINT Adapter,
	  D3DDEVTYPE          DeviceType,
	  D3DFORMAT           SurfaceFormat,
	  BOOL                Windowed,
	  D3DMULTISAMPLE_TYPE MultiSampleType,
	 DWORD               *pQualityLevels)
{
	HookDebug("TrampolineIDirect3D9:: CheckDeviceMultiSampleType()\n");

	return orig_vtable.CheckDeviceMultiSampleType(((IDirect3D9Trampoline*)This)->orig_this, Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels);
}
 template<typename D3D9>
 static HRESULT STDMETHODCALLTYPE TrampolineCheckDeviceType(
	 D3D9 * This, UINT Adapter,
	 D3DDEVTYPE DeviceType,
	 D3DFORMAT  DisplayFormat,
	 D3DFORMAT  BackBufferFormat,
	 BOOL       Windowed)
{
	HookDebug("TrampolineIDirect3D9:: CheckDeviceType()\n");

	return orig_vtable.CheckDeviceType(((IDirect3D9Trampoline*)This)->orig_this, Adapter, DeviceType, DisplayFormat, BackBufferFormat, Windowed);
}
 template<typename D3D9>
 static HRESULT STDMETHODCALLTYPE TrampolineEnumAdapterModes(
	 D3D9 * This, UINT Adapter,
	  D3DFORMAT      Format,
	  UINT           Mode,
	 D3DDISPLAYMODE *pMode)
{
	HookDebug("TrampolineIDirect3D9:: EnumAdapterModes()\n");

	return orig_vtable.EnumAdapterModes(((IDirect3D9Trampoline*)This)->orig_this, Adapter, Format, Mode, pMode);
}
 template<typename D3D9>
 static UINT STDMETHODCALLTYPE TrampolineGetAdapterCount(
	 D3D9 * This)
{
	HookDebug("TrampolineIDirect3D9:: EnumAdapterModes()\n");

	return orig_vtable.GetAdapterCount(((IDirect3D9Trampoline*)This)->orig_this);
}
 template<typename D3D9>
 static HRESULT STDMETHODCALLTYPE TrampolineGetAdapterDisplayMode(
	 D3D9 * This, UINT Adapter,
	 D3DDISPLAYMODE *pMode)
{
	HookDebug("TrampolineIDirect3D9:: GetAdapterDisplayMode()\n");

	return orig_vtable.GetAdapterDisplayMode(((IDirect3D9Trampoline*)This)->orig_this, Adapter, pMode);
 }
 template<typename D3D9>
 static HRESULT STDMETHODCALLTYPE TrampolineGetAdapterIdentifier(
	 D3D9 * This, UINT Adapter,
	  DWORD Flags,
	 D3DADAPTER_IDENTIFIER9 *pIdentifier)
{
	HookDebug("TrampolineIDirect3D9:: GetAdapterIdentifier()\n");

	return orig_vtable.GetAdapterIdentifier(((IDirect3D9Trampoline*)This)->orig_this, Adapter, Flags, pIdentifier);
}
 template<typename D3D9>
 static UINT STDMETHODCALLTYPE TrampolineGetAdapterModeCount(
	 D3D9 * This, UINT Adapter,
	 D3DFORMAT Format)
{
	HookDebug("TrampolineIDirect3D9:: GetAdapterModeCount()\n");

	return orig_vtable.GetAdapterModeCount(((IDirect3D9Trampoline*)This)->orig_this, Adapter, Format);
}

 template<typename D3D9>
 static HMONITOR STDMETHODCALLTYPE TrampolineGetAdapterMonitor(
	 D3D9 * This, UINT Adapter)
{
	HookDebug("TrampolineIDirect3D9:: GetAdapterMonitor()\n");

	return orig_vtable.GetAdapterMonitor(((IDirect3D9Trampoline*)This)->orig_this, Adapter);
}
 template<typename D3D9>
 static HRESULT STDMETHODCALLTYPE TrampolineGetDeviceCaps(
	 D3D9 * This, UINT Adapter,
	  D3DDEVTYPE DeviceType,
	 D3DCAPS9   *pCaps)
{
	HookDebug("TrampolineIDirect3D9:: GetDeviceCaps()\n");

	return orig_vtable.GetDeviceCaps(((IDirect3D9Trampoline*)This)->orig_this, Adapter, DeviceType, pCaps);
}
 template<typename D3D9>
 static HRESULT STDMETHODCALLTYPE TrampolineRegisterSoftwareDevice(
	 D3D9 * This, void *pInitializeFunction)
{
	HookDebug("TrampolineIDirect3D9:: RegisterSoftwareDevice()\n");

	return orig_vtable.RegisterSoftwareDevice(((IDirect3D9Trampoline*)This)->orig_this, pInitializeFunction);
}
 template<typename D3D9>
 static HRESULT STDMETHODCALLTYPE TrampolineCreateDevice(
	 D3D9 * This, UINT Adapter,
	          D3DDEVTYPE            DeviceType,
	          HWND                  hFocusWindow,
	          DWORD                 BehaviorFlags,
	     D3DPRESENT_PARAMETERS *pPresentationParameters,
	 IDirect3DDevice9      **ppReturnedDeviceInterface)
{
	HookDebug("TrampolineIDirect3D9:: CreateDevice()\n");

	return orig_vtable.CreateDevice(((IDirect3D9Trampoline*)This)->orig_this, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
}

static HRESULT STDMETHODCALLTYPE TrampolineCreateDeviceEx(
	IDirect3D9Ex2 * This,
	          UINT                  Adapter,
	          D3DDEVTYPE            DeviceType,
	          HWND                  hFocusWindow,
	          DWORD                 BehaviorFlags,
	     D3DPRESENT_PARAMETERS *pPresentationParameters,
	     D3DDISPLAYMODEEX      *pFullscreenDisplayMode,
	 IDirect3DDevice9Ex    **ppReturnedDeviceInterface)

{
	HookDebug("TrampolineIDirect3D9:: CreateDeviceEx()\n");

	return orig_vtable_ex.CreateDeviceEx(((IDirect3D9ExTrampoline*)This)->orig_this, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);
}

static HRESULT STDMETHODCALLTYPE TrampolineEnumAdapterModesEx(
	IDirect3D9Ex2 * This,
	                UINT                 Adapter,
	          const D3DDISPLAYMODEFILTER *pFilter,
	                UINT                 Mode,
	       D3DDISPLAYMODEEX     *pMode)

{
	HookDebug("TrampolineIDirect3D9:: EnumAdapterModesEx()\n");

	return orig_vtable_ex.EnumAdapterModesEx(((IDirect3D9ExTrampoline*)This)->orig_this, Adapter, pFilter, Mode, pMode);
}


static HRESULT STDMETHODCALLTYPE TrampolineGetAdapterDisplayModeEx(
	IDirect3D9Ex2 * This,
	      UINT               Adapter,
	 D3DDISPLAYMODEEX   *pMode,
	 D3DDISPLAYROTATION *pRotation)

{
	HookDebug("TrampolineIDirect3D9:: GetAdapterDisplayModeEx()\n");

	return orig_vtable_ex.GetAdapterDisplayModeEx(((IDirect3D9ExTrampoline*)This)->orig_this, Adapter, pMode, pRotation);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetAdapterLUID(
	IDirect3D9Ex2 * This,
	 UINT Adapter,
	 LUID *pLUID)

{
	HookDebug("TrampolineIDirect3D9:: GetAdapterLUID()\n");

	return orig_vtable_ex.GetAdapterLUID(((IDirect3D9ExTrampoline*)This)->orig_this, Adapter, pLUID);
}

static UINT STDMETHODCALLTYPE TrampolineGetAdapterModeCountEx(
	IDirect3D9Ex2 * This,
	       UINT                 Adapter,
	 const D3DDISPLAYMODEFILTER *pFilter)

{
	HookDebug("TrampolineIDirect3D9:: GetAdapterModeCountEx()\n");

	return orig_vtable_ex.GetAdapterModeCountEx(((IDirect3D9ExTrampoline*)This)->orig_this, Adapter, pFilter);
}

static CONST_VTBL struct IDirect3D9Vtbl trampoline_vtable = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolineRegisterSoftwareDevice,
	TrampolineGetAdapterCount,
	TrampolineGetAdapterIdentifier,
	TrampolineGetAdapterModeCount,
	TrampolineEnumAdapterModes,
	TrampolineGetAdapterDisplayMode,
	TrampolineCheckDeviceType,
	TrampolineCheckDeviceFormat,
	TrampolineCheckDeviceMultiSampleType,
	TrampolineCheckDepthStencilMatch,
	TrampolineCheckDeviceFormatConversion,
	TrampolineGetDeviceCaps,
	TrampolineGetAdapterMonitor,
	TrampolineCreateDevice
};
static CONST_VTBL struct IDirect3D9Ex2Vtbl trampoline_ex_vtable = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolineRegisterSoftwareDevice,
	TrampolineGetAdapterCount,
	TrampolineGetAdapterIdentifier,
	TrampolineGetAdapterModeCount,
	TrampolineEnumAdapterModes,
	TrampolineGetAdapterDisplayMode,
	TrampolineCheckDeviceType,
	TrampolineCheckDeviceFormat,
	TrampolineCheckDeviceMultiSampleType,
	TrampolineCheckDepthStencilMatch,
	TrampolineCheckDeviceFormatConversion,
	TrampolineGetDeviceCaps,
	TrampolineGetAdapterMonitor,
	TrampolineCreateDevice,
	TrampolineGetAdapterModeCountEx,
	TrampolineEnumAdapterModesEx,
	TrampolineGetAdapterDisplayModeEx,
	TrampolineCreateDeviceEx,
	TrampolineGetAdapterLUID

};

IDirect3D9Ex* hook_D9(IDirect3D9Ex *orig_D9, IDirect3D9Ex *hacker_D9)
{
	IDirect3D9ExTrampoline *trampoline_D9 = new IDirect3D9ExTrampoline();
	trampoline_D9->lpVtbl = &trampoline_ex_vtable;
	trampoline_D9->orig_this = (IDirect3D9Ex2*)orig_D9;
	install_hooks((IDirect3D9Ex2*)orig_D9);
	EnterCriticalSection(&D9_map_lock);
	D9_map[(IDirect3D9*)orig_D9] = (IDirect3D9*)hacker_D9;
	LeaveCriticalSection(&D9_map_lock);

	return (IDirect3D9Ex*)trampoline_D9;
}
IDirect3D9* hook_D9(IDirect3D9 *orig_D9, IDirect3D9 *hacker_D9)
{
	IDirect3D9Trampoline *trampoline_D9 = new IDirect3D9Trampoline();
	trampoline_D9->lpVtbl = &trampoline_vtable;
	trampoline_D9->orig_this = orig_D9;
	install_hooks(orig_D9);
	EnterCriticalSection(&D9_map_lock);
	D9_map[orig_D9] = hacker_D9;
	LeaveCriticalSection(&D9_map_lock);

	return (IDirect3D9*)trampoline_D9;
}
