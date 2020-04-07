#pragma once
#define CINTERFACE
#define D3D9_NO_HELPERS
#define COBJMACROS
#include <d3d9.h>
#undef COBJMACROS
#undef D3D9_NO_HELPERS
#undef CINTERFACE
::IDirect3DQuery9* hook_query(::IDirect3DQuery9 *orig_query, ::IDirect3DQuery9 *hacker_query);
::IDirect3DQuery9* lookup_hooked_query(::IDirect3DQuery9 *orig_query);
