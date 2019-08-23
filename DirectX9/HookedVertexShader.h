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
D3D9Base::IDirect3DVertexShader9* hook_vertex_shader(D3D9Base::IDirect3DVertexShader9 *orig_vertex_shader, D3D9Base::IDirect3DVertexShader9 *hacker_vertex_shader);
D3D9Base::IDirect3DVertexShader9* lookup_hooked_vertex_shader(D3D9Base::IDirect3DVertexShader9 *orig_vertex_shader);
