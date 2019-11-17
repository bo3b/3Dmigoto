#pragma once
#define CINTERFACE
#define D3D9_NO_HELPERS
#define COBJMACROS
#include <d3d9.h>
#undef COBJMACROS
#undef D3D9_NO_HELPERS
#undef CINTERFACE
::IDirect3DStateBlock9* hook_stateblock(::IDirect3DStateBlock9 *orig_stateblock, ::IDirect3DStateBlock9 *hacker_stateblock);
::IDirect3DStateBlock9* lookup_hooked_stateblock(::IDirect3DStateBlock9 *orig_stateblock);
