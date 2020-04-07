#pragma once
#define CINTERFACE
#define D3D9_NO_HELPERS
#define COBJMACROS
#include <d3d9.h>
#undef COBJMACROS
#undef D3D9_NO_HELPERS
#undef CINTERFACE
::IDirect3DCubeTexture9* hook_cube_texture(::IDirect3DCubeTexture9 *orig_cube_texture, ::IDirect3DCubeTexture9 *hacker_cube_texture);
::IDirect3DCubeTexture9* lookup_hooked_cube_texture(::IDirect3DCubeTexture9 *orig_cube_texture);
