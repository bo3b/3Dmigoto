
struct VS_OUTPUT
{
    float4 Position   : SV_Position;
    float4 Colour : COLOR0;
};

int SwitchValue;

VS_OUTPUT main( in float4 vPosition : POSITION )
{
    VS_OUTPUT Output;
    

    Output.Position = vPosition;
    
    switch(SwitchValue)
    {
        case 0:
        {
            Output.Colour = float4(1, 0, 0, 1);
            break;
        }
        case 1:
        {
            Output.Colour = float4(1, 1, 0, 1);
            break;
        }
        default:
        {
            Output.Colour = float4(1, 1, 1, 1);
            break;
        }
    }
    
    return Output;
}


