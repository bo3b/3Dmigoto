
struct PS_INPUT
{
    float4 TexC : TEXCOORD0;
};
SamplerState TextureSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

Texture1D TextureBase;
Texture1D TextureDetail;

float LodToSample;

float4 main( PS_INPUT input ) : SV_Target
{
    float4 base = TextureBase.SampleLevel(TextureSampler, input.TexC, LodToSample);
    float4 detail = TextureDetail.SampleLevel(TextureSampler, input.TexC, LodToSample, 1);
    return base * detail;
}


