#include <unordered_map>

#include "HookedVolume.h"


#include "DLLMainHookDX9.h"


#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

typedef std::unordered_map<IDirect3DVolume9 *, IDirect3DVolume9 *> volumeMap;
static volumeMap volume_map;
static CRITICAL_SECTION volume_map_lock;

static struct IDirect3DVolume9Vtbl orig_vtable;

static bool hooks_installed = false;

IDirect3DVolume9* lookup_hooked_volume(IDirect3DVolume9 *orig_volume)
{
	volumeMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&volume_map_lock);
	i = volume_map.find(orig_volume);
	if (i == volume_map.end()) {
		LeaveCriticalSection(&volume_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&volume_map_lock);

	return i->second;
}

// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3DVolume9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3DVolume9 *volume = lookup_hooked_volume(This);

	HookDebug("HookedVolume:: QueryInterface()\n");

	if (volume)
		return IDirect3DVolume9_QueryInterface(volume, riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3DVolume9 * This)
{
	IDirect3DVolume9 *volume = lookup_hooked_volume(This);

	HookDebug("HookedVolume:: AddRef()\n");

	if (volume)
		return IDirect3DVolume9_AddRef(volume);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3DVolume9 * This)
{
	volumeMap::iterator i;
	ULONG ref;

	HookDebug("HookedVolume:: Release()\n");

	EnterCriticalSection(&volume_map_lock);
	i = volume_map.find(This);
	if (i != volume_map.end()) {
		ref = IDirect3DVolume9_Release(i->second);
		if (!ref)
			volume_map.erase(i);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&volume_map_lock);

	return ref;
}

static HRESULT STDMETHODCALLTYPE FreePrivateData(
	IDirect3DVolume9 * This, REFGUID refguid)
{
	IDirect3DVolume9 *volume = lookup_hooked_volume(This);

	HookDebug("HookedVolume:: FreePrivateData()\n");

	if (volume)
		return IDirect3DVolume9_FreePrivateData(volume, refguid);

	return orig_vtable.FreePrivateData(This, refguid);
}

static HRESULT STDMETHODCALLTYPE GetDevice(
	IDirect3DVolume9 * This, IDirect3DDevice9 **ppDevice)
{
	IDirect3DVolume9 *volume = lookup_hooked_volume(This);

	HookDebug("HookedVolume:: GetDevice()\n");

	if (volume)
		return IDirect3DVolume9_GetDevice(volume, ppDevice);

	return orig_vtable.GetDevice(This, ppDevice);
}

static HRESULT STDMETHODCALLTYPE GetPrivateData(
	IDirect3DVolume9 * This,
	      REFGUID refguid,
	 void    *pData,
	 DWORD   *pSizeOfData)
{
	IDirect3DVolume9 *volume = lookup_hooked_volume(This);

	HookDebug("HookedVolume:: GetPrivateData()\n");

	if (volume)
		return IDirect3DVolume9_GetPrivateData(volume, refguid, pData, pSizeOfData);

	return orig_vtable.GetPrivateData(This, refguid, pData, pSizeOfData);
}

static HRESULT STDMETHODCALLTYPE SetPrivateData(
	IDirect3DVolume9 * This,
	       REFGUID refguid,
	 const void    *pData,
	       DWORD   SizeOfData,
	       DWORD   Flags)
{
	IDirect3DVolume9 *volume = lookup_hooked_volume(This);

	HookDebug("HookedVolume:: SetPrivateData()\n");

	if (volume)
		return IDirect3DVolume9_SetPrivateData(volume, refguid, pData, SizeOfData, Flags);

	return orig_vtable.SetPrivateData(This, refguid, pData, SizeOfData, Flags);
}

static HRESULT STDMETHODCALLTYPE GetDesc(
	IDirect3DVolume9 * This,
	D3DVOLUME_DESC *pDesc)
{
	IDirect3DVolume9 *volume = lookup_hooked_volume(This);

	HookDebug("HookedVolume:: GetDesc()\n");

	if (volume)
		return IDirect3DVolume9_GetDesc(volume, pDesc);

	return orig_vtable.GetDesc(This, pDesc);
}

static HRESULT STDMETHODCALLTYPE LockBox(
	IDirect3DVolume9 * This,
	       D3DLOCKED_BOX *pLockedVolume,
	  const D3DBOX        *pBox,
	        DWORD         Flags)
{
	IDirect3DVolume9 *volume = lookup_hooked_volume(This);

	HookDebug("HookedVolume:: LockBox()\n");

	if (volume)
		return IDirect3DVolume9_LockBox(volume, pLockedVolume, pBox, Flags);

	return orig_vtable.LockBox(This, pLockedVolume, pBox, Flags);
}

static HRESULT STDMETHODCALLTYPE UnlockBox(
	IDirect3DVolume9 * This)
{
	IDirect3DVolume9 *volume = lookup_hooked_volume(This);

	HookDebug("HookedVolume:: UnlockBox()\n");

	if (volume)
		return IDirect3DVolume9_UnlockBox(volume);

	return orig_vtable.UnlockBox(This);
}

static HRESULT STDMETHODCALLTYPE GetContainer(
	IDirect3DVolume9 * This,
	  REFIID riid,
	 void   **ppContainer)
{
	IDirect3DVolume9 *volume = lookup_hooked_volume(This);

	HookDebug("HookedVolume:: GetContainer()\n");

	if (volume)
		return IDirect3DVolume9_GetContainer(volume, riid, ppContainer);

	return orig_vtable.GetContainer(This, riid, ppContainer);
}


static void install_hooks(IDirect3DVolume9 *volume)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSection(&volume_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, volume->lpVtbl, sizeof(struct IDirect3DVolume9Vtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, volume->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, volume->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, volume->lpVtbl->Release, Release);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.FreePrivateData, volume->lpVtbl->FreePrivateData, FreePrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDevice, volume->lpVtbl->GetDevice, GetDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPrivateData, volume->lpVtbl->GetPrivateData, GetPrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPrivateData, volume->lpVtbl->SetPrivateData, SetPrivateData);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDesc, volume->lpVtbl->GetDesc, GetDesc);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetContainer, volume->lpVtbl->GetContainer, GetContainer);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.LockBox, volume->lpVtbl->LockBox, LockBox);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.UnlockBox, volume->lpVtbl->UnlockBox, UnlockBox);

}

typedef struct IDirect3DVolume9Trampoline {
	CONST_VTBL struct IDirect3DVolume9Vtbl *lpVtbl;
	IDirect3DVolume9 *orig_this;
} IDirect3DVolume9Trampoline;


// IUnknown
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	IDirect3DVolume9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineIDirect3DVolume9:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3DVolume9Trampoline*)This)->orig_this, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	IDirect3DVolume9 * This)
{
	HookDebug("TrampolineIDirect3DVolume9:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3DVolume9Trampoline*)This)->orig_this);
}

static ULONG STDMETHODCALLTYPE TrampolineRelease(
	IDirect3DVolume9 * This)
{
	HookDebug("TrampolineIDirect3DVolume9:: Release()\n");

	return orig_vtable.Release(((IDirect3DVolume9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineFreePrivateData(
	IDirect3DVolume9 * This, REFGUID refguid)
{
	HookDebug("TrampolineIDirect3DVolume9:: FreePrivateData()\n");

	return orig_vtable.FreePrivateData(((IDirect3DVolume9Trampoline*)This)->orig_this, refguid);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDevice(
	IDirect3DVolume9 * This, IDirect3DDevice9 **ppDevice)
{
	HookDebug("TrampolineIDirect3DVolume9:: GetDevice()\n");

	return orig_vtable.GetDevice(((IDirect3DVolume9Trampoline*)This)->orig_this, ppDevice);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetPrivateData(
	IDirect3DVolume9 * This,
	      REFGUID refguid,
	 void    *pData,
	 DWORD   *pSizeOfData)
{
	HookDebug("TrampolineIDirect3DVolume9:: GetPrivateData()\n");

	return orig_vtable.GetPrivateData(((IDirect3DVolume9Trampoline*)This)->orig_this, refguid, pData, pSizeOfData);
}

static HRESULT STDMETHODCALLTYPE TrampolineSetPrivateData(
	IDirect3DVolume9 * This,
	       REFGUID refguid,
	 const void    *pData,
	       DWORD   SizeOfData,
	       DWORD   Flags)
{
	HookDebug("TrampolineIDirect3DVolume9::SetPrivateData()\n");

	return orig_vtable.SetPrivateData(((IDirect3DVolume9Trampoline*)This)->orig_this, refguid, pData, SizeOfData, Flags);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDesc(
	IDirect3DVolume9 * This,
	D3DVOLUME_DESC *pDesc)
{
	HookDebug("TrampolineIDirect3DVolume9::GetDesc()\n");

	return orig_vtable.GetDesc(((IDirect3DVolume9Trampoline*)This)->orig_this, pDesc);
}

static HRESULT STDMETHODCALLTYPE TrampolineLockBox(
	IDirect3DVolume9 * This,
	       D3DLOCKED_BOX *pLockedVolume,
	  const D3DBOX        *pBox,
	        DWORD         Flags)
{
	HookDebug("TrampolineIDirect3DVolume9::LockBox()\n");

	return orig_vtable.LockBox(((IDirect3DVolume9Trampoline*)This)->orig_this, pLockedVolume, pBox, Flags);
}

static HRESULT STDMETHODCALLTYPE TrampolineUnlockBox(
	IDirect3DVolume9 * This)
{
	HookDebug("TrampolineIDirect3DVolume9::UnlockBox()\n");

	return orig_vtable.UnlockBox(((IDirect3DVolume9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetContainer(
	IDirect3DVolume9 * This,
	  REFIID riid,
	 void   **ppContainer)
{
	HookDebug("TrampolineIDirect3DVolume9::GetContainer()\n");

	return orig_vtable.GetContainer(((IDirect3DVolume9Trampoline*)This)->orig_this, riid, ppContainer);
}

static CONST_VTBL struct IDirect3DVolume9Vtbl trampoline_ex_vtable = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolineGetDevice,
	TrampolineSetPrivateData,
	TrampolineGetPrivateData,
	TrampolineFreePrivateData,
	TrampolineGetContainer,
	TrampolineGetDesc,
	TrampolineLockBox,
	TrampolineUnlockBox
};

IDirect3DVolume9* hook_volume(IDirect3DVolume9 *orig_volume, IDirect3DVolume9 *hacker_volume)
{
	IDirect3DVolume9Trampoline *trampoline_volume = new IDirect3DVolume9Trampoline();
	trampoline_volume->lpVtbl = &trampoline_ex_vtable;
	trampoline_volume->orig_this = orig_volume;

	install_hooks(orig_volume);
	EnterCriticalSection(&volume_map_lock);
	volume_map[orig_volume] = hacker_volume;
	LeaveCriticalSection(&volume_map_lock);

	return (IDirect3DVolume9*)trampoline_volume;
}
