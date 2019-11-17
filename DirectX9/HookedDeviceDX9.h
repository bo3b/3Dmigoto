#pragma once
#define CINTERFACE
#define D3D9_NO_HELPERS
#define COBJMACROS
#include <d3d9.h>
#undef COBJMACROS
#undef D3D9_NO_HELPERS
#undef CINTERFACE
enum class EnableHooksDX9;
::IDirect3DDevice9* lookup_hooked_device_dx9(::IDirect3DDevice9 *orig_device);
::IDirect3DDevice9Ex* lookup_hooked_device_dx9(::IDirect3DDevice9Ex *orig_device);

::IDirect3DDevice9Ex* hook_device(::IDirect3DDevice9Ex *orig_device, ::IDirect3DDevice9Ex *hacker_device, EnableHooksDX9 enable_hooks);
::IDirect3DDevice9* hook_device(::IDirect3DDevice9 *orig_device, ::IDirect3DDevice9 *hacker_device, EnableHooksDX9 enable_hooks);
void remove_hooked_device(::IDirect3DDevice9 *orig_device);
