
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

static const uint BaseTexIndex = 0;
static const uint DetailTexIndex = 1;

Texture2D SomeTextures[2];

float4 main( PS_INPUT input ) : SV_Target
{
    float4 base = SomeTextures[BaseTexIndex].Sample(TextureSampler, input.TexC);
    float4 detail = SomeTextures[DetailTexIndex].Sample(TextureSampler, input.TexC);
    return base * detail;
}


