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
D3D9Base::IDirect3DVolume9* hook_volume(D3D9Base::IDirect3DVolume9 *orig_volume, D3D9Base::IDirect3DVolume9 *hacker_volume);
D3D9Base::IDirect3DVolume9* lookup_hooked_volume(D3D9Base::IDirect3DVolume9 *orig_volume);
