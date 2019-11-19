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
	float3 LuminanceConv = { 0.2125f, 0.7154f, 0.0721f };
    return dot(g_txColourBuffer.Sample( samLinear, input.Tex ), LuminanceConv);
}
