
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

Texture3D TextureBase;
Texture3D TextureDetail;

float LodToSample;

float4 main( PS_INPUT input ) : SV_Target
{
    float4 base = TextureBase.SampleLevel(TextureSampler, input.TexC, LodToSample);
    float4 detail = TextureDetail.SampleLevel(TextureSampler, input.TexC, LodToSample, int3(3, 2, 1));
    return base * detail;
}


