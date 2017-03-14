void main(
	float4 ipos : SV_Position,
	uint id : SV_VertexID,
	out float4 opos : SV_Position,
	out float clip0 : SV_ClipDistance0,
	out float cull0 : SV_CullDistance0,
	out min16float v0 : TEXCOORD0,
	out min10float v1 : TEXCOORD1,
	out min16int v2 : TEXCOORD2,
	out min12int v3 : TEXCOORD3,
	out min16uint v4 : TEXCOORD4
	)
{
	opos = ipos;
	opos.x += id;
	clip0 = 0;
	cull0 = 0;
}

// Funky:
//SV_Depth	Depth buffer data. Can be written or read by any shader.	float
//SV_DepthGreaterEqual(n)	Valid in any shader, tests whether the value is greater than or equal to the depth data value.	unknown
//SV_DepthLessEqual(n)	Valid in any shader, tests whether the value is less than or equal to the depth data value.	unknown
