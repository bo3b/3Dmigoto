// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://create.msdn.com/en-US/education/catalog/sample/stock_effects


Texture2D<float4>   Texture        : register(t0);
TextureCube<float4> EnvironmentMap : register(t1);

sampler Sampler       : register(s0);
sampler EnvMapSampler : register(s1);


cbuffer Parameters : register(b0)
{
    float3 EnvironmentMapSpecular   : packoffset(c0);
    float  EnvironmentMapAmount     : packoffset(c1.x);
    float  FresnelFactor            : packoffset(c1.y);

    float4 DiffuseColor             : packoffset(c2);
    float3 EmissiveColor            : packoffset(c3);

    float3 LightDirection[3]        : packoffset(c4);
    float3 LightDiffuseColor[3]     : packoffset(c7);

    float3 EyePosition              : packoffset(c10);

    float3 FogColor                 : packoffset(c11);
    float4 FogVector                : packoffset(c12);

    float4x4 World                  : packoffset(c13);
    float3x3 WorldInverseTranspose  : packoffset(c17);
    float4x4 WorldViewProj          : packoffset(c20);
};


// We don't use these parameters, but Lighting.fxh won't compile without them.
#define SpecularPower       0
#define SpecularColor       0
#define LightSpecularColor  float3(0, 0, 0)


#include "Structures.fxh"
#include "Common.fxh"
#include "Lighting.fxh"


float ComputeFresnelFactor(float3 eyeVector, float3 worldNormal)
{
    float viewAngle = dot(eyeVector, worldNormal);

    return pow(max(1 - abs(viewAngle), 0), FresnelFactor) * EnvironmentMapAmount;
}


VSOutputTxEnvMap ComputeEnvMapVSOutput(VSInputNmTx vin, uniform bool useFresnel, uniform int numLights)
{
    VSOutputTxEnvMap vout;

    float4 pos_ws = mul(vin.Position, World);
    float3 eyeVector = normalize(EyePosition - pos_ws.xyz);
    float3 worldNormal = normalize(mul(vin.Normal, WorldInverseTranspose));

    ColorPair lightResult = ComputeLights(eyeVector, worldNormal, numLights);

    vout.PositionPS = mul(vin.Position, WorldViewProj);
    vout.Diffuse = float4(lightResult.Diffuse, DiffuseColor.a);

    if (useFresnel)
        vout.Specular.rgb = ComputeFresnelFactor(eyeVector, worldNormal);
    else
        vout.Specular.rgb = EnvironmentMapAmount;

    vout.Specular.a = ComputeFogFactor(vin.Position);
    vout.TexCoord = vin.TexCoord;
    vout.EnvCoord = reflect(-eyeVector, worldNormal);

    return vout;
}


// Vertex shader: basic.
VSOutputTxEnvMap VSEnvMap(VSInputNmTx vin)
{
    return ComputeEnvMapVSOutput(vin, false, 3);
}


// Vertex shader: fresnel.
VSOutputTxEnvMap VSEnvMapFresnel(VSInputNmTx vin)
{
    return ComputeEnvMapVSOutput(vin, true, 3);
}


// Vertex shader: one light.
VSOutputTxEnvMap VSEnvMapOneLight(VSInputNmTx vin)
{
    return ComputeEnvMapVSOutput(vin, false, 1);
}


// Vertex shader: one light, fresnel.
VSOutputTxEnvMap VSEnvMapOneLightFresnel(VSInputNmTx vin)
{
    return ComputeEnvMapVSOutput(vin, true, 1);
}


// Pixel shader: basic.
float4 PSEnvMap(PSInputTxEnvMap pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;
    float4 envmap = EnvironmentMap.Sample(EnvMapSampler, pin.EnvCoord) * color.a;

    color.rgb = lerp(color.rgb, envmap.rgb, pin.Specular.rgb);

    ApplyFog(color, pin.Specular.w);

    return color;
}


// Pixel shader: no fog.
float4 PSEnvMapNoFog(PSInputTxEnvMap pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;
    float4 envmap = EnvironmentMap.Sample(EnvMapSampler, pin.EnvCoord) * color.a;

    color.rgb = lerp(color.rgb, envmap.rgb, pin.Specular.rgb);

    return color;
}


// Pixel shader: specular.
float4 PSEnvMapSpecular(PSInputTxEnvMap pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;
    float4 envmap = EnvironmentMap.Sample(EnvMapSampler, pin.EnvCoord) * color.a;

    color.rgb = lerp(color.rgb, envmap.rgb, pin.Specular.rgb);
    color.rgb += EnvironmentMapSpecular * envmap.a;

    ApplyFog(color, pin.Specular.w);

    return color;
}


// Pixel shader: specular, no fog.
float4 PSEnvMapSpecularNoFog(PSInputTxEnvMap pin) : SV_Target0
{
    float4 color = Texture.Sample(Sampler, pin.TexCoord) * pin.Diffuse;
    float4 envmap = EnvironmentMap.Sample(EnvMapSampler, pin.EnvCoord) * color.a;

    color.rgb = lerp(color.rgb, envmap.rgb, pin.Specular.rgb);
    color.rgb += EnvironmentMapSpecular * envmap.a;

    return color;
}
