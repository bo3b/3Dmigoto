#include <unordered_map>

#include "HookedVertexBuffer.h"


#include "DLLMainHookDX9.h"


#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

typedef std::unordered_map<IDirect3DVertexBuffer9 *, IDirect3DVertexBuffer9 *> VertexBufferMap;
static VertexBufferMap vertex_buffer_map;
static CRITICAL_SECTION vertex_buffer_map_lock;

static struct IDirect3DVertexBuffer9Vtbl orig_vtable;

static bool hooks_installed = false;

IDirect3DVertexBuffer9* lookup_hooked_vertex_buffer(IDirect3DVertexBuffer9 *orig_vertex_buffer)
{
	VertexBufferMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&vertex_buffer_map_lock);
	i = vertex_buffer_map.find(orig_vertex_buffer);
	if (i == vertex_buffer_map.end()) {
		LeaveCriticalSection(&vertex_buffer_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&vertex_buffer_map_lock);

	return i->second;
}

// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3DVertexBuffer9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3DVertexBuffer9 *vertex_buffer = lookup_hooked_vertex_buffer(This);

	HookDebug("HookedVertexBuffer:: QueryInterface()\n");

	if (vertex_buffer)
		return IDirect3DVertexBuffer9_QueryInterface(vertex_buffer, riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3DVertexBuffer9 * This)
{
	IDirect3DVertexBuffer9 *vertex_buffer = lookup_hooked_vertex_buffer(This);

	HookDebug("HookedVertexBuffer:: AddRef()\n");

	if (vertex_buffer)
		return IDirect3DVertexBuffer9_AddRef(vertex_buffer);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3DVertexBuffer9 * This)
{
	VertexBufferMap::iterator i;
	ULONG ref;

	HookDebug("HookedVertexBuffer:: Release()\n");

	EnterCriticalSection(&vertex_buffer_map_lock);
	i = vertex_buffer_map.find(This);
	if (i != vertex_buffer_map.end()) {
		ref = IDirect3DVertexBuffer9_Release(i->second);
		if (!ref)
			vertex_buffer_map.erase(i);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&vertex_buffer_map_lock);

	return ref;
}

static HRESULT STDMETHODCALLTYPE FreePrivateData(
	IDirect3DVertexBuffer9 * This, REFGUID refguid)
{
	IDirect3DVertexBuffer9 *vertex_buffer = lookup_hooked_vertex_buffer(This);

	HookDebug("HookedVertexBuffer:: FreePrivateData()\n");

	if (vertex_buffer)
		return IDirect3DVertexBuffer9_FreePrivateData(vertex_buffer, refguid);

	return orig_vtable.FreePrivateData(This, refguid);
}

static HRESULT STDMETHODCALLTYPE GetDevice(
	IDirect3DVertexBuffer9 * This, IDirect3DDevice9 **ppDevice)
{
	IDirect3DVertexBuffer9 *vertex_buffer = lookup_hooked_vertex_buffer(This);

	HookDebug("HookedVertexBuffer:: GetDevice()\n");

	if (vertex_buffer)
		return IDirect3DVertexBuffer9_GetDevice(vertex_buffer, ppDevice);

	return orig_vtable.GetDevice(This, ppDevice);
}

static DWORD STDMETHODCALLTYPE GetPriority(
	IDirect3DVertexBuffer9 * This)
{
	IDirect3DVertexBuffer9 *vertex_buffer = lookup_hooked_vertex_buffer(This);

	HookDebug("HookedVertexBuffer:: GetPriority()\n");

	if (vertex_buffer)
		return IDirect3DVertexBuffer9_GetPriority(vertex_buffer);

	return orig_vtable.GetPriority(This);
}

static HRESULT STDMETHODCALLTYPE GetPrivateData(
	IDirect3DVertexBuffer9 * This,
	      REFGUID refguid,
	 void    *pData,
	 DWORD   *pSizeOfData)
{
	IDirect3DVertexBuffer9 *vertex_buffer = lookup_hooked_vertex_buffer(This);

	HookDebug("HookedVertexBuffer:: GetPrivateData()\n");

	if (vertex_buffer)
		return IDirect3DVertexBuffer9_GetPrivateData(vertex_buffer, refguid, pData, pSizeOfData);

	return orig_vtable.GetPrivateData(This, refguid, pData, pSizeOfData);
}

static D3DRESOURCETYPE STDMETHODCALLTYPE GetType(
	IDirect3DVertexBuffer9 * This)
{
	IDirect3DVertexBuffer9 *vertex_buffer = lookup_hooked_vertex_buffer(This);

	HookDebug("HookedVertexBuffer:: GetType()\n");

	if (vertex_buffer)
		return IDirect3DVertexBuffer9_GetType(vertex_buffer);

	return orig_vtable.GetType(This);
}

static void STDMETHODCALLTYPE PreLoad(
	IDirect3DVertexBuffer9 * This)
{
	IDirect3DVertexBuffer9 *vertex_buffer = lookup_hooked_vertex_buffer(This);

	HookDebug("HookedVertexBuffer:: PreLoad()\n");

	if (vertex_buffer)
		return IDirect3DVertexBuffer9_PreLoad(vertex_buffer);

	return orig_vtable.PreLoad(This);
}

static DWORD STDMETHODCALLTYPE SetPriority(
	IDirect3DVertexBuffer9 * This, DWORD PriorityNew)
{
	IDirect3DVertexBuffer9 *vertex_buffer = lookup_hooked_vertex_buffer(This);

	HookDebug("HookedVertexBuffer:: SetPriority()\n");

	if (vertex_buffer)
		return IDirect3DVertexBuffer9_SetPriority(vertex_buffer, PriorityNew);

	return orig_vtable.SetPriority(This, PriorityNew);
}


static HRESULT STDMETHODCALLTYPE SetPrivateData(
	IDirect3DVertexBuffer9 * This,
	       REFGUID refguid,
	 const void    *pData,
	       DWORD   SizeOfData,
	       DWORD   Flags)
{
	IDirect3DVertexBuffer9 *vertex_buffer = lookup_hooked_vertex_buffer(This);

	HookDebug("HookedVertexBuffer:: SetPrivateData()\n");

	if (vertex_buffer)
		return IDirect3DVertexBuffer9_SetPrivateData(vertex_buffer, refguid, pData, SizeOfData, Flags);

	return orig_vtable.SetPrivateData(This, refguid, pData, SizeOfData, Flags);
}

static HRESULT STDMETHODCALLTYPE GetDesc(
	IDirect3DVertexBuffer9 * This,
	D3DVERTEXBUFFER_DESC *pDesc)
{
	IDirect3DVertexBuffer9 *vertex_buffer = lookup_hooked_vertex_buffer(This);

	HookDebug("HookedVertexBuffer:: GetDesc()\n");

	if (vertex_buffer)
		return IDirect3DVertexBuffer9_GetDesc(vertex_buffer, pDesc);

	return orig_vtable.GetDesc(This, pDesc);
}

static HRESULT STDMETHODCALLTYPE Lock(
	IDirect3DVertexBuffer9 * This,
	  UINT  OffsetToLock,
	  UINT  SizeToLock,
	 VOID  **ppbData,
	  DWORD Flags)
{
	IDirect3DVertexBuffer9 *vertex_buffer = lookup_hooked_vertex_buffer(This);

	HookDebug("HookedVertexBuffer:: Lock()\n");

	if (vertex_buffer)
		return IDirect3DVertexBuffer9_Lock(vertex_buffer, OffsetToLock, SizeToLock, ppbData, Flags);

	return orig_vtable.Lock(This, OffsetToLock, SizeToLock, ppbData, Flags);
}

static HRESULT STDMETHODCALLTYPE Unlock(
	IDirect3DVertexBuffer9 * This)
{
	IDirect3DVertexBuffer9 *vertex_buffer = lookup_hooked_vertex_buffer(This);

	HookDebug("HookedVertexBuffer:: Unlock()\n");

	if (vertex_buffer)
		return IDirect3DVertexBuffer9_Unlock(vertex_buffer);

	return orig_vtable.Unlock(This);
}

static void install_hooks(IDirect3DVertexBuffer9 *vertex_buffer)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSection(&vertex_buffer_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, vertex_buffer->lpVtbl, sizeof(struct IDirect3DVertexBuffer9Vtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, vertex_buffer->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, vertex_buffer->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, vertex_buffer->lpVtbl->Release, Release);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.FreePrivateData, vertex_buffer->lpVtbl->FreePrivateData, FreePrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDevice, vertex_buffer->lpVtbl->GetDevice, GetDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPriority, vertex_buffer->lpVtbl->GetPriority, GetPriority);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPrivateData, vertex_buffer->lpVtbl->GetPrivateData, GetPrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetType, vertex_buffer->lpVtbl->GetType, GetType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.PreLoad, vertex_buffer->lpVtbl->PreLoad, PreLoad);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPriority, vertex_buffer->lpVtbl->SetPriority, SetPriority);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPrivateData, vertex_buffer->lpVtbl->SetPrivateData, SetPrivateData);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDesc, vertex_buffer->lpVtbl->GetDesc, GetDesc);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Lock, vertex_buffer->lpVtbl->Lock, Lock);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Unlock, vertex_buffer->lpVtbl->Unlock, Unlock);

}

typedef struct IDirect3DVertexBuffer9Trampoline {
	CONST_VTBL struct IDirect3DVertexBuffer9Vtbl *lpVtbl;
	IDirect3DVertexBuffer9 *orig_this;
} IDirect3DVertexBuffer9Trampoline;


// IUnknown
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	IDirect3DVertexBuffer9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineIDirect3DVertexBuffer9:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3DVertexBuffer9Trampoline*)This)->orig_this, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	IDirect3DVertexBuffer9 * This)
{
	HookDebug("TrampolineIDirect3DVertexBuffer9:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3DVertexBuffer9Trampoline*)This)->orig_this);
}

static ULONG STDMETHODCALLTYPE TrampolineRelease(
	IDirect3DVertexBuffer9 * This)
{
	HookDebug("TrampolineIDirect3DVertexBuffer9:: Release()\n");

	return orig_vtable.Release(((IDirect3DVertexBuffer9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineFreePrivateData(
	IDirect3DVertexBuffer9 * This, REFGUID refguid)
{
	HookDebug("TrampolineIDirect3DVertexBuffer9:: FreePrivateData()\n");

	return orig_vtable.FreePrivateData(((IDirect3DVertexBuffer9Trampoline*)This)->orig_this, refguid);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDevice(
	IDirect3DVertexBuffer9 * This, IDirect3DDevice9 **ppDevice)
{
	HookDebug("TrampolineIDirect3DVertexBuffer9:: GetDevice()\n");

	return orig_vtable.GetDevice(((IDirect3DVertexBuffer9Trampoline*)This)->orig_this, ppDevice);
}

static DWORD STDMETHODCALLTYPE TrampolineGetPriority(
	IDirect3DVertexBuffer9 * This)
{
	HookDebug("TrampolineIDirect3DVertexBuffer9:: GetPriority()\n");

	return orig_vtable.GetPriority(((IDirect3DVertexBuffer9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetPrivateData(
	IDirect3DVertexBuffer9 * This,
	      REFGUID refguid,
	 void    *pData,
	 DWORD   *pSizeOfData)
{
	HookDebug("TrampolineIDirect3DVertexBuffer9:: GetPrivateData()\n");

	return orig_vtable.GetPrivateData(((IDirect3DVertexBuffer9Trampoline*)This)->orig_this, refguid, pData, pSizeOfData);
}

static D3DRESOURCETYPE STDMETHODCALLTYPE TrampolineGetType(
	IDirect3DVertexBuffer9 * This)
{
	HookDebug("TrampolineIDirect3DVertexBuffer9:: GetType()\n");

	return orig_vtable.GetType(((IDirect3DVertexBuffer9Trampoline*)This)->orig_this);
}

static void STDMETHODCALLTYPE TrampolinePreLoad(
	IDirect3DVertexBuffer9 * This)
{
	HookDebug("TrampolineIDirect3DVertexBuffer9::PreLoad()\n");

	return orig_vtable.PreLoad(((IDirect3DVertexBuffer9Trampoline*)This)->orig_this);
}

static DWORD STDMETHODCALLTYPE TrampolineSetPriority(
	IDirect3DVertexBuffer9 * This, DWORD PriorityNew)
{
	HookDebug("TrampolineIDirect3DVertexBuffer9::SetPriority()\n");

	return orig_vtable.SetPriority(((IDirect3DVertexBuffer9Trampoline*)This)->orig_this, PriorityNew);
}

static HRESULT STDMETHODCALLTYPE TrampolineSetPrivateData(
	IDirect3DVertexBuffer9 * This,
	       REFGUID refguid,
	 const void    *pData,
	       DWORD   SizeOfData,
	       DWORD   Flags)
{
	HookDebug("TrampolineIDirect3DVertexBuffer9::SetPrivateData()\n");

	return orig_vtable.SetPrivateData(((IDirect3DVertexBuffer9Trampoline*)This)->orig_this, refguid, pData, SizeOfData, Flags);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDesc(
	IDirect3DVertexBuffer9 * This,
	D3DVERTEXBUFFER_DESC *pDesc)
{
	HookDebug("TrampolineIDirect3DVertexBuffer9::GetDesc()\n");

	return orig_vtable.GetDesc(((IDirect3DVertexBuffer9Trampoline*)This)->orig_this, pDesc);
}

static HRESULT STDMETHODCALLTYPE TrampolineLock(
	IDirect3DVertexBuffer9 * This,
	  UINT  OffsetToLock,
	  UINT  SizeToLock,
	 VOID  **ppbData,
	  DWORD Flags)
{
	HookDebug("TrampolineIDirect3DVertexBuffer9::Lock()\n");

	return orig_vtable.Lock(((IDirect3DVertexBuffer9Trampoline*)This)->orig_this, OffsetToLock, SizeToLock, ppbData, Flags);
}

static HRESULT STDMETHODCALLTYPE TrampolineUnlock(
	IDirect3DVertexBuffer9 * This)
{
	HookDebug("TrampolineIDirect3DVertexBuffer9::Unlock()\n");

	return orig_vtable.Unlock(((IDirect3DVertexBuffer9Trampoline*)This)->orig_this);
}

static CONST_VTBL struct IDirect3DVertexBuffer9Vtbl trampoline_ex_vtable = {
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

IDirect3DVertexBuffer9* hook_vertex_buffer(IDirect3DVertexBuffer9 *orig_vertex_buffer, IDirect3DVertexBuffer9 *hacker_vertex_buffer)
{
	IDirect3DVertexBuffer9Trampoline *trampoline_vertex_buffer = new IDirect3DVertexBuffer9Trampoline();
	trampoline_vertex_buffer->lpVtbl = &trampoline_ex_vtable;
	trampoline_vertex_buffer->orig_this = orig_vertex_buffer;

	install_hooks(orig_vertex_buffer);
	EnterCriticalSection(&vertex_buffer_map_lock);
	vertex_buffer_map[orig_vertex_buffer] = hacker_vertex_buffer;
	LeaveCriticalSection(&vertex_buffer_map_lock);

	return (IDirect3DVertexBuffer9*)trampoline_vertex_buffer;
}
