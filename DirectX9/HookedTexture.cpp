#include <unordered_map>

#include "HookedTexture.h"


#include "DLLMainHookDX9.h"


#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

typedef std::unordered_map<IDirect3DTexture9 *, IDirect3DTexture9 *> TextureMap;
static TextureMap texture_map;
static CRITICAL_SECTION texture_map_lock;

static struct IDirect3DTexture9Vtbl orig_vtable;

static bool hooks_installed = false;

IDirect3DTexture9* lookup_hooked_texture(IDirect3DTexture9 *orig_texture)
{
	TextureMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&texture_map_lock);
	i = texture_map.find(orig_texture);
	if (i == texture_map.end()) {
		LeaveCriticalSection(&texture_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&texture_map_lock);

	return i->second;
}

// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3DTexture9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: QueryInterface()\n");

	if (texture)
		return IDirect3DTexture9_QueryInterface(texture, riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3DTexture9 * This)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: AddRef()\n");

	if (texture)
		return IDirect3DTexture9_AddRef(texture);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3DTexture9 * This)
{
	TextureMap::iterator i;
	ULONG ref;

	HookDebug("HookedTexture:: Release()\n");

	EnterCriticalSection(&texture_map_lock);
	i = texture_map.find(This);
	if (i != texture_map.end()) {
		ref = IDirect3DTexture9_Release(i->second);
		if (!ref)
			texture_map.erase(i);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&texture_map_lock);

	return ref;
}

static HRESULT STDMETHODCALLTYPE FreePrivateData(
	IDirect3DTexture9 * This, REFGUID refguid)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: FreePrivateData()\n");

	if (texture)
		return IDirect3DTexture9_FreePrivateData(texture, refguid);

	return orig_vtable.FreePrivateData(This, refguid);
}

static HRESULT STDMETHODCALLTYPE GetDevice(
	IDirect3DTexture9 * This, IDirect3DDevice9 **ppDevice)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: GetDevice()\n");

	if (texture)
		return IDirect3DTexture9_GetDevice(texture, ppDevice);

	return orig_vtable.GetDevice(This, ppDevice);
}

static DWORD STDMETHODCALLTYPE GetPriority(
	IDirect3DTexture9 * This)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: GetPriority()\n");

	if (texture)
		return IDirect3DTexture9_GetPriority(texture);

	return orig_vtable.GetPriority(This);
}

static HRESULT STDMETHODCALLTYPE GetPrivateData(
	IDirect3DTexture9 * This,
	      REFGUID refguid,
	 void    *pData,
	 DWORD   *pSizeOfData)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: GetPrivateData()\n");

	if (texture)
		return IDirect3DTexture9_GetPrivateData(texture, refguid, pData, pSizeOfData);

	return orig_vtable.GetPrivateData(This, refguid, pData, pSizeOfData);
}

static D3DRESOURCETYPE STDMETHODCALLTYPE GetType(
	IDirect3DTexture9 * This)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: GetType()\n");

	if (texture)
		return IDirect3DTexture9_GetType(texture);

	return orig_vtable.GetType(This);
}

static void STDMETHODCALLTYPE PreLoad(
	IDirect3DTexture9 * This)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: PreLoad()\n");

	if (texture)
		return IDirect3DTexture9_PreLoad(texture);

	return orig_vtable.PreLoad(This);
}

static DWORD STDMETHODCALLTYPE SetPriority(
	IDirect3DTexture9 * This, DWORD PriorityNew)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: SetPriority()\n");

	if (texture)
		return IDirect3DTexture9_SetPriority(texture, PriorityNew);

	return orig_vtable.SetPriority(This, PriorityNew);
}


static HRESULT STDMETHODCALLTYPE SetPrivateData(
	IDirect3DTexture9 * This,
	       REFGUID refguid,
	 const void    *pData,
	       DWORD   SizeOfData,
	       DWORD   Flags)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: SetPrivateData()\n");

	if (texture)
		return IDirect3DTexture9_SetPrivateData(texture, refguid, pData, SizeOfData, Flags);

	return orig_vtable.SetPrivateData(This, refguid, pData, SizeOfData, Flags);
}

static VOID STDMETHODCALLTYPE GenerateMipSubLevels(
	IDirect3DTexture9 * This)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: GenerateMipSubLevels()\n");

	if (texture)
		return IDirect3DTexture9_GenerateMipSubLevels(texture);

	return orig_vtable.GenerateMipSubLevels(This);
}

static D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType(
	IDirect3DTexture9 * This)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: GetAutoGenFilterType()\n");

	if (texture)
		return IDirect3DTexture9_GetAutoGenFilterType(texture);

	return orig_vtable.GetAutoGenFilterType(This);
}

static DWORD STDMETHODCALLTYPE GetLevelCount(
	IDirect3DTexture9 * This)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: GetLevelCount()\n");

	if (texture)
		return IDirect3DTexture9_GetLevelCount(texture);

	return orig_vtable.GetLevelCount(This);
}

static DWORD STDMETHODCALLTYPE GetLOD(
	IDirect3DTexture9 * This)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: GetLOD ()\n");

	if (texture)
		return IDirect3DTexture9_GetLOD(texture);

	return orig_vtable.GetLOD(This);
}

static HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(
	IDirect3DTexture9 * This, D3DTEXTUREFILTERTYPE FilterType)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: SetAutoGenFilterType ()\n");

	if (texture)
		return IDirect3DTexture9_SetAutoGenFilterType(texture, FilterType);

	return orig_vtable.SetAutoGenFilterType(This, FilterType);
}

static DWORD STDMETHODCALLTYPE SetLOD(
	IDirect3DTexture9 * This, DWORD LODNew)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: SetLOD ()\n");

	if (texture)
		return IDirect3DTexture9_SetLOD(texture, LODNew);

	return orig_vtable.SetLOD(This, LODNew);
}

static HRESULT STDMETHODCALLTYPE AddDirtyRect(
	IDirect3DTexture9 * This,
	const RECT *pDirtyRect)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: AddDirtyRect()\n");

	if (texture)
		return IDirect3DTexture9_AddDirtyRect(texture, pDirtyRect);

	return orig_vtable.AddDirtyRect(This, pDirtyRect);
}

static HRESULT STDMETHODCALLTYPE GetSurfaceLevel(
	IDirect3DTexture9 * This,
	          UINT              Level,
	 IDirect3DSurface9 **ppSurfaceLevel)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: GetSurfaceLevel()\n");

	if (texture)
		return IDirect3DTexture9_GetSurfaceLevel(texture, Level, ppSurfaceLevel);

	return orig_vtable.GetSurfaceLevel(This, Level, ppSurfaceLevel);
}

static HRESULT STDMETHODCALLTYPE GetLevelDesc(
	IDirect3DTexture9 * This,
	  UINT            Level,
	 D3DSURFACE_DESC *pDesc)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: GetLevelDesc()\n");

	if (texture)
		return IDirect3DTexture9_GetLevelDesc(texture, Level, pDesc);

	return orig_vtable.GetLevelDesc(This, Level, pDesc);
}


static HRESULT STDMETHODCALLTYPE LockRect(
	IDirect3DTexture9 * This,
	        UINT           Level,
	       D3DLOCKED_RECT *pLockedRect,
	  const RECT           *pRect,
	        DWORD          Flags)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: LockRect()\n");

	if (texture)
		return IDirect3DTexture9_LockRect(texture, Level, pLockedRect, pRect, Flags);

	return orig_vtable.LockRect(This, Level, pLockedRect, pRect, Flags);
}

static HRESULT STDMETHODCALLTYPE UnlockRect(
	IDirect3DTexture9 * This,
	 UINT Level)
{
	IDirect3DTexture9 *texture = lookup_hooked_texture(This);

	HookDebug("HookedTexture:: UnlockRect()\n");

	if (texture)
		return IDirect3DTexture9_UnlockRect(texture, Level);

	return orig_vtable.UnlockRect(This, Level);
}


static void install_hooks(IDirect3DTexture9 *texture)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSection(&texture_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, texture->lpVtbl, sizeof(struct IDirect3DTexture9Vtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, texture->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, texture->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, texture->lpVtbl->Release, Release);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.FreePrivateData, texture->lpVtbl->FreePrivateData, FreePrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDevice, texture->lpVtbl->GetDevice, GetDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPriority, texture->lpVtbl->GetPriority, GetPriority);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPrivateData, texture->lpVtbl->GetPrivateData, GetPrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetType, texture->lpVtbl->GetType, GetType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.PreLoad, texture->lpVtbl->PreLoad, PreLoad);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPriority, texture->lpVtbl->SetPriority, SetPriority);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPrivateData, texture->lpVtbl->SetPrivateData, SetPrivateData);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GenerateMipSubLevels, texture->lpVtbl->GenerateMipSubLevels, GenerateMipSubLevels);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetAutoGenFilterType, texture->lpVtbl->GetAutoGenFilterType, GetAutoGenFilterType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetLevelCount, texture->lpVtbl->GetLevelCount, GetLevelCount);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetLOD, texture->lpVtbl->GetLOD, GetLOD);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetAutoGenFilterType, texture->lpVtbl->SetAutoGenFilterType, SetAutoGenFilterType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetLOD, texture->lpVtbl->SetLOD, SetLOD);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddDirtyRect, texture->lpVtbl->AddDirtyRect, AddDirtyRect);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetSurfaceLevel, texture->lpVtbl->GetSurfaceLevel, GetSurfaceLevel);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetLevelDesc, texture->lpVtbl->GetLevelDesc, GetLevelDesc);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.LockRect, texture->lpVtbl->LockRect, LockRect);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.UnlockRect, texture->lpVtbl->UnlockRect, UnlockRect);

}

typedef struct IDirect3DTexture9Trampoline {
	CONST_VTBL struct IDirect3DTexture9Vtbl *lpVtbl;
	IDirect3DTexture9 *orig_this;
} IDirect3DTexture9Trampoline;


// IUnknown
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	IDirect3DTexture9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineIDirect3DTexture9:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3DTexture9Trampoline*)This)->orig_this, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	IDirect3DTexture9 * This)
{
	HookDebug("TrampolineIDirect3DTexture9:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3DTexture9Trampoline*)This)->orig_this);
}

static ULONG STDMETHODCALLTYPE TrampolineRelease(
	IDirect3DTexture9 * This)
{
	HookDebug("TrampolineIDirect3DTexture9:: Release()\n");

	return orig_vtable.Release(((IDirect3DTexture9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineFreePrivateData(
	IDirect3DTexture9 * This, REFGUID refguid)
{
	HookDebug("TrampolineIDirect3DTexture9:: FreePrivateData()\n");

	return orig_vtable.FreePrivateData(((IDirect3DTexture9Trampoline*)This)->orig_this, refguid);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDevice(
	IDirect3DTexture9 * This, IDirect3DDevice9 **ppDevice)
{
	HookDebug("TrampolineIDirect3DTexture9:: GetDevice()\n");

	return orig_vtable.GetDevice(((IDirect3DTexture9Trampoline*)This)->orig_this, ppDevice);
}

static DWORD STDMETHODCALLTYPE TrampolineGetPriority(
	IDirect3DTexture9 * This)
{
	HookDebug("TrampolineIDirect3DTexture9:: GetPriority()\n");

	return orig_vtable.GetPriority(((IDirect3DTexture9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetPrivateData(
	IDirect3DTexture9 * This,
	      REFGUID refguid,
	 void    *pData,
	 DWORD   *pSizeOfData)
{
	HookDebug("TrampolineIDirect3DTexture9:: GetPrivateData()\n");

	return orig_vtable.GetPrivateData(((IDirect3DTexture9Trampoline*)This)->orig_this, refguid, pData, pSizeOfData);
}

static D3DRESOURCETYPE STDMETHODCALLTYPE TrampolineGetType(
	IDirect3DTexture9 * This)
{
	HookDebug("TrampolineIDirect3DTexture9:: GetType()\n");

	return orig_vtable.GetType(((IDirect3DTexture9Trampoline*)This)->orig_this);
}

static void STDMETHODCALLTYPE TrampolinePreLoad(
	IDirect3DTexture9 * This)
{
	HookDebug("TrampolineIDirect3DTexture9::PreLoad()\n");

	return orig_vtable.PreLoad(((IDirect3DTexture9Trampoline*)This)->orig_this);
}

static DWORD STDMETHODCALLTYPE TrampolineSetPriority(
	IDirect3DTexture9 * This, DWORD PriorityNew)
{
	HookDebug("TrampolineIDirect3DTexture9::SetPriority()\n");

	return orig_vtable.SetPriority(((IDirect3DTexture9Trampoline*)This)->orig_this, PriorityNew);
}

static HRESULT STDMETHODCALLTYPE TrampolineSetPrivateData(
	IDirect3DTexture9 * This,
	       REFGUID refguid,
	 const void    *pData,
	       DWORD   SizeOfData,
	       DWORD   Flags)
{
	HookDebug("TrampolineIDirect3DTexture9::SetPrivateData()\n");

	return orig_vtable.SetPrivateData(((IDirect3DTexture9Trampoline*)This)->orig_this, refguid, pData, SizeOfData, Flags);
}

static VOID STDMETHODCALLTYPE TrampolineGenerateMipSubLevels(
	IDirect3DTexture9 * This)
{
	HookDebug("TrampolineIDirect3DTexture9::GenerateMipSubLevels()\n");

	return orig_vtable.GenerateMipSubLevels(((IDirect3DTexture9Trampoline*)This)->orig_this);
}

static D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE TrampolineGetAutoGenFilterType(
	IDirect3DTexture9 * This)
{
	HookDebug("TrampolineIDirect3DTexture9::GetAutoGenFilterType()\n");

	return orig_vtable.GetAutoGenFilterType(((IDirect3DTexture9Trampoline*)This)->orig_this);
}

static DWORD STDMETHODCALLTYPE TrampolineGetLevelCount(
	IDirect3DTexture9 * This)
{
	HookDebug("TrampolineIDirect3DTexture9::GetLevelCount()\n");

	return orig_vtable.GetLevelCount(((IDirect3DTexture9Trampoline*)This)->orig_this);
}

static DWORD STDMETHODCALLTYPE TrampolineGetLOD(
	IDirect3DTexture9 * This)
{
	HookDebug("TrampolineIDirect3DTexture9::GetLOD()\n");

	return orig_vtable.GetLOD(((IDirect3DTexture9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineSetAutoGenFilterType(
	IDirect3DTexture9 * This, D3DTEXTUREFILTERTYPE FilterType)
{
	HookDebug("TrampolineIDirect3DTexture9::SetAutoGenFilterType()\n");

	return orig_vtable.SetAutoGenFilterType(((IDirect3DTexture9Trampoline*)This)->orig_this, FilterType);
}

static DWORD STDMETHODCALLTYPE TrampolineSetLOD(
	IDirect3DTexture9 * This, DWORD LODNew)
{
	HookDebug("TrampolineIDirect3DTexture9::SetLOD()\n");

	return orig_vtable.SetLOD(((IDirect3DTexture9Trampoline*)This)->orig_this, LODNew);
}

static HRESULT STDMETHODCALLTYPE TrampolineAddDirtyRect(
	IDirect3DTexture9 * This,
	const RECT *pDirtyRect)
{
	HookDebug("TrampolineIDirect3DTexture9::AddDirtyRect()\n");

	return orig_vtable.AddDirtyRect(((IDirect3DTexture9Trampoline*)This)->orig_this, pDirtyRect);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetSurfaceLevel(
	IDirect3DTexture9 * This,
	          UINT              Level,
	 IDirect3DSurface9 **ppSurfaceLevel)
{
	HookDebug("TrampolineIDirect3DTexture9::GetSurfaceLevel()\n");

	return orig_vtable.GetSurfaceLevel(((IDirect3DTexture9Trampoline*)This)->orig_this, Level, ppSurfaceLevel);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetLevelDesc(
	IDirect3DTexture9 * This,
	  UINT            Level,
	 D3DSURFACE_DESC *pDesc)
{
	HookDebug("TrampolineIDirect3DTexture9::GetLevelDesc()\n");

	return orig_vtable.GetLevelDesc(((IDirect3DTexture9Trampoline*)This)->orig_this, Level, pDesc);
}

static HRESULT STDMETHODCALLTYPE TrampolineLockRect(
	IDirect3DTexture9 * This,
	        UINT           Level,
	       D3DLOCKED_RECT *pLockedRect,
	  const RECT           *pRect,
	        DWORD          Flags)
{
	HookDebug("TrampolineIDirect3DTexture9::LockRect()\n");

	return orig_vtable.LockRect(((IDirect3DTexture9Trampoline*)This)->orig_this, Level, pLockedRect, pRect, Flags);
}

static HRESULT STDMETHODCALLTYPE TrampolineUnlockRect(
	IDirect3DTexture9 * This,
	 UINT             Level)
{
	HookDebug("TrampolineIDirect3DTexture9::UnlockRect()\n");

	return orig_vtable.UnlockRect(((IDirect3DTexture9Trampoline*)This)->orig_this, Level);
}


static CONST_VTBL struct IDirect3DTexture9Vtbl trampoline_ex_vtable = {
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
	TrampolineGetSurfaceLevel,
	TrampolineLockRect,
	TrampolineUnlockRect,
	TrampolineAddDirtyRect

};

IDirect3DTexture9* hook_texture(IDirect3DTexture9 *orig_texture, IDirect3DTexture9 *hacker_texture)
{
	IDirect3DTexture9Trampoline *trampoline_texture = new IDirect3DTexture9Trampoline();
	trampoline_texture->lpVtbl = &trampoline_ex_vtable;
	trampoline_texture->orig_this = orig_texture;

	install_hooks(orig_texture);
	EnterCriticalSection(&texture_map_lock);
	texture_map[orig_texture] = hacker_texture;
	LeaveCriticalSection(&texture_map_lock);

	return (IDirect3DTexture9*)trampoline_texture;
}
