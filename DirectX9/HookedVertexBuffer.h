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
D3D9Base::IDirect3DVertexBuffer9* hook_vertex_buffer(D3D9Base::IDirect3DVertexBuffer9 *orig_vertex_buffer, D3D9Base::IDirect3DVertexBuffer9 *hacker_vertex_buffer);
D3D9Base::IDirect3DVertexBuffer9* lookup_hooked_vertex_buffer(D3D9Base::IDirect3DVertexBuffer9 *orig_vertex_buffer);