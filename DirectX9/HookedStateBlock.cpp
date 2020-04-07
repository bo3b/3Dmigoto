#include <unordered_map>

#include "HookedStateBlock.h"


#include "DLLMainHookDX9.h"


#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

typedef std::unordered_map<IDirect3DStateBlock9 *, IDirect3DStateBlock9 *> stateblockMap;
static stateblockMap stateblock_map;
static CRITICAL_SECTION stateblock_map_lock;

static struct IDirect3DStateBlock9Vtbl orig_vtable;

static bool hooks_installed = false;

IDirect3DStateBlock9* lookup_hooked_stateblock(IDirect3DStateBlock9 *orig_stateblock)
{
	stateblockMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&stateblock_map_lock);
	i = stateblock_map.find(orig_stateblock);
	if (i == stateblock_map.end()) {
		LeaveCriticalSection(&stateblock_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&stateblock_map_lock);

	return i->second;
}

// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3DStateBlock9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3DStateBlock9 *stateblock = lookup_hooked_stateblock(This);

	HookDebug("HookedStateBlock:: QueryInterface()\n");

	if (stateblock)
		return IDirect3DStateBlock9_QueryInterface(stateblock, riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3DStateBlock9 * This)
{
	IDirect3DStateBlock9 *stateblock = lookup_hooked_stateblock(This);

	HookDebug("HookedStateBlock:: AddRef()\n");

	if (stateblock)
		return IDirect3DStateBlock9_AddRef(stateblock);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3DStateBlock9 * This)
{
	stateblockMap::iterator i;
	ULONG ref;

	HookDebug("HookedStateBlock:: Release()\n");

	EnterCriticalSection(&stateblock_map_lock);
	i = stateblock_map.find(This);
	if (i != stateblock_map.end()) {
		ref = IDirect3DStateBlock9_Release(i->second);
		if (!ref)
			stateblock_map.erase(i);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&stateblock_map_lock);

	return ref;
}

static HRESULT STDMETHODCALLTYPE Apply(
	IDirect3DStateBlock9 * This)
{
	IDirect3DStateBlock9 *stateblock = lookup_hooked_stateblock(This);

	HookDebug("HookedStateBlock:: Apply()\n");

	if (stateblock)
		return IDirect3DStateBlock9_Apply(stateblock);

	return orig_vtable.Apply(This);
}

static HRESULT STDMETHODCALLTYPE GetDevice(
	IDirect3DStateBlock9 * This, IDirect3DDevice9 **ppDevice)
{
	IDirect3DStateBlock9 *stateblock = lookup_hooked_stateblock(This);

	HookDebug("HookedStateBlock:: GetDevice()\n");

	if (stateblock)
		return IDirect3DStateBlock9_GetDevice(stateblock, ppDevice);

	return orig_vtable.GetDevice(This, ppDevice);
}

static HRESULT STDMETHODCALLTYPE Capture(
	IDirect3DStateBlock9 * This)
{
	IDirect3DStateBlock9 *stateblock = lookup_hooked_stateblock(This);

	HookDebug("HookedStateBlock:: Capture()\n");

	if (stateblock)
		return IDirect3DStateBlock9_Capture(stateblock);

	return orig_vtable.Capture(This);

}

static void install_hooks(IDirect3DStateBlock9 *stateblock)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSection(&stateblock_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, stateblock->lpVtbl, sizeof(struct IDirect3DStateBlock9));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, stateblock->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, stateblock->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, stateblock->lpVtbl->Release, Release);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Apply, stateblock->lpVtbl->Apply, Apply);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDevice, stateblock->lpVtbl->GetDevice, GetDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Capture, stateblock->lpVtbl->Capture, Capture);
}

typedef struct IDirect3DStateBlock9Trampoline {
	CONST_VTBL struct IDirect3DStateBlock9Vtbl *lpVtbl;
	IDirect3DStateBlock9 *orig_this;
} IDirect3DStateBlock9Trampoline;


// IUnknown
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	IDirect3DStateBlock9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineIDirect3DStateBlock9:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3DStateBlock9Trampoline*)This)->orig_this, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	IDirect3DStateBlock9 * This)
{
	HookDebug("TrampolineIDirect3DStateBlock9:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3DStateBlock9Trampoline*)This)->orig_this);
}

static ULONG STDMETHODCALLTYPE TrampolineRelease(
	IDirect3DStateBlock9 * This)
{
	HookDebug("TrampolineIDirect3DStateBlock9:: Release()\n");

	return orig_vtable.Release(((IDirect3DStateBlock9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDevice(
	IDirect3DStateBlock9 * This, IDirect3DDevice9 **ppDevice)
{
	HookDebug("TrampolineIDirect3DStateBlock9:: GetDevice()\n");

	return orig_vtable.GetDevice(((IDirect3DStateBlock9Trampoline*)This)->orig_this, ppDevice);
}

static HRESULT STDMETHODCALLTYPE TrampolineApply(
	IDirect3DStateBlock9 * This)
{
	HookDebug("TrampolineIDirect3DStateBlock9:: Apply()\n");

	return orig_vtable.Apply(((IDirect3DStateBlock9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineCapture(
	IDirect3DStateBlock9 * This)
{
	HookDebug("TrampolineIDirect3DStateBlock9:: Capture()\n");

	return orig_vtable.Capture(((IDirect3DStateBlock9Trampoline*)This)->orig_this);
}

static CONST_VTBL struct IDirect3DStateBlock9Vtbl trampoline_ex_vtable = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolineGetDevice,
	TrampolineCapture,
	TrampolineApply
};

IDirect3DStateBlock9* hook_stateblock (IDirect3DStateBlock9 *orig_stateblock, IDirect3DStateBlock9 *hacker_stateblock)
{
	IDirect3DStateBlock9Trampoline *trampoline_stateblock = new IDirect3DStateBlock9Trampoline();
	trampoline_stateblock->lpVtbl = &trampoline_ex_vtable;
	trampoline_stateblock->orig_this = orig_stateblock;

	install_hooks(orig_stateblock);
	EnterCriticalSection(&stateblock_map_lock);
	stateblock_map[orig_stateblock] = hacker_stateblock;
	LeaveCriticalSection(&stateblock_map_lock);

	return (IDirect3DStateBlock9*)trampoline_stateblock;
}
