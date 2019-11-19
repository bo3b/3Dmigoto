
struct PS_INPUT
{
    float4 colour : COLOR0;
    float depth : DEPTH;
};

struct PS_OUTPUT_GE
{
    float colour : SV_Target;
    float depth : SV_DepthGreaterEqual;
};

struct PS_OUTPUT_LE
{
    float colour : SV_Target;
    float depth : SV_DepthLessEqual;
};


PS_OUTPUT_GE DepthGreaterThan( PS_INPUT Input )
{
    PS_OUTPUT_GE Output;

    Output.colour = Input.colour;
    Output.depth = Input.depth;
    
    return Output;
}

PS_OUTPUT_LE DepthLessThan( PS_INPUT Input )
{
    PS_OUTPUT_LE Output;

    Output.colour = Input.colour;
    Output.depth = Input.depth;
    
    return Output;
}


