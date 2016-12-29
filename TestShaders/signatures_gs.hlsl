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

// Funky:
//SV_Depth	Depth buffer data. Can be written or read by any shader.	float
//SV_DepthGreaterEqual(n)	Valid in any shader, tests whether the value is greater than or equal to the depth data value.	unknown
//SV_DepthLessEqual(n)	Valid in any shader, tests whether the value is less than or equal to the depth data value.	unknown
