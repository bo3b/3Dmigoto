
struct PS_INPUT
{
    float4 colour : COLOR;
    uint sampleIdx : SV_SampleIndex;
    bool coverageIn : SV_Coverage;
};

struct PS_OUTPUT
{
   float4 colour : SV_Target0;
   bool coverage : SV_Coverage;
};

PS_OUTPUT main( PS_INPUT input )
{
   PS_OUTPUT outp;


    outp.colour = input.colour;
    outp.coverage = 1<<input.sampleIdx & input.coverageIn;
    return outp;
}


