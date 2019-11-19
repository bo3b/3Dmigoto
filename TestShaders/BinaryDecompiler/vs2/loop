
//Compile with /Od to use loop opcode.

struct SVSInput {
float4 vPos : POSITION;
float3 vNormal : NORMAL;
float2 vUV : TEXCOORD0;

float4 vIndices    : BLENDINDICES;
float4 vWeight    : BLENDWEIGHT;
};

struct SPSInput {
float4 vPos : POSITION;
float3 vNormal : TEXCOORD0;
float2 vUV : TEXCOORD2;
};

#define SVSOutput SPSInput

/////////////////////////////////////////////////////////////

float4x4 g_mW2C;

/////////////////////////////////////////////////////////////

#define JOINT_MTX_NUM (48)
float4x4 vJointMatrix[JOINT_MTX_NUM];

SVSOutput main(SVSInput In)
{
SVSOutput Out = (SVSOutput)0;

for (int i = 0; i < 4; i++) {
    float idx = In.vIndices[i];
    float weight = In.vWeight[i];

    float4x4 mtx = vJointMatrix[idx];

    Out.vPos += mul(In.vPos, mtx) * weight;
    Out.vNormal += mul(In.vNormal, (float3x3)mtx) * weight;
}

Out.vPos = mul(Out.vPos, g_mW2C);

Out.vUV = In.vUV;

Out.vUV.y = 1.0f - Out.vUV.y;

return Out;
}
