#include <unordered_map>

#include "HookedIndexBuffer.h"


#include "DLLMainHookDX9.h"


#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

typedef std::unordered_map<IDirect3DIndexBuffer9 *, IDirect3DIndexBuffer9 *> IndexBufferMap;
static IndexBufferMap index_buffer_map;
static CRITICAL_SECTION index_buffer_map_lock;

static struct IDirect3DIndexBuffer9Vtbl orig_vtable;

static bool hooks_installed = false;

IDirect3DIndexBuffer9* lookup_hooked_index_buffer(IDirect3DIndexBuffer9 *orig_index_buffer)
{
	IndexBufferMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&index_buffer_map_lock);
	i = index_buffer_map.find(orig_index_buffer);
	if (i == index_buffer_map.end()) {
		LeaveCriticalSection(&index_buffer_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&index_buffer_map_lock);

	return i->second;
}

// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3DIndexBuffer9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3DIndexBuffer9 *index_buffer = lookup_hooked_index_buffer(This);

	HookDebug("HookedIndexBuffer:: QueryInterface()\n");

	if (index_buffer)
		return IDirect3DIndexBuffer9_QueryInterface(index_buffer, riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3DIndexBuffer9 * This)
{
	IDirect3DIndexBuffer9 *index_buffer = lookup_hooked_index_buffer(This);

	HookDebug("HookedIndexBuffer:: AddRef()\n");

	if (index_buffer)
		return IDirect3DIndexBuffer9_AddRef(index_buffer);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3DIndexBuffer9 * This)
{
	IndexBufferMap::iterator i;
	ULONG ref;

	HookDebug("HookedIndexBuffer:: Release()\n");

	EnterCriticalSection(&index_buffer_map_lock);
	i = index_buffer_map.find(This);
	if (i != index_buffer_map.end()) {
		ref = IDirect3DIndexBuffer9_Release(i->second);
		if (!ref)
			index_buffer_map.erase(i);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&index_buffer_map_lock);

	return ref;
}

static HRESULT STDMETHODCALLTYPE FreePrivateData(
	IDirect3DIndexBuffer9 * This, REFGUID refguid)
{
	IDirect3DIndexBuffer9 *index_buffer = lookup_hooked_index_buffer(This);

	HookDebug("HookedIndexBuffer:: FreePrivateData()\n");

	if (index_buffer)
		return IDirect3DIndexBuffer9_FreePrivateData(index_buffer, refguid);

	return orig_vtable.FreePrivateData(This, refguid);
}

static HRESULT STDMETHODCALLTYPE GetDevice(
	IDirect3DIndexBuffer9 * This, IDirect3DDevice9 **ppDevice)
{
	IDirect3DIndexBuffer9 *index_buffer = lookup_hooked_index_buffer(This);

	HookDebug("HookedIndexBuffer:: GetDevice()\n");

	if (index_buffer)
		return IDirect3DIndexBuffer9_GetDevice(index_buffer, ppDevice);

	return orig_vtable.GetDevice(This, ppDevice);
}

static DWORD STDMETHODCALLTYPE GetPriority(
	IDirect3DIndexBuffer9 * This)
{
	IDirect3DIndexBuffer9 *index_buffer = lookup_hooked_index_buffer(This);

	HookDebug("HookedIndexBuffer:: GetPriority()\n");

	if (index_buffer)
		return IDirect3DIndexBuffer9_GetPriority(index_buffer);

	return orig_vtable.GetPriority(This);
}

static HRESULT STDMETHODCALLTYPE GetPrivateData(
	IDirect3DIndexBuffer9 * This,
	      REFGUID refguid,
	 void    *pData,
	 DWORD   *pSizeOfData)
{
	IDirect3DIndexBuffer9 *index_buffer = lookup_hooked_index_buffer(This);

	HookDebug("HookedIndexBuffer:: GetPrivateData()\n");

	if (index_buffer)
		return IDirect3DIndexBuffer9_GetPrivateData(index_buffer, refguid, pData, pSizeOfData);

	return orig_vtable.GetPrivateData(This, refguid, pData, pSizeOfData);
}

static D3DRESOURCETYPE STDMETHODCALLTYPE GetType(
	IDirect3DIndexBuffer9 * This)
{
	IDirect3DIndexBuffer9 *index_buffer = lookup_hooked_index_buffer(This);

	HookDebug("HookedIndexBuffer:: GetType()\n");

	if (index_buffer)
		return IDirect3DIndexBuffer9_GetType(index_buffer);

	return orig_vtable.GetType(This);
}

static void STDMETHODCALLTYPE PreLoad(
	IDirect3DIndexBuffer9 * This)
{
	IDirect3DIndexBuffer9 *index_buffer = lookup_hooked_index_buffer(This);

	HookDebug("HookedIndexBuffer:: PreLoad()\n");

	if (index_buffer)
		return IDirect3DIndexBuffer9_PreLoad(index_buffer);

	return orig_vtable.PreLoad(This);
}

static DWORD STDMETHODCALLTYPE SetPriority(
	IDirect3DIndexBuffer9 * This, DWORD PriorityNew)
{
	IDirect3DIndexBuffer9 *index_buffer = lookup_hooked_index_buffer(This);

	HookDebug("HookedIndexBuffer:: SetPriority()\n");

	if (index_buffer)
		return IDirect3DIndexBuffer9_SetPriority(index_buffer, PriorityNew);

	return orig_vtable.SetPriority(This, PriorityNew);
}


static HRESULT STDMETHODCALLTYPE SetPrivateData(
	IDirect3DIndexBuffer9 * This,
	       REFGUID refguid,
	 const void    *pData,
	       DWORD   SizeOfData,
	       DWORD   Flags)
{
	IDirect3DIndexBuffer9 *index_buffer = lookup_hooked_index_buffer(This);

	HookDebug("HookedIndexBuffer:: SetPrivateData()\n");

	if (index_buffer)
		return IDirect3DIndexBuffer9_SetPrivateData(index_buffer, refguid, pData, SizeOfData, Flags);

	return orig_vtable.SetPrivateData(This, refguid, pData, SizeOfData, Flags);
}

static HRESULT STDMETHODCALLTYPE GetDesc(
	IDirect3DIndexBuffer9 * This,
	D3DINDEXBUFFER_DESC *pDesc)
{
	IDirect3DIndexBuffer9 *index_buffer = lookup_hooked_index_buffer(This);

	HookDebug("HookedIndexBuffer:: GetDesc()\n");

	if (index_buffer)
		return IDirect3DIndexBuffer9_GetDesc(index_buffer, pDesc);

	return orig_vtable.GetDesc(This, pDesc);
}

static HRESULT STDMETHODCALLTYPE Lock(
	IDirect3DIndexBuffer9 * This,
	  UINT  OffsetToLock,
	  UINT  SizeToLock,
	 VOID  **ppbData,
	  DWORD Flags)
{
	IDirect3DIndexBuffer9 *index_buffer = lookup_hooked_index_buffer(This);

	HookDebug("HookedIndexBuffer:: Lock()\n");

	if (index_buffer)
		return IDirect3DIndexBuffer9_Lock(index_buffer, OffsetToLock, SizeToLock, ppbData, Flags);

	return orig_vtable.Lock(This, OffsetToLock, SizeToLock, ppbData, Flags);
}

static HRESULT STDMETHODCALLTYPE Unlock(
	IDirect3DIndexBuffer9 * This)
{
	IDirect3DIndexBuffer9 *index_buffer = lookup_hooked_index_buffer(This);

	HookDebug("HookedIndexBuffer:: Unlock()\n");

	if (index_buffer)
		return IDirect3DIndexBuffer9_Unlock(index_buffer);

	return orig_vtable.Unlock(This);
}

static void install_hooks(IDirect3DIndexBuffer9 *index_buffer)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSection(&index_buffer_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, index_buffer->lpVtbl, sizeof(struct IDirect3DIndexBuffer9Vtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, index_buffer->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, index_buffer->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, index_buffer->lpVtbl->Release, Release);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.FreePrivateData, index_buffer->lpVtbl->FreePrivateData, FreePrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDevice, index_buffer->lpVtbl->GetDevice, GetDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPriority, index_buffer->lpVtbl->GetPriority, GetPriority);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPrivateData, index_buffer->lpVtbl->GetPrivateData, GetPrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetType, index_buffer->lpVtbl->GetType, GetType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.PreLoad, index_buffer->lpVtbl->PreLoad, PreLoad);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPriority, index_buffer->lpVtbl->SetPriority, SetPriority);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPrivateData, index_buffer->lpVtbl->SetPrivateData, SetPrivateData);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDesc, index_buffer->lpVtbl->GetDesc, GetDesc);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Lock, index_buffer->lpVtbl->Lock, Lock);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Unlock, index_buffer->lpVtbl->Unlock, Unlock);

}

typedef struct IDirect3DIndexBuffer9Trampoline {
	CONST_VTBL struct IDirect3DIndexBuffer9Vtbl *lpVtbl;
	IDirect3DIndexBuffer9 *orig_this;
} IDirect3DIndexBuffer9Trampoline;


// IUnknown
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	IDirect3DIndexBuffer9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineIDirect3DIndexBuffer9:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3DIndexBuffer9Trampoline*)This)->orig_this, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	IDirect3DIndexBuffer9 * This)
{
	HookDebug("TrampolineIDirect3DIndexBuffer9:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3DIndexBuffer9Trampoline*)This)->orig_this);
}

static ULONG STDMETHODCALLTYPE TrampolineRelease(
	IDirect3DIndexBuffer9 * This)
{
	HookDebug("TrampolineIDirect3DIndexBuffer9:: Release()\n");

	return orig_vtable.Release(((IDirect3DIndexBuffer9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineFreePrivateData(
	IDirect3DIndexBuffer9 * This, REFGUID refguid)
{
	HookDebug("TrampolineIDirect3DIndexBuffer9:: FreePrivateData()\n");

	return orig_vtable.FreePrivateData(((IDirect3DIndexBuffer9Trampoline*)This)->orig_this, refguid);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDevice(
	IDirect3DIndexBuffer9 * This, IDirect3DDevice9 **ppDevice)
{
	HookDebug("TrampolineIDirect3DIndexBuffer9:: GetDevice()\n");

	return orig_vtable.GetDevice(((IDirect3DIndexBuffer9Trampoline*)This)->orig_this, ppDevice);
}

static DWORD STDMETHODCALLTYPE TrampolineGetPriority(
	IDirect3DIndexBuffer9 * This)
{
	HookDebug("TrampolineIDirect3DIndexBuffer9:: GetPriority()\n");

	return orig_vtable.GetPriority(((IDirect3DIndexBuffer9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetPrivateData(
	IDirect3DIndexBuffer9 * This,
	      REFGUID refguid,
	 void    *pData,
	 DWORD   *pSizeOfData)
{
	HookDebug("TrampolineIDirect3DIndexBuffer9:: GetPrivateData()\n");

	return orig_vtable.GetPrivateData(((IDirect3DIndexBuffer9Trampoline*)This)->orig_this, refguid, pData, pSizeOfData);
}

static D3DRESOURCETYPE STDMETHODCALLTYPE TrampolineGetType(
	IDirect3DIndexBuffer9 * This)
{
	HookDebug("TrampolineIDirect3DIndexBuffer9:: GetType()\n");

	return orig_vtable.GetType(((IDirect3DIndexBuffer9Trampoline*)This)->orig_this);
}

static void STDMETHODCALLTYPE TrampolinePreLoad(
	IDirect3DIndexBuffer9 * This)
{
	HookDebug("TrampolineIDirect3DIndexBuffer9::PreLoad()\n");

	return orig_vtable.PreLoad(((IDirect3DIndexBuffer9Trampoline*)This)->orig_this);
}

static DWORD STDMETHODCALLTYPE TrampolineSetPriority(
	IDirect3DIndexBuffer9 * This, DWORD PriorityNew)
{
	HookDebug("TrampolineIDirect3DIndexBuffer9::SetPriority()\n");

	return orig_vtable.SetPriority(((IDirect3DIndexBuffer9Trampoline*)This)->orig_this, PriorityNew);
}

static HRESULT STDMETHODCALLTYPE TrampolineSetPrivateData(
	IDirect3DIndexBuffer9 * This,
	       REFGUID refguid,
	 const void    *pData,
	       DWORD   SizeOfData,
	       DWORD   Flags)
{
	HookDebug("TrampolineIDirect3DIndexBuffer9::SetPrivateData()\n");

	return orig_vtable.SetPrivateData(((IDirect3DIndexBuffer9Trampoline*)This)->orig_this, refguid, pData, SizeOfData, Flags);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDesc(
	IDirect3DIndexBuffer9 * This,
	D3DINDEXBUFFER_DESC *pDesc)
{
	HookDebug("TrampolineIDirect3DIndexBuffer9::GetDesc()\n");

	return orig_vtable.GetDesc(((IDirect3DIndexBuffer9Trampoline*)This)->orig_this, pDesc);
}

static HRESULT STDMETHODCALLTYPE TrampolineLock(
	IDirect3DIndexBuffer9 * This,
	  UINT  OffsetToLock,
	  UINT  SizeToLock,
	 VOID  **ppbData,
	  DWORD Flags)
{
	HookDebug("TrampolineIDirect3DIndexBuffer9::Lock()\n");

	return orig_vtable.Lock(((IDirect3DIndexBuffer9Trampoline*)This)->orig_this, OffsetToLock, SizeToLock, ppbData, Flags);
}

static HRESULT STDMETHODCALLTYPE TrampolineUnlock(
	IDirect3DIndexBuffer9 * This)
{
	HookDebug("TrampolineIDirect3DIndexBuffer9::Unlock()\n");

	return orig_vtable.Unlock(((IDirect3DIndexBuffer9Trampoline*)This)->orig_this);
}

static CONST_VTBL struct IDirect3DIndexBuffer9Vtbl trampoline_ex_vtable = {
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
	TrampolineLock,
	TrampolineUnlock,
	TrampolineGetDesc
};

IDirect3DIndexBuffer9* hook_index_buffer(IDirect3DIndexBuffer9 *orig_index_buffer, IDirect3DIndexBuffer9 *hacker_index_buffer)
{
	IDirect3DIndexBuffer9Trampoline *trampoline_index_buffer = new IDirect3DIndexBuffer9Trampoline();
	trampoline_index_buffer->lpVtbl = &trampoline_ex_vtable;
	trampoline_index_buffer->orig_this = orig_index_buffer;

	install_hooks(orig_index_buffer);
	EnterCriticalSection(&index_buffer_map_lock);
	index_buffer_map[orig_index_buffer] = hacker_index_buffer;
	LeaveCriticalSection(&index_buffer_map_lock);

	return (IDirect3DIndexBuffer9*)trampoline_index_buffer;
}
