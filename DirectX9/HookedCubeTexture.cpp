#include <unordered_map>
#include "HookedCubeTexture.h"


#include "DLLMainHookDX9.h"

#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

typedef std::unordered_map<IDirect3DCubeTexture9 *, IDirect3DCubeTexture9 *> CubeTextureMap;
static CubeTextureMap cube_texture_map;
static CRITICAL_SECTION cube_texture_map_lock;

static struct IDirect3DCubeTexture9Vtbl orig_vtable;

static bool hooks_installed = false;

IDirect3DCubeTexture9* lookup_hooked_cube_texture(IDirect3DCubeTexture9 *orig_cube_texture)
{
	CubeTextureMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&cube_texture_map_lock);
	i = cube_texture_map.find(orig_cube_texture);
	if (i == cube_texture_map.end()) {
		LeaveCriticalSection(&cube_texture_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&cube_texture_map_lock);

	return i->second;
}

// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3DCubeTexture9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: QueryInterface()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_QueryInterface(cube_texture, riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3DCubeTexture9 * This)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: AddRef()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_AddRef(cube_texture);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3DCubeTexture9 * This)
{
	CubeTextureMap::iterator i;
	ULONG ref;

	HookDebug("HookedCubeTexture:: Release()\n");

	EnterCriticalSection(&cube_texture_map_lock);
	i = cube_texture_map.find(This);
	if (i != cube_texture_map.end()) {
		ref = IDirect3DCubeTexture9_Release(i->second);
		if (!ref)
			cube_texture_map.erase(i);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&cube_texture_map_lock);

	return ref;
}

static HRESULT STDMETHODCALLTYPE FreePrivateData(
	IDirect3DCubeTexture9 * This, REFGUID refguid)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: FreePrivateData()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_FreePrivateData(cube_texture, refguid);

	return orig_vtable.FreePrivateData(This, refguid);
}

static HRESULT STDMETHODCALLTYPE GetDevice(
	IDirect3DCubeTexture9 * This, IDirect3DDevice9 **ppDevice)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: GetDevice()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_GetDevice(cube_texture, ppDevice);

	return orig_vtable.GetDevice(This, ppDevice);
}

static DWORD STDMETHODCALLTYPE GetPriority(
	IDirect3DCubeTexture9 * This)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: GetPriority()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_GetPriority(cube_texture);

	return orig_vtable.GetPriority(This);
}

static HRESULT STDMETHODCALLTYPE GetPrivateData(
	IDirect3DCubeTexture9 * This,
	      REFGUID refguid,
	 void    *pData,
	 DWORD   *pSizeOfData)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: GetPrivateData()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_GetPrivateData(cube_texture, refguid, pData, pSizeOfData);

	return orig_vtable.GetPrivateData(This, refguid, pData, pSizeOfData);
}

static D3DRESOURCETYPE STDMETHODCALLTYPE GetType(
	IDirect3DCubeTexture9 * This)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: GetType()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_GetType(cube_texture);

	return orig_vtable.GetType(This);
}

static void STDMETHODCALLTYPE PreLoad(
	IDirect3DCubeTexture9 * This)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: PreLoad()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_PreLoad(cube_texture);

	return orig_vtable.PreLoad(This);
}

static DWORD STDMETHODCALLTYPE SetPriority(
	IDirect3DCubeTexture9 * This, DWORD PriorityNew)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: SetPriority()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_SetPriority(cube_texture, PriorityNew);

	return orig_vtable.SetPriority(This, PriorityNew);
}


static HRESULT STDMETHODCALLTYPE SetPrivateData(
	IDirect3DCubeTexture9 * This,
	       REFGUID refguid,
	 const void    *pData,
	       DWORD   SizeOfData,
	       DWORD   Flags)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: SetPrivateData()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_SetPrivateData(cube_texture, refguid, pData, SizeOfData, Flags);

	return orig_vtable.SetPrivateData(This, refguid, pData, SizeOfData, Flags);
}

static VOID STDMETHODCALLTYPE GenerateMipSubLevels(
	IDirect3DCubeTexture9 * This)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: GenerateMipSubLevels()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_GenerateMipSubLevels(cube_texture);

	return orig_vtable.GenerateMipSubLevels(This);
}

static D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType(
	IDirect3DCubeTexture9 * This)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: GetAutoGenFilterType()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_GetAutoGenFilterType(cube_texture);

	return orig_vtable.GetAutoGenFilterType(This);
}

static DWORD STDMETHODCALLTYPE GetLevelCount(
	IDirect3DCubeTexture9 * This)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: GetLevelCount()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_GetLevelCount(cube_texture);

	return orig_vtable.GetLevelCount(This);
}

static DWORD STDMETHODCALLTYPE GetLOD(
	IDirect3DCubeTexture9 * This)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: GetLOD ()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_GetLOD(cube_texture);

	return orig_vtable.GetLOD(This);
}

static HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(
	IDirect3DCubeTexture9 * This, D3DTEXTUREFILTERTYPE FilterType)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: SetAutoGenFilterType ()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_SetAutoGenFilterType(cube_texture, FilterType);

	return orig_vtable.SetAutoGenFilterType(This, FilterType);
}

static DWORD STDMETHODCALLTYPE SetLOD(
	IDirect3DCubeTexture9 * This, DWORD LODNew)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: SetLOD ()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_SetLOD(cube_texture, LODNew);

	return orig_vtable.SetLOD(This, LODNew);
}

static HRESULT STDMETHODCALLTYPE AddDirtyRect(
	IDirect3DCubeTexture9 * This,
	       D3DCUBEMAP_FACES FaceType,
	 const RECT             *pDirtyRect)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: AddDirtyRect()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_AddDirtyRect(cube_texture, FaceType, pDirtyRect);

	return orig_vtable.AddDirtyRect(This, FaceType, pDirtyRect);
}

static HRESULT STDMETHODCALLTYPE GetCubeMapSurface(
	IDirect3DCubeTexture9 * This,
	  D3DCUBEMAP_FACES  FaceType,
	  UINT              Level,
	 IDirect3DSurface9 **ppCubeMapSurface)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: GetCubeMapSurface()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_GetCubeMapSurface(cube_texture, FaceType, Level, ppCubeMapSurface);

	return orig_vtable.GetCubeMapSurface(This, FaceType, Level, ppCubeMapSurface);
}

static HRESULT STDMETHODCALLTYPE GetLevelDesc(
	IDirect3DCubeTexture9 * This,
	  UINT            Level,
	 D3DSURFACE_DESC *pDesc)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: GetLevelDesc()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_GetLevelDesc(cube_texture, Level, pDesc);

	return orig_vtable.GetLevelDesc(This, Level, pDesc);
}


static HRESULT STDMETHODCALLTYPE LockRect(
	IDirect3DCubeTexture9 * This,
	        D3DCUBEMAP_FACES FaceType,
	        UINT             Level,
	       D3DLOCKED_RECT   *pLockedRect,
	  const RECT             *pRect,
	        DWORD            Flags)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: LockRect()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_LockRect(cube_texture, FaceType, Level, pLockedRect, pRect, Flags);

	return orig_vtable.LockRect(This, FaceType, Level, pLockedRect, pRect, Flags);
}

static HRESULT STDMETHODCALLTYPE UnlockRect(
	IDirect3DCubeTexture9 * This,
	 D3DCUBEMAP_FACES FaceType,
	 UINT             Level)
{
	IDirect3DCubeTexture9 *cube_texture = lookup_hooked_cube_texture(This);

	HookDebug("HookedCubeTexture:: UnlockRect()\n");

	if (cube_texture)
		return IDirect3DCubeTexture9_UnlockRect(cube_texture, FaceType, Level);

	return orig_vtable.UnlockRect(This, FaceType, Level);
}


static void install_hooks(IDirect3DCubeTexture9 *cube_texture)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSection(&cube_texture_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, cube_texture->lpVtbl, sizeof(struct IDirect3DCubeTexture9Vtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, cube_texture->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, cube_texture->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, cube_texture->lpVtbl->Release, Release);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.FreePrivateData, cube_texture->lpVtbl->FreePrivateData, FreePrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDevice, cube_texture->lpVtbl->GetDevice, GetDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPriority, cube_texture->lpVtbl->GetPriority, GetPriority);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPrivateData, cube_texture->lpVtbl->GetPrivateData, GetPrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetType, cube_texture->lpVtbl->GetType, GetType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.PreLoad, cube_texture->lpVtbl->PreLoad, PreLoad);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPriority, cube_texture->lpVtbl->SetPriority, SetPriority);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPrivateData, cube_texture->lpVtbl->SetPrivateData, SetPrivateData);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GenerateMipSubLevels, cube_texture->lpVtbl->GenerateMipSubLevels, GenerateMipSubLevels);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetAutoGenFilterType, cube_texture->lpVtbl->GetAutoGenFilterType, GetAutoGenFilterType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetLevelCount, cube_texture->lpVtbl->GetLevelCount, GetLevelCount);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetLOD, cube_texture->lpVtbl->GetLOD, GetLOD);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetAutoGenFilterType, cube_texture->lpVtbl->SetAutoGenFilterType, SetAutoGenFilterType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetLOD, cube_texture->lpVtbl->SetLOD, SetLOD);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddDirtyRect, cube_texture->lpVtbl->AddDirtyRect, AddDirtyRect);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetCubeMapSurface, cube_texture->lpVtbl->GetCubeMapSurface, GetCubeMapSurface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetLevelDesc, cube_texture->lpVtbl->GetLevelDesc, GetLevelDesc);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.LockRect, cube_texture->lpVtbl->LockRect, LockRect);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.UnlockRect, cube_texture->lpVtbl->UnlockRect, UnlockRect);

}

typedef struct IDirect3DCubeTexture9Trampoline {
	CONST_VTBL struct IDirect3DCubeTexture9Vtbl *lpVtbl;
	IDirect3DCubeTexture9 *orig_this;
} IDirect3DCubeTexture9Trampoline;


// IUnknown
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	IDirect3DCubeTexture9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineIDirect3DCubeTexture9:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3DCubeTexture9Trampoline*)This)->orig_this, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	IDirect3DCubeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DCubeTexture9:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3DCubeTexture9Trampoline*)This)->orig_this);
}

static ULONG STDMETHODCALLTYPE TrampolineRelease(
	IDirect3DCubeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DCubeTexture9:: Release()\n");

	return orig_vtable.Release(((IDirect3DCubeTexture9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineFreePrivateData(
	IDirect3DCubeTexture9 * This, REFGUID refguid)
{
	HookDebug("TrampolineIDirect3DCubeTexture9:: FreePrivateData()\n");

	return orig_vtable.FreePrivateData(((IDirect3DCubeTexture9Trampoline*)This)->orig_this, refguid);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDevice(
	IDirect3DCubeTexture9 * This, IDirect3DDevice9 **ppDevice)
{
	HookDebug("TrampolineIDirect3DCubeTexture9:: GetDevice()\n");

	return orig_vtable.GetDevice(((IDirect3DCubeTexture9Trampoline*)This)->orig_this, ppDevice);
}

static DWORD STDMETHODCALLTYPE TrampolineGetPriority(
	IDirect3DCubeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DCubeTexture9:: GetPriority()\n");

	return orig_vtable.GetPriority(((IDirect3DCubeTexture9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetPrivateData(
	IDirect3DCubeTexture9 * This,
	      REFGUID refguid,
	 void    *pData,
	 DWORD   *pSizeOfData)
{
	HookDebug("TrampolineIDirect3DCubeTexture9:: GetPrivateData()\n");

	return orig_vtable.GetPrivateData(((IDirect3DCubeTexture9Trampoline*)This)->orig_this, refguid, pData, pSizeOfData);
}

static D3DRESOURCETYPE STDMETHODCALLTYPE TrampolineGetType(
	IDirect3DCubeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DCubeTexture9:: GetType()\n");

	return orig_vtable.GetType(((IDirect3DCubeTexture9Trampoline*)This)->orig_this);
}

static void STDMETHODCALLTYPE TrampolinePreLoad(
	IDirect3DCubeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DCubeTexture9::PreLoad()\n");

	return orig_vtable.PreLoad(((IDirect3DCubeTexture9Trampoline*)This)->orig_this);
}

static DWORD STDMETHODCALLTYPE TrampolineSetPriority(
	IDirect3DCubeTexture9 * This, DWORD PriorityNew)
{
	HookDebug("TrampolineIDirect3DCubeTexture9::SetPriority()\n");

	return orig_vtable.SetPriority(((IDirect3DCubeTexture9Trampoline*)This)->orig_this, PriorityNew);
}

static HRESULT STDMETHODCALLTYPE TrampolineSetPrivateData(
	IDirect3DCubeTexture9 * This,
	       REFGUID refguid,
	 const void    *pData,
	       DWORD   SizeOfData,
	       DWORD   Flags)
{
	HookDebug("TrampolineIDirect3DCubeTexture9::SetPrivateData()\n");

	return orig_vtable.SetPrivateData(((IDirect3DCubeTexture9Trampoline*)This)->orig_this, refguid, pData, SizeOfData, Flags);
}

static VOID STDMETHODCALLTYPE TrampolineGenerateMipSubLevels(
	IDirect3DCubeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DCubeTexture9::GenerateMipSubLevels()\n");

	return orig_vtable.GenerateMipSubLevels(((IDirect3DCubeTexture9Trampoline*)This)->orig_this);
}

static D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE TrampolineGetAutoGenFilterType(
	IDirect3DCubeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DCubeTexture9::GetAutoGenFilterType()\n");

	return orig_vtable.GetAutoGenFilterType(((IDirect3DCubeTexture9Trampoline*)This)->orig_this);
}

static DWORD STDMETHODCALLTYPE TrampolineGetLevelCount(
	IDirect3DCubeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DCubeTexture9::GetLevelCount()\n");

	return orig_vtable.GetLevelCount(((IDirect3DCubeTexture9Trampoline*)This)->orig_this);
}

static DWORD STDMETHODCALLTYPE TrampolineGetLOD(
	IDirect3DCubeTexture9 * This)
{
	HookDebug("TrampolineIDirect3DCubeTexture9::GetLOD()\n");

	return orig_vtable.GetLOD(((IDirect3DCubeTexture9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineSetAutoGenFilterType(
	IDirect3DCubeTexture9 * This, D3DTEXTUREFILTERTYPE FilterType)
{
	HookDebug("TrampolineIDirect3DCubeTexture9::SetAutoGenFilterType()\n");

	return orig_vtable.SetAutoGenFilterType(((IDirect3DCubeTexture9Trampoline*)This)->orig_this, FilterType);
}

static DWORD STDMETHODCALLTYPE TrampolineSetLOD(
	IDirect3DCubeTexture9 * This, DWORD LODNew)
{
	HookDebug("TrampolineIDirect3DCubeTexture9::SetLOD()\n");

	return orig_vtable.SetLOD(((IDirect3DCubeTexture9Trampoline*)This)->orig_this, LODNew);
}

static HRESULT STDMETHODCALLTYPE TrampolineAddDirtyRect(
	IDirect3DCubeTexture9 * This,
	       D3DCUBEMAP_FACES FaceType,
	 const RECT             *pDirtyRect)
{
	HookDebug("TrampolineIDirect3DCubeTexture9::AddDirtyRect()\n");

	return orig_vtable.AddDirtyRect(((IDirect3DCubeTexture9Trampoline*)This)->orig_this, FaceType, pDirtyRect);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetCubeMapSurface(
	IDirect3DCubeTexture9 * This,
	  D3DCUBEMAP_FACES  FaceType,
	  UINT              Level,
	 IDirect3DSurface9 **ppCubeMapSurface)
{
	HookDebug("TrampolineIDirect3DCubeTexture9::GetCubeMapSurface()\n");

	return orig_vtable.GetCubeMapSurface(((IDirect3DCubeTexture9Trampoline*)This)->orig_this, FaceType, Level, ppCubeMapSurface);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetLevelDesc(
	IDirect3DCubeTexture9 * This,
	  UINT            Level,
	 D3DSURFACE_DESC *pDesc)
{
	HookDebug("TrampolineIDirect3DCubeTexture9::GetLevelDesc()\n");

	return orig_vtable.GetLevelDesc(((IDirect3DCubeTexture9Trampoline*)This)->orig_this, Level, pDesc);
}

static HRESULT STDMETHODCALLTYPE TrampolineLockRect(
	IDirect3DCubeTexture9 * This,
	        D3DCUBEMAP_FACES FaceType,
	        UINT             Level,
	       D3DLOCKED_RECT   *pLockedRect,
	  const RECT             *pRect,
	        DWORD            Flags)
{
	HookDebug("TrampolineIDirect3DCubeTexture9::LockRect()\n");

	return orig_vtable.LockRect(((IDirect3DCubeTexture9Trampoline*)This)->orig_this, FaceType, Level, pLockedRect, pRect, Flags);
}

static HRESULT STDMETHODCALLTYPE TrampolineUnlockRect(
	IDirect3DCubeTexture9 * This,
	 D3DCUBEMAP_FACES FaceType,
	 UINT             Level)
{
	HookDebug("TrampolineIDirect3DCubeTexture9::UnlockRect()\n");

	return orig_vtable.UnlockRect(((IDirect3DCubeTexture9Trampoline*)This)->orig_this, FaceType, Level);
}


static CONST_VTBL struct IDirect3DCubeTexture9Vtbl trampoline_ex_vtable = {
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
	TrampolineGetCubeMapSurface,
	TrampolineLockRect,
	TrampolineUnlockRect,
	TrampolineAddDirtyRect

};

IDirect3DCubeTexture9* hook_cube_texture(IDirect3DCubeTexture9 *orig_cube_texture, IDirect3DCubeTexture9 *hacker_cube_texture)
{
	IDirect3DCubeTexture9Trampoline *trampoline_cube_texture = new IDirect3DCubeTexture9Trampoline();
	trampoline_cube_texture->lpVtbl = &trampoline_ex_vtable;
	trampoline_cube_texture->orig_this = orig_cube_texture;

	install_hooks(orig_cube_texture);
	EnterCriticalSection(&cube_texture_map_lock);
	cube_texture_map[orig_cube_texture] = hacker_cube_texture;
	LeaveCriticalSection(&cube_texture_map_lock);

	return (IDirect3DCubeTexture9*)trampoline_cube_texture;
}
