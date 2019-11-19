
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

struct PS_OUTPUT
{
    float4 Colour0 : SV_Target0;
    float4 Colour1 : SV_Target1;
    float4 Colour2 : SV_Target2;
};

PS_OUTPUT main( PS_INPUT input )
{
    PS_OUTPUT outPix;
    float4 base = TextureBase.Sample(TextureSampler, input.TexC.xyz);
    float4 detail = TextureDetail.Sample(TextureSampler, input.TexC.xyz, int3(1, 2, 3));

    float4 grad_base = TextureBase.SampleGrad(TextureSampler, input.TexC.xyz, 0.3, 0.4, 4);
    float4 grad_detail = TextureDetail.SampleGrad(TextureSampler, input.TexC.xyz, 0.3, 0.4, int3(0, 3, 1));

    float4 bias_base = TextureBase.SampleBias(TextureSampler, input.TexC.xyz, 0.2);
    float4 bias_detail = TextureDetail.SampleBias(TextureSampler, input.TexC.xyz, 0.2, int3(1, 2, -3));

    outPix.Colour0 =  base * detail;
    outPix.Colour1 =  grad_base * grad_detail;
    outPix.Colour2 =  bias_base * bias_detail;

    return outPix;
}


