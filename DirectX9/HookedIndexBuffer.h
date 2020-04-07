#pragma once
#define CINTERFACE
#define D3D9_NO_HELPERS
#define COBJMACROS
#include <d3d9.h>
#undef COBJMACROS
#undef D3D9_NO_HELPERS
#undef CINTERFACE
::IDirect3DIndexBuffer9* hook_index_buffer(::IDirect3DIndexBuffer9 *orig_index_buffer, ::IDirect3DIndexBuffer9 *hacker_index_buffer);
::IDirect3DIndexBuffer9* lookup_hooked_index_buffer(::IDirect3DIndexBuffer9 *orig_index_buffer);
