
struct VS_OUTPUT
{
    float4 Position   : SV_Position;
};

int LOOP_COUNT;

VS_OUTPUT main( in float4 vPosition : POSITION )
{
    VS_OUTPUT Output;

    Output.Position = vPosition;

	for(int i=0; i<LOOP_COUNT; i++)
	{
		Output.Position = Output.Position * float4(2, 1, 0.5, 1);
	}
    
    return Output;
}


