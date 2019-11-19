
//Compile with /Od to use b0 constant

struct VS_OUTPUT
{
    float4 Position   : SV_Position;
};

bool LOOP_COUNT;

VS_OUTPUT main( in float4 vPosition : POSITION )
{
    VS_OUTPUT Output;

    Output.Position = vPosition;

	if(LOOP_COUNT)
	{
		Output.Position = Output.Position * float4(2, 1, 0.5, 1);
	}
	else
	{
		Output.Position = Output.Position * float4(2, 1, 1, 1);
	}
    
    return Output;
}


