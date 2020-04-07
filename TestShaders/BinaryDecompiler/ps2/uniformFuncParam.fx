
float4 main(  float2 vTex0 : TEXCOORD0,
              uniform sampler2D diffuseMap      : register(S0),
              uniform float4    diffuseMaterialColor : register(C0)) : COLOR0
{
    return (tex2D( diffuseMap, vTex0 ) * diffuseMaterialColor);
}
