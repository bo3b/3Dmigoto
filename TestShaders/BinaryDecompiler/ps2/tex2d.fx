
texture g_txScene : TEXTURE;		// texture for scene rendering

sampler g_samScene =
sampler_state
{
    Texture = <g_txScene>;
    MinFilter = Linear;
    MagFilter = Linear;
    MipFilter = Linear;
};

float4 ColOffset;

float4 main(	float2 vTex0 : TEXCOORD0,
			float4 vColor : COLOR0 ) : COLOR0
{
    // Lookup texture and modulate it with diffuse
    return tex2D( g_samScene, vTex0 ) * vColor + ColOffset;
}
