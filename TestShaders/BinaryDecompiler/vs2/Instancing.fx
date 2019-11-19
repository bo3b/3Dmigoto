//--------------------------------------------------------------------------------------
// File: Instancing.fx
//
// The effect file for the Instancing sample.  
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
texture g_txScene : TEXTURE;		// texture for scene rendering
float4x4 g_mWorld : WORLD : register(c0); // World matrix for object
float4x4 g_mView : VIEW : register(c4);			// World matrix for object
float4x4 g_mProj : PROJECTION : register(c8);		// World matrix for object

// only used for Constants instancing
float4 g_BoxInstance_Position : BOXINSTANCE_POSITION : register(c13);
float4 g_BoxInstance_Color : BOXINSTANCE_COLOR : register(c14);

// only used for vs_2_0 Shader instancing
#define g_nNumBatchInstance 120
float4 g_vBoxInstance_Position[g_nNumBatchInstance] : BOXINSTANCEARRAY_POSITION : register(c16);
float4 g_vBoxInstance_Color[g_nNumBatchInstance] : BOXINSTANCEARRAY_COLOR : register(c136);

//-----------------------------------------------------------------------------
// Texture samplers
//-----------------------------------------------------------------------------
sampler g_samScene =
sampler_state
{
    Texture = <g_txScene>;
    MinFilter = Linear;
    MagFilter = Linear;
    MipFilter = Linear;
};

//-----------------------------------------------------------------------------
// Name: VS_HWInstancing
// Type: Vertex shader (HW Instancing)
// Desc: This shader computes standard transform and lighting for unlit, texture-mapped triangles.
//-----------------------------------------------------------------------------
void VS_HWInstancing( float4 vPos : POSITION,
					float3 vNormal : NORMAL,
					float2 vTex0 : TEXCOORD0,
					float4 vColor : COLOR0,
					float4 vBoxInstance : COLOR1,
					out float4 oPos : POSITION,
					out float4 oColor : COLOR0,
					out float2 oTex0 : TEXCOORD0 )
{
	//Use the fourth component of the vBoxInstance to rotate the box:
	vBoxInstance.w *= 2 * 3.1415;
	float4 vRotatedPos = vPos;
	vRotatedPos.x = vPos.x * cos(vBoxInstance.w) + vPos.z * sin(vBoxInstance.w);
	vRotatedPos.z = vPos.z * cos(vBoxInstance.w) - vPos.x * sin(vBoxInstance.w);
	
	//Use the instance position to offset the incoming box corner position:
	//  The "* 32 - 16" is to scale the incoming 0-1 intrapos range so that it maps to 8 box widths, covering
	//  the signed range -8 to 8. Boxes are 2 word units wide.
	vRotatedPos += float4( vBoxInstance.xyz * 32 - 16, 0 );
	
	// Transform the position from object space to homogeneous projection space
	oPos = mul( vRotatedPos, g_mWorld );
	oPos = mul( oPos, g_mView );
	oPos = mul( oPos, g_mProj );
	
	// Just copy the texture coordinate & color through
	oTex0 = vTex0;
	oColor = vColor;
}


//-----------------------------------------------------------------------------
// Name: VS_ShaderInstancing
// Type: Vertex shader (Shader Instancing)
// Desc: This shader computes standard transform and lighting for unlit, texture-mapped triangles.
//-----------------------------------------------------------------------------
void VS_ShaderInstancing( float4 vPos : POSITION,
						float3 vNormal : NORMAL,
						float2 vTex0 : TEXCOORD0,
						float vBoxInstanceIndex : TEXCOORD1,
						out float4 oPos : POSITION,
						out float4 oColor : COLOR0,
						out float2 oTex0 : TEXCOORD0 )
{
	// Use the fourth component of the vBoxInstance to rotate the box:
	float4 vBoxInstance = g_vBoxInstance_Position[vBoxInstanceIndex];
	vBoxInstance.w *= 2 * 3.1415;
	float4 vRotatedPos = vPos;
	vRotatedPos.x = vPos.x * cos(vBoxInstance.w) + vPos.z * sin(vBoxInstance.w);
	vRotatedPos.z = vPos.z * cos(vBoxInstance.w) - vPos.x * sin(vBoxInstance.w);
	
	//Use the instance position to offset the incoming box corner position
	//  The "* 32 - 16" is to scale the incoming 0-1 intrapos range so that it maps to 8 box widths, covering
	//  the signed range -8 to 8. Boxes are 2 word units wide.
	vRotatedPos += float4( vBoxInstance.xyz * 32 - 16, 0 );
	
	// Transform the position from object space to homogeneous projection space
	oPos = mul( vRotatedPos, g_mWorld );
	oPos = mul( oPos, g_mView );
	oPos = mul( oPos, g_mProj );
	
	// Just copy the texture coordinate & color through
	oTex0 = vTex0;
	oColor = g_vBoxInstance_Color[vBoxInstanceIndex];
}


//-----------------------------------------------------------------------------
// Name: VS_ConstantsInstancing
// Type: Vertex shader (Constants Instancing)
// Desc: This shader computes standard transform and lighting for unlit, texture-mapped triangles.
//-----------------------------------------------------------------------------
void VS_ConstantsInstancing( float4 vPos : POSITION,
							float3 vNormal : NORMAL,
							float2 vTex0 : TEXCOORD0,
							out float4 oPos : POSITION,
							out float4 oColor : COLOR0,
							out float2 oTex0 : TEXCOORD0 )
{
	// Use the fourth component of the vBoxInstance to rotate the box:
	float4 vBoxInstance = g_BoxInstance_Position;
	vBoxInstance.w *= 2 * 3.1415;
	float4 vRotatedPos = vPos;
	vRotatedPos.x = vPos.x * cos(vBoxInstance.w) + vPos.z * sin(vBoxInstance.w);
	vRotatedPos.z = vPos.z * cos(vBoxInstance.w) - vPos.x * sin(vBoxInstance.w);
	
	// Use the instance position to offset the incoming box corner position
	//  The "* 32 - 16" is to scale the incoming 0-1 intrapos range so that it maps to 8 box widths, covering
	//  the signed range -8 to 8. Boxes are 2 word units wide.
	vRotatedPos += float4( vBoxInstance.xyz * 32 - 16, 0 );
	
	// Transform the position from object space to homogeneous projection space
	oPos = mul( vRotatedPos, g_mWorld );
	oPos = mul( oPos, g_mView );
	oPos = mul( oPos, g_mProj );
	
	// Just copy the texture coordinate & color through
    oTex0 = vTex0;
    oColor = g_BoxInstance_Color;
}


//-----------------------------------------------------------------------------
// Name: PS
// Type: Pixel shader
// Desc: This shader outputs the pixel's color by modulating the texture's
//		 color with diffuse material color
//-----------------------------------------------------------------------------
float4 PS(	float2 vTex0 : TEXCOORD0,
			float4 vColor : COLOR0 ) : COLOR0
{
    // Lookup texture and modulate it with diffuse
    return tex2D( g_samScene, vTex0 ) * vColor;
}


//-----------------------------------------------------------------------------
// Name: THW_Instancing
// Type: Technique
// Desc: Renders scene through Hardware instancing
//-----------------------------------------------------------------------------
technique THW_Instancing
{
    pass P0
    {
        VertexShader = compile vs_2_0 VS_HWInstancing();
        PixelShader  = compile ps_2_0 PS();
        ZEnable = true;
        AlphaBlendEnable = false;
    }
}


//-----------------------------------------------------------------------------
// Name: TShader_Instancing
// Type: Technique
// Desc: Renders scene through Shader instancing (batching)
//-----------------------------------------------------------------------------
technique TShader_Instancing
{
    pass P0
    {
        VertexShader = compile vs_2_0 VS_ShaderInstancing();
        PixelShader  = compile ps_2_0 PS();
        ZEnable = true;
        AlphaBlendEnable = false;
    }
}


//-----------------------------------------------------------------------------
// Name: TConstants_Instancing
// Type: Technique
// Desc: Renders scene through Constants instancing (no batching)
//-----------------------------------------------------------------------------
technique TConstants_Instancing
{
    pass P0
    {
        VertexShader = compile vs_2_0 VS_ConstantsInstancing();
        PixelShader  = compile ps_2_0 PS();
        ZEnable = true;
        AlphaBlendEnable = false;
    }
}
