#pragma once
#define CINTERFACE
#define D3D9_NO_HELPERS
#define COBJMACROS
#include <d3d9.h>
#undef COBJMACROS
#undef D3D9_NO_HELPERS
#undef CINTERFACE
::IDirect3DVertexBuffer9* hook_vertex_buffer(::IDirect3DVertexBuffer9 *orig_vertex_buffer, ::IDirect3DVertexBuffer9 *hacker_vertex_buffer);
::IDirect3DVertexBuffer9* lookup_hooked_vertex_buffer(::IDirect3DVertexBuffer9 *orig_vertex_buffer);
