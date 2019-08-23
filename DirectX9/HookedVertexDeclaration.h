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
D3D9Base::IDirect3DVertexDeclaration9* hook_vertex_declaration(D3D9Base::IDirect3DVertexDeclaration9 *orig_vertex_declaration, D3D9Base::IDirect3DVertexDeclaration9 *hacker_vertex_declaration);
D3D9Base::IDirect3DVertexDeclaration9* lookup_hooked_vertex_declaration(D3D9Base::IDirect3DVertexDeclaration9 *orig_vertex_declaration);
