#include <unordered_map>

#include "HookedPixelShader.h"


#include "DLLMainHookDX9.h"


#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif

typedef std::unordered_map<IDirect3DPixelShader9 *, IDirect3DPixelShader9 *> PixelShaderMap;
static PixelShaderMap pixel_shader_map;
static CRITICAL_SECTION pixel_shader_map_lock;

static struct IDirect3DPixelShader9Vtbl orig_vtable;

static bool hooks_installed = false;

IDirect3DPixelShader9* lookup_hooked_pixel_shader(IDirect3DPixelShader9 *orig_pixel_shader)
{
	PixelShaderMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSection(&pixel_shader_map_lock);
	i = pixel_shader_map.find(orig_pixel_shader);
	if (i == pixel_shader_map.end()) {
		LeaveCriticalSection(&pixel_shader_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&pixel_shader_map_lock);

	return i->second;
}

// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
	IDirect3DPixelShader9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	IDirect3DPixelShader9 *pixel_shader = lookup_hooked_pixel_shader(This);

	HookDebug("HookedPixelShader:: QueryInterface()\n");

	if (pixel_shader)
		return IDirect3DPixelShader9_QueryInterface(pixel_shader, riid, ppvObject);
	return orig_vtable.QueryInterface(This, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE AddRef(
	IDirect3DPixelShader9 * This)
{
	IDirect3DPixelShader9 *pixel_shader = lookup_hooked_pixel_shader(This);

	HookDebug("HookedPixelShader:: AddRef()\n");

	if (pixel_shader)
		return IDirect3DPixelShader9_AddRef(pixel_shader);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
	IDirect3DPixelShader9 * This)
{
	PixelShaderMap::iterator i;
	ULONG ref;

	HookDebug("HookedPixelShader:: Release()\n");

	EnterCriticalSection(&pixel_shader_map_lock);
	i = pixel_shader_map.find(This);
	if (i != pixel_shader_map.end()) {
		ref = IDirect3DPixelShader9_Release(i->second);
		if (!ref)
			pixel_shader_map.erase(i);
	}
	else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&pixel_shader_map_lock);

	return ref;
}

static HRESULT STDMETHODCALLTYPE GetDevice(
	IDirect3DPixelShader9 * This, IDirect3DDevice9 **ppDevice)
{
	IDirect3DPixelShader9 *pixel_shader = lookup_hooked_pixel_shader(This);

	HookDebug("HookedPixelShader:: GetDevice()\n");

	if (pixel_shader)
		return IDirect3DPixelShader9_GetDevice(pixel_shader, ppDevice);

	return orig_vtable.GetDevice(This, ppDevice);
}

static HRESULT STDMETHODCALLTYPE GetFunction(
	IDirect3DPixelShader9 * This,
	 void *pData,
	 UINT *pSizeOfData)
{
	IDirect3DPixelShader9 *pixel_shader = lookup_hooked_pixel_shader(This);

	HookDebug("HookedPixelShader:: GetFunction()\n");

	if (pixel_shader)
		return IDirect3DPixelShader9_GetFunction(pixel_shader, pData, pSizeOfData);

	return orig_vtable.GetFunction(This, pData, pSizeOfData);
}

static void install_hooks(IDirect3DPixelShader9 *pixel_shader)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSection(&pixel_shader_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, pixel_shader->lpVtbl, sizeof(struct IDirect3DPixelShader9Vtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface, pixel_shader->lpVtbl->QueryInterface, QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef, pixel_shader->lpVtbl->AddRef, AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release, pixel_shader->lpVtbl->Release, Release);

	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDevice, pixel_shader->lpVtbl->GetDevice, GetDevice);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetFunction, pixel_shader->lpVtbl->GetFunction, GetFunction);

}

typedef struct IDirect3DPixelShader9Trampoline {
	CONST_VTBL struct IDirect3DPixelShader9Vtbl *lpVtbl;
	IDirect3DPixelShader9 *orig_this;
} IDirect3DPixelShader9Trampoline;


// IUnknown
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
	IDirect3DPixelShader9 * This,
	REFIID riid,
	__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineIDirect3DPixelShader9:: QueryInterface()\n");

	return orig_vtable.QueryInterface(((IDirect3DPixelShader9Trampoline*)This)->orig_this, riid, ppvObject);
}


static ULONG STDMETHODCALLTYPE TrampolineAddRef(
	IDirect3DPixelShader9 * This)
{
	HookDebug("TrampolineIDirect3DPixelShader9:: AddRef()\n");

	return orig_vtable.AddRef(((IDirect3DPixelShader9Trampoline*)This)->orig_this);
}

static ULONG STDMETHODCALLTYPE TrampolineRelease(
	IDirect3DPixelShader9 * This)
{
	HookDebug("TrampolineIDirect3DPixelShader9:: Release()\n");

	return orig_vtable.Release(((IDirect3DPixelShader9Trampoline*)This)->orig_this);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetDevice(
	IDirect3DPixelShader9 * This, IDirect3DDevice9 **ppDevice)
{
	HookDebug("TrampolineIDirect3DPixelShader9:: GetDevice()\n");

	return orig_vtable.GetDevice(((IDirect3DPixelShader9Trampoline*)This)->orig_this, ppDevice);
}

static HRESULT STDMETHODCALLTYPE TrampolineGetFunction(
	IDirect3DPixelShader9 * This,
	 void *pData,
	 UINT *pSizeOfData)
{
	HookDebug("TrampolineIDirect3DPixelShader9:: GetFunction()\n");

	return orig_vtable.GetFunction(((IDirect3DPixelShader9Trampoline*)This)->orig_this, pData, pSizeOfData);
}

static CONST_VTBL struct IDirect3DPixelShader9Vtbl trampoline_ex_vtable = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolineGetDevice,
	TrampolineGetFunction
};

IDirect3DPixelShader9* hook_pixel_shader(IDirect3DPixelShader9 *orig_pixel_shader, IDirect3DPixelShader9 *hacker_pixel_shader)
{
	IDirect3DPixelShader9Trampoline *trampoline_pixel_shader = new IDirect3DPixelShader9Trampoline();
	trampoline_pixel_shader->lpVtbl = &trampoline_ex_vtable;
	trampoline_pixel_shader->orig_this = orig_pixel_shader;

	install_hooks(orig_pixel_shader);
	EnterCriticalSection(&pixel_shader_map_lock);
	pixel_shader_map[orig_pixel_shader] = hacker_pixel_shader;
	LeaveCriticalSection(&pixel_shader_map_lock);

	return (IDirect3DPixelShader9*)trampoline_pixel_shader;
}
