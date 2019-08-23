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
D3D9Base::IDirect3DStateBlock9* hook_stateblock(D3D9Base::IDirect3DStateBlock9 *orig_stateblock, D3D9Base::IDirect3DStateBlock9 *hacker_stateblock);
D3D9Base::IDirect3DStateBlock9* lookup_hooked_stateblock(D3D9Base::IDirect3DStateBlock9 *orig_stateblock);