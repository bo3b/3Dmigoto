#include <unordered_map>

#include "HookedVertexShader.h"


#include "DLLMainHookDX9.h"


#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

typedef std::unordered_map<IDirect3DVertexShader9 *, IDirect3DVertexShader9 *> VertexShaderMap;
static VertexShaderMap vertex_shader_map;
static CRITICAL_SECTION vertex_shader_map_lock;

static struct IDirect3DVertexShader9Vtbl orig_vtable;

static bool hooks_installed = false;

IDirect3DVertexShader9* lookup_hooked_vertex_shader(IDirect3DVertexShader9 *orig_vertex_shader)
{
	VertexShaderMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&vertex_shader_map_lock);
	i = vertex_shader_map.find(orig_vertex_shader);
	if (i == vertex_shader_map.end()) {
		LeaveCriticalSection(&vertex_shader_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&vertex_shader_map_lock);

	return i->second;
}

// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3DVertexShader9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3DVertexShader9 *vertex_shader = lookup_hooked_vertex_shader(This);

	HookDebug("HookedVertexShader:: QueryInterface()\n");

	if (vertex_shader)
		return IDirect3DVertexShader9_QueryInterface(vertex_shader, riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3DVertexShader9 * This)
{
	IDirect3DVertexShader9 *vertex_shader = lookup_hooked_vertex_shader(This);

	HookDebug("HookedVertexShader:: AddRef()\n");

	if (vertex_shader)
		return IDirect3DVertexShader9_AddRef(vertex_shader);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3DVertexShader9 * This)
{
	VertexShaderMap::iterator i;
	ULONG ref;

	HookDebug("HookedVertexShader:: Release()\n");

	EnterCriticalSection(&vertex_shader_map_lock);
	i = vertex_shader_map.find(This);
	if (i != vertex_shader_map.end()) {
		ref = IDirect3DVertexShader9_Release(i->second);
		if (!ref)
			vertex_shader_map.erase(i);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&vertex_shader_map_lock);

	return ref;
}

static HRESULT STDMETHODCALLTYPE GetDevice(
	IDirect3DVertexShader9 * This, IDirect3DDevice9 **ppDevice)
{
	IDirect3DVertexShader9 *vertex_shader = lookup_hooked_vertex_shader(This);

	HookDebug("HookedVertexShader:: GetDevice()\n");

	if (vertex_shader)
		return IDirect3DVertexShader9_GetDevice(vertex_shader, ppDevice);

	return orig_vtable.GetDevice(This, ppDevice);
}

static HRESULT STDMETHODCALLTYPE GetFunction(
	IDirect3DVertexShader9 * This,
	 void *pData,
	 UINT *pSizeOfData)
{
	IDirect3DVertexShader9 *vertex_shader = lookup_hooked_vertex_shader(This);

	HookDebug("HookedVertexShader:: GetFunction()\n");

	if (vertex_shader)
		return IDirect3DVertexShader9_GetFunction(vertex_shader, pData, pSizeOfData);

	return orig_vtable.GetFunction(This, pData, pSizeOfData);
}

static void install_hooks(IDirect3DVertexShader9 *vertex_shader)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSection(&vertex_shader_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, vertex_shader->lpVtbl, sizeof(struct IDirect3DVertexShader9Vtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, vertex_shader->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, vertex_shader->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, vertex_shader->lpVtbl->Release, Release);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDevice, vertex_shader->lpVtbl->GetDevice, GetDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetFunction, vertex_shader->lpVtbl->GetFunction, GetFunction);

}

typedef struct IDirect3DVertexShader9Trampoline {
	CONST_VTBL struct IDirect3DVertexShader9Vtbl *lpVtbl;
	IDirect3DVertexShader9 *orig_this;
} IDirect3DVertexShader9Trampoline;


// IUnknown
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	IDirect3DVertexShader9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineIDirect3DVertexShader9:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3DVertexShader9Trampoline*)This)->orig_this, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	IDirect3DVertexShader9 * This)
{
	HookDebug("TrampolineIDirect3DVertexShader9:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3DVertexShader9Trampoline*)This)->orig_this);
}

static ULONG STDMETHODCALLTYPE TrampolineRelease(
	IDirect3DVertexShader9 * This)
{
	HookDebug("TrampolineIDirect3DVertexShader9:: Release()\n");

	return orig_vtable.Release(((IDirect3DVertexShader9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDevice(
	IDirect3DVertexShader9 * This, IDirect3DDevice9 **ppDevice)
{
	HookDebug("TrampolineIDirect3DVertexShader9:: GetDevice()\n");

	return orig_vtable.GetDevice(((IDirect3DVertexShader9Trampoline*)This)->orig_this, ppDevice);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetFunction(
	IDirect3DVertexShader9 * This,
	 void *pData,
	 UINT *pSizeOfData)
{
	HookDebug("TrampolineIDirect3DVertexShader9:: GetFunction()\n");

	return orig_vtable.GetFunction(((IDirect3DVertexShader9Trampoline*)This)->orig_this, pData, pSizeOfData);
}

static CONST_VTBL struct IDirect3DVertexShader9Vtbl trampoline_ex_vtable = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolineGetDevice,
	TrampolineGetFunction
};

IDirect3DVertexShader9* hook_vertex_shader(IDirect3DVertexShader9 *orig_vertex_shader, IDirect3DVertexShader9 *hacker_vertex_shader)
{
	IDirect3DVertexShader9Trampoline *trampoline_vertex_shader = new IDirect3DVertexShader9Trampoline();
	trampoline_vertex_shader->lpVtbl = &trampoline_ex_vtable;
	trampoline_vertex_shader->orig_this = orig_vertex_shader;

	install_hooks(orig_vertex_shader);
	EnterCriticalSection(&vertex_shader_map_lock);
	vertex_shader_map[orig_vertex_shader] = hacker_vertex_shader;
	LeaveCriticalSection(&vertex_shader_map_lock);

	return (IDirect3DVertexShader9*)trampoline_vertex_shader;
}
