// Upscaling: This shader creates a quad and texture coordinates to be used by the 
// pixel shader for full screen blits.

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
