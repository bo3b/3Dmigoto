
struct PS_INPUT
{
    uint PrimID : SV_PrimitiveID;
};

float4 main( PS_INPUT input ) : SV_Target
{
    float4 colour;
    float fPrimID = input.PrimID;
    colour.r = fPrimID;
    colour.g = 2 * fPrimID;
    colour.b = (1.0/64.0) * fPrimID;
    colour.a = (1.0/32.0) * fPrimID;
    return colour;
}


