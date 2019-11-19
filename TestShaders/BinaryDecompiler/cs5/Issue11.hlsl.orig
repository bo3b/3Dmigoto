Texture2D<float4> Input;
RWTexture2D<float4> Result;

#define blocksize 8
#define groupthreads (blocksize*blocksize)
groupshared float4 accum[groupthreads];

[numthreads(blocksize,blocksize,1)]
void main( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex )
{
	accum[GI] = Input[DTid.xy];

	// Parallel reduction algorithm
	GroupMemoryBarrierWithGroupSync();
	if (GI < 32)
		accum[GI] += accum[32+GI];
	GroupMemoryBarrierWithGroupSync();
	if (GI < 16)
		accum[GI] += accum[16+GI];
	GroupMemoryBarrierWithGroupSync();
	if (GI < 8)
		accum[GI] += accum[8+GI];
	GroupMemoryBarrierWithGroupSync();
	if (GI < 4)
		accum[GI] += accum[4+GI];
	GroupMemoryBarrierWithGroupSync();
	if (GI < 2)
		accum[GI] += accum[2+GI];
	GroupMemoryBarrierWithGroupSync();
	if (GI < 1)
		accum[GI] += accum[1+GI];

	if (GI == 0)
	{                
		Result[uint2(Gid.x,Gid.y)] = accum[0] / groupthreads;
	}
}
