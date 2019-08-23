// Set to 0, 1 or 2 to test different depth semantics:
#define TEST_DEPTH 0

void main(
	nointerpolation float texcoord0 : TEXCOORD0,
	linear float texcoord1 : TEXCOORD1,
	linear centroid float texcoord2 : TEXCOORD2,
	linear noperspective float texcoord3 : TEXCOORD3,
	linear noperspective centroid float texcoord4 : TEXCOORD4,
	linear sample float texcoord5 : TEXCOORD5,
	linear noperspective sample float texcoord6 : TEXCOORD6,

	linear sample float4 ipos : SV_Position,
	out float4 target0 : SV_Target0,
	uint coverage_in : SV_Coverage,
	out uint coverage_out : SV_Coverage,
	float clip : SV_ClipDistance0,
	uint vi : SV_ViewportArrayIndex,
#if TEST_DEPTH == 1
	out float dge : SV_DepthGreaterEqual,
#elif TEST_DEPTH == 2
	out float dle : SV_DepthLessEqual,
#else
	out float depth_out : SV_Depth,
#endif
	uint rt : SV_RenderTargetArrayIndex,
	uint prim : SV_PrimitiveID,
	uint si : SV_SampleIndex
	)
{
	target0 = 1;

	target0 += texcoord0;
	target0 += texcoord1;
	target0 += texcoord2;
	target0 += texcoord3;
	target0 += texcoord4;
	target0 += texcoord5;
	target0 += texcoord6;

	coverage_out = 0;
	target0 += ipos;
	target0 += coverage_in;
	target0 += clip;
	target0 += vi; // exposes an assembler bug
#if TEST_DEPTH == 1
	dge = 1;
#elif TEST_DEPTH == 2
	dle = 1;
#else
	depth_out = 0.2;
#endif
	target0 += rt;
	target0 += si;

	// eval_sample_index
	target0 += EvaluateAttributeAtSample(texcoord1, 1);

	// eval_sample_snapped
	target0 += EvaluateAttributeSnapped(texcoord1, int2(2,3));

	// eval_sample_centroidd
	target0 += EvaluateAttributeCentroid(texcoord1);
}

// Funky:
//uint SV_Coverage, pixel:io
//SV_Depth	Depth buffer data. Can be written or read by any shader.	float
//SV_DepthGreaterEqual(n)	Valid in any shader, tests whether the value is greater than or equal to the depth data value.	unknown
//SV_DepthLessEqual(n)	Valid in any shader, tests whether the value is less than or equal to the depth data value.	unknown

// Untested:
//SV_StencilRef - needs ps_5_1
//SV_InnerCoverage - needs ps_5_1

