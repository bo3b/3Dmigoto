struct PSSceneIn {
	float4 pos : SV_Position;
	float4 coord : TEXCOORD0;
};

[maxvertexcount(3)]
[instance(4)] // Adds dcl_gsinstances 4
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
