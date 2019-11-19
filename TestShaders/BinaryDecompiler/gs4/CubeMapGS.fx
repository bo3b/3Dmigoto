//--------------------------------------------------------------------------------------
// File: CubeMapGS.fx
//
// The effect file for the CubeMapGS sample.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define IOR 2.5

cbuffer cbMultiPerFrameFrame
{
	matrix mWorldViewProj : WORLDVIEWPROJECTION;
	matrix mWorldView : WORLDVIEW;
	matrix mWorld : WORLD;
	matrix mView : VIEW;
	matrix mProj : PROJECTION;
	float3 vEye;  // Eye point in world space
};

cbuffer cbPerMaterial
{
	float4 vMaterialDiff;
	float4 vMaterialSpec;
};

cbuffer cbPerCubeRender
{
	matrix g_mViewCM[6]; // View matrices for cube map rendering
};

cbuffer cbConstants
{
	float fReflectivity = 1.0f;
	float3 skyDir = { 0.0,1.0,0.0 };
	float R0Constant = ((1.0- (1.0/IOR) )*(1.0- (1.0/IOR) ))/((1.0+ (1.0/IOR) )*(1.0+ (1.0/IOR) ));
	float R0Inv = 1.0 - ((1.0- (1.0/IOR) )*(1.0- (1.0/IOR) ))/((1.0+ (1.0/IOR) )*(1.0+ (1.0/IOR) ));
	float4 vFrontColor = { 0.3, 0.1, 0.6, 1.0 };
	float4 vBackColor = { 0.0, 0.3, 0.3, 1.0 };
	float4 vHighlight1 = { 0.9, 0.8, 0.9, 1.0 };
	float4 vHighlight2 = { 1.0, 1.0, 0.6, 1.0 };
	float lightMul = 3.0;
	float4 vOne = { 1,1,1,1 };
};

Texture2D g_txDiffuse;
Texture2D g_txFalloff;  // falloff texture for diffuse color shifting
TextureCube g_txEnvMap;
Texture2D g_txVisual;

SamplerState g_samLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerState g_samPoint
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState g_samCube
{
    Filter = ANISOTROPIC;
    AddressU = Clamp;
    AddressV = Clamp;
};


RasterizerState RasNoCull
{
    CullMode = None;
};


BlendState NoBlend
{
    BlendEnable[0] = FALSE;
};

BlendState GlassBlendState
{
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = TRUE;
    SrcBlend = SRC_ALPHA;
    DestBlend = INV_SRC_ALPHA;
    BlendOp = ADD;
    SrcBlendAlpha = ZERO;
    DestBlendAlpha = ZERO;
    BlendOpAlpha = ADD;
    RenderTargetWriteMask[0] = 0x0F;
};


//--------------------------------------------------------------------------------------
// Cubemap via Geometry Shader
//--------------------------------------------------------------------------------------
struct VS_CUBEMAP_IN
{
	float4 Pos		: POSITION;
	float3 Normal	: NORMAL;
	float2 Tex		: TEXCOORD0;
};

struct GS_CUBEMAP_IN
{
	float4 Pos		: SV_POSITION;    // World position
    float2 Tex		: TEXCOORD0;         // Texture coord
};


struct PS_CUBEMAP_IN
{
    float4 Pos : SV_POSITION;     // Projection coord
    float2 Tex : TEXCOORD0;       // Texture coord
    uint RTIndex : SV_RenderTargetArrayIndex;
};

GS_CUBEMAP_IN VS_CubeMap( VS_CUBEMAP_IN input )
{
    GS_CUBEMAP_IN output = (GS_CUBEMAP_IN)0.0f;

    // Compute world position
    output.Pos = mul( input.Pos, mWorld );

    // Propagate tex coord
    output.Tex = input.Tex;

    return output;
}

[maxvertexcount(18)]
void GS_CubeMap( triangle GS_CUBEMAP_IN input[3], inout TriangleStream<PS_CUBEMAP_IN> CubeMapStream )
{
    for( int f = 0; f < 6; ++f )
    {
        // Compute screen coordinates
        PS_CUBEMAP_IN output;
        output.RTIndex = f;
        for( int v = 0; v < 3; v++ )
        {
            output.Pos = mul( input[v].Pos, g_mViewCM[f] );
            output.Pos = mul( output.Pos, mProj );
            output.Tex = input[v].Tex;
            CubeMapStream.Append( output );
        }
        CubeMapStream.RestartStrip();
    }
}


float4 PS_CubeMap( PS_CUBEMAP_IN input ) : SV_Target
{
    return g_txDiffuse.Sample( g_samLinear, input.Tex );
}


//--------------------------------------------------------------------------------------
// Cubemap via Instancing
//--------------------------------------------------------------------------------------
struct VS_INSTCUBEMAP_IN
{
	float4 Pos		: POSITION;
	float3 Normal	: NORMAL;
	float2 Tex		: TEXCOORD0;
	uint   CubeSize : SV_InstanceID;
};

struct GS_INSTCUBEMAP_IN
{
    float4 Pos : SV_POSITION;     // Projection coord
    float2 Tex : VTX_TEXCOORD0;       // Texture coord
    uint RTIndex : RTARRAYINDEX;
};

GS_INSTCUBEMAP_IN VS_CubeMap_Inst( VS_INSTCUBEMAP_IN input )
{
    GS_INSTCUBEMAP_IN output = (GS_INSTCUBEMAP_IN)0.0f;

    // Compute world position
    output.Pos = mul( input.Pos, mWorld );
    output.Pos = mul( output.Pos, g_mViewCM[ input.CubeSize ] );
    output.Pos = mul( output.Pos, mProj );
    output.RTIndex = input.CubeSize;

    // Propagate tex coord
    output.Tex = input.Tex;

    return output;
}

[maxvertexcount(3)]
void GS_CubeMap_Inst( triangle GS_INSTCUBEMAP_IN input[3], inout TriangleStream<PS_CUBEMAP_IN> CubeMapStream )
{
	PS_CUBEMAP_IN output;
    for( int v = 0; v < 3; v++ )
    {
        output.Pos = input[v].Pos;
        output.Tex = input[v].Tex;
        output.RTIndex = input[v].RTIndex;
        CubeMapStream.Append( output );
    }
}

//--------------------------------------------------------------------------------------
// Scene rendering
//--------------------------------------------------------------------------------------


struct VS_OUTPUT_SCENE
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};


VS_OUTPUT_SCENE VS_Scene( float4 Pos : POSITION, float3 Normal : NORMAL, float2 Tex : TEXCOORD )
{
    VS_OUTPUT_SCENE o = (VS_OUTPUT_SCENE)0.0f;

    // Output position
    o.Pos = mul( Pos, mWorldViewProj );

    // Propagate tex coord
    o.Tex = Tex;

    return o;
}


float4 PS_Scene( VS_OUTPUT_SCENE vin ) : SV_Target
{
    return g_txDiffuse.Sample( g_samLinear, vin.Tex );
}


//--------------------------------------------------------------------------------------
// Environment-mapped scene
//--------------------------------------------------------------------------------------

struct VS_OUTPUT_SCENEENV
{
    float4 Pos : SV_POSITION;
    float2 Bary : TEXCOORD0;	// Barycentric interpolants
    float4 wPos : TEXCOORD1; // World space position
    float4 wvPos : TEXCOORD2; // WorldView space position
    float3 wN : TEXCOORD3;       // World space normal
    
    float3 Normals[6] : SIXNORMS; // 6 normal positions
};

float4 ColorApprox( float3 incident, float3 normal )
{
    float d = saturate( dot(incident,normal)-0.01 );
    float Ramp = (float)g_txFalloff.Sample( g_samPoint, float2(d,0) );
    d = d*Ramp;

    return vFrontColor*(d) + vBackColor*(1.0-d);
}

float FresnelApprox( float3 incident, float3 normal )
{
     return R0Constant + R0Inv * pow( 1.0-dot(incident,normal),5.0 );
}

VS_OUTPUT_SCENEENV VS_EnvMappedScene( float4 Pos : POSITION, float3 Normal : NORMAL, float2 Tex : TEXCOORD )
{
    VS_OUTPUT_SCENEENV o = (VS_OUTPUT_SCENEENV)0.0f;

    // Output position
    o.Pos = mul( Pos, mWorldViewProj );

    // Compute world space position
    o.wPos = mul( Pos, mWorld );
    
    // Compute worldview space position
    o.wvPos = mul( Pos, mWorldView );
    
    // Propogate the normal
    o.wN = Normal;

    return o;
}

[maxvertexcount(3)]
void GS_SetupNormalInterp( triangle VS_OUTPUT_SCENEENV In[3], inout TriangleStream<VS_OUTPUT_SCENEENV> SceneEnvStream )
{	
	In[0].Bary = float2(0,0);
	In[1].Bary = float2(1,0);
	In[2].Bary = float2(0,1);
	
	float3 nNorm1 = normalize( In[0].wN + In[1].wN );
	float3 nNorm3 = normalize( In[2].wN + In[0].wN );
	float3 nNorm5 = normalize( In[1].wN + In[2].wN );

	In[0].Normals[0] = In[0].wN;
	In[0].Normals[1] = nNorm1;
	In[0].Normals[2] = In[1].wN;
	In[0].Normals[3] = nNorm3;
	In[0].Normals[4] = In[2].wN;
	In[0].Normals[5] = nNorm5;
	SceneEnvStream.Append( In[0] );
	
	In[1].Normals[0] = In[0].wN;
	In[1].Normals[1] = nNorm1;
	In[1].Normals[2] = In[1].wN;
	In[1].Normals[3] = nNorm3;
	In[1].Normals[4] = In[2].wN;
	In[1].Normals[5] = nNorm5;
	SceneEnvStream.Append( In[1] );
	
	In[2].Normals[0] = In[0].wN;
	In[2].Normals[1] = nNorm1;
	In[2].Normals[2] = In[1].wN;
	In[2].Normals[3] = nNorm3;
	In[2].Normals[4] = In[2].wN;
	In[2].Normals[5] = nNorm5;
	SceneEnvStream.Append( In[2] );

	SceneEnvStream.RestartStrip();
}

float3 HighOrderInterpolate( float3 normals[6], float x, float y )
{
	float p0 = 2*x*x + 2*y*y + 4*x*y - 3*x - 3*y + 1;
	float p1 = -4*x*x - 4*x*y + 4*x;
	float p2 = 2*x*x - x;
	float p3 = -4*y*y - 4*x*y + 4*y;
	float p4 = 2*y*y - y;
	float p5 = 4*x*y;
	
	return p0*normals[0] + p1*normals[1] + p2*normals[2] + p3*normals[3] + p4*normals[4] + p5*normals[5];
}

float4 PS_EnvMappedScene( VS_OUTPUT_SCENEENV vin ) : SV_Target
{
    float3 wN = HighOrderInterpolate( vin.Normals, vin.Bary.x, vin.Bary.y );
    float3 wvN = ( mul( wN, (float3x3)mWorldView ) );
    wN = ( mul( wN, (float3x3)mWorld ) );
    
    
    float3 I = vin.wPos.xyz - vEye;
    float3 wR = I - 2.0f * dot( I, wN ) * wN;
    float4 CubeSample = lightMul*g_txEnvMap.Sample( g_samCube, wR ); 
    float4 Diff = ColorApprox( float3(0,0,-1), wvN );
    float4 Shellac = FresnelApprox( float3(0,0,-1), wvN );
    
     // Compute Specular for the Diffuse and Shellac layers of paint in view space
    float3 L = skyDir;
    float3 wvSHV = normalize(2 * dot(wvN, L) * wvN - L);
    float3 V = -normalize( vin.wvPos );

    float4 SpecDiff = pow(max(0, dot(wvSHV, V)), 32)*vHighlight1;   // specular for base paint
    float4 SpecShellac = pow(max(0, dot(wvSHV, V)), 64)*vHighlight2;   // specular for shellac layer
    
    //combine them all
    float4 DiffColor = dot(wN, skyDir)*Diff + 1.25*SpecDiff;
    float4 ShellacColor = Shellac*(CubeSample) + 1.60*SpecShellac;
    return DiffColor + fReflectivity*ShellacColor;
}

float4 PS_EnvMappedScene_NoTexture( VS_OUTPUT_SCENEENV vin ) : SV_Target
{
    float3 wN = HighOrderInterpolate( vin.Normals, vin.Bary.x, vin.Bary.y );
    wN = mul( wN, (float3x3)mWorld );
    
    float fLight = saturate( dot( skyDir, wN ) ) + 0.2f;
    
    float3 I = vin.wPos.xyz - vEye;
    float3 wR = I - 2.0f * dot( I, wN ) * wN;
    float4 CubeSample = g_txEnvMap.Sample( g_samCube, wR );
   
    //combine them all
    float4 outCol = 0.3*vMaterialDiff*fLight + 1.5 * vMaterialSpec*CubeSample;
    outCol.a = vMaterialDiff.a;	//preserve alpha
    return outCol;
}


//--------------------------------------------------------------------------------------
// Visualization shaders
//--------------------------------------------------------------------------------------


VS_OUTPUT_SCENE VS_Visualize( float3 Pos : POSITION, float3 Normal : NORMAL, float2 Tex : TEXCOORD )
{
    VS_OUTPUT_SCENE o;

    o.Pos = float4( Pos, 1.0f );
    o.Tex = Tex;

    return o;
}


float4 PS_Visualize( VS_OUTPUT_SCENE vin ) : SV_Target
{
    return g_txVisual.Sample( g_samPoint, vin.Tex );
}


//--------------------------------------------------------------------------------------
// Techniques
//--------------------------------------------------------------------------------------


technique10 RenderCubeMap
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, VS_CubeMap() ) );
        SetGeometryShader( CompileShader( gs_4_0, GS_CubeMap() ) );
        SetPixelShader( CompileShader( ps_4_0, PS_CubeMap() ) );
        
        SetBlendState( NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
};

technique10 RenderCubeMap_Inst
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, VS_CubeMap_Inst() ) );
        SetGeometryShader( CompileShader( gs_4_0, GS_CubeMap_Inst() ) );
        SetPixelShader( CompileShader( ps_4_0, PS_CubeMap() ) );
        
        SetBlendState( NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
};


technique10 RenderScene
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, VS_Scene() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_Scene() ) );
        
        SetBlendState( NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
};


technique10 RenderEnvMappedScene
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, VS_EnvMappedScene() ) );
        SetGeometryShader( CompileShader( gs_4_0, GS_SetupNormalInterp() ) );
        SetPixelShader( CompileShader( ps_4_0, PS_EnvMappedScene() ) );
        
        SetBlendState( NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
};

technique10 RenderEnvMappedScene_NoTexture
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, VS_EnvMappedScene() ) );
        SetGeometryShader( CompileShader( gs_4_0, GS_SetupNormalInterp() ) );
        SetPixelShader( CompileShader( ps_4_0, PS_EnvMappedScene_NoTexture() ) );
        
        SetBlendState( NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
};

technique10 RenderEnvMappedGlass
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, VS_EnvMappedScene() ) );
        SetGeometryShader( CompileShader( gs_4_0, GS_SetupNormalInterp() ) );
        SetPixelShader( CompileShader( ps_4_0, PS_EnvMappedScene_NoTexture() ) );
        
        SetBlendState( GlassBlendState, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
};


technique10 VisualizeCubeMap
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, VS_Visualize() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_Visualize() ) );
        
        SetBlendState( NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
};
