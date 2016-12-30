struct PSSceneIn {
	float4 pos : SV_Position;
	float4 coord : TEXCOORD0;
	float clip : SV_ClipDistance;        // dcl_output_siv o2.x, clip_distance
	float cull : SV_CullDistance;        // dcl_output_siv o2.y, cull_distance
	bool ff : SV_IsFrontFace;            // dcl_output_sgv o3.x, is_front_face
	uint rt : SV_RenderTargetArrayIndex; // dcl_output_siv o3.y, rendertarget_array_index
	uint vp : SV_ViewportArrayIndex;     // dcl_output_siv o3.z, viewport_array_index
};

[maxvertexcount(3)]
void main(
	triangle float4 ipos[3] : SV_Position,
	//inout TriangleStream<PSSceneIn> OutputStream,
	inout PointStream<PSSceneIn> OutputStream,
	inout PointStream<PSSceneIn> OutputStream1,
	uint id : SV_GSInstanceID,
	uint prim : SV_PrimitiveID
	)
{
	PSSceneIn o = (PSSceneIn)0;

	o.pos = 0;
	o.coord = id;
	o.coord += prim;
	OutputStream.Append(o);
	OutputStream1.Append(o);
	OutputStream.RestartStrip();
	OutputStream1.RestartStrip();
	OutputStream.Append(o);
	OutputStream1.Append(o);
	OutputStream.RestartStrip();
	OutputStream1.RestartStrip();
	OutputStream.Append(o);
	OutputStream1.Append(o);
}

// Funky:
//SV_Depth	Depth buffer data. Can be written or read by any shader.	float
//SV_DepthGreaterEqual(n)	Valid in any shader, tests whether the value is greater than or equal to the depth data value.	unknown
//SV_DepthLessEqual(n)	Valid in any shader, tests whether the value is less than or equal to the depth data value.	unknown
