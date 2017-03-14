struct vs2gs {
	uint idx : TEXCOORD0;
};

struct gs2ps {
	float4 pos : SV_Position0;
};

// The max here is dictated by 1024 / sizeof(gs2ps)
[maxvertexcount(144)]
void main(lineadj vs2gs input[4], inout PointStream<gs2ps> ostream)
{
	gs2ps output = (gs2ps)0;

	ostream.Append(output);
	ostream.RestartStrip();
	ostream.Append(output);
	ostream.RestartStrip();
	ostream.Append(output);
}
