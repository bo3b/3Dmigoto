#pragma once
#define CINTERFACE
#define D3D9_NO_HELPERS
#define COBJMACROS
namespace D3D9Base
{

#include <d3d9.h>

}
#undef COBJMACROS
#undef D3D9_NO_HELPERS
#undef CINTERFACE
D3D9Base::IDirect3DSurface9* hook_surface(D3D9Base::IDirect3DSurface9 *orig_surface, D3D9Base::IDirect3DSurface9 *hacker_surface);
D3D9Base::IDirect3DSurface9* lookup_hooked_surface(D3D9Base::IDirect3DSurface9 *orig_surface);
