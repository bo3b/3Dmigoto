#pragma once
#define CINTERFACE
#define D3D9_NO_HELPERS
#define COBJMACROS
#include <d3d9.h>
#undef COBJMACROS
#undef D3D9_NO_HELPERS
#undef CINTERFACE
::IDirect3DSurface9* hook_surface(::IDirect3DSurface9 *orig_surface, ::IDirect3DSurface9 *hacker_surface);
::IDirect3DSurface9* lookup_hooked_surface(::IDirect3DSurface9 *orig_surface);
