Texture2DMS<float4> tex;

void main(
	int4 v0: TEXCOORD,
	int4 v1: TEXCOORD1,
	out float4 o0: SV_Target0
)
{
	o0.xy = tex.GetSamplePosition(v0.x);
	o0.xy += tex.GetSamplePosition(v1.y);
	o0.zw = tex.GetSamplePosition(0);
	o0.zw += tex.GetSamplePosition(1);
}
