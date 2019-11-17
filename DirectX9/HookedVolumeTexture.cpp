#include <unordered_map>

#include "HookedVolumeTexture.h"


#include "DLLMainHookDX9.h"


#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

typedef std::unordered_map<IDirect3DVolumeTexture9 *, IDirect3DVolumeTexture9 *> VolumeTextureMap;
static VolumeTextureMap volume_texture_map;
static CRITICAL_SECTION volume_texture_map_lock;

static struct IDirect3DVolumeTexture9Vtbl orig_vtable;

static bool hooks_installed = false;

IDirect3DVolumeTexture9* lookup_hooked_volume_texture(IDirect3DVolumeTexture9 *orig_volume_texture)
{
	VolumeTextureMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&volume_texture_map_lock);
	i = volume_texture_map.find(orig_volume_texture);
	if (i == volume_texture_map.end()) {
		LeaveCriticalSection(&volume_texture_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&volume_texture_map_lock);

	return i->second;
}

// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3DVolumeTexture9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: QueryInterface()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_QueryInterface(volume_texture, riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3DVolumeTexture9 * This)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: AddRef()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_AddRef(volume_texture);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3DVolumeTexture9 * This)
{
	VolumeTextureMap::iterator i;
	ULONG ref;

	HookDebug("HookedVolumeTexture:: Release()\n");

	EnterCriticalSection(&volume_texture_map_lock);
	i = volume_texture_map.find(This);
	if (i != volume_texture_map.end()) {
		ref = IDirect3DVolumeTexture9_Release(i->second);
		if (!ref)
			volume_texture_map.erase(i);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&volume_texture_map_lock);

	return ref;
}

static HRESULT STDMETHODCALLTYPE FreePrivateData(
	IDirect3DVolumeTexture9 * This, REFGUID refguid)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: FreePrivateData()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_FreePrivateData(volume_texture, refguid);

	return orig_vtable.FreePrivateData(This, refguid);
}

static HRESULT STDMETHODCALLTYPE GetDevice(
	IDirect3DVolumeTexture9 * This, IDirect3DDevice9 **ppDevice)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: GetDevice()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_GetDevice(volume_texture, ppDevice);

	return orig_vtable.GetDevice(This, ppDevice);
}

static DWORD STDMETHODCALLTYPE GetPriority(
	IDirect3DVolumeTexture9 * This)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: GetPriority()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_GetPriority(volume_texture);

	return orig_vtable.GetPriority(This);
}

static HRESULT STDMETHODCALLTYPE GetPrivateData(
	IDirect3DVolumeTexture9 * This,
	      REFGUID refguid,
	 void    *pData,
	 DWORD   *pSizeOfData)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: GetPrivateData()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_GetPrivateData(volume_texture, refguid, pData, pSizeOfData);

	return orig_vtable.GetPrivateData(This, refguid, pData, pSizeOfData);
}

static D3DRESOURCETYPE STDMETHODCALLTYPE GetType(
	IDirect3DVolumeTexture9 * This)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: GetType()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_GetType(volume_texture);

	return orig_vtable.GetType(This);
}

static void STDMETHODCALLTYPE PreLoad(
	IDirect3DVolumeTexture9 * This)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: PreLoad()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_PreLoad(volume_texture);

	return orig_vtable.PreLoad(This);
}

static DWORD STDMETHODCALLTYPE SetPriority(
	IDirect3DVolumeTexture9 * This, DWORD PriorityNew)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: SetPriority()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_SetPriority(volume_texture, PriorityNew);

	return orig_vtable.SetPriority(This, PriorityNew);
}


static HRESULT STDMETHODCALLTYPE SetPrivateData(
	IDirect3DVolumeTexture9 * This,
	       REFGUID refguid,
	 const void    *pData,
	       DWORD   SizeOfData,
	       DWORD   Flags)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: SetPrivateData()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_SetPrivateData(volume_texture, refguid, pData, SizeOfData, Flags);

	return orig_vtable.SetPrivateData(This, refguid, pData, SizeOfData, Flags);
}

static VOID STDMETHODCALLTYPE GenerateMipSubLevels(
	IDirect3DVolumeTexture9 * This)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: GenerateMipSubLevels()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_GenerateMipSubLevels(volume_texture);

	return orig_vtable.GenerateMipSubLevels(This);
}

static D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType(
	IDirect3DVolumeTexture9 * This)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: GetAutoGenFilterType()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_GetAutoGenFilterType(volume_texture);

	return orig_vtable.GetAutoGenFilterType(This);
}

static DWORD STDMETHODCALLTYPE GetLevelCount(
	IDirect3DVolumeTexture9 * This)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: GetLevelCount()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_GetLevelCount(volume_texture);

	return orig_vtable.GetLevelCount(This);
}

static DWORD STDMETHODCALLTYPE GetLOD(
	IDirect3DVolumeTexture9 * This)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: GetLOD ()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_GetLOD(volume_texture);

	return orig_vtable.GetLOD(This);
}

static HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(
	IDirect3DVolumeTexture9 * This, D3DTEXTUREFILTERTYPE FilterType)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: SetAutoGenFilterType ()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_SetAutoGenFilterType(volume_texture, FilterType);

	return orig_vtable.SetAutoGenFilterType(This, FilterType);
}

static DWORD STDMETHODCALLTYPE SetLOD(
	IDirect3DVolumeTexture9 * This, DWORD LODNew)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: SetLOD ()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_SetLOD(volume_texture, LODNew);

	return orig_vtable.SetLOD(This, LODNew);
}

static HRESULT STDMETHODCALLTYPE AddDirtyBox(
	IDirect3DVolumeTexture9 * This,
	const D3DBOX *pDirtyBox)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: AddDirtyBox()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_AddDirtyBox(volume_texture, pDirtyBox);

	return orig_vtable.AddDirtyBox(This, pDirtyBox);
}

static HRESULT STDMETHODCALLTYPE GetVolumeLevel(
	IDirect3DVolumeTexture9 * This,
	          UINT             Level,
	 IDirect3DVolume9 **ppVolumeLevel)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: GetVolumeLevel()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_GetVolumeLevel(volume_texture, Level, ppVolumeLevel);

	return orig_vtable.GetVolumeLevel(This, Level, ppVolumeLevel);
}

static HRESULT STDMETHODCALLTYPE GetLevelDesc(
	IDirect3DVolumeTexture9 * This,
	  UINT           Level,
	 D3DVOLUME_DESC *pDesc)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: GetLevelDesc()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_GetLevelDesc(volume_texture, Level, pDesc);

	return orig_vtable.GetLevelDesc(This, Level, pDesc);
}


static HRESULT STDMETHODCALLTYPE LockBox(
	IDirect3DVolumeTexture9 * This,
	        UINT          Level,
	       D3DLOCKED_BOX *pLockedVolume,
	  const D3DBOX        *pBox,
	        DWORD         Flags)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: LockBox()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_LockBox(volume_texture, Level, pLockedVolume, pBox, Flags);

	return orig_vtable.LockBox(This, Level, pLockedVolume, pBox, Flags);
}

static HRESULT STDMETHODCALLTYPE UnlockBox(
	IDirect3DVolumeTexture9 * This,
	 UINT             Level)
{
	IDirect3DVolumeTexture9 *volume_texture = lookup_hooked_volume_texture(This);

	HookDebug("HookedVolumeTexture:: UnlockBox()\n");

	if (volume_texture)
		return IDirect3DVolumeTexture9_UnlockBox(volume_texture, Level);

	return orig_vtable.UnlockBox(This, Level);
}


static void install_hooks(IDirect3DVolumeTexture9 *volume_texture)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSection(&volume_texture_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, volume_texture->lpVtbl, sizeof(struct IDirect3DVolumeTexture9Vtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, volume_texture->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, volume_texture->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, volume_texture->lpVtbl->Release, Release);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.FreePrivateData, volume_texture->lpVtbl->FreePrivateData, FreePrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDevice, volume_texture->lpVtbl->GetDevice, GetDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPriority, volume_texture->lpVtbl->GetPriority, GetPriority);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPrivateData, volume_texture->lpVtbl->GetPrivateData, GetPrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetType, volume_texture->lpVtbl->GetType, GetType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.PreLoad, volume_texture->lpVtbl->PreLoad, PreLoad);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPriority, volume_texture->lpVtbl->SetPriority, SetPriority);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPrivateData, volume_texture->lpVtbl->SetPrivateData, SetPrivateData);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GenerateMipSubLevels, volume_texture->lpVtbl->GenerateMipSubLevels, GenerateMipSubLevels);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetAutoGenFilterType, volume_texture->lpVtbl->GetAutoGenFilterType, GetAutoGenFilterType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetLevelCount, volume_texture->lpVtbl->GetLevelCount, GetLevelCount);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetLOD, volume_texture->lpVtbl->GetLOD, GetLOD);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetAutoGenFilterType, volume_texture->lpVtbl->SetAutoGenFilterType, SetAutoGenFilterType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetLOD, volume_texture->lpVtbl->SetLOD, SetLOD);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddDirtyBox, volume_texture->lpVtbl->AddDirtyBox, AddDirtyBox);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetVolumeLevel, volume_texture->lpVtbl->GetVolumeLevel, GetVolumeLevel);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetLevelDesc, volume_texture->lpVtbl->GetLevelDesc, GetLevelDesc);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.LockBox, volume_texture->lpVtbl->LockBox, LockBox);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.UnlockBox, volume_texture->lpVtbl->UnlockBox, UnlockBox);

}

typedef struct IDirect3DVolumeTexture9Trampoline {
	CONST_VTBL struct IDirect3DVolumeTexture9Vtbl *lpVtbl;
	IDirect3DVolumeTexture9 *orig_this;
} IDirect3DVolumeTexture9Trampoline;


// IUnknown
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	IDirect3DVolumeTexture9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	IDirect3DVolumeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this);
}

static ULONG STDMETHODCALLTYPE TrampolineRelease(
	IDirect3DVolumeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9:: Release()\n");

	return orig_vtable.Release(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineFreePrivateData(
	IDirect3DVolumeTexture9 * This, REFGUID refguid)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9:: FreePrivateData()\n");

	return orig_vtable.FreePrivateData(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this, refguid);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDevice(
	IDirect3DVolumeTexture9 * This, IDirect3DDevice9 **ppDevice)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9:: GetDevice()\n");

	return orig_vtable.GetDevice(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this, ppDevice);
}

static DWORD STDMETHODCALLTYPE TrampolineGetPriority(
	IDirect3DVolumeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9:: GetPriority()\n");

	return orig_vtable.GetPriority(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetPrivateData(
	IDirect3DVolumeTexture9 * This,
	      REFGUID refguid,
	 void    *pData,
	 DWORD   *pSizeOfData)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9:: GetPrivateData()\n");

	return orig_vtable.GetPrivateData(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this, refguid, pData, pSizeOfData);
}

static D3DRESOURCETYPE STDMETHODCALLTYPE TrampolineGetType(
	IDirect3DVolumeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9:: GetType()\n");

	return orig_vtable.GetType(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this);
}

static void STDMETHODCALLTYPE TrampolinePreLoad(
	IDirect3DVolumeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9::PreLoad()\n");

	return orig_vtable.PreLoad(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this);
}

static DWORD STDMETHODCALLTYPE TrampolineSetPriority(
	IDirect3DVolumeTexture9 * This, DWORD PriorityNew)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9::SetPriority()\n");

	return orig_vtable.SetPriority(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this, PriorityNew);
}

static HRESULT STDMETHODCALLTYPE TrampolineSetPrivateData(
	IDirect3DVolumeTexture9 * This,
	       REFGUID refguid,
	 const void    *pData,
	       DWORD   SizeOfData,
	       DWORD   Flags)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9::SetPrivateData()\n");

	return orig_vtable.SetPrivateData(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this, refguid, pData, SizeOfData, Flags);
}

static VOID STDMETHODCALLTYPE TrampolineGenerateMipSubLevels(
	IDirect3DVolumeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9::GenerateMipSubLevels()\n");

	return orig_vtable.GenerateMipSubLevels(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this);
}

static D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE TrampolineGetAutoGenFilterType(
	IDirect3DVolumeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9::GetAutoGenFilterType()\n");

	return orig_vtable.GetAutoGenFilterType(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this);
}

static DWORD STDMETHODCALLTYPE TrampolineGetLevelCount(
	IDirect3DVolumeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9::GetLevelCount()\n");

	return orig_vtable.GetLevelCount(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this);
}

static DWORD STDMETHODCALLTYPE TrampolineGetLOD(
	IDirect3DVolumeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9::GetLOD()\n");

	return orig_vtable.GetLOD(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineSetAutoGenFilterType(
	IDirect3DVolumeTexture9 * This, D3DTEXTUREFILTERTYPE FilterType)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9::SetAutoGenFilterType()\n");

	return orig_vtable.SetAutoGenFilterType(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this, FilterType);
}

static DWORD STDMETHODCALLTYPE TrampolineSetLOD(
	IDirect3DVolumeTexture9 * This, DWORD LODNew)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9::SetLOD()\n");

	return orig_vtable.SetLOD(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this, LODNew);
}

static HRESULT STDMETHODCALLTYPE TrampolineAddDirtyBox(
	IDirect3DVolumeTexture9 * This,
	 const D3DBOX *pDirtyBox)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9::AddDirtyBox()\n");

	return orig_vtable.AddDirtyBox(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this, pDirtyBox);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetVolumeLevel(
	IDirect3DVolumeTexture9 * This,
	          UINT             Level,
	 IDirect3DVolume9 **ppVolumeLevel)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9::GetVolumeLevel()\n");

	return orig_vtable.GetVolumeLevel(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this, Level, ppVolumeLevel);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetLevelDesc(
	IDirect3DVolumeTexture9 * This,
	  UINT            Level,
	 D3DVOLUME_DESC *pDesc)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9::GetLevelDesc()\n");

	return orig_vtable.GetLevelDesc(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this, Level, pDesc);
}

static HRESULT STDMETHODCALLTYPE TrampolineLockBox(
	IDirect3DVolumeTexture9 * This,
	        UINT          Level,
	       D3DLOCKED_BOX *pLockedVolume,
	  const D3DBOX        *pBox,
	        DWORD         Flags)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9::LockBox()\n");

	return orig_vtable.LockBox(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this, Level, pLockedVolume, pBox, Flags);
}

static HRESULT STDMETHODCALLTYPE TrampolineUnlockBox(
	IDirect3DVolumeTexture9 * This,
	 UINT             Level)
{
	HookDebug("TrampolineIDirect3DVolumeTexture9::UnlockBox()\n");

	return orig_vtable.UnlockBox(((IDirect3DVolumeTexture9Trampoline*)This)->orig_this, Level);
}


static CONST_VTBL struct IDirect3DVolumeTexture9Vtbl trampoline_ex_vtable = {
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
	TrampolineSetLOD,
	TrampolineGetLOD,
	TrampolineGetLevelCount,
	TrampolineSetAutoGenFilterType,
	TrampolineGetAutoGenFilterType,
	TrampolineGenerateMipSubLevels,
	TrampolineGetLevelDesc,
	TrampolineGetVolumeLevel,
	TrampolineLockBox,
	TrampolineUnlockBox,
	TrampolineAddDirtyBox

};

IDirect3DVolumeTexture9* hook_volume_texture(IDirect3DVolumeTexture9 *orig_volume_texture, IDirect3DVolumeTexture9 *hacker_volume_texture)
{
	IDirect3DVolumeTexture9Trampoline *trampoline_volume_texture = new IDirect3DVolumeTexture9Trampoline();
	trampoline_volume_texture->lpVtbl = &trampoline_ex_vtable;
	trampoline_volume_texture->orig_this = orig_volume_texture;

	install_hooks(orig_volume_texture);
	EnterCriticalSection(&volume_texture_map_lock);
	volume_texture_map[orig_volume_texture] = hacker_volume_texture;
	LeaveCriticalSection(&volume_texture_map_lock);

	return (IDirect3DVolumeTexture9*)trampoline_volume_texture;
}
