struct PS_INPUT
{
    linear float4 Pos : SV_POSITION;
    linear centroid float4 LinCent : TEXCOORD0;
    sample float4 Smp : TEXCOORD1;
    nointerpolation float4 NoInterp : TEXCOORD2;
    noperspective float4 NoPersp : TEXCOORD3;
    noperspective centroid float4 NoPerspCent : TEXCOORD4;
};

struct PS_OUTPUT
{
   float4 C0 : SV_Target;
   float4 C1 : SV_Target1;
   float4 C2 : SV_Target2;
   float4 C3 : SV_Target3;
   float4 C4 : SV_Target4;
};

PS_OUTPUT main( PS_INPUT input)
{
    PS_OUTPUT result;

    result.C0 = input.LinCent;
    result.C1 = input.Smp;
    result.C2 = input.NoInterp;
    result.C3 = input.NoPersp;
    result.C4 = input.NoPerspCent;

    return result;
}

