#pragma once
#define CINTERFACE
#define D3D9_NO_HELPERS
#define COBJMACROS
#include <d3d9.h>
#undef COBJMACROS
#undef D3D9_NO_HELPERS
#undef CINTERFACE
::IDirect3DVertexDeclaration9* hook_vertex_declaration(::IDirect3DVertexDeclaration9 *orig_vertex_declaration, ::IDirect3DVertexDeclaration9 *hacker_vertex_declaration);
::IDirect3DVertexDeclaration9* lookup_hooked_vertex_declaration(::IDirect3DVertexDeclaration9 *orig_vertex_declaration);
