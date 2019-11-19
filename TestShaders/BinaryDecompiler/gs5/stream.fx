struct InVertex
{
    float3 pos          : POSITION;
    float3 scale         : NORMAL;
};
struct OutVertex1
{
    float3 pos          : OUT_POSITION;
};

struct OutVertex2
{
    int3 pos          : OUT_POSITION;
};

[maxvertexcount(18)]
void main( 
    line InVertex verts[2], 
    inout PointStream<OutVertex1> myStream1, 
    inout PointStream<OutVertex2> myStream2 )
{
	OutVertex1 myVert1;
	OutVertex2 myVert2;
    myVert1.pos = verts[0].pos * verts[0].scale;
    myVert2.pos = verts[1].pos * verts[1].scale;
    myStream1.Append( myVert1 );
    myStream2.Append( myVert2 );
}
