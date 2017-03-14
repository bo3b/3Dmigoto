// Resource type tests that require shader model 5
// Compile with fxc /T ps_5_0 /Fo resource_types5.bin

Texture2D<float>        float_tex   : register(t0); // Same as float1
Texture2D<float1>       float1_tex  : register(t1);
Texture2D<float2>       float2_tex  : register(t2);
Texture2D<float3>       float3_tex  : register(t3);
Texture2D<float4>       float4_tex  : register(t4);

Buffer<float4> buf_resource : register(t109);

SamplerState samp : register(s0);
SamplerComparisonState samp_c : register(s1);

RWTexture2D<float> rwfloat4_tex : register(u1);

void main(out float4 output : SV_Target0)
{
	output = 0;
	uint uwidth, uheight, umips, udim;
	float fwidth, fheight, fmips;

	// Use all textures to ensure the compiler doesn't optimise them out,
	// and to see what the load instructions look like:

	// bufinfo_indexable(...)(...):
	buf_resource.GetDimensions(udim);
	output += udim;

	// gather4_indexable
	output += float_tex.Gather(samp, float2(0.5, 0.3));

	// gather4_aoffimmi_indexable
	output += float_tex.Gather(samp, float2(0.5, 0.3), int2(1,2));

	// gather4_c_indexable
	output += float_tex.GatherCmp(samp_c, float2(0.5, 0.3), 0.5);

	// gather4_c_aoffimmi_indexable
	output += float_tex.GatherCmp(samp_c, float2(0.5, 0.3), 0.2, int2(1,2));

	// gather4_po_indexable
	float2 offset = float2_tex.Load(0);
	output += float_tex.Gather(samp, float2(0.5, 0.3), offset);

	// gather4_c_po_indexable
	output += float_tex.GatherCmp(samp_c, float2(0.5, 0.3), 0.2, offset);

	// ld_uav_typed_indexable
	output += rwfloat4_tex.Load(0);
}
