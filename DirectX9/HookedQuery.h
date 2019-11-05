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
D3D9Base::IDirect3DQuery9* hook_query(D3D9Base::IDirect3DQuery9 *orig_query, D3D9Base::IDirect3DQuery9 *hacker_query);
D3D9Base::IDirect3DQuery9* lookup_hooked_query(D3D9Base::IDirect3DQuery9 *orig_query);
