struct foo {
	float foo;
	uint bar;
	snorm float baz;
	int buz;
	float4 f4; // Aligned
	float4 g4;
	float oopsie;
	float4 m4; // Misaligned
	float4 n4;
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
	float binary_decompiler_array_size_calculation_looks_sketchy;
	int2 really[3];
	float sketchy;
	float3 did[5];
	float i;
	float4 mention[7];
	float how_sketchy;
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
StructuredBuffer<matrix> prim_struct_matrix : register(t94);
StructuredBuffer<dword> prim_struct_dword; // RANT: "word" really == CPU bitness, but MS screwed up their definition. No vector or matrix variants of "dword" exist in HLSL
StructuredBuffer<bool1> prim_struct_bool1;
StructuredBuffer<bool3> prim_struct_bool3;
StructuredBuffer<float4x4> prim_struct_float4x4; StructuredBuffer<float4x3> prim_struct_float4x3; StructuredBuffer<float4x2> prim_struct_float4x2; StructuredBuffer<float4x1> prim_struct_float4x1;
StructuredBuffer<float3x4> prim_struct_float3x4; StructuredBuffer<float3x3> prim_struct_float3x3; StructuredBuffer<float3x2> prim_struct_float3x2; StructuredBuffer<float3x1> prim_struct_float3x1;
StructuredBuffer<float2x4> prim_struct_float2x4; StructuredBuffer<float2x3> prim_struct_float2x3; StructuredBuffer<float2x2> prim_struct_float2x2; StructuredBuffer<float2x1> prim_struct_float2x1;
StructuredBuffer<float1x4> prim_struct_float1x4; StructuredBuffer<float1x3> prim_struct_float1x3; StructuredBuffer<float1x2> prim_struct_float1x2; StructuredBuffer<float1x1> prim_struct_float1x1;
StructuredBuffer<half4x4> prim_struct_half4x4; StructuredBuffer<half4x3> prim_struct_half4x3; StructuredBuffer<half4x2> prim_struct_half4x2; StructuredBuffer<half4x1> prim_struct_half4x1;
StructuredBuffer<half3x4> prim_struct_half3x4; StructuredBuffer<half3x3> prim_struct_half3x3; StructuredBuffer<half3x2> prim_struct_half3x2; StructuredBuffer<half3x1> prim_struct_half3x1;
StructuredBuffer<half2x4> prim_struct_half2x4; StructuredBuffer<half2x3> prim_struct_half2x3; StructuredBuffer<half2x2> prim_struct_half2x2; StructuredBuffer<half2x1> prim_struct_half2x1;
StructuredBuffer<half1x4> prim_struct_half1x4; StructuredBuffer<half1x3> prim_struct_half1x3; StructuredBuffer<half1x2> prim_struct_half1x2; StructuredBuffer<half1x1> prim_struct_half1x1;
#ifdef USE_DOUBLES
StructuredBuffer<double4x4> prim_struct_double4x4; StructuredBuffer<double4x3> prim_struct_double4x3; StructuredBuffer<double4x2> prim_struct_double4x2; StructuredBuffer<double4x1> prim_struct_double4x1;
StructuredBuffer<double3x4> prim_struct_double3x4; StructuredBuffer<double3x3> prim_struct_double3x3; StructuredBuffer<double3x2> prim_struct_double3x2; StructuredBuffer<double3x1> prim_struct_double3x1;
StructuredBuffer<double2x4> prim_struct_double2x4; StructuredBuffer<double2x3> prim_struct_double2x3; StructuredBuffer<double2x2> prim_struct_double2x2; StructuredBuffer<double2x1> prim_struct_double2x1;
StructuredBuffer<double1x4> prim_struct_double1x4; StructuredBuffer<double1x3> prim_struct_double1x3; StructuredBuffer<double1x2> prim_struct_double1x2; StructuredBuffer<double1x1> prim_struct_double1x1;
#endif
StructuredBuffer<uint4x4> prim_struct_uint4x4; StructuredBuffer<uint4x3> prim_struct_uint4x3; StructuredBuffer<uint4x2> prim_struct_uint4x2; StructuredBuffer<uint4x1> prim_struct_uint4x1;
StructuredBuffer<uint3x4> prim_struct_uint3x4; StructuredBuffer<uint3x3> prim_struct_uint3x3; StructuredBuffer<uint3x2> prim_struct_uint3x2; StructuredBuffer<uint3x1> prim_struct_uint3x1;
StructuredBuffer<uint2x4> prim_struct_uint2x4; StructuredBuffer<uint2x3> prim_struct_uint2x3; StructuredBuffer<uint2x2> prim_struct_uint2x2; StructuredBuffer<uint2x1> prim_struct_uint2x1;
StructuredBuffer<uint1x4> prim_struct_uint1x4; StructuredBuffer<uint1x3> prim_struct_uint1x3; StructuredBuffer<uint1x2> prim_struct_uint1x2; StructuredBuffer<uint1x1> prim_struct_uint1x1;
StructuredBuffer<int4x4> prim_struct_int4x4; StructuredBuffer<int4x3> prim_struct_int4x3; StructuredBuffer<int4x2> prim_struct_int4x2; StructuredBuffer<int4x1> prim_struct_int4x1;
StructuredBuffer<int3x4> prim_struct_int3x4; StructuredBuffer<int3x3> prim_struct_int3x3; StructuredBuffer<int3x2> prim_struct_int3x2; StructuredBuffer<int3x1> prim_struct_int3x1;
StructuredBuffer<int2x4> prim_struct_int2x4; StructuredBuffer<int2x3> prim_struct_int2x3; StructuredBuffer<int2x2> prim_struct_int2x2; StructuredBuffer<int2x1> prim_struct_int2x1;
StructuredBuffer<int1x4> prim_struct_int1x4; StructuredBuffer<int1x3> prim_struct_int1x3; StructuredBuffer<int1x2> prim_struct_int1x2; StructuredBuffer<int1x1> prim_struct_int1x1;
StructuredBuffer<bool4x4> prim_struct_bool4x4; StructuredBuffer<bool4x3> prim_struct_bool4x3; StructuredBuffer<bool4x2> prim_struct_bool4x2; StructuredBuffer<bool4x1> prim_struct_bool4x1;
StructuredBuffer<bool3x4> prim_struct_bool3x4; StructuredBuffer<bool3x3> prim_struct_bool3x3; StructuredBuffer<bool3x2> prim_struct_bool3x2; StructuredBuffer<bool3x1> prim_struct_bool3x1;
StructuredBuffer<bool2x4> prim_struct_bool2x4; StructuredBuffer<bool2x3> prim_struct_bool2x3; StructuredBuffer<bool2x2> prim_struct_bool2x2; StructuredBuffer<bool2x1> prim_struct_bool2x1;
StructuredBuffer<bool1x4> prim_struct_bool1x4; StructuredBuffer<bool1x3> prim_struct_bool1x3; StructuredBuffer<bool1x2> prim_struct_bool1x2; StructuredBuffer<bool1x1> prim_struct_bool1x1;
#endif

#ifdef USE_RW_STRUCTURED_BUFFER
RWStructuredBuffer<struct foo> rw_struct_buf_1 : register(u1);
RWStructuredBuffer<struct bar> rw_struct_buf_2 : register(u2);
RWStructuredBuffer<float3> rw_prim_float3 : register(u3);
RWStructuredBuffer<float4x4> rw_prim_float4x4;
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
	output += dot(struct_buf_1[idx+1].f4, struct_buf_1[idx+2].g4);
	output += dot(struct_buf_1[idx+1].m4, struct_buf_1[idx+2].n4);
	output += struct_buf_2[idx].foofoo;
	output += struct_buf_2[idx].foobar;
#ifdef USE_INNER_STRUCT
	output += struct_buf_2[idx].inner_struct_1.foo;
	output += struct_buf_2[idx].inner_struct_1.bar;
	output += struct_buf_2[idx].inner_struct_2[0].baz;
	output += struct_buf_2[idx].inner_struct_2[1].bug;
#ifdef USE_DYNAMICALLY_INDEXED_ARRAYS
	output += struct_buf_2[idx].inner_struct_2[idx].inner_struct_3.buz;
#endif
#endif
	output += struct_buf_2[idx].foobaz;
	output += struct_buf_2[idx].foobuz[7];
	output += struct_buf_2[idx].binary_decompiler_array_size_calculation_looks_sketchy;
	output += struct_buf_2[idx].really[2].y;
	output += struct_buf_2[idx].sketchy;
	output += struct_buf_2[idx].did[4].z;
	output += struct_buf_2[idx].i;
	output += struct_buf_2[idx].mention[6].w;
	output += struct_buf_2[idx].how_sketchy;
#ifdef USE_DYNAMICALLY_INDEXED_ARRAYS
	output += struct_buf_2[idx].foobuz[idx];
#endif
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
	output.xy += prim_struct_float4[idx+1].xy;
	output.xy += prim_struct_float4[idx+2].zw;
	output.xz += prim_struct_float4[idx+3].y;
	output += prim_struct_float4[idx+4].yzwx;
	output += prim_struct_float4x4[idx]._m00_m10_m30_m20;
	output += prim_struct_matrix[idx]._m00_m11_m22_m33;
	output += prim_struct_matrix[idx+1][idx];
	output += prim_struct_dword[idx];
	output += prim_struct_bool1[idx].x;
	output += prim_struct_bool3[idx].z;
	output += prim_struct_float4x4[idx]._m33; output += prim_struct_float4x3[idx]._m32; output += prim_struct_float4x2[idx]._m31; output += prim_struct_float4x1[idx]._m30;
	output += prim_struct_float3x4[idx]._m23; output += prim_struct_float3x3[idx]._m22; output += prim_struct_float3x2[idx]._m21; output += prim_struct_float3x1[idx]._m20;
	output += prim_struct_float2x4[idx]._m13; output += prim_struct_float2x3[idx]._m12; output += prim_struct_float2x2[idx]._m11; output += prim_struct_float2x1[idx]._m10;
	output += prim_struct_float1x4[idx]._m03; output += prim_struct_float1x3[idx]._m02; output += prim_struct_float1x2[idx]._m01; output += prim_struct_float1x1[idx]._m00;
	output += prim_struct_half4x4[idx]._m33; output += prim_struct_half4x3[idx]._m32; output += prim_struct_half4x2[idx]._m31; output += prim_struct_half4x1[idx]._m30;
	output += prim_struct_half3x4[idx]._m23; output += prim_struct_half3x3[idx]._m22; output += prim_struct_half3x2[idx]._m21; output += prim_struct_half3x1[idx]._m20;
	output += prim_struct_half2x4[idx]._m13; output += prim_struct_half2x3[idx]._m12; output += prim_struct_half2x2[idx]._m11; output += prim_struct_half2x1[idx]._m10;
	output += prim_struct_half1x4[idx]._m03; output += prim_struct_half1x3[idx]._m02; output += prim_struct_half1x2[idx]._m01; output += prim_struct_half1x1[idx]._m00;
#ifdef USE_DOUBLES
	output += (float)prim_struct_double4x4[idx]._m33; output += (float)prim_struct_double4x3[idx]._m32; output += (float)prim_struct_double4x2[idx]._m31; output += (float)prim_struct_double4x1[idx]._m30;
	output += (float)prim_struct_double3x4[idx]._m23; output += (float)prim_struct_double3x3[idx]._m22; output += (float)prim_struct_double3x2[idx]._m21; output += (float)prim_struct_double3x1[idx]._m20;
	output += (float)prim_struct_double2x4[idx]._m13; output += (float)prim_struct_double2x3[idx]._m12; output += (float)prim_struct_double2x2[idx]._m11; output += (float)prim_struct_double2x1[idx]._m10;
	output += (float)prim_struct_double1x4[idx]._m03; output += (float)prim_struct_double1x3[idx]._m02; output += (float)prim_struct_double1x2[idx]._m01; output += (float)prim_struct_double1x1[idx]._m00;
#endif
	output += prim_struct_uint4x4[idx]._m33; output += prim_struct_uint4x3[idx]._m32; output += prim_struct_uint4x2[idx]._m31; output += prim_struct_uint4x1[idx]._m30;
	output += prim_struct_uint3x4[idx]._m23; output += prim_struct_uint3x3[idx]._m22; output += prim_struct_uint3x2[idx]._m21; output += prim_struct_uint3x1[idx]._m20;
	output += prim_struct_uint2x4[idx]._m13; output += prim_struct_uint2x3[idx]._m12; output += prim_struct_uint2x2[idx]._m11; output += prim_struct_uint2x1[idx]._m10;
	output += prim_struct_uint1x4[idx]._m03; output += prim_struct_uint1x3[idx]._m02; output += prim_struct_uint1x2[idx]._m01; output += prim_struct_uint1x1[idx]._m00;
	output += prim_struct_int4x4[idx]._m33; output += prim_struct_int4x3[idx]._m32; output += prim_struct_int4x2[idx]._m31; output += prim_struct_int4x1[idx]._m30;
	output += prim_struct_int3x4[idx]._m23; output += prim_struct_int3x3[idx]._m22; output += prim_struct_int3x2[idx]._m21; output += prim_struct_int3x1[idx]._m20;
	output += prim_struct_int2x4[idx]._m13; output += prim_struct_int2x3[idx]._m12; output += prim_struct_int2x2[idx]._m11; output += prim_struct_int2x1[idx]._m10;
	output += prim_struct_int1x4[idx]._m03; output += prim_struct_int1x3[idx]._m02; output += prim_struct_int1x2[idx]._m01; output += prim_struct_int1x1[idx]._m00;
	output += prim_struct_bool4x4[idx]._m33; output += prim_struct_bool4x3[idx]._m32; output += prim_struct_bool4x2[idx]._m31; output += prim_struct_bool4x1[idx]._m30;
	output += prim_struct_bool3x4[idx]._m23; output += prim_struct_bool3x3[idx]._m22; output += prim_struct_bool3x2[idx]._m21; output += prim_struct_bool3x1[idx]._m20;
	output += prim_struct_bool2x4[idx]._m13; output += prim_struct_bool2x3[idx]._m12; output += prim_struct_bool2x2[idx]._m11; output += prim_struct_bool2x1[idx]._m10;
	output += prim_struct_bool1x4[idx]._m03; output += prim_struct_bool1x3[idx]._m02; output += prim_struct_bool1x2[idx]._m01; output += prim_struct_bool1x1[idx]._m00;
#endif

#ifdef USE_RW_STRUCTURED_BUFFER
	output += rw_struct_buf_1[idx].bar;
	rw_struct_buf_1[idx].foo = val;
	rw_struct_buf_1[3].buz = val;
	rw_struct_buf_1[2].f4.yzw = rw_struct_buf_1[3].g4.wxz;
	rw_struct_buf_1[2].m4.xw = rw_struct_buf_1[3].n4.wz;
	rw_struct_buf_2[idx].foofoo = val;
	rw_struct_buf_2[idx].foobar = val;
#ifdef USE_INNER_STRUCT
	rw_struct_buf_2[idx].inner_struct_1.foo = val;
	rw_struct_buf_2[idx].inner_struct_1.bar = val;
	rw_struct_buf_2[idx].inner_struct_2[0].baz = val;
	rw_struct_buf_2[idx].inner_struct_2[1].bug = val;
#ifdef USE_DYNAMICALLY_INDEXED_ARRAYS
	rw_struct_buf_2[idx].inner_struct_2[idx].inner_struct_3.buz = val;
#endif
#endif
	rw_struct_buf_2[idx].foobaz = val;
	rw_struct_buf_2[idx].foobuz[7] = val;
#ifdef USE_DYNAMICALLY_INDEXED_ARRAYS
	rw_struct_buf_2[idx].foobuz[idx] = val;
#endif
	rw_prim_float3[0].yzx = rw_prim_float3[1].xyz;
	rw_prim_float3[2].zx = rw_prim_float3[3].yx;
	rw_prim_float4x4[0] = transpose(rw_prim_float4x4[1]);
#endif
}
