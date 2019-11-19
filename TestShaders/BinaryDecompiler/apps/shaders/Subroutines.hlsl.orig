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

interface iChangeColour
{
    float4 ChangeColour(float4 colour);
};

class cRedColour : iChangeColour
{
    float4 ChangeColour(float4 colour) {
        return colour * float4( 1, 0, 0, 1);
    }
};

class cGreenColour : iChangeColour
{
    float4 ChangeColour(float4 colour) {
        return colour * float4( 0, 1, 0, 1);
    }
};

class cBlueColour : iChangeColour
{
    float4 ChangeColour(float4 colour) {
        return colour * float4( 0, 0, 1, 1);
    }
};

class cMonochromeColour : iChangeColour
{
    float4 ChangeColour(float4 colour) {
        float3 LuminanceConv = { 0.2125f, 0.7154f, 0.0721f };
        return dot(colour.xyz, LuminanceConv);
    }
};

class cFullColour : iChangeColour
{
    float4 ChangeColour(float4 colour) {
      return colour;
    }
};

iChangeColour gAbstractColourChanger;

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( PS_INPUT input) : SV_Target
{
    float4 texLookup = g_txDiffuse.Sample( samLinear, input.Tex );
    return gAbstractColourChanger.ChangeColour(texLookup);
}

