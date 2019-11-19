struct InVertex
{
    float3 pos          : POSITION;
    float3 scale         : NORMAL;
};
struct OutVertex
{
    float3 pos          : OUT_POSITION;
};

[maxvertexcount(18)]
[instance(24)]
void main(point InVertex verts[1], 
	inout PointStream<OutVertex> myStream,
	uint InstanceID : SV_GSInstanceID)
{
	OutVertex myVert;
	myVert.pos = verts[0].pos * verts[0].scale * InstanceID;
	myStream.Append( myVert );
}
