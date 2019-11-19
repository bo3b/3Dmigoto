Texture2D<float4> Input;
RWTexture2D<float4> Result;

#define blocksize 8
#define groupthreads (blocksize*blocksize)
struct AccumStruct
{
	float accumA;
	float4 accumPadding;
	float2 accumB;
	
};
groupshared AccumStruct accum[groupthreads];

[numthreads(blocksize,blocksize,1)]
void main( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex )
{
	accum[GI].accumA = Input[DTid.xy];
	accum[GI].accumB = float2(1.0 - accum[GI].accumA, 0.5 - accum[GI].accumA);

	// Parallel reduction algorithm
	GroupMemoryBarrierWithGroupSync();
	if (GI < 32)
	{
		accum[GI].accumA += accum[32+GI].accumA;
		accum[GI].accumB += accum[32+GI].accumB;
	}
	GroupMemoryBarrierWithGroupSync();
	if (GI < 16)
		accum[GI].accumA += accum[16+GI].accumA;
	GroupMemoryBarrierWithGroupSync();
	if (GI < 8)
		accum[GI].accumA += accum[8+GI].accumA;
	GroupMemoryBarrierWithGroupSync();
	if (GI < 4)
		accum[GI].accumA += accum[4+GI].accumA;
	GroupMemoryBarrierWithGroupSync();
	if (GI < 2)
		accum[GI].accumA += accum[2+GI].accumA;
	GroupMemoryBarrierWithGroupSync();
	if (GI < 1)
		accum[GI].accumA += accum[1+GI].accumA;

	if (GI == 0)
	{                
		Result[uint2(Gid.x,Gid.y)] = (accum[0].accumA + accum[0].accumB.x * accum[0].accumB.y) / groupthreads;
	}
}
