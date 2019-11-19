cbuffer cbConstant
{
    float3 vLightDir;
};

cbuffer cbChangesEveryFrame
{
    matrix World;
    matrix View;
    matrix Projection;
    float Time;
};

Texture2D g_txDiffuse;
SamplerState samLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

struct VS_INPUT
{
    float3 Pos          : POSITION;
    float3 Norm         : NORMAL;
    float2 Tex          : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 Norm : TEXCOORD0;
    float2 Tex : TEXCOORD1;
    float ClipD : SV_ClipDistance;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Norm : TEXCOORD0;
    float2 Tex : TEXCOORD1;
};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS( VS_INPUT input )
{
    VS_OUTPUT output = (VS_OUTPUT)0;
    
    output.Pos = mul( float4(input.Pos,1), World );
    output.Pos = mul( output.Pos, View );

    float angle = 0.17 * Time; //10 degrees per second
    float4 plane = float4(cos(angle), sin(angle), 0, 7);
    output.ClipD = dot(output.Pos, plane);

    output.Pos = mul( output.Pos, Projection );
    output.Norm.xyz = mul( input.Norm, (float3x3)World );
    output.Norm.w = 1;
    output.Tex = input.Tex;
    
    return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( PS_INPUT input) : SV_Target
{
    return g_txDiffuse.Sample( samLinear, input.Tex );
}
