
//New precision types from d3d 11.1
//min16float - minimum 16-bit floating point value.
//min10float - minimum 10-bit floating point value.
//min16int - minimum 16-bit signed integer.
//min12int - minimum 12-bit signed integer.
//min16uint - minimum 16-bit unsigned integer.

struct PS_INPUT
{
    float4 TexC : TEXCOORD0;
    min10float4 offset : COLOR0;
};
SamplerState TextureSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

Texture2D TextureBase;
Texture2D TextureDetail;

float4 main( PS_INPUT input ) : SV_Target
{
    min16float4 base = TextureBase.Sample(TextureSampler, input.TexC);
    min16float4 detail = TextureDetail.Sample(TextureSampler, input.TexC);
    return base * detail + input.offset;
}


