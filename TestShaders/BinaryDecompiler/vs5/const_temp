
struct VS_OUTPUT
{
    float4 Position   : SV_Position;
};

row_major float4x4 mWorldViewProj;

VS_OUTPUT main( in float4 vPosition : POSITION, in float4 vTexCoord : TEXCOORD0 )
{
    VS_OUTPUT Output;

    Output.Position = mul(vPosition, mWorldViewProj) + vTexCoord;

    return Output;
}


