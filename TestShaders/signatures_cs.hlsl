RWByteAddressBuffer buf : register(u0);

[numthreads(4, 2, 1)]
void main(
	uint3 tid : SV_DispatchThreadID,
	uint3 gid : SV_GroupID,
	uint gi : SV_GroupIndex,
	uint3 gtid: SV_GroupThreadID
	)
{
	buf.InterlockedCompareStore(0, 1, (uint)(tid + gid + gi + gtid));
}
