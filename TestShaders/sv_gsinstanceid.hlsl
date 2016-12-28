struct PSSceneIn {
	float4 pos : SV_Position;
	float4 coord : TEXCOORD0;
};

[maxvertexcount(3)]
void main(
		triangle float4 ipos[3] : SV_Position,
		inout TriangleStream<PSSceneIn> OutputStream,
		uint id : SV_GSInstanceID
	 )
{
	PSSceneIn o;
	o.pos = 0;
	o.coord = id;
	OutputStream.Append(o);
}
