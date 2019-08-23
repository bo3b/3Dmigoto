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
D3D9Base::IDirect3DCubeTexture9* hook_cube_texture(D3D9Base::IDirect3DCubeTexture9 *orig_cube_texture, D3D9Base::IDirect3DCubeTexture9 *hacker_cube_texture);
D3D9Base::IDirect3DCubeTexture9* lookup_hooked_cube_texture(D3D9Base::IDirect3DCubeTexture9 *orig_cube_texture);