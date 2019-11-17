#include <unordered_map>

#include "HookedVertexDeclaration.h"


#include "DLLMainHookDX9.h"


#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

typedef std::unordered_map<IDirect3DVertexDeclaration9 *, IDirect3DVertexDeclaration9 *> VertexDeclarationMap;
static VertexDeclarationMap vertex_declaration_map;
static CRITICAL_SECTION vertex_declaration_map_lock;

static struct IDirect3DVertexDeclaration9Vtbl orig_vtable;

static bool hooks_installed = false;

IDirect3DVertexDeclaration9* lookup_hooked_vertex_declaration(IDirect3DVertexDeclaration9 *orig_vertex_declaration)
{
	VertexDeclarationMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&vertex_declaration_map_lock);
	i = vertex_declaration_map.find(orig_vertex_declaration);
	if (i == vertex_declaration_map.end()) {
		LeaveCriticalSection(&vertex_declaration_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&vertex_declaration_map_lock);

	return i->second;
}

// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3DVertexDeclaration9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3DVertexDeclaration9 *vertex_declaration = lookup_hooked_vertex_declaration(This);

	HookDebug("HookedVertexDeclaration:: QueryInterface()\n");

	if (vertex_declaration)
		return IDirect3DVertexDeclaration9_QueryInterface(vertex_declaration, riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3DVertexDeclaration9 * This)
{
	IDirect3DVertexDeclaration9 *vertex_declaration = lookup_hooked_vertex_declaration(This);

	HookDebug("HookedVertexDeclaration:: AddRef()\n");

	if (vertex_declaration)
		return IDirect3DVertexDeclaration9_AddRef(vertex_declaration);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3DVertexDeclaration9 * This)
{
	VertexDeclarationMap::iterator i;
	ULONG ref;

	HookDebug("HookedVertexDeclaration:: Release()\n");

	EnterCriticalSection(&vertex_declaration_map_lock);
	i = vertex_declaration_map.find(This);
	if (i != vertex_declaration_map.end()) {
		ref = IDirect3DVertexDeclaration9_Release(i->second);
		if (!ref)
			vertex_declaration_map.erase(i);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&vertex_declaration_map_lock);

	return ref;
}

static HRESULT STDMETHODCALLTYPE GetDevice(
	IDirect3DVertexDeclaration9 * This, IDirect3DDevice9 **ppDevice)
{
	IDirect3DVertexDeclaration9 *vertex_declaration = lookup_hooked_vertex_declaration(This);

	HookDebug("HookedVertexDeclaration:: GetDevice()\n");

	if (vertex_declaration)
		return IDirect3DVertexDeclaration9_GetDevice(vertex_declaration, ppDevice);

	return orig_vtable.GetDevice(This, ppDevice);
}

static HRESULT STDMETHODCALLTYPE GetDeclaration(
	IDirect3DVertexDeclaration9 * This,
	 D3DVERTEXELEMENT9 *pDecl,
	     UINT              *pNumElements)
{
	IDirect3DVertexDeclaration9 *vertex_declaration = lookup_hooked_vertex_declaration(This);

	HookDebug("HookedVertexDeclaration:: GetFunction()\n");

	if (vertex_declaration)
		return IDirect3DVertexDeclaration9_GetDeclaration(vertex_declaration, pDecl, pNumElements);

	return orig_vtable.GetDeclaration(This, pDecl, pNumElements);
}

static void install_hooks(IDirect3DVertexDeclaration9 *vertex_declaration)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSection(&vertex_declaration_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, vertex_declaration->lpVtbl, sizeof(struct IDirect3DVertexDeclaration9Vtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, vertex_declaration->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, vertex_declaration->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, vertex_declaration->lpVtbl->Release, Release);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDevice, vertex_declaration->lpVtbl->GetDevice, GetDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDeclaration, vertex_declaration->lpVtbl->GetDeclaration, GetDeclaration);

}

typedef struct IDirect3DVertexDeclaration9Trampoline {
	CONST_VTBL struct IDirect3DVertexDeclaration9Vtbl *lpVtbl;
	IDirect3DVertexDeclaration9 *orig_this;
} IDirect3DVertexDeclaration9Trampoline;


// IUnknown
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	IDirect3DVertexDeclaration9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineIDirect3DVertexDeclaration9:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3DVertexDeclaration9Trampoline*)This)->orig_this, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	IDirect3DVertexDeclaration9 * This)
{
	HookDebug("TrampolineIDirect3DVertexDeclaration9:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3DVertexDeclaration9Trampoline*)This)->orig_this);
}

static ULONG STDMETHODCALLTYPE TrampolineRelease(
	IDirect3DVertexDeclaration9 * This)
{
	HookDebug("TrampolineIDirect3DVertexDeclaration9:: Release()\n");

	return orig_vtable.Release(((IDirect3DVertexDeclaration9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDevice(
	IDirect3DVertexDeclaration9 * This, IDirect3DDevice9 **ppDevice)
{
	HookDebug("TrampolineIDirect3DVertexDeclaration9:: GetDevice()\n");

	return orig_vtable.GetDevice(((IDirect3DVertexDeclaration9Trampoline*)This)->orig_this, ppDevice);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDeclaration(
	IDirect3DVertexDeclaration9 * This,
	 D3DVERTEXELEMENT9 *pDecl,
	     UINT              *pNumElements)
{
	HookDebug("TrampolineIDirect3DVertexDeclaration9:: GetDeclaration()\n");

	return orig_vtable.GetDeclaration(((IDirect3DVertexDeclaration9Trampoline*)This)->orig_this, pDecl, pNumElements);
}

static CONST_VTBL struct IDirect3DVertexDeclaration9Vtbl trampoline_ex_vtable = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolineGetDevice,
	TrampolineGetDeclaration
};

IDirect3DVertexDeclaration9* hook_vertex_declaration(IDirect3DVertexDeclaration9 *orig_vertex_declaration, IDirect3DVertexDeclaration9 *hacker_vertex_declaration)
{
	IDirect3DVertexDeclaration9Trampoline *trampoline_vertex_declaration = new IDirect3DVertexDeclaration9Trampoline();
	trampoline_vertex_declaration->lpVtbl = &trampoline_ex_vtable;
	trampoline_vertex_declaration->orig_this = orig_vertex_declaration;

	install_hooks(orig_vertex_declaration);
	EnterCriticalSection(&vertex_declaration_map_lock);
	vertex_declaration_map[orig_vertex_declaration] = hacker_vertex_declaration;
	LeaveCriticalSection(&vertex_declaration_map_lock);

	return (IDirect3DVertexDeclaration9*)trampoline_vertex_declaration;
}
