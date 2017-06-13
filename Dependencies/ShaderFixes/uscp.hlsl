Texture2D<float4> t101 : register(t101);
SamplerState SampleType;


void main(float4 pos : SV_Position0,  float2 texcoord : TEXCOORD0, out float4 result : SV_Target0)
{
	float2 tex = texcoord.xy;
	result = t101.Sample(SampleType, tex);
}
