RWTexture2D<float4> Output;

float Time;

[numthreads(32, 32, 1)]
void main( uint3 threadID : SV_DispatchThreadID, uint2 localID : SV_GroupThreadID )
{
	float4 from = float4(length(localID.xy)/32, 0, 0, 1);
	float4 to = float4(0, length(localID.xy)/32, 0, 1);
	float4 colour = lerp(from, to, sin(Time));

    Output[threadID.xy] = colour;
}
