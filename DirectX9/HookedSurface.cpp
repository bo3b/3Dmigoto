#include <unordered_map>

#include "HookedSurface.h"


#include "DLLMainHookDX9.h"


#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

typedef std::unordered_map<IDirect3DSurface9 *, IDirect3DSurface9 *> SurfaceMap;
static SurfaceMap surface_map;
static CRITICAL_SECTION surface_map_lock;

static struct IDirect3DSurface9Vtbl orig_vtable;

static bool hooks_installed = false;

IDirect3DSurface9* lookup_hooked_surface(IDirect3DSurface9 *orig_surface)
{
	SurfaceMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&surface_map_lock);
	i = surface_map.find(orig_surface);
	if (i == surface_map.end()) {
		LeaveCriticalSection(&surface_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&surface_map_lock);

	return i->second;
}

// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3DSurface9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: QueryInterface()\n");

	if (surface)
		return IDirect3DSurface9_QueryInterface(surface, riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3DSurface9 * This)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: AddRef()\n");

	if (surface)
		return IDirect3DSurface9_AddRef(surface);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3DSurface9 * This)
{
	SurfaceMap::iterator i;
	ULONG ref;

	HookDebug("HookedSurface:: Release()\n");

	EnterCriticalSection(&surface_map_lock);
	i = surface_map.find(This);
	if (i != surface_map.end()) {
		ref = IDirect3DSurface9_Release(i->second);
		if (!ref)
			surface_map.erase(i);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&surface_map_lock);

	return ref;
}

static HRESULT STDMETHODCALLTYPE FreePrivateData(
	IDirect3DSurface9 * This, REFGUID refguid)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: FreePrivateData()\n");

	if (surface)
		return IDirect3DSurface9_FreePrivateData(surface, refguid);

	return orig_vtable.FreePrivateData(This, refguid);
}

static HRESULT STDMETHODCALLTYPE GetDevice(
	IDirect3DSurface9 * This, IDirect3DDevice9 **ppDevice)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: GetDevice()\n");

	if (surface)
		return IDirect3DSurface9_GetDevice(surface, ppDevice);

	return orig_vtable.GetDevice(This, ppDevice);
}

static DWORD STDMETHODCALLTYPE GetPriority(
	IDirect3DSurface9 * This)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: GetPriority()\n");

	if (surface)
		return IDirect3DSurface9_GetPriority(surface);

	return orig_vtable.GetPriority(This);
}

static HRESULT STDMETHODCALLTYPE GetPrivateData(
	IDirect3DSurface9 * This,
	      REFGUID refguid,
	 void    *pData,
	 DWORD   *pSizeOfData)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: GetPrivateData()\n");

	if (surface)
		return IDirect3DSurface9_GetPrivateData(surface, refguid, pData, pSizeOfData);

	return orig_vtable.GetPrivateData(This, refguid, pData, pSizeOfData);
}

static D3DRESOURCETYPE STDMETHODCALLTYPE GetType(
	IDirect3DSurface9 * This)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: GetType()\n");

	if (surface)
		return IDirect3DSurface9_GetType(surface);

	return orig_vtable.GetType(This);
}

static void STDMETHODCALLTYPE PreLoad(
	IDirect3DSurface9 * This)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: PreLoad()\n");

	if (surface)
		return IDirect3DSurface9_PreLoad(surface);

	return orig_vtable.PreLoad(This);
}

static DWORD STDMETHODCALLTYPE SetPriority(
	IDirect3DSurface9 * This, DWORD PriorityNew)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: SetPriority()\n");

	if (surface)
		return IDirect3DSurface9_SetPriority(surface, PriorityNew);

	return orig_vtable.SetPriority(This, PriorityNew);
}


static HRESULT STDMETHODCALLTYPE SetPrivateData(
	IDirect3DSurface9 * This,
	       REFGUID refguid,
	 const void    *pData,
	       DWORD   SizeOfData,
	       DWORD   Flags)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: SetPrivateData()\n");

	if (surface)
		return IDirect3DSurface9_SetPrivateData(surface, refguid, pData, SizeOfData, Flags);

	return orig_vtable.SetPrivateData(This, refguid, pData, SizeOfData, Flags);
}

static HRESULT STDMETHODCALLTYPE GetDesc(
	IDirect3DSurface9 * This,
	D3DSURFACE_DESC *pDesc)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: GetDesc()\n");

	if (surface)
		return IDirect3DSurface9_GetDesc(surface, pDesc);

	return orig_vtable.GetDesc(This, pDesc);
}

static HRESULT STDMETHODCALLTYPE LockRect(
	IDirect3DSurface9 * This,
	       D3DLOCKED_RECT *pLockedRect,
	  const RECT           *pRect,
	        DWORD          Flags)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: Lock()\n");

	if (surface)
		return IDirect3DSurface9_LockRect(surface, pLockedRect, pRect, Flags);

	return orig_vtable.LockRect(This, pLockedRect, pRect, Flags);
}

static HRESULT STDMETHODCALLTYPE UnlockRect(
	IDirect3DSurface9 * This)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: UnlockRect()\n");

	if (surface)
		return IDirect3DSurface9_UnlockRect(surface);

	return orig_vtable.UnlockRect(This);
}

static HRESULT STDMETHODCALLTYPE GetContainer(
	IDirect3DSurface9 * This,
	  REFIID riid,
	 void   **ppContainer)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: GetContainer()\n");

	if (surface)
		return IDirect3DSurface9_GetContainer(surface, riid, ppContainer);

	return orig_vtable.GetContainer(This, riid, ppContainer);
}

static HRESULT STDMETHODCALLTYPE _GetDC(
	IDirect3DSurface9 * This,
	HDC *phdc)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: GetDC()\n");

	if (surface)
		return IDirect3DSurface9_GetDC(surface, phdc);

	return orig_vtable.GetDC(This, phdc);
}

static HRESULT STDMETHODCALLTYPE _ReleaseDC(
	IDirect3DSurface9 * This,
	HDC hdc)
{
	IDirect3DSurface9 *surface = lookup_hooked_surface(This);

	HookDebug("HookedSurface:: ReleaseDC()\n");

	if (surface)
		return IDirect3DSurface9_ReleaseDC(surface, hdc);

	return orig_vtable.ReleaseDC(This, hdc);
}



static void install_hooks(IDirect3DSurface9 *surface)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSection(&surface_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, surface->lpVtbl, sizeof(struct IDirect3DSurface9Vtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, surface->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, surface->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, surface->lpVtbl->Release, Release);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.FreePrivateData, surface->lpVtbl->FreePrivateData, FreePrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDevice, surface->lpVtbl->GetDevice, GetDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPriority, surface->lpVtbl->GetPriority, GetPriority);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPrivateData, surface->lpVtbl->GetPrivateData, GetPrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetType, surface->lpVtbl->GetType, GetType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.PreLoad, surface->lpVtbl->PreLoad, PreLoad);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPriority, surface->lpVtbl->SetPriority, SetPriority);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPrivateData, surface->lpVtbl->SetPrivateData, SetPrivateData);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDesc, surface->lpVtbl->GetDesc, GetDesc);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetContainer, surface->lpVtbl->GetContainer, GetContainer);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDC, surface->lpVtbl->GetDC, _GetDC);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.LockRect, surface->lpVtbl->LockRect, LockRect);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.ReleaseDC, surface->lpVtbl->ReleaseDC, _ReleaseDC);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.UnlockRect, surface->lpVtbl->UnlockRect, UnlockRect);

}

typedef struct IDirect3DSurface9Trampoline {
	CONST_VTBL struct IDirect3DSurface9Vtbl *lpVtbl;
	IDirect3DSurface9 *orig_this;
} IDirect3DSurface9Trampoline;


// IUnknown
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	IDirect3DSurface9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineIDirect3DSurface9:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3DSurface9Trampoline*)This)->orig_this, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	IDirect3DSurface9 * This)
{
	HookDebug("TrampolineIDirect3DSurface9:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3DSurface9Trampoline*)This)->orig_this);
}

static ULONG STDMETHODCALLTYPE TrampolineRelease(
	IDirect3DSurface9 * This)
{
	HookDebug("TrampolineIDirect3DSurface9:: Release()\n");

	return orig_vtable.Release(((IDirect3DSurface9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineFreePrivateData(
	IDirect3DSurface9 * This, REFGUID refguid)
{
	HookDebug("TrampolineIDirect3DSurface9:: FreePrivateData()\n");

	return orig_vtable.FreePrivateData(((IDirect3DSurface9Trampoline*)This)->orig_this, refguid);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDevice(
	IDirect3DSurface9 * This, IDirect3DDevice9 **ppDevice)
{
	HookDebug("TrampolineIDirect3DSurface9:: GetDevice()\n");

	return orig_vtable.GetDevice(((IDirect3DSurface9Trampoline*)This)->orig_this, ppDevice);
}

static DWORD STDMETHODCALLTYPE TrampolineGetPriority(
	IDirect3DSurface9 * This)
{
	HookDebug("TrampolineIDirect3DSurface9:: GetPriority()\n");

	return orig_vtable.GetPriority(((IDirect3DSurface9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetPrivateData(
	IDirect3DSurface9 * This,
	      REFGUID refguid,
	 void    *pData,
	 DWORD   *pSizeOfData)
{
	HookDebug("TrampolineIDirect3DSurface9:: GetPrivateData()\n");

	return orig_vtable.GetPrivateData(((IDirect3DSurface9Trampoline*)This)->orig_this, refguid, pData, pSizeOfData);
}

static D3DRESOURCETYPE STDMETHODCALLTYPE TrampolineGetType(
	IDirect3DSurface9 * This)
{
	HookDebug("TrampolineIDirect3DSurface9:: GetType()\n");

	return orig_vtable.GetType(((IDirect3DSurface9Trampoline*)This)->orig_this);
}

static void STDMETHODCALLTYPE TrampolinePreLoad(
	IDirect3DSurface9 * This)
{
	HookDebug("TrampolineIDirect3DSurface9::PreLoad()\n");

	return orig_vtable.PreLoad(((IDirect3DSurface9Trampoline*)This)->orig_this);
}

static DWORD STDMETHODCALLTYPE TrampolineSetPriority(
	IDirect3DSurface9 * This, DWORD PriorityNew)
{
	HookDebug("TrampolineIDirect3DSurface9::SetPriority()\n");

	return orig_vtable.SetPriority(((IDirect3DSurface9Trampoline*)This)->orig_this, PriorityNew);
}

static HRESULT STDMETHODCALLTYPE TrampolineSetPrivateData(
	IDirect3DSurface9 * This,
	       REFGUID refguid,
	 const void    *pData,
	       DWORD   SizeOfData,
	       DWORD   Flags)
{
	HookDebug("TrampolineIDirect3DSurface9::SetPrivateData()\n");

	return orig_vtable.SetPrivateData(((IDirect3DSurface9Trampoline*)This)->orig_this, refguid, pData, SizeOfData, Flags);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDesc(
	IDirect3DSurface9 * This,
	D3DSURFACE_DESC *pDesc)
{
	HookDebug("TrampolineIDirect3DSurface9::GetDesc()\n");

	return orig_vtable.GetDesc(((IDirect3DSurface9Trampoline*)This)->orig_this, pDesc);
}

static HRESULT STDMETHODCALLTYPE TrampolineLockRect(
	IDirect3DSurface9 * This,
	       D3DLOCKED_RECT *pLockedRect,
	  const RECT           *pRect,
	        DWORD          Flags)
{
	HookDebug("TrampolineIDirect3DSurface9::LockRect()\n");

	return orig_vtable.LockRect(((IDirect3DSurface9Trampoline*)This)->orig_this, pLockedRect, pRect, Flags);
}

static HRESULT STDMETHODCALLTYPE TrampolineUnlockRect(
	IDirect3DSurface9 * This)
{
	HookDebug("TrampolineIDirect3DSurface9::UnlockRect()\n");

	return orig_vtable.UnlockRect(((IDirect3DSurface9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetContainer(
	IDirect3DSurface9 * This,
	  REFIID riid,
	 void   **ppContainer)
{
	HookDebug("TrampolineIDirect3DSurface9::GetContainer()\n");

	return orig_vtable.GetContainer(((IDirect3DSurface9Trampoline*)This)->orig_this, riid, ppContainer);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDC(
	IDirect3DSurface9 * This,
	HDC *phdc)
{
	HookDebug("TrampolineIDirect3DSurface9::GetDC()\n");

	return orig_vtable.GetDC(((IDirect3DSurface9Trampoline*)This)->orig_this, phdc);
}

static HRESULT STDMETHODCALLTYPE TrampolineReleaseDC(
	IDirect3DSurface9 * This,
	HDC hdc)
{
	HookDebug("TrampolineIDirect3DSurface9::ReleaseDC()\n");

	return orig_vtable.ReleaseDC(((IDirect3DSurface9Trampoline*)This)->orig_this, hdc);
}

static CONST_VTBL struct IDirect3DSurface9Vtbl trampoline_ex_vtable = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolineGetDevice,
	TrampolineSetPrivateData,
	TrampolineGetPrivateData,
	TrampolineFreePrivateData,
	TrampolineSetPriority,
	TrampolineGetPriority,
	TrampolinePreLoad,
	TrampolineGetType,
	TrampolineGetContainer,
	TrampolineGetDesc,
	TrampolineLockRect,
	TrampolineUnlockRect,
	TrampolineGetDC,
	TrampolineReleaseDC

};

IDirect3DSurface9* hook_surface(IDirect3DSurface9 *orig_surface, IDirect3DSurface9 *hacker_surface)
{
	IDirect3DSurface9Trampoline *trampoline_surface = new IDirect3DSurface9Trampoline();
	trampoline_surface->lpVtbl = &trampoline_ex_vtable;
	trampoline_surface->orig_this = orig_surface;

	install_hooks(orig_surface);
	EnterCriticalSection(&surface_map_lock);
	surface_map[orig_surface] = hacker_surface;
	LeaveCriticalSection(&surface_map_lock);

	return (IDirect3DSurface9*)trampoline_surface;
}
