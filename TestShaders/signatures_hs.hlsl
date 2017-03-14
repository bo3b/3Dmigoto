


// Input control point
struct VS_CONTROL_POINT_OUTPUT
{
	float3 vPosition : WORLDPOS;
	float2 vUV       : TEXCOORD0;
	float3 vTangent  : TANGENT;
};

// Output control point
struct BEZIER_CONTROL_POINT
{
	float3 vPosition    : BEZIERPOS;
};


// Output patch constant data.
struct HS_CONSTANT_DATA_OUTPUT
{
	float Edges[4]        : SV_TessFactor;
	float Inside[2]       : SV_InsideTessFactor;

	float3 vTangent[4]    : TANGENT;
	float2 vUV[4]         : TEXCOORD;
	float3 vTanUCorner[4] : TANUCORNER;
	float3 vTanVCorner[4] : TANVCORNER;
	float4 vCWts          : TANWEIGHTS;
};

#define MAX_POINTS 32

// Patch Constant Function
HS_CONSTANT_DATA_OUTPUT SubDToBezierConstantsHS(
		InputPatch<VS_CONTROL_POINT_OUTPUT, MAX_POINTS> ip,
		uint PatchID : SV_PrimitiveID )
{
	HS_CONSTANT_DATA_OUTPUT Output;

	// Insert code to compute Output here
	Output.Edges[0] = 0;
	Output.Edges[1] = 0;
	Output.Edges[2] = 0;
	Output.Edges[3] = 0;
	Output.Inside[0] = 0;
	Output.Inside[1] = 0;

	return Output;
}

[domain("quad")] 
[partitioning("integer")] 
[outputtopology("triangle_cw")] 
[outputcontrolpoints(16)] 
[patchconstantfunc("SubDToBezierConstantsHS")] 
BEZIER_CONTROL_POINT main(  
		InputPatch<VS_CONTROL_POINT_OUTPUT, MAX_POINTS> ip,  
		uint i : SV_OutputControlPointID, 
		uint PatchID : SV_PrimitiveID ) 
{ 
	BEZIER_CONTROL_POINT Output; 

	// Insert code to compute Output here. 
	Output.vPosition.x = PatchID;

	return Output; 
} 

// Funky:
//SV_Depth	Depth buffer data. Can be written or read by any shader.	float
//SV_DepthGreaterEqual(n)	Valid in any shader, tests whether the value is greater than or equal to the depth data value.	unknown
//SV_DepthLessEqual(n)	Valid in any shader, tests whether the value is less than or equal to the depth data value.	unknown

//SV_DomainLocation 	Defines the location on the hull of the current domain point being evaluated. Available as input to the domain shader. (read only)	float2|3
//SV_InsideTessFactor 	Defines the tessellation amount within a patch surface. Available in the hull shader for writing, and available in the domain shader for reading.	float|float[2]
//SV_OutputControlPointID 	Defines the index of the control point ID being operated on by an invocation of the main entry point of the hull shader. Can be read by the hull shader only.	uint
//SV_TessFactor 	Defines the tessellation amount on each edge of a patch. Available for writing in the hull shader and reading in the domain shader.	float[2|3|4]
