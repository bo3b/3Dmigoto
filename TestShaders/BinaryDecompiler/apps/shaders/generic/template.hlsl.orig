cbuffer cbConstant
{
    float3 vLightDir = float3(-0.577,0.577,-0.577);
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

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Norm : TEXCOORD0;
    float2 Tex : TEXCOORD1;
};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS( VS_INPUT input )
{
    PS_INPUT output = (PS_INPUT)0;
    
    output.Pos = mul( float4(input.Pos,1), World );
    output.Pos = mul( output.Pos, View );
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


//Post Processing
Texture2D g_txColourBuffer;
Texture2D g_txDepthBuffer;

struct VS_POSTFX_INPUT
{
    float3 Pos          : POSITION;
};

struct PS_POSTFX_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

PS_POSTFX_INPUT VS_PostFX( VS_POSTFX_INPUT input )
{
    PS_POSTFX_INPUT output = (PS_POSTFX_INPUT)0;
    
    output.Pos = float4(input.Pos, 1);
    output.Tex = (float2(1, 1) + input.Pos.xy) / float2(2, 2);

    return output;
}

float4 PS_PostFX( PS_POSTFX_INPUT input) : SV_Target
{
    return g_txColourBuffer.Sample( samLinear, input.Tex );
}
