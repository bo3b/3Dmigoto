//Post Processing
Texture2D g_txColourBuffer;
Texture2D g_txDepthBuffer;
SamplerState samLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

struct PS_POSTFX_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

float4 PS( PS_POSTFX_INPUT input) : SV_Target
{
    return float4(1, 1, 1, 1) - g_txColourBuffer.Sample( samLinear, input.Tex );
}
