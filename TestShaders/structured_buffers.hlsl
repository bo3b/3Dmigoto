struct foo {
	float foo;
	uint bar;
	snorm float baz;
	int buz;
};

struct bar {
	float foofoo;
	uint foobar;
#ifdef USE_INNER_STRUCT
	struct {
		float foo;
		float bar;
	} inner_struct_1;
	struct {
		float baz;
		float bug;
		struct foo inner_struct_3;
	} inner_struct_2[2];
#endif
	snorm float foobaz;
	int foobuz[8];
};

StructuredBuffer<struct foo> struct_buf_1 : register(t110);
StructuredBuffer<struct bar> struct_buf_2 : register(t111);
#ifdef USE_DUP_NAME
StructuredBuffer<struct foo> struct_buf_3 : register(t112);
#endif

#ifdef USE_PRIMITIVE_TYPES
StructuredBuffer<float> prim_struct_float : register(t90);
StructuredBuffer<float2> prim_struct_float2 : register(t91);
StructuredBuffer<float4> prim_struct_float4 : register(t92);
StructuredBuffer<float4x4> prim_struct_float4x4 : register(t93);
StructuredBuffer<matrix> prim_struct_matrix : register(t94);
#endif

#ifdef USE_RW_STRUCTURED_BUFFER
RWStructuredBuffer<struct foo> rw_struct_buf : register(u1);
#endif

void main(uint idx : TEXCOORD0, float val : TEXCOORD1, out float4 output : SV_Target0)
{
	output = 0;

	// ps_4_0 ld_structured
	// ps_5_0 ld_structured_indexable
	// .Load() syntax needs a newer version of fxc than shipped with the
	// Win 8.0 SDK, so only using [] syntax here:
	output += struct_buf_1[0].foo;
	output += struct_buf_1[2].bar;
	output += struct_buf_1[1].baz;
	output += struct_buf_1[3].buz;
	output += struct_buf_2[idx].foofoo;
	output += struct_buf_2[idx].foobar;
#ifdef USE_INNER_STRUCT
	output += struct_buf_2[idx].inner_struct_1.foo;
	output += struct_buf_2[idx].inner_struct_1.bar;
	output += struct_buf_2[idx].inner_struct_2[0].baz;
	output += struct_buf_2[idx].inner_struct_2[1].bug;
	output += struct_buf_2[idx].inner_struct_2[idx].inner_struct_3.buz;
#endif
	output += struct_buf_2[idx].foobaz;
	output += struct_buf_2[idx].foobuz[3];
#ifdef USE_DUP_NAME
	output += struct_buf_3[idx].foo;
	output += struct_buf_3[idx].bar;
	output += struct_buf_3[idx].baz;
	output += struct_buf_3[idx].buz;
#endif

#ifdef USE_PRIMITIVE_TYPES
	output += prim_struct_float[idx];
	output.xy += prim_struct_float2[idx];
	output += prim_struct_float4[idx];
	output += prim_struct_float4x4[idx]._m00_m10_m30_m20;
	output += prim_struct_matrix[idx]._m00_m11_m22_m33;
#endif

#ifdef USE_RW_STRUCTURED_BUFFER
	rw_struct_buf[idx].foo = val;
#endif
}
