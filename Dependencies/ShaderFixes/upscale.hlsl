// Upscaling: This shader creates a quad and texture coordinates to be used by the 
// pixel shader for full screen blits.

#ifdef VERTEX_SHADER
void main(
		out float4 pos : SV_Position0,  out float2 texcoord : TEXCOORD0,
		uint vertex : SV_VertexID)
{
	// Not using vertex buffers so manufacture our own coordinates.
	switch(vertex) {
		case 0:
			pos.xy = float2(-1, -1);
			texcoord = float2(0,1);
			break;
		case 1:
			pos.xy = float2(-1, 1);
			texcoord = float2(0,0);
			break;
		case 2:
			pos.xy = float2(1, -1);
			texcoord = float2(1,1);
			break;
		case 3:
			pos.xy = float2(1, 1);
			texcoord = float2(1,0);
			break;
		default:
			pos.xy = 0;
			texcoord = float2(0,0);
			break;
	};
	pos.zw = float2(0, 1);
}
#endif /* VERTEX_SHADER */

#ifdef PIXEL_SHADER
// This shader uses provided texture coordinates and sampler to blit
// the game screen with 3Dmigoto overlays to the actual swap chain

Texture2D<float4> t101 : register(t101);
SamplerState SampleType;

void main(float4 pos : SV_Position0,  float2 texcoord : TEXCOORD0, out float4 result : SV_Target0)
{
	float2 tex = texcoord.xy;
	result = t101.Sample(SampleType, tex);
}
#endif /* PIXEL_SHADER */
