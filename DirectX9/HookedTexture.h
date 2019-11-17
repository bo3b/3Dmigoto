#pragma once
#define CINTERFACE
#define D3D9_NO_HELPERS
#define COBJMACROS
#include <d3d9.h>
#undef COBJMACROS
#undef D3D9_NO_HELPERS
#undef CINTERFACE
::IDirect3DTexture9* hook_texture(::IDirect3DTexture9 *orig_texture, ::IDirect3DTexture9 *hacker_texture);
::IDirect3DTexture9* lookup_hooked_texture(::IDirect3DTexture9 *orig_texture);
