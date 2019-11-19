
interface iChangeColour
{
    float3 ChangeColour(float3 colour);
};

class cDoubleColour : iChangeColour
{
    float3 ChangeColour(float3 colour) {
        return colour * float3(2, 2, 2);
    }
};

class cHalfColour : iChangeColour
{
    float3 ChangeColour(float3 colour) {
       return colour / float3(2, 2, 2);
    }
};


class cUnchangedColour : iChangeColour
{
    float3 ChangeColour(float3 colour) {
       return colour;
    }
};

interface iAlpha
{
    float ChooseAlpha();
};

class OneAlpha : iAlpha
{
    float ChooseAlpha() {
        return 1.0;
    }
};

class TwoThirdsAlpha : iAlpha
{
    float ChooseAlpha() {
        return 0.66;
    }
};

struct PS_INPUT
{
    float3 Colour : COLOR0;
};

iChangeColour gAbstractColourChanger;
iChangeColour gAbstractColourChangerB;
iAlpha gAlphaChooser;

float4 main( PS_INPUT Input ) : SV_TARGET
{
    float3 ModifiedColour = gAbstractColourChanger.ChangeColour(Input.Colour);
    ModifiedColour = gAbstractColourChangerB.ChangeColour(ModifiedColour);

    return float4(ModifiedColour,gAlphaChooser.ChooseAlpha()); 
}

