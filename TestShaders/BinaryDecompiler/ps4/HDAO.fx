//--------------------------------------------------------------------------------------
// File: HDAO10.1.fx
//
// These shaders demonstrate the use of the DX10.1 Gather instruction to accelerate the
// HDAO technique 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------


cbuffer cb0
{
    float3 g_f3LightDir;                // Light's direction in world space
    float3 g_f3EyePt;                   // Camera position
    float g_fTime;						// App's time in seconds
    float4x4 g_f4x4World;               // World matrix for object
    float4x4 g_f4x4View;                // View matrix
    float4x4 g_f4x4WorldViewProjection;	// World * View * Projection matrix
    float4x4 g_f4x4InvProjection;       // Used by HDAO shaders to convert depth to camera space
    float2 g_f2RTSize;                  // Used by HDAO shaders for scaling texture coords
    float g_fHDAORejectRadius;          // HDAO param
    float g_fHDAOIntensity;             // HDAO param
    float g_fHDAOAcceptRadius;          // HDAO param
    float g_fZFar;
    float g_fZNear;
    float g_fQ;                         // far / (far - near)
    float g_fQTimesZNear;               // Q * near
    float g_fNormalScale;               // Normal scale
    float g_fAcceptAngle;               // Accept angle
    float4 g_f4MaterialDiffuse;
    float4 g_f4MaterialSpecular;
    float g_fTanH;
    float g_fTanV;
}

// Textures
Texture2D g_txScene;
Texture2D g_txNormals;
Texture2D g_txNormalsZ;
Texture2D g_txNormalsXY;
Texture2D g_txHDAO;
Texture2D g_txDepth;
Texture2D g_txDiffuse;
Texture2D g_txNormal;


//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------


SamplerState g_SamplePoint
{
    Filter                      = MIN_MAG_MIP_POINT;
    AddressU                    = CLAMP;
    AddressV                    = CLAMP;
};


SamplerState g_SampleLinear
{
    Filter                      = MIN_MAG_MIP_LINEAR;
    AddressU                    = WRAP;
    AddressV                    = WRAP;
};


//--------------------------------------------------------------------------------------
// Vertex & Pixel shader structures
//--------------------------------------------------------------------------------------


struct VS_RenderSceneInput
{
    float3 f3Position  : POSITION;   
    float3 f3Normal    : NORMAL;     
    float2 f2TexCoord  : TEXTURE0;
};

struct PS_RenderSceneInput
{
    float4 f4Position   : SV_Position;
    float4 f4Normal     : NORMAL; 
    float4 f4Diffuse    : COLOR0; 
    float2 f2TexCoord   : TEXTURE0;  
};

struct VS_HQRenderSceneInput
{
	float3 f3Position   : POSITION;  
    float3 f3Normal     : NORMAL;     
    float2 f2TexCoord   : TEXTURE0;
    float3 f3Tangent    : TANGENT;    
};

struct PS_HQRenderSceneInput
{
    float4 f4Position	: SV_Position;
    float3 f3Normal     : NORMAL; 
    float2 f2TexCoord   : TEXTURE0;
    float3 f3Tangent    : TEXCOORD1;
    float3 f3WorldPos   : TEXCOORD2;  
};

struct VS_RenderQuadInput
{
    float3 f3Position : POSITION; 
    float2 f2TexCoord : TEXTURE0; 
};

struct PS_RenderQuadInput
{
    float4 f4Position : SV_Position; 
    float2 f2TexCoord : TEXTURE0;
};

struct PS_RenderOutput_10_0
{
    float4 f4Color	: SV_Target0;
    float4 f4Normal	: SV_Target1;
};

struct PS_RenderOutput_10_1
{
    float4 f4Color      : SV_Target0;
    float  fNormalZ     : SV_Target1;
    float2 f2NormalXY	: SV_Target2;
};


//--------------------------------------------------------------------------------------
// Gather pattern
//--------------------------------------------------------------------------------------

// Gather defines
#define RING_1    (1)
#define RING_2    (2)
#define RING_3    (3)
#define RING_4    (4)
#define NUM_RING_1_GATHERS    (2)
#define NUM_RING_2_GATHERS    (6)
#define NUM_RING_3_GATHERS    (12)
#define NUM_RING_4_GATHERS    (20)

// Ring sample pattern
static const float2 g_f2HDAORingPattern[NUM_RING_4_GATHERS] = 
{
    // Ring 1
    { 1, -1 },
    { 0, 1 },
    
    // Ring 2
    { 0, 3 },
    { 2, 1 },
    { 3, -1 },
    { 1, -3 },
        
    // Ring 3
    { 1, -5 },
    { 3, -3 },
    { 5, -1 },
    { 4, 1 },
    { 2, 3 },
    { 0, 5 },
    
    // Ring 4
    { 0, 7 },
    { 2, 5 },
    { 4, 3 },
    { 6, 1 },
    { 7, -1 },
    { 5, -3 },
    { 3, -5 },
    { 1, -7 },
};

// Ring weights
static const float4 g_f4HDAORingWeight[NUM_RING_4_GATHERS] = 
{
    // Ring 1 (Sum = 5.30864)
    { 1.00000, 0.50000, 0.44721, 0.70711 },
    { 0.50000, 0.44721, 0.70711, 1.00000 },
    
    // Ring 2 (Sum = 6.08746)
    { 0.30000, 0.29104, 0.37947, 0.40000 },
    { 0.42426, 0.33282, 0.37947, 0.53666 },
    { 0.40000, 0.30000, 0.29104, 0.37947 },
    { 0.53666, 0.42426, 0.33282, 0.37947 },
    
    // Ring 3 (Sum = 6.53067)
    { 0.31530, 0.29069, 0.24140, 0.25495 },
    { 0.36056, 0.29069, 0.26000, 0.30641 },
    { 0.26000, 0.21667, 0.21372, 0.25495 },
    { 0.29069, 0.24140, 0.25495, 0.31530 },
    { 0.29069, 0.26000, 0.30641, 0.36056 },
    { 0.21667, 0.21372, 0.25495, 0.26000 },
    
    // Ring 4 (Sum = 7.00962)
    { 0.17500, 0.17365, 0.19799, 0.20000 },
    { 0.22136, 0.20870, 0.24010, 0.25997 },
    { 0.24749, 0.21864, 0.24010, 0.28000 },
    { 0.22136, 0.19230, 0.19799, 0.23016 },
    { 0.20000, 0.17500, 0.17365, 0.19799 },
    { 0.25997, 0.22136, 0.20870, 0.24010 },
    { 0.28000, 0.24749, 0.21864, 0.24010 },
    { 0.23016, 0.22136, 0.19230, 0.19799 },
};

static const float g_fRingWeightsTotal[RING_4] =
{
    5.30864,
    11.39610,
    17.92677,
    24.93639,
};

#define NUM_NORMAL_LOADS (4)
static const int2 g_i2NormalLoadPattern[NUM_NORMAL_LOADS] = 
{
    { 1, 8 },
    { 8, -1 },
    { 5, 4 },
    { 4, -4 },
};


//----------------------------------------------------------------------------------------
// Helper function to Gather samples in 10.1 and 10.0 modes 
//----------------------------------------------------------------------------------------
float4 GatherSamples( Texture2D Tex, float2 f2TexCoord, uniform bool b10_1 )
{
    float4 f4Ret;
    
    if( b10_1 )
    {
#ifdef DX10_1_ENABLED
        f4Ret = Tex.Gather( g_SamplePoint, f2TexCoord );
#endif
    }
    else
    {
        f4Ret.x = Tex.SampleLevel( g_SamplePoint, f2TexCoord, 0, int2( 0, 1 ) ).x;
        f4Ret.y = Tex.SampleLevel( g_SamplePoint, f2TexCoord, 0, int2( 1, 1 ) ).x;
        f4Ret.z = Tex.SampleLevel( g_SamplePoint, f2TexCoord, 0, int2( 1, 0 ) ).x;
        f4Ret.w = Tex.SampleLevel( g_SamplePoint, f2TexCoord, 0, int2( 0, 0 ) ).x;
    }
        
    return f4Ret;
}


//--------------------------------------------------------------------------------------
// Helper function to gather Z values in 10.1 and 10.0 modes
//--------------------------------------------------------------------------------------
float4 GatherZSamples( Texture2D Tex, float2 f2TexCoord, uniform bool b10_1 )
{
    float4 f4Ret;
    float4 f4Gather;
        
    f4Gather = GatherSamples( Tex, f2TexCoord, b10_1 );
    f4Ret = -g_fQTimesZNear.xxxx / ( f4Gather - g_fQ.xxxx );
    
    return f4Ret;
}


//--------------------------------------------------------------------------------------
// Helper function to compute camera space XYZ from depth and screen coords
//--------------------------------------------------------------------------------------
float3 GetCameraXYZFromDepth( float fDepth, int2 i2ScreenCoord )
{
    float3 f3CameraPos;
    
    // Compute camera Z
    f3CameraPos.z = -g_fQTimesZNear / ( fDepth - g_fQ );
    
    // Convert screen coords to projection space XY
    f3CameraPos.xy = float2( 2.0f, 2.0f ) * ( (float2)i2ScreenCoord / g_f2RTSize ) - float2( 1.0f, 1.0f );
    
    // Compute camera X
    f3CameraPos.x = g_fTanH * f3CameraPos.x * f3CameraPos.z;
    
    // Compute camera Y
    f3CameraPos.y = -g_fTanV * f3CameraPos.y * f3CameraPos.z;
    
    return f3CameraPos;
}


//--------------------------------------------------------------------------------------
// Used as an early rejection test - based on geometry
//--------------------------------------------------------------------------------------
float GeometryRejectionTest( int2 i2ScreenCoord, uniform bool b10_1 )
{
    float3 f3N[3];
    float3 f3Pos[3];
    float3 f3Dir[2];
    float fDot;
    float fSummedDot = 0.0f;
    int2 i2MirrorPattern;
    int2 i2OffsetScreenCoord;
    int2 i2MirrorOffsetScreenCoord;
    float fDepth;
    
    fDepth = g_txDepth.Load( int3( i2ScreenCoord, 0 ) ).x;
    f3Pos[0] = GetCameraXYZFromDepth( fDepth, i2ScreenCoord );
    
    if( b10_1 )
    {
        f3N[0].z = g_txNormalsZ.Load( int3( i2ScreenCoord, 0) ).x;
        f3N[0].xy = g_txNormalsXY.Load( int3( i2ScreenCoord, 0) ).xy;
    }
    else
    {
        f3N[0].zxy = g_txNormals.Load( int3( i2ScreenCoord, 0) ).xyz;
    }
    f3Pos[0] -= ( f3N[0] * g_fNormalScale );

    for( int iNormal=0; iNormal<NUM_NORMAL_LOADS; iNormal++ )
    {
        i2MirrorPattern = ( g_i2NormalLoadPattern[iNormal] + int2( 1, 1 ) ) * int2( -1, -1 );
        i2OffsetScreenCoord = i2ScreenCoord + g_i2NormalLoadPattern[iNormal];
        i2MirrorOffsetScreenCoord = i2ScreenCoord + i2MirrorPattern;
        
        // Clamp our test to screen coordinates
        i2OffsetScreenCoord = ( i2OffsetScreenCoord > ( g_f2RTSize - float2( 1.0f, 1.0f ) ) ) ? ( g_f2RTSize - float2( 1.0f, 1.0f ) ) : ( i2OffsetScreenCoord );
        i2MirrorOffsetScreenCoord = ( i2MirrorOffsetScreenCoord > ( g_f2RTSize - float2( 1.0f, 1.0f ) ) ) ? ( g_f2RTSize - float2( 1.0f, 1.0f ) ) : ( i2MirrorOffsetScreenCoord );
        i2OffsetScreenCoord = ( i2OffsetScreenCoord < 0 ) ? ( 0 ) : ( i2OffsetScreenCoord );
        i2MirrorOffsetScreenCoord = ( i2MirrorOffsetScreenCoord < 0 ) ? ( 0 ) : ( i2MirrorOffsetScreenCoord );
        
        fDepth = g_txDepth.Load( int3( i2OffsetScreenCoord, 0 ) ).x;
        f3Pos[1] = GetCameraXYZFromDepth( fDepth, i2OffsetScreenCoord );
        fDepth = g_txDepth.Load( int3( i2MirrorOffsetScreenCoord, 0 ) ).x;
        f3Pos[2] = GetCameraXYZFromDepth( fDepth, i2MirrorOffsetScreenCoord );
        
        if( b10_1 )
        {
            f3N[1].z = g_txNormalsZ.Load( int3( i2OffsetScreenCoord, 0) ).x;
            f3N[2].z = g_txNormalsZ.Load( int3( i2MirrorOffsetScreenCoord, 0) ).x;
            f3N[1].xy = g_txNormalsXY.Load( int3( i2OffsetScreenCoord, 0) ).xy;
            f3N[2].xy = g_txNormalsXY.Load( int3( i2MirrorOffsetScreenCoord, 0) ).xy;
        }
        else
        {
            f3N[1].zxy = g_txNormals.Load( int3( i2OffsetScreenCoord, 0) ).xyz;
            f3N[2].zxy = g_txNormals.Load( int3( i2MirrorOffsetScreenCoord, 0) ).xyz;
        }
                
        f3Pos[1] -= ( f3N[1] * g_fNormalScale );
        f3Pos[2] -= ( f3N[2] * g_fNormalScale );
        
        f3Dir[0] = f3Pos[1] - f3Pos[0];
        f3Dir[1] = f3Pos[2] - f3Pos[0];
        
        f3Dir[0] = normalize( f3Dir[0] );
        f3Dir[1] = normalize( f3Dir[1] );
        
        fDot = dot( f3Dir[0], f3Dir[1] );
        
        fSummedDot += ( fDot + 2.0f );
    }
        
    return ( fSummedDot * 0.125f );
}


//--------------------------------------------------------------------------------------
// Used as an early rejection test - based on normals
//--------------------------------------------------------------------------------------
float NormalRejectionTest( int2 i2ScreenCoord, uniform bool b10_1 )
{
    float3 f3N1;
    float3 f3N2;
    float fDot;
    float fSummedDot = 0.0f;
    int2 i2MirrorPattern;
    int2 i2OffsetScreenCoord;
    int2 i2MirrorOffsetScreenCoord;

    for( int iNormal=0; iNormal<NUM_NORMAL_LOADS; iNormal++ )
    {
        i2MirrorPattern = ( g_i2NormalLoadPattern[iNormal] + int2( 1, 1 ) ) * int2( -1, -1 );
        i2OffsetScreenCoord = i2ScreenCoord + g_i2NormalLoadPattern[iNormal];
        i2MirrorOffsetScreenCoord = i2ScreenCoord + i2MirrorPattern;
        
        // Clamp our test to screen coordinates
        i2OffsetScreenCoord = ( i2OffsetScreenCoord > ( g_f2RTSize - float2( 1.0f, 1.0f ) ) ) ? ( g_f2RTSize - float2( 1.0f, 1.0f ) ) : ( i2OffsetScreenCoord );
        i2MirrorOffsetScreenCoord = ( i2MirrorOffsetScreenCoord > ( g_f2RTSize - float2( 1.0f, 1.0f ) ) ) ? ( g_f2RTSize - float2( 1.0f, 1.0f ) ) : ( i2MirrorOffsetScreenCoord );
        i2OffsetScreenCoord = ( i2OffsetScreenCoord < 0 ) ? ( 0 ) : ( i2OffsetScreenCoord );
        i2MirrorOffsetScreenCoord = ( i2MirrorOffsetScreenCoord < 0 ) ? ( 0 ) : ( i2MirrorOffsetScreenCoord );
                        
        if( b10_1 )
        {
            f3N1.z = g_txNormalsZ.Load( int3( i2OffsetScreenCoord, 0) ).x;
            f3N2.z = g_txNormalsZ.Load( int3( i2MirrorOffsetScreenCoord, 0) ).x;
            f3N1.xy = g_txNormalsXY.Load( int3( i2OffsetScreenCoord, 0) ).xy;
            f3N2.xy = g_txNormalsXY.Load( int3( i2MirrorOffsetScreenCoord, 0) ).xy;
        }
        else
        {
            f3N1.zxy = g_txNormals.Load( int3( i2OffsetScreenCoord, 0) ).xyz;
            f3N2.zxy = g_txNormals.Load( int3( i2MirrorOffsetScreenCoord, 0) ).xyz;
        }
        
        fDot = dot( f3N1, f3N2 );
        
        fSummedDot += ( fDot > g_fAcceptAngle ) ? ( 0.0f ) : ( 1.0f - ( abs( fDot ) * 0.25f ) );
    }
        
    return ( 0.5f + fSummedDot * 0.25f  );
}


//--------------------------------------------------------------------------------------
// HDAO : Performs valley detection in Camera Z space, and optionally offsets by the Z 
// component of the camera space normal
//--------------------------------------------------------------------------------------
float PS_RenderHDAO( PS_RenderQuadInput I, uniform bool b10_1, uniform int iNumRingGathers,
                      uniform int iNumRings, uniform bool bUseNormals ) : SV_Target
{
    // Locals
    int2 i2ScreenCoord;
    float2 f2ScreenCoord_10_1;
    float2 f2ScreenCoord;
    float2 f2MirrorScreenCoord;
    float2 f2TexCoord_10_1;
    float2 f2MirrorTexCoord_10_1;
    float2 f2TexCoord;
    float2 f2MirrorTexCoord;
    float2 f2InvRTSize;
    float fCenterZ;
    float fOffsetCenterZ;
    float fCenterNormalZ;
    float4 f4SampledZ[2];
    float4 f4OffsetSampledZ[2];
    float4 f4SampledNormalZ[2];
    float4 f4Diff;
    float4 f4Compare[2];
    float4 f4Occlusion = 0.0f;
    float fOcclusion;
    int iGather;
    float fDot = 1.0f;
    float2 f2KernelScale = float2( g_f2RTSize.x / 1024.0f, g_f2RTSize.y / 768.0f );
    float3 f3CameraPos;
                            
    // Compute integer screen coord, and store off the inverse of the RT Size
    f2InvRTSize = 1.0f / g_f2RTSize;
    f2ScreenCoord = I.f2TexCoord * g_f2RTSize;
    i2ScreenCoord = int2( f2ScreenCoord );
                
    // Test the normals to see if we should apply occlusion
    fDot = NormalRejectionTest( i2ScreenCoord, b10_1 );    

    if( fDot > 0.5f )
    {                
        // Sample the center pixel for camera Z
        if( b10_1 )
        {
            // For Gather we need to snap the screen coords
            f2ScreenCoord_10_1 = float2( i2ScreenCoord );
            f2TexCoord = float2( f2ScreenCoord_10_1 * f2InvRTSize );
        }
        else
        {
            f2TexCoord = float2( f2ScreenCoord * f2InvRTSize );
        }

        float fDepth = g_txDepth.SampleLevel( g_SamplePoint, f2TexCoord, 0 ).x;
        fCenterZ = -g_fQTimesZNear / ( fDepth - g_fQ );
        
        if( bUseNormals )
        {
            if( b10_1 )
            {
                fCenterNormalZ = g_txNormalsZ.SampleLevel( g_SamplePoint, f2TexCoord, 0 ).x;
            }
            else
            {
                fCenterNormalZ = g_txNormals.SampleLevel( g_SamplePoint, f2TexCoord, 0 ).x;
            }
            fOffsetCenterZ = fCenterZ + fCenterNormalZ * g_fNormalScale;
        }
            
        // Loop through each gather location, and compare with its mirrored location
        for( iGather=0; iGather<iNumRingGathers; iGather++ )
        {
            f2MirrorScreenCoord = ( ( f2KernelScale * g_f2HDAORingPattern[iGather] ) + float2( 1.0f, 1.0f ) ) * float2( -1.0f, -1.0f );
            
            f2TexCoord = float2( ( f2ScreenCoord + ( f2KernelScale * g_f2HDAORingPattern[iGather] ) ) * f2InvRTSize );
            f2MirrorTexCoord = float2( ( f2ScreenCoord + ( f2MirrorScreenCoord ) ) * f2InvRTSize );
            
            // Sample
            if( b10_1 )
            {
                f2TexCoord_10_1 = float2( ( f2ScreenCoord_10_1 + ( ( f2KernelScale * g_f2HDAORingPattern[iGather] ) + float2( 1.0f, 1.0f ) ) ) * f2InvRTSize );
                f2MirrorTexCoord_10_1 = float2( ( f2ScreenCoord_10_1 + ( f2MirrorScreenCoord + float2( 1.0f, 1.0f ) ) ) * f2InvRTSize );
                
                f4SampledZ[0] = GatherZSamples( g_txDepth, f2TexCoord_10_1, b10_1 );
                f4SampledZ[1] = GatherZSamples( g_txDepth, f2MirrorTexCoord_10_1, b10_1 );
            }
            else
            {
                f4SampledZ[0] = GatherZSamples( g_txDepth, f2TexCoord, b10_1 );
                f4SampledZ[1] = GatherZSamples( g_txDepth, f2MirrorTexCoord, b10_1 );
            }
                        
            // Detect valleys
            f4Diff = fCenterZ.xxxx - f4SampledZ[0];
            f4Compare[0] = ( f4Diff < g_fHDAORejectRadius.xxxx ) ? ( 1.0f ) : ( 0.0f );
            f4Compare[0] *= ( f4Diff > g_fHDAOAcceptRadius.xxxx ) ? ( 1.0f ) : ( 0.0f );
            
            f4Diff = fCenterZ.xxxx - f4SampledZ[1];
            f4Compare[1] = ( f4Diff < g_fHDAORejectRadius.xxxx ) ? ( 1.0f ) : ( 0.0f );
            f4Compare[1] *= ( f4Diff > g_fHDAOAcceptRadius.xxxx ) ? ( 1.0f ) : ( 0.0f );
            
            f4Occlusion.xyzw += ( g_f4HDAORingWeight[iGather].xyzw * ( f4Compare[0].xyzw * f4Compare[1].zwxy ) * fDot );    
                
            if( bUseNormals )
            {
                // Sample normals
                if( b10_1 )
                {
        
                    f4SampledNormalZ[0] = GatherSamples( g_txNormalsZ, f2TexCoord_10_1, b10_1 );
                    f4SampledNormalZ[1] = GatherSamples( g_txNormalsZ, f2MirrorTexCoord_10_1, b10_1 );
                }
                else
                {
                    f4SampledNormalZ[0] = GatherSamples( g_txNormals, f2TexCoord, b10_1 );
                    f4SampledNormalZ[1] = GatherSamples( g_txNormals, f2MirrorTexCoord, b10_1 );
                }
                    
                // Scale normals
                f4OffsetSampledZ[0] = f4SampledZ[0] + ( f4SampledNormalZ[0] * g_fNormalScale );
                f4OffsetSampledZ[1] = f4SampledZ[1] + ( f4SampledNormalZ[1] * g_fNormalScale );
                            
                // Detect valleys
                f4Diff = fOffsetCenterZ.xxxx - f4OffsetSampledZ[0];
                f4Compare[0] = ( f4Diff < g_fHDAORejectRadius.xxxx ) ? ( 1.0f ) : ( 0.0f );
                f4Compare[0] *= ( f4Diff > g_fHDAOAcceptRadius.xxxx ) ? ( 1.0f ) : ( 0.0f );
                
                f4Diff = fOffsetCenterZ.xxxx - f4OffsetSampledZ[1];
                f4Compare[1] = ( f4Diff < g_fHDAORejectRadius.xxxx ) ? ( 1.0f ) : ( 0.0f );
                f4Compare[1] *= ( f4Diff > g_fHDAOAcceptRadius.xxxx ) ? ( 1.0f ) : ( 0.0f );
                
                f4Occlusion.xyzw += ( g_f4HDAORingWeight[iGather].xyzw * ( f4Compare[0].xyzw * f4Compare[1].zwxy ) * fDot );    
            }
        }
    }
                    
    // Finally calculate the HDAO occlusion value
    if( bUseNormals )
    {
        fOcclusion = ( ( f4Occlusion.x + f4Occlusion.y + f4Occlusion.z + f4Occlusion.w ) / ( 3.0f * g_fRingWeightsTotal[iNumRings - 1] ) );
    }
    else
    {
        fOcclusion = ( ( f4Occlusion.x + f4Occlusion.y + f4Occlusion.z + f4Occlusion.w ) / ( 2.0f * g_fRingWeightsTotal[iNumRings - 1] ) );
    }
    fOcclusion *= ( g_fHDAOIntensity );
    fOcclusion = 1.0f - saturate( fOcclusion );
    return fOcclusion;
}


//--------------------------------------------------------------------------------------
// This vertex shader passes params through to the pixel shader for
// higher quality per pixel lighting 
//--------------------------------------------------------------------------------------
PS_HQRenderSceneInput VS_HQRenderScene( VS_HQRenderSceneInput I )
{
    PS_HQRenderSceneInput O;
    
    O.f3WorldPos = mul( float4( I.f3Position, 1.0f ), g_f4x4World );
    O.f4Position = mul( float4( I.f3Position, 1.0f ), g_f4x4WorldViewProjection );
    O.f3Normal = normalize( mul( I.f3Normal, (float3x3)g_f4x4World ) );
    O.f3Tangent = normalize( mul( I.f3Tangent, (float3x3)g_f4x4World ) );
    O.f2TexCoord = I.f2TexCoord;
    
    return O;
}


//--------------------------------------------------------------------------------------
// This pixel shader computes higher quality per pixel lighting 10.0
//--------------------------------------------------------------------------------------

static float4 g_f4Directional1 = float4( 0.992, 1.0, 0.880, 0.0 );
static float4 g_f4Directional2 = float4( 0.595, 0.6, 0.528, 0.0 );
static float4 g_f4Ambient = float4(0.525, 0.474, 0.474, 0.0);
static float3 g_f3LightDir1 = float3( 1.705f, 5.557f, -9.380f );
static float3 g_f3LightDir2 = float3( -5.947f, -5.342f, -5.733f );

PS_RenderOutput_10_0 PS_HQRenderTexturedScene_10_0( PS_HQRenderSceneInput I )
{
    PS_RenderOutput_10_0 O;
    
    float3 LD1 = normalize( mul( g_f3LightDir1, (float3x3)g_f4x4World ) );
    float3 LD2 = normalize( mul( g_f3LightDir2, (float3x3)g_f4x4World ) );
        
    float4 f4Diffuse = g_txDiffuse.Sample( g_SampleLinear, I.f2TexCoord );
    float fSpecMask = f4Diffuse.a;
    float3 f3Norm = g_txNormal.Sample( g_SampleLinear, I.f2TexCoord );
    f3Norm *= 2.0f;
    f3Norm -= float3( 1.0f, 1.0f, 1.0f );
    
    float3 f3Binorm = normalize( cross( I.f3Normal, I.f3Tangent ) );
    float3x3 f3x3BasisMatrix = float3x3( f3Binorm, I.f3Tangent, I.f3Normal );
    f3Norm = normalize( mul( f3Norm, f3x3BasisMatrix ) );
   
    // Write out the camera space normal 
    O.f4Normal.x = f3Norm.z;     
    O.f4Normal.y = f3Norm.x;    
    O.f4Normal.z = f3Norm.y;    
    O.f4Normal.w = 0.0f;
                
    // Diffuse lighting
    float4 f4Lighting = saturate( dot( f3Norm, LD1.xyz ) ) * g_f4Directional1;
    f4Lighting += saturate( dot( f3Norm, LD2.xyz ) ) * g_f4Directional2;
    f4Lighting += ( g_f4Ambient );
    
    // Calculate specular power
    float3 f3ViewDir = normalize( g_f3EyePt - I.f3WorldPos );
    float3 f3HalfAngle = normalize( f3ViewDir + LD1.xyz );
    float4 f4SpecPower1 = pow( saturate( dot( f3HalfAngle, f3Norm ) ), 32 ) * g_f4Directional1;
    
    f3HalfAngle = normalize( f3ViewDir + LD2.xyz );
    float4 f4SpecPower2 = pow( saturate( dot( f3HalfAngle, f3Norm ) ), 32 ) * g_f4Directional2;
    
    O.f4Color = f4Lighting * f4Diffuse + ( f4SpecPower1 + f4SpecPower2 ) * fSpecMask;
    
    return O;
}


//--------------------------------------------------------------------------------------
// This pixel shader computes higher quality per pixel lighting 10.1
//--------------------------------------------------------------------------------------
PS_RenderOutput_10_1 PS_HQRenderTexturedScene_10_1( PS_HQRenderSceneInput I )
{
    PS_RenderOutput_10_1 O;
    
    float3 LD1 = normalize( mul( g_f3LightDir1, (float3x3)g_f4x4World ) );
    float3 LD2 = normalize( mul( g_f3LightDir2, (float3x3)g_f4x4World ) );
        
    float4 f4Diffuse = g_txDiffuse.Sample( g_SampleLinear, I.f2TexCoord );
    float fSpecMask = f4Diffuse.a;
    float3 f3Norm = g_txNormal.Sample( g_SampleLinear, I.f2TexCoord );
    f3Norm *= 2.0f;
    f3Norm -= float3( 1.0f, 1.0f, 1.0f );
    
    float3 f3Binorm = normalize( cross( I.f3Normal, I.f3Tangent ) );
    float3x3 f3x3BasisMatrix = float3x3( f3Binorm, I.f3Tangent, I.f3Normal );
    f3Norm = normalize( mul( f3Norm, f3x3BasisMatrix ) );
    
    // Write out the camera space normal
    O.fNormalZ.x = f3Norm.z;        // we deliberately place Z in a seperate surface for gather4 instruction 
    O.f2NormalXY.xy = f3Norm.xy;    
                
    // Diffuse lighting
    float4 f4Lighting = saturate( dot( f3Norm, LD1.xyz ) ) * g_f4Directional1;
    f4Lighting += saturate( dot( f3Norm, LD2.xyz ) ) * g_f4Directional2;
    f4Lighting += ( g_f4Ambient );
    
    // Calculate specular power
    float3 f3ViewDir = normalize( g_f3EyePt - I.f3WorldPos );
    float3 f3HalfAngle = normalize( f3ViewDir + LD1.xyz );
    float4 f4SpecPower1 = pow( saturate( dot( f3HalfAngle, f3Norm ) ), 32 ) * g_f4Directional1;
    
    f3HalfAngle = normalize( f3ViewDir + LD2.xyz );
    float4 f4SpecPower2 = pow( saturate( dot( f3HalfAngle, f3Norm ) ), 32 ) * g_f4Directional2;
    
    O.f4Color = f4Lighting * f4Diffuse + ( f4SpecPower1 + f4SpecPower2 ) * fSpecMask;
    
    return O;
}


//--------------------------------------------------------------------------------------
// This shader computes standard transform and lighting
//--------------------------------------------------------------------------------------
PS_RenderSceneInput VS_RenderScene( VS_RenderSceneInput I )
{
    PS_RenderSceneInput O;
    float3 f3NormalWorldSpace;
    
    // Transform the position from object space to homogeneous projection space
    O.f4Position = mul( float4( I.f3Position, 1.0f ), g_f4x4WorldViewProjection );
    
    // Transform the normal from object space to world space    
    f3NormalWorldSpace = normalize( mul( I.f3Normal, (float3x3)g_f4x4World ) );
    
    // Output camera space normals
    O.f4Normal.xyz = mul( f3NormalWorldSpace,  (float3x3)g_f4x4View );
    O.f4Normal.w = 0.0f;
       
    // Calc diffuse color
    O.f4Diffuse.rgb = max( 0, dot( f3NormalWorldSpace, g_f3LightDir ) );   
    
    // Calc diffuse color    
    float4 f4MaterialDiffuseColor = float4( 1.0f, 1.0f, 1.0f, 1.0f );
    float4 f4LightDiffuse = float4( 1.0f, 1.0f, 1.0f, 1.0f );
    float4 f4MaterialAmbientColor = float4( 0.2f, 0.2f, 0.2f, 1.0f );
    O.f4Diffuse.rgb = g_f4MaterialDiffuse * f4LightDiffuse * max( 0, dot( f3NormalWorldSpace, g_f3LightDir ) ) + 
                      f4MaterialAmbientColor + g_f4MaterialSpecular;  
    O.f4Diffuse.a = 1.0f; 
    
    // Propogate texture coordinate
    O.f2TexCoord = I.f2TexCoord;
    
    return O;    
}


//--------------------------------------------------------------------------------------
// This shader outputs the pixel's color by modulating the texture's
// color with diffuse material color
//--------------------------------------------------------------------------------------
PS_RenderOutput_10_0 PS_RenderScene_10_0( PS_RenderSceneInput I )
{
    PS_RenderOutput_10_0 O;
    
    O.f4Normal.xyzw = I.f4Normal.zxyw;
    
    O.f4Color = I.f4Diffuse;
    
    return O;
}


//--------------------------------------------------------------------------------------
// This shader outputs the pixel's color by modulating the texture's
// color with diffuse material color
//--------------------------------------------------------------------------------------
PS_RenderOutput_10_1 PS_RenderScene_10_1( PS_RenderSceneInput I )
{
    PS_RenderOutput_10_1 O;
    
    O.fNormalZ = I.f4Normal.z;
    O.f2NormalXY = I.f4Normal.xy;
        
    O.f4Color = I.f4Diffuse;
    
    return O;
}


//--------------------------------------------------------------------------------------
// This shader outputs the pixel's color by modulating the texture's
// color with diffuse material color
//--------------------------------------------------------------------------------------
PS_RenderOutput_10_0 PS_RenderTexturedScene_10_0( PS_RenderSceneInput I )
{ 
    PS_RenderOutput_10_0 O;
    
    O.f4Normal.xyzw = I.f4Normal.zxyw;
    
    O.f4Color = g_txDiffuse.Sample( g_SampleLinear, I.f2TexCoord );
    
    return O;
}


//--------------------------------------------------------------------------------------
// This shader outputs the pixel's color by modulating the texture's
// color with diffuse material color
//--------------------------------------------------------------------------------------
PS_RenderOutput_10_1 PS_RenderTexturedScene_10_1( PS_RenderSceneInput I )
{ 
    PS_RenderOutput_10_1 O;
    
    O.fNormalZ.x = I.f4Normal.z;
    O.f2NormalXY.xy = I.f4Normal.xy;
    
    O.f4Color = g_txDiffuse.Sample( g_SampleLinear, I.f2TexCoord );
    
    return O;
}


//--------------------------------------------------------------------------------------
// Renders a textured fullscreen quad
//--------------------------------------------------------------------------------------
PS_RenderQuadInput VS_RenderQuad( VS_RenderQuadInput I )
{
    PS_RenderQuadInput O;
    
    O.f4Position.x = I.f3Position.x;
    O.f4Position.y = I.f3Position.y;
    O.f4Position.z = I.f3Position.z;
    O.f4Position.w = 1.0f;
    
    O.f2TexCoord = I.f2TexCoord;
    
    return O;    
}


//--------------------------------------------------------------------------------------
// Combines the main scene render with the HDAO texture
//--------------------------------------------------------------------------------------
float4 PS_RenderCombined( PS_RenderQuadInput I ) : SV_Target
{ 
    // Sample the scene and HDAO textures
    float fHDAO = g_txHDAO.Sample( g_SamplePoint, I.f2TexCoord );
    float4 f4Scene = g_txScene.Sample( g_SamplePoint, I.f2TexCoord );

    // Finally combine the scene color with the HDAO occlusion value
    return f4Scene * fHDAO;
}


//--------------------------------------------------------------------------------------
// Renders the HDAO buffer
//--------------------------------------------------------------------------------------
float4 PS_RenderHDAOBuffer( PS_RenderQuadInput I ) : SV_Target
{ 
    // Sample the HDAO texture
    return g_txHDAO.Sample( g_SamplePoint, I.f2TexCoord ).xxxx;
}


//--------------------------------------------------------------------------------------
// Renders camera space Z
//--------------------------------------------------------------------------------------
float4 PS_RenderCameraZ( PS_RenderQuadInput I ) : SV_Target
{ 
    float fDepth;
    float4 f4CameraZ;
    
    fDepth = g_txDepth.SampleLevel( g_SamplePoint, I.f2TexCoord, 0 ).x;
    f4CameraZ.x = -g_fQTimesZNear / ( fDepth - g_fQ );
    
    return f4CameraZ.xxxx * 0.005f;
}


//--------------------------------------------------------------------------------------
// Renders the normals buffer
//--------------------------------------------------------------------------------------
float4 PS_RenderNormalsBuffer( PS_RenderQuadInput I, uniform bool b10_1 ) : SV_Target
{ 
    float4 f4Ret;
    
    if( b10_1 )
    {    
        f4Ret.z = g_txNormalsZ.Sample( g_SamplePoint, I.f2TexCoord ).x;
        f4Ret.xy = g_txNormalsXY.Sample( g_SamplePoint, I.f2TexCoord ).xy;
    }
    else
    {
        f4Ret.zxy = g_txNormals.Sample( g_SamplePoint, I.f2TexCoord ).xyz;
    }
    
    f4Ret.w = 0.0f;
    
    return f4Ret;
}


//--------------------------------------------------------------------------------------
// Render the scene without HDAO combined
//--------------------------------------------------------------------------------------
float4 PS_RenderUnCombined( PS_RenderQuadInput I ) : SV_Target
{ 
    return g_txScene.Sample( g_SamplePoint, I.f2TexCoord );
}


//--------------------------------------------------------------------------------------
// Render states 
//--------------------------------------------------------------------------------------


DepthStencilState EnableDepthTestWrite
{
    DepthEnable = TRUE;
    DepthWriteMask = 1;
    StencilEnable = FALSE;
};

DepthStencilState DisableDepthTestWrite
{
    DepthEnable = FALSE;
    DepthWriteMask = 0;
    StencilEnable = FALSE;
    StencilReadMask = 0xFF;
};

BlendState NoBlending
{
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = FALSE;
    RenderTargetWriteMask[0] = 0x0F;
};


//--------------------------------------------------------------------------------------
// Renders scene 
//--------------------------------------------------------------------------------------
technique10 RenderScene_10_0
{
    pass P0
    {  
        SetDepthStencilState( EnableDepthTestWrite, 0x00 );
        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
             
        SetVertexShader( CompileShader( vs_4_0, VS_RenderScene() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_RenderScene_10_0() ) );
    }
}

technique10 RenderScene_10_1
{
    pass P0
    {  
        SetDepthStencilState( EnableDepthTestWrite, 0x00 );
        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
             
        SetVertexShader( CompileShader( vs_4_0, VS_RenderScene() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_RenderScene_10_1() ) );
    }
}


//--------------------------------------------------------------------------------------
// Renders scene 
//--------------------------------------------------------------------------------------
technique10 RenderTexturedScene_10_0
{
    pass P0
    {       
        SetDepthStencilState( EnableDepthTestWrite, 0x00 );
        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        
        SetVertexShader( CompileShader( vs_4_0, VS_RenderScene() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_RenderTexturedScene_10_0() ) );
    }
}

technique10 RenderTexturedScene_10_1
{
    pass P0
    {       
        SetDepthStencilState( EnableDepthTestWrite, 0x00 );
        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        
        SetVertexShader( CompileShader( vs_4_0, VS_RenderScene() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_RenderTexturedScene_10_1() ) );
    }
}

technique10 RenderHQTexturedScene_10_0
{
    pass P0
    {       
        SetDepthStencilState( EnableDepthTestWrite, 0x00 );
        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        
        SetVertexShader( CompileShader( vs_4_0, VS_HQRenderScene() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_HQRenderTexturedScene_10_0() ) );
    }
}

technique10 RenderHQTexturedScene_10_1
{
    pass P0
    {       
        SetDepthStencilState( EnableDepthTestWrite, 0x00 );
        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        
        SetVertexShader( CompileShader( vs_4_0, VS_HQRenderScene() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_HQRenderTexturedScene_10_1() ) );
    }
}


//--------------------------------------------------------------------------------------
// Renders HDAO 10.1 version 
//--------------------------------------------------------------------------------------

#ifdef DX10_1_ENABLED

technique10 RenderHDAO_Normals_10_1
{
    pass P0
    {       
        SetDepthStencilState( DisableDepthTestWrite, 0x00 );
        
        SetVertexShader( CompileShader( vs_4_0, VS_RenderQuad() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_1, PS_RenderHDAO( true, NUM_RING_4_GATHERS, RING_4, true ) ) );
    }
}

technique10 RenderHDAO_10_1
{
    pass P0
    {       
        SetDepthStencilState( DisableDepthTestWrite, 0x00 );
        
        SetVertexShader( CompileShader( vs_4_0, VS_RenderQuad() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_1, PS_RenderHDAO( true, NUM_RING_4_GATHERS, RING_4, false ) ) );
    }
}

#endif

//--------------------------------------------------------------------------------------
// Renders HDAO 10.0 version 
//--------------------------------------------------------------------------------------
technique10 RenderHDAO_Normals_10_0
{
    pass P0
    {       
        SetDepthStencilState( DisableDepthTestWrite, 0x00 );
                
        SetVertexShader( CompileShader( vs_4_0, VS_RenderQuad() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_RenderHDAO( false, NUM_RING_4_GATHERS, RING_4, true ) ) );
    }
}

technique10 RenderHDAO_10_0
{
    pass P0
    {       
        SetDepthStencilState( DisableDepthTestWrite, 0x00 );
                
        SetVertexShader( CompileShader( vs_4_0, VS_RenderQuad() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_RenderHDAO( false, NUM_RING_4_GATHERS, RING_4, false ) ) );
    }
}


//--------------------------------------------------------------------------------------
// Renders combined HDAO and scene 
//--------------------------------------------------------------------------------------
technique10 RenderCombined
{
    pass P0
    {       
        SetDepthStencilState( DisableDepthTestWrite, 0x00 );
        
        SetVertexShader( CompileShader( vs_4_0, VS_RenderQuad() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_RenderCombined() ) );
    }
}


//--------------------------------------------------------------------------------------
// Renders the HDAO buffer 
//--------------------------------------------------------------------------------------
technique10 RenderHDAOBuffer
{
    pass P0
    {       
        SetDepthStencilState( DisableDepthTestWrite, 0x00 );
        
        SetVertexShader( CompileShader( vs_4_0, VS_RenderQuad() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_RenderHDAOBuffer() ) );
    }
}


//--------------------------------------------------------------------------------------
// Renders camera space Z 
//--------------------------------------------------------------------------------------
technique10 RenderCameraZ
{
    pass P0
    {       
        SetDepthStencilState( DisableDepthTestWrite, 0x00 );
        
        SetVertexShader( CompileShader( vs_4_0, VS_RenderQuad() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_RenderCameraZ() ) );
    }
}


//--------------------------------------------------------------------------------------
// Renders the Normals buffer 
//--------------------------------------------------------------------------------------
technique10 RenderNormalsBuffer_10_0
{
    pass P0
    {       
        SetDepthStencilState( DisableDepthTestWrite, 0x00 );
        
        SetVertexShader( CompileShader( vs_4_0, VS_RenderQuad() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_RenderNormalsBuffer( false ) ) );
    }
}

technique10 RenderNormalsBuffer_10_1
{
    pass P0
    {       
        SetDepthStencilState( DisableDepthTestWrite, 0x00 );
        
        SetVertexShader( CompileShader( vs_4_0, VS_RenderQuad() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_RenderNormalsBuffer( true ) ) );
    }
}


//--------------------------------------------------------------------------------------
// Renders scene 
//--------------------------------------------------------------------------------------
technique10 RenderUnCombined
{
    pass P0
    {    
        SetDepthStencilState( DisableDepthTestWrite, 0x00 );
           
        SetVertexShader( CompileShader( vs_4_0, VS_RenderQuad() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PS_RenderUnCombined() ) );
    }
}


//--------------------------------------------------------------------------------------
// EOF 
//--------------------------------------------------------------------------------------

























