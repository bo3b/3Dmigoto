
struct VS_OUTPUT
{
    float4 Position   : SV_Position;
    float4 Albedo : COLOR0;
};

cbuffer transformsA
{       
    float4x4 modelview;
    int unusedTestA;
}
cbuffer transformsB
{
    int unusedTestB;
    float4x4 projection;
}

//default/global cbuffer
float4 diffuse;

VS_OUTPUT main( in float4 vPosition : POSITION )
{
    VS_OUTPUT Output;
    
    float4x4 mvp = modelview * projection;

    Output.Position = mul(vPosition, mvp);
    Output.Albedo = diffuse;
    
    return Output;
}


