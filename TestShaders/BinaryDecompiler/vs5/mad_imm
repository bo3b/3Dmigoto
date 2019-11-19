
struct VS_OUTPUT
{
    float4 Position   : SV_Position;
};

VS_OUTPUT main( in float4 vPosition : POSITION, in float4 vTexCoord : TEXCOORD0 )
{
    VS_OUTPUT Output;
    float4 Offset;
    
    Offset.x = 0.1;
    Offset.y = 0.2;
    Offset.z = 0.3;
    Offset.w = 0.4;

    Output.Position = vPosition + Offset * vTexCoord;
    
    return Output;
}


