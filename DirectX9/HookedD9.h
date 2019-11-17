#pragma once
#define CINTERFACE
#define D3D9_NO_HELPERS
#define COBJMACROS
#include <d3d9.h>
#undef COBJMACROS
#undef D3D9_NO_HELPERS
#undef CINTERFACE

::IDirect3D9Ex* hook_D9(::IDirect3D9Ex *orig_D9, ::IDirect3D9Ex *hacker_D9);
::IDirect3D9* hook_D9(::IDirect3D9 *orig_D9, ::IDirect3D9 *hacker_D9);

::IDirect3D9Ex* lookup_hooked_D9(::IDirect3D9Ex *orig_D9);
::IDirect3D9* lookup_hooked_D9(::IDirect3D9 *orig_D9);
