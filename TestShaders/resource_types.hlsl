// To test ld_indexable and co:
//   Compile with fxc /T ps_5_0 /Fo resource_types.bin
// To test ld and co:
//   Compile with fxc /T ps_4_0 /Fo resource_types.bin

// HLSL scalar types:
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb509646(v=vs.85).aspx

// HLSL Vector types:
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb509707(v=vs.85).aspx

Texture2D<float>        float_tex   : register(t0); // Same as float1
Texture2D<float1>       float1_tex  : register(t1);
Texture2D<float2>       float2_tex  : register(t2);
Texture2D<float3>       float3_tex  : register(t3);
Texture2D<float4>       float4_tex  : register(t4);

Texture2D<uint>         uint_tex    : register(t5);
Texture2D<uint1>        uint1_tex   : register(t6);
Texture2D<uint2>        uint2_tex   : register(t7);
Texture2D<uint3>        uint3_tex   : register(t8);
Texture2D<uint4>        uint4_tex   : register(t9);

Texture2D<int>          sint_tex    : register(t10);
Texture2D<int1>         sint1_tex   : register(t11);
Texture2D<int2>         sint2_tex   : register(t12);
Texture2D<int3>         sint3_tex   : register(t13);
Texture2D<int4>         sint4_tex   : register(t14);

Texture2D<unorm float>  unorm_tex   : register(t15);
Texture2D<unorm float1> unorm1_tex  : register(t16);
Texture2D<unorm float2> unorm2_tex  : register(t17);
Texture2D<unorm float3> unorm3_tex  : register(t18);
Texture2D<unorm float4> unorm4_tex  : register(t19);

Texture2D<snorm float>  snorm_tex   : register(t20);
Texture2D<snorm float1> snorm1_tex  : register(t21);
Texture2D<snorm float2> snorm2_tex  : register(t22);
Texture2D<snorm float3> snorm3_tex  : register(t23);
Texture2D<snorm float4> snorm4_tex  : register(t24);

// bool becomes uint
Texture2D<bool>         bool_tex    : register(t25);
Texture2D<bool1>        bool1_tex   : register(t26);
Texture2D<bool2>        bool2_tex   : register(t27);
Texture2D<bool3>        bool3_tex   : register(t28);
Texture2D<bool4>        bool4_tex   : register(t29);

// dword becomes uint
// No dword1-4:
Texture2D<dword>        dword_tex   : register(t30);

// half is for language compatibility only - will become regular float:
Texture2D<half>         half_tex    : register(t31);
Texture2D<half1>        half1_tex   : register(t32);
Texture2D<half2>        half2_tex   : register(t33);
Texture2D<half3>        half3_tex   : register(t34);
Texture2D<half4>        half4_tex   : register(t35);

// Double precision moved to double_precision.hlsl so we can compile this for
// ps_4_0 to test ld (not _indexable) variants

// min precision types moved to min_precision.hlsl

typedef vector<int, 1> v1;
Texture2D<v1> vector_tex : register(t44);

// XXX: Is it possible to have mixed types in anything other than a structured
// buffer?

Texture2DMS<float4, 1> msaa1_tex : register(t45);
Texture2DMS<float4, 2> msaa2_tex : register(t46);
Texture2DMS<float4, 3> msaa3_tex : register(t47);
Texture2DMS<float4, 4> msaa4_tex : register(t48);
Texture2DMS<float4, 5> msaa5_tex : register(t49);
Texture2DMS<float4, 6> msaa6_tex : register(t50);
Texture2DMS<float4, 7> msaa7_tex : register(t51);
Texture2DMS<float4, 8> msaa8_tex : register(t52);
Texture2DMS<float4, 9> msaa9_tex : register(t53);
Texture2DMS<float4, 10> msaa10_tex : register(t54);
Texture2DMS<float4, 11> msaa11_tex : register(t55);
Texture2DMS<float4, 12> msaa12_tex : register(t56);
Texture2DMS<float4, 13> msaa13_tex : register(t57);
Texture2DMS<float4, 14> msaa14_tex : register(t58);
Texture2DMS<float4, 15> msaa15_tex : register(t59);
Texture2DMS<float4, 16> msaa16_tex : register(t60);
Texture2DMS<float4, 17> msaa17_tex : register(t61);
Texture2DMS<float4, 18> msaa18_tex : register(t62);
Texture2DMS<float4, 19> msaa19_tex : register(t63);
Texture2DMS<float4, 20> msaa20_tex : register(t64);
Texture2DMS<float4, 21> msaa21_tex : register(t65);
Texture2DMS<float4, 22> msaa22_tex : register(t66);
Texture2DMS<float4, 23> msaa23_tex : register(t67);
Texture2DMS<float4, 24> msaa24_tex : register(t68);
Texture2DMS<float4, 25> msaa25_tex : register(t69);
Texture2DMS<float4, 26> msaa26_tex : register(t70);
Texture2DMS<float4, 27> msaa27_tex : register(t71);
Texture2DMS<float4, 28> msaa28_tex : register(t72);
Texture2DMS<float4, 29> msaa29_tex : register(t73);
Texture2DMS<float4, 30> msaa30_tex : register(t74);
Texture2DMS<float4, 31> msaa31_tex : register(t75);
Texture2DMS<float4, 32> msaa32_tex : register(t76);

Texture2DMSArray<float4, 1> msaa1_array : register(t77);
Texture2DMSArray<float4, 2> msaa2_array : register(t78);
Texture2DMSArray<float4, 3> msaa3_array : register(t79);
Texture2DMSArray<float4, 4> msaa4_array : register(t80);
Texture2DMSArray<float4, 5> msaa5_array : register(t81);
Texture2DMSArray<float4, 6> msaa6_array : register(t82);
Texture2DMSArray<float4, 7> msaa7_array : register(t83);
Texture2DMSArray<float4, 8> msaa8_array : register(t84);
Texture2DMSArray<float4, 9> msaa9_array : register(t85);
Texture2DMSArray<float4, 10> msaa10_array : register(t86);
Texture2DMSArray<float4, 11> msaa11_array : register(t87);
Texture2DMSArray<float4, 12> msaa12_array : register(t88);
Texture2DMSArray<float4, 13> msaa13_array : register(t89);
Texture2DMSArray<float4, 14> msaa14_array : register(t90);
Texture2DMSArray<float4, 15> msaa15_array : register(t91);
Texture2DMSArray<float4, 16> msaa16_array : register(t92);
Texture2DMSArray<float4, 17> msaa17_array : register(t93);
Texture2DMSArray<float4, 18> msaa18_array : register(t94);
Texture2DMSArray<float4, 19> msaa19_array : register(t95);
Texture2DMSArray<float4, 20> msaa20_array : register(t96);
Texture2DMSArray<float4, 21> msaa21_array : register(t97);
Texture2DMSArray<float4, 22> msaa22_array : register(t98);
Texture2DMSArray<float4, 23> msaa23_array : register(t99);
Texture2DMSArray<float4, 24> msaa24_array : register(t100);
Texture2DMSArray<float4, 25> msaa25_array : register(t101);
Texture2DMSArray<float4, 26> msaa26_array : register(t102);
Texture2DMSArray<float4, 27> msaa27_array : register(t103);
Texture2DMSArray<float4, 28> msaa28_array : register(t104);
Texture2DMSArray<float4, 29> msaa29_array : register(t105);
Texture2DMSArray<float4, 30> msaa30_array : register(t106);
Texture2DMSArray<float4, 31> msaa31_array : register(t107);
Texture2DMSArray<float4, 32> msaa32_array : register(t108);

SamplerState samp : register(s0);
SamplerComparisonState samp_c : register(s1);

ByteAddressBuffer byte_buf : register(t110);

struct foo {
	float foo;
	uint bar;
	snorm float baz;
	int buz;
};
StructuredBuffer<struct foo> struct_buf : register(t111);

void main(out float4 output : SV_Target0)
{
	output = 0;
	uint uwidth, uheight, umips, udim;
	float fwidth, fheight, fmips;

	// Use all textures to ensure the compiler doesn't optimise them out,
	// and to see what the load instructions look like:

	// All become float4 textures in the bytecode, only the signature
	// sections differentiate these:
	output += float_tex.Load(0);
	output += float1_tex.Load(0);
	output += float4(float2_tex.Load(0), 0, 0);
	output += float4(float3_tex.Load(0), 0);
	output += float4_tex.Load(0);

	output += uint_tex.Load(0);
	output += uint1_tex.Load(0);
	output += float4(uint2_tex.Load(0), 0, 0);
	output += float4(uint3_tex.Load(0), 0);
	output += uint4_tex.Load(0);

	output += sint_tex.Load(0);
	output += sint1_tex.Load(0);
	output += float4(sint2_tex.Load(0), 0, 0);
	output += float4(sint3_tex.Load(0), 0);
	output += sint4_tex.Load(0);

	output += unorm_tex.Load(0);
	output += unorm1_tex.Load(0);
	output += float4(unorm2_tex.Load(0), 0, 0);
	output += float4(unorm3_tex.Load(0), 0);
	output += unorm4_tex.Load(0);

	output += snorm_tex.Load(0);
	output += snorm1_tex.Load(0);
	output += float4(snorm2_tex.Load(0), 0, 0);
	output += float4(snorm3_tex.Load(0), 0);
	output += snorm4_tex.Load(0);

	output += bool_tex.Load(0);
	output += bool1_tex.Load(0);
	output += float4(bool2_tex.Load(0), 0, 0);
	output += float4(bool3_tex.Load(0), 0);
	output += bool4_tex.Load(0);

	output += dword_tex.Load(0);

	output += half_tex.Load(0);
	output += half1_tex.Load(0);
	output += float4(half2_tex.Load(0), 0, 0);
	output += float4(half3_tex.Load(0), 0);
	output += half4_tex.Load(0);

	output += vector_tex.Load(0);

	output += msaa1_tex.Load(0, 0);
	output += msaa2_tex.Load(0, 0);
	output += msaa3_tex.Load(0, 0);
	output += msaa4_tex.Load(0, 0);
	output += msaa5_tex.Load(0, 0);
	output += msaa6_tex.Load(0, 0);
	output += msaa7_tex.Load(0, 0);
	output += msaa8_tex.Load(0, 0);
	output += msaa9_tex.Load(0, 0);
	output += msaa10_tex.Load(0, 0);
	output += msaa11_tex.Load(0, 0);
	output += msaa12_tex.Load(0, 0);
	output += msaa13_tex.Load(0, 0);
	output += msaa14_tex.Load(0, 0);
	output += msaa15_tex.Load(0, 0);
	output += msaa16_tex.Load(0, 0);
	output += msaa17_tex.Load(0, 0);
	output += msaa18_tex.Load(0, 0);
	output += msaa19_tex.Load(0, 0);
	output += msaa20_tex.Load(0, 0);
	output += msaa21_tex.Load(0, 0);
	output += msaa22_tex.Load(0, 0);
	output += msaa23_tex.Load(0, 0);
	output += msaa24_tex.Load(0, 0);
	output += msaa25_tex.Load(0, 0);
	output += msaa26_tex.Load(0, 0);
	output += msaa27_tex.Load(0, 0);
	output += msaa28_tex.Load(0, 0);
	output += msaa29_tex.Load(0, 0);
	output += msaa30_tex.Load(0, 0);
	output += msaa31_tex.Load(0, 0);
	output += msaa32_tex.Load(0, 0);

	output += msaa1_array.Load(0, 0);
	output += msaa2_array.Load(0, 0);
	output += msaa3_array.Load(0, 0);
	output += msaa4_array.Load(0, 0);
	output += msaa5_array.Load(0, 0);
	output += msaa6_array.Load(0, 0);
	output += msaa7_array.Load(0, 0);
	output += msaa8_array.Load(0, 0);
	output += msaa9_array.Load(0, 0);
	output += msaa10_array.Load(0, 0);
	output += msaa11_array.Load(0, 0);
	output += msaa12_array.Load(0, 0);
	output += msaa13_array.Load(0, 0);
	output += msaa14_array.Load(0, 0);
	output += msaa15_array.Load(0, 0);
	output += msaa16_array.Load(0, 0);
	output += msaa17_array.Load(0, 0);
	output += msaa18_array.Load(0, 0);
	output += msaa19_array.Load(0, 0);
	output += msaa20_array.Load(0, 0);
	output += msaa21_array.Load(0, 0);
	output += msaa22_array.Load(0, 0);
	output += msaa23_array.Load(0, 0);
	output += msaa24_array.Load(0, 0);
	output += msaa25_array.Load(0, 0);
	output += msaa26_array.Load(0, 0);
	output += msaa27_array.Load(0, 0);
	output += msaa28_array.Load(0, 0);
	output += msaa29_array.Load(0, 0);
	output += msaa30_array.Load(0, 0);
	output += msaa31_array.Load(0, 0);
	output += msaa32_array.Load(0, 0);

	// ps_4_0: ld_aoffimmi:
	// ps_5_0: ld_aoffimmi_indexable:
	output += float_tex.Load(int3(5,5,5), int2(2, 2));

	// ld_indexable (same as .Load but without mip maps):
	output += float_tex[uint2(2,3)];

	// ld_indexable again (same as .Load with mip maps):
	output += float_tex.mips[4][uint2(2,3)];

	// Requires D3D 11.2 / Win8.1:
	// uint status;
	// output += float_tex.Load(int3(5,5,0), int2(2, 2), status);
	// output.x +=  CheckAccessFullyMapped(status);

	// ps_5_0: resinfo_indexable(...)(...)_uint
	// ps_4_0: resinfo_uint
	float_tex.GetDimensions(0, uwidth, uheight, umips);
	output += uwidth + uheight + umips;

	// ps_5_0: resinfo_indexable(...)(...)
	// ps_4_0: resinfo
	float_tex.GetDimensions(0, fwidth, fheight, fmips);
	output += fwidth + fheight + fmips;

	// TODO: resinfo_rcpFloat

	// ps_4_0: ldms_aoffimmi:
	// ps_5_0: ldms_aoffimmi_indexable:
	output += msaa1_tex.Load(0, 0, int2(2, 2));

	// ps_4_0: sample
	// ps_5_0: sample_indexable
	output += float_tex.Sample(samp, float2(0.2, 0.3));

	// ps_4_0: sample_aoffimmi
	// ps_5_0: sample_aoffimmi_indexable
	output += float_tex.Sample(samp, float2(0.2, 0.3), int2(2,3));

	// ps_4_0: sample_c
	// ps_5_0: sample_c_indexable
	output += float_tex.SampleCmp(samp_c, float2(0.1, 0.2), 0.5);

	// ps_4_0: sample_c_aoffimmi
	// ps_5_0: sample_c_aoffimmi_indexable
	output += float_tex.SampleCmp(samp_c, float2(0.1, 0.2), 0.5, int2(1,2));

	// ps_4_0: sample_c_lz
	// ps_5_0: sample_c_lz_indexable
	output += float_tex.SampleCmpLevelZero(samp_c, float2(0.1, 0.2), 0.5);

	// ps_4_0: sample_c_lz_aoffimmi
	// ps_5_0: sample_c_lz_aoffimmi_indexable
	output += float_tex.SampleCmpLevelZero(samp_c, float2(0.1, 0.2), 0.5, int2(1,2));

	// ps_4_0: sample_b
	// ps_5_0: sample_b_indexable
	output += float_tex.SampleBias(samp, float2(0.4, 0.1), 0.1);

	// ps_4_0: sample_b_aoffimmi
	// ps_5_0: sample_b_aoffimmi_indexable
	output += float_tex.SampleBias(samp, float2(0.4, 0.1), 0.1, int2(2,3));

	// ps_4_0: sample_d
	// ps_5_0: sample_d_indexable
	output += float_tex.SampleGrad(samp, float2(0.4, 0.1), 0.6, 0.7);

	// ps_4_0: sample_d_aoffimmi
	// ps_5_0: sample_d_aoffimmi_indexable
	output += float_tex.SampleGrad(samp, float2(0.4, 0.1), 0.6, 0.7, int2(3,4));

	// ps_4_0 ld_raw
	// ps_5_0 ld_raw_indexable
	output += byte_buf.Load(0);
	output += float4(byte_buf.Load2(1), 0, 0);
	output += float4(byte_buf.Load3(2), 0);
	output += byte_buf.Load4(3);

	// ps_4_0 ld_structured
	// ps_5_0 ld_structured_indexable
	output += struct_buf.Load(0).foo;
	output += struct_buf.Load(2).bar;
	output += struct_buf.Load(1).baz;
	output += struct_buf.Load(3).buz;
}
