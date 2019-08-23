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
D3D9Base::IDirect3DTexture9* hook_texture(D3D9Base::IDirect3DTexture9 *orig_texture, D3D9Base::IDirect3DTexture9 *hacker_texture);
D3D9Base::IDirect3DTexture9* lookup_hooked_texture(D3D9Base::IDirect3DTexture9 *orig_texture);