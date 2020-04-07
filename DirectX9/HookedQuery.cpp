#include <unordered_map>

#include "HookedQuery.h"


#include "DLLMainHookDX9.h"


#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

typedef std::unordered_map<IDirect3DQuery9 *, IDirect3DQuery9 *> QueryMap;
static QueryMap query_map;
static CRITICAL_SECTION query_map_lock;

static struct IDirect3DQuery9Vtbl orig_vtable;

static bool hooks_installed = false;

IDirect3DQuery9* lookup_hooked_query(IDirect3DQuery9 *orig_query)
{
	QueryMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&query_map_lock);
	i = query_map.find(orig_query);
	if (i == query_map.end()) {
		LeaveCriticalSection(&query_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&query_map_lock);

	return i->second;
}

// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3DQuery9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3DQuery9 *query = lookup_hooked_query(This);

	HookDebug("HookedQuery:: QueryInterface()\n");

	if (query)
		return IDirect3DQuery9_QueryInterface(query, riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3DQuery9 * This)
{
	IDirect3DQuery9 *query = lookup_hooked_query(This);

	HookDebug("HookedQuery:: AddRef()\n");

	if (query)
		return IDirect3DQuery9_AddRef(query);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3DQuery9 * This)
{
	QueryMap::iterator i;
	ULONG ref;

	HookDebug("HookedQuery:: Release()\n");

	EnterCriticalSection(&query_map_lock);
	i = query_map.find(This);
	if (i != query_map.end()) {
		ref = IDirect3DQuery9_Release(i->second);
		if (!ref)
			query_map.erase(i);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&query_map_lock);

	return ref;
}

static HRESULT STDMETHODCALLTYPE GetDevice(
	IDirect3DQuery9 * This, IDirect3DDevice9 **ppDevice)
{
	IDirect3DQuery9 *query = lookup_hooked_query(This);

	HookDebug("HookedQuery:: GetDevice()\n");

	if (query)
		return IDirect3DQuery9_GetDevice(query, ppDevice);

	return orig_vtable.GetDevice(This, ppDevice);
}

static HRESULT STDMETHODCALLTYPE GetData(
	IDirect3DQuery9 * This,
	 void  *pData,
	      DWORD dwSize,
	      DWORD dwGetDataFlags)
{
	IDirect3DQuery9 *query = lookup_hooked_query(This);

	HookDebug("HookedQuery:: GetData()\n");

	if (query)
		return IDirect3DQuery9_GetData(query, pData, dwSize, dwGetDataFlags);

	return orig_vtable.GetData(This, pData, dwSize, dwGetDataFlags);
}

static DWORD STDMETHODCALLTYPE GetDataSize(
	IDirect3DQuery9 * This)
{
	IDirect3DQuery9 *query = lookup_hooked_query(This);

	HookDebug("HookedQuery:: GetDataSize()\n");

	if (query)
		return IDirect3DQuery9_GetDataSize(query);

	return orig_vtable.GetDataSize(This);
}

static D3DQUERYTYPE STDMETHODCALLTYPE GetType(
	IDirect3DQuery9 * This)
{
	IDirect3DQuery9 *query = lookup_hooked_query(This);

	HookDebug("HookedQuery:: GetType()\n");

	if (query)
		return IDirect3DQuery9_GetType(query);

	return orig_vtable.GetType(This);
}

static HRESULT STDMETHODCALLTYPE Issue(
	IDirect3DQuery9 * This, DWORD dwIssueFlags)
{
	IDirect3DQuery9 *query = lookup_hooked_query(This);

	HookDebug("HookedQuery:: Issue()\n");

	if (query)
		return IDirect3DQuery9_Issue(query, dwIssueFlags);

	return orig_vtable.Issue(This, dwIssueFlags);
}


static void install_hooks(IDirect3DQuery9 *query)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSection(&query_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, query->lpVtbl, sizeof(struct IDirect3DQuery9Vtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, query->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, query->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, query->lpVtbl->Release, Release);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDevice, query->lpVtbl->GetDevice, GetDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetData, query->lpVtbl->GetData, GetData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDataSize, query->lpVtbl->GetDataSize, GetDataSize);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetType, query->lpVtbl->GetType, GetType);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Issue, query->lpVtbl->Issue, Issue);

}

typedef struct IDirect3DQuery9Trampoline {
	CONST_VTBL struct IDirect3DQuery9Vtbl *lpVtbl;
	IDirect3DQuery9 *orig_this;
} IDirect3DQuery9Trampoline;


// IUnknown
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	IDirect3DQuery9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineIDirect3DQuery9:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3DQuery9Trampoline*)This)->orig_this, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	IDirect3DQuery9 * This)
{
	HookDebug("TrampolineIDirect3DQuery9:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3DQuery9Trampoline*)This)->orig_this);
}

static ULONG STDMETHODCALLTYPE TrampolineRelease(
	IDirect3DQuery9 * This)
{
	HookDebug("TrampolineIDirect3DQuery9:: Release()\n");

	return orig_vtable.Release(((IDirect3DQuery9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDevice(
	IDirect3DQuery9 * This, IDirect3DDevice9 **ppDevice)
{
	HookDebug("TrampolineIDirect3DQuery9:: GetDevice()\n");

	return orig_vtable.GetDevice(((IDirect3DQuery9Trampoline*)This)->orig_this, ppDevice);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetData(
	IDirect3DQuery9 * This,
	 void  *pData,
	      DWORD dwSize,
	      DWORD dwGetDataFlags)
{
	HookDebug("TrampolineIDirect3DQuery9:: GetData()\n");

	return orig_vtable.GetData(((IDirect3DQuery9Trampoline*)This)->orig_this, pData, dwSize, dwGetDataFlags);
}

static DWORD STDMETHODCALLTYPE TrampolineGetDataSize(
	IDirect3DQuery9 * This)
{
	HookDebug("TrampolineIDirect3DQuery9:: GetDataSize()\n");

	return orig_vtable.GetDataSize(((IDirect3DQuery9Trampoline*)This)->orig_this);
}

static D3DQUERYTYPE STDMETHODCALLTYPE TrampolineGetType(
	IDirect3DQuery9 * This)
{
	HookDebug("TrampolineIDirect3DQuery9:: GetType()\n");

	return orig_vtable.GetType(((IDirect3DQuery9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineIssue(
	IDirect3DQuery9 * This, DWORD dwIssueFlags)
{
	HookDebug("TrampolineIDirect3DQuery9:: Issue()\n");

	return orig_vtable.Issue(((IDirect3DQuery9Trampoline*)This)->orig_this, dwIssueFlags);
}

static CONST_VTBL struct IDirect3DQuery9Vtbl trampoline_ex_vtable = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolineGetDevice,
	TrampolineGetType,
	TrampolineGetDataSize,
	TrampolineIssue,
	TrampolineGetData
};

IDirect3DQuery9* hook_query(IDirect3DQuery9 *orig_query, IDirect3DQuery9 *hacker_query)
{
	IDirect3DQuery9Trampoline *trampoline_query = new IDirect3DQuery9Trampoline();
	trampoline_query->lpVtbl = &trampoline_ex_vtable;
	trampoline_query->orig_this = orig_query;

	install_hooks(orig_query);
	EnterCriticalSection(&query_map_lock);
	query_map[orig_query] = hacker_query;
	LeaveCriticalSection(&query_map_lock);

	return (IDirect3DQuery9*)trampoline_query;
}
