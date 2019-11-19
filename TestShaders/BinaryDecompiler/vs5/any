
struct VS_OUTPUT
{
    float4 Position   : SV_Position;
};

VS_OUTPUT main( in float4 vPosition : POSITION )
{
    VS_OUTPUT Output;
    
    if(any(vPosition))
    {
        vPosition.z = 2.0;
    }

    Output.Position = vPosition;
    
    return Output;
}


