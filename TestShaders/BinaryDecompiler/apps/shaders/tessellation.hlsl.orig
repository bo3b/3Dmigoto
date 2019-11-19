struct IA_OUTPUT
{
    float4 cpoint : CPOINT;
    float4 colour : COLOR;
};

struct VS_OUTPUT
{
    float4 cpoint : SV_Position;
    float4 colour : COLOR;
};

matrix World;
matrix View;
matrix Projection;

VS_OUTPUT VS(IA_OUTPUT input)
{
    VS_OUTPUT output;

    output.cpoint = mul( input.cpoint, World );
    output.cpoint = mul( output.cpoint, View );
    output.cpoint = mul( output.cpoint, Projection );

    output.colour = input.colour;
    return output;
}

struct HS_CONSTANT_OUTPUT
{
    float edges[4] : SV_TessFactor;
    float innerEdges[2] : SV_InsideTessFactor;
};

struct HS_OUTPUT
{
    float4 cpoint : SV_Position;
    float4 colour : COLOR;
};

float InnerFactor = 1.0f;
float OuterFactor = 1.0f;

HS_CONSTANT_OUTPUT HSConst()
{
    HS_CONSTANT_OUTPUT output;

    output.innerEdges[0] = InnerFactor;
    output.innerEdges[1] = InnerFactor;
    output.edges[0] = OuterFactor;
    output.edges[1] = OuterFactor;
    output.edges[2] = OuterFactor;
    output.edges[3] = OuterFactor;

    return output;
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(4)]
[patchconstantfunc("HSConst")]
HS_OUTPUT HS(InputPatch<VS_OUTPUT, 4> ip, uint id : SV_OutputControlPointID)
{
    HS_OUTPUT output;
    output.cpoint = ip[id].cpoint;
    output.colour = ip[id].colour;
    return output;
}

struct DS_OUTPUT
{
    float4 position : SV_Position;
    float4 colour : COLOR;
};


float4 interpolate(float2 TessCoord, float4 v0, float4 v1, float4 v2, float4 v3)
{
	float4 a = lerp(v0, v1, TessCoord.x);
	float4 b = lerp(v3, v2, TessCoord.x);
	return lerp(a, b, TessCoord.y);
}

[domain("quad")]
DS_OUTPUT DS(HS_CONSTANT_OUTPUT input, OutputPatch<HS_OUTPUT, 4> op, float2 uv : SV_DomainLocation)
{
    DS_OUTPUT output;

    output.position = interpolate(uv, op[0].cpoint, op[1].cpoint, op[2].cpoint, op[3].cpoint);

    output.colour = interpolate(uv, op[0].colour, op[1].colour, op[2].colour, op[3].colour);

    return output;
}


float4 PS(DS_OUTPUT input) : SV_Target0
{
    return input.colour;
}


