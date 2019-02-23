Texture2DMS<float4> t0 : register(t0);
Texture2DMS<float4, 8> t1 : register(t1);
Texture2DMSArray<float4, 8> t2 : register(t2);

void main(
	int4 v0: TEXCOORD,
	int4 v1: TEXCOORD1,
	out float4 o0: SV_Target0
)
{
	// samplepos on a multi-sampled texture
	o0.xy = t0.GetSamplePosition(v0.x);
	o0.zw = t0.GetSamplePosition(v1.y);
	o0.zw += t1.GetSamplePosition(0);
	o0.zw += t1.GetSamplePosition(1);
	o0.zw += t2.GetSamplePosition(0);
	o0.zw += t2.GetSamplePosition(v0.y + 2);

	// samplepos on the rasterizer register
	o0.xy += GetRenderTargetSamplePosition(0);
	o0.xy += GetRenderTargetSamplePosition(1);
	o0.xy += GetRenderTargetSamplePosition(v0.y);
	o0.xy += GetRenderTargetSamplePosition(v1.y);

	o0.z += GetRenderTargetSampleCount();
}
