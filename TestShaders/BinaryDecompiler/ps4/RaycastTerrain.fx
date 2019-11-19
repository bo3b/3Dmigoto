//--------------------------------------------------------------------------------------
// File: BasicHLSL10.fx
//
// The effect file for the BasicHLSL sample.  
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// Maximum number of binary searches to make
#define BINARY_STEPS 8

// Maximum number of steps to find any intersection for shadows
#define MAX_ANY_STEPS 256

// Maximum number of steps to make for relief mapping of details
#define MAX_DETAIL_STEPS 128

// Maximum number of steps to take when cone-step mapping
#define MAX_CONE_STEPS 512

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
cbuffer cbOnRender
{
    float3		g_LightDir;     
    float3		g_LightDirTex;    
    float4		g_LightDiffuse;     
    float4x4	g_mWorldViewProjection;   
    float4x4	g_mWorld;                 
    float3		g_vTextureEyePt;
    float4x4	g_mWorldToTerrain;
    float4x4	g_mTexToViewProj;
    float4x4	g_mLightViewProj;
    float4x4	g_mTexToLightViewProj;
};

cbuffer cbConstant
{
    float g_InvMapSize = 1.0/1024.0;
    float g_MapSize = 1024.0;
    
    float g_InvDetailMapSize = 1.0/256.0;
    float g_DetailMapSize = 256.0;
    
    float g_HeightRatio = 0.1;
};

cbuffer cbUI
{	
    float g_DetailRepeat = 16;
    float g_InvDetailRepeat = 1/16.0;
    float g_DetailHeight = 0.1;
    float g_ShadowBias = 0.01;
    float g_DetailDistanceSq = 1.0;
};

//--------------------------------------------------------------------------------------
// DepthStates
//--------------------------------------------------------------------------------------
DepthStencilState EnableDepth
{
    DepthEnable = TRUE;
    DepthWriteMask = ALL;
    DepthFunc = LESS_EQUAL;
};

DepthStencilState DisableDepth
{
    DepthEnable = FALSE;
    DepthWriteMask = 0;
    DepthFunc = LESS_EQUAL;
};

DepthStencilState DepthRead
{
    DepthEnable = TRUE;
    DepthWriteMask = 0;
    DepthFunc = LESS_EQUAL;
};

BlendState DisableBlending
{
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = FALSE;
    BlendEnable[1] = FALSE;
    RenderTargetWriteMask[0] = 0x0F;
    RenderTargetWriteMask[1] = 0x0F;
};

RasterizerState Wireframe
{
    CullMode = BACK;
    FillMode = WIREFRAME;
};

RasterizerState Solid
{
    CullMode = BACK;
    FillMode = SOLID;
};

//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
Texture2D g_txDiffuse;				// Color texture for mesh
Texture2D g_txDetailDiffuse[4];		// Color texture for detail
Texture2D g_txDetailGrad_RedGreen;	// Gradient detail textures
Texture2D g_txDetailGrad_BlueAlpha;	// Gradient detail textures
Texture2D g_txHeight;				// height texture
Texture2D g_txMask;					// 4 channel mask
Texture2D g_txDetailHeight;			// Detail height texture
Texture2D g_txDepthMap;				// Depth map from the light POV

SamplerState g_samLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState g_samLinearPoint
{
    Filter = MIN_MAG_LINEAR_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState g_samWrap
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

SamplerComparisonState g_samComparison
{
    Filter = COMPARISON_MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
    ComparisonFunc = LESS;
};


//--------------------------------------------------------------------------------------
// Vertex shader output structure
//--------------------------------------------------------------------------------------
struct VS_QUADINPUT
{
    float3 vPosition			: POSITION;
};

struct VS_RAYOUTPUT
{
    float3 vTextureRayEnd		: RAYEND;
    float4 vPosition			: SV_POSITION;
};

struct PS_RAYINPUT
{
    float3 vTextureRayEnd		: RAYEND;
};

struct VS_MESHINPUT
{
    float3 vPosition			: POSITION;
    float3 vNormal				: NORMAL;
    float2 vTex					: TEXCOORD0;
};

struct VS_MESHOUTPUT
{
    float3 vNormal				: NORMAL;
    float2 vTex					: TEXCOORD0;
    float3 vWorldPos			: TEXCOORD1;
    float4 vPosition			: SV_POSITION;
};

struct PS_MESHINPUT
{
    float3 vNormal				: NORMAL;
    float2 vTex					: TEXCOORD0;
    float3 vWorldPos			: TEXCOORD1;
};

//--------------------------------------------------------------------------------------
// Helpers for shadowmapping
//--------------------------------------------------------------------------------------
float CalculateShadowAmount( float3 vPosition )
{
    // Shadowmap
    float4 vLightSpacePos = mul( float4(vPosition,1), g_mLightViewProj );
    float2 vLightTex = ( vLightSpacePos.xy / vLightSpacePos.w ) * 0.5 + 0.5;
    vLightTex.y = 1 - vLightTex.y;
    float fTest = ( vLightSpacePos.z / vLightSpacePos.w ) - g_ShadowBias;
    float fShadow = g_txDepthMap.SampleCmpLevelZero( g_samComparison, vLightTex, fTest );
    return fShadow;
}

float CalculateShadowAmountRaycast( float3 vPosition )
{
    // Shadowmap
    float4 vLightSpacePos = mul( float4(vPosition,1), g_mTexToLightViewProj );
    float2 vLightTex = ( vLightSpacePos.xy / vLightSpacePos.w ) * 0.5 + 0.5;
    vLightTex.y = 1 - vLightTex.y;
    float fTest = ( vLightSpacePos.z / vLightSpacePos.w ) - g_ShadowBias;
    float fShadow = g_txDepthMap.SampleCmpLevelZero( g_samComparison, vLightTex, fTest );
    return fShadow;
}

//--------------------------------------------------------------------------------------
// Simple VS for rendering a mesh
//--------------------------------------------------------------------------------------
VS_MESHOUTPUT MeshVS( VS_MESHINPUT Input )
{
    VS_MESHOUTPUT Output;
    
    Output.vPosition = mul( float4( Input.vPosition, 1 ), g_mWorldViewProjection );
    Output.vNormal = mul( Input.vNormal, (float3x3)g_mWorld );
    Output.vTex = Input.vTex;
    Output.vWorldPos = mul( float4( Input.vPosition, 1 ), g_mWorld ).xyz;
    
    return Output;
}

float4 MeshPS( PS_MESHINPUT Input, uniform bool bShadow ) : SV_TARGET
{
    float fLighting = dot( normalize( Input.vNormal ), g_LightDir );
    float4 vDiffuse = g_txDiffuse.Sample( g_samLinear, Input.vTex );
    
    float fShadow = 1;
    if( bShadow )
    {
        fShadow = CalculateShadowAmount( Input.vWorldPos );
    }
    
    return vDiffuse * max( 0.1, fLighting * fShadow );
}

float4 GeometryTerrainPS( PS_MESHINPUT Input, uniform bool bShadow ) : SV_TARGET
{
    float fLighting = dot( normalize( Input.vNormal ), g_LightDir );
    float4 vDiffuse = float4( 1, 1, 1, 1 );
    
    float fShadow = 1;
    
    if( bShadow )
    {
        fShadow = CalculateShadowAmount( Input.vWorldPos );
    }
    
    return vDiffuse * max( 0.1, fLighting * fShadow );
}

//--------------------------------------------------------------------------------------
// Interpolate texture space rays across the quad
//--------------------------------------------------------------------------------------
VS_RAYOUTPUT ShootRayVS( VS_QUADINPUT Input )
{
    VS_RAYOUTPUT Output;
    
    // Pass position through
    Output.vPosition = mul( float4( Input.vPosition, 1 ), g_mWorldViewProjection );
    
    // Transform the ray into texture space
    Output.vTextureRayEnd = Input.vPosition.xzy;
    
    return Output;    
}

//--------------------------------------------------------------------------------------
// Interpolate texture space rays across the quad
//--------------------------------------------------------------------------------------
float4 RayTextureCoordsPS( PS_RAYINPUT Input ) : SV_TARGET
{
    return float4( Input.vTextureRayEnd, 1 );
}

//--------------------------------------------------------------------------------------
// Sampling
//--------------------------------------------------------------------------------------
float4 SmoothSample( float2 tex )
{
    return g_txHeight.SampleLevel( g_samLinearPoint, tex, 0 );
}

float SmoothSampleDetail( float2 tex, inout float4 value1, inout float4 mask )
{
    value1 = g_txHeight.SampleLevel( g_samLinearPoint, tex, 0 );
    mask = g_txMask.SampleLevel( g_samLinearPoint, tex, 0 );
    float4 value2 = g_txDetailHeight.SampleLevel( g_samWrap, tex * g_DetailRepeat, 0 );
    
    float detailheight = dot( value2, mask );
    return value1.r - ( 1 - detailheight ) * g_DetailHeight;
}

//--------------------------------------------------------------------------------------
// Bisection method for finding the exact point of intersection
//--------------------------------------------------------------------------------------
float3 Bisect( float3 vRayStart, float3 vRayDelta, inout float2 dduddv )
{
    // search around first point (depth) for closest match
    float t = 0.5;
    float binaryStep = 0.25;
    float3 vBestPoint = vRayStart + vRayDelta;
    
    for ( int i=0; i<BINARY_STEPS; i++) 
    { 
        // Get the middle of the ray
        float3 vMidPoint = vRayStart + t * vRayDelta;
    
        // sample	
        float4 vValue = SmoothSample( vMidPoint.xy );
        
        if( vMidPoint.z <= vValue.r )
        { 
            // Step backwards
            t -= 2 * binaryStep;
            vBestPoint = vMidPoint;
            dduddv = vValue.zw;
        } 
        // Step forwards
        t += binaryStep;
        
        binaryStep *= 0.5;
    }
    
    return vBestPoint;
}

float3 BisectDetail( float3 vRayStart, float3 vRayDelta, inout float2 dduddv1, inout float4 mask )
{
    // search around first point (depth) for closest match
    float t = 0.5;
    float binaryStep = 0.25;
    float3 vBestPoint = vRayStart + vRayDelta;
    
    for ( int i=0; i<BINARY_STEPS; i++) 
    { 
        // Get the middle of the ray
        float3 vMidPoint = vRayStart + t * vRayDelta;
    
        // sample	
        float4 vValue1, vMask;
        float Height = SmoothSampleDetail( vMidPoint.xy, vValue1, vMask );
        
        if( vMidPoint.z <= Height )
        { 
            // Step backwards
            t -= 2 * binaryStep;
            vBestPoint = vMidPoint;
            dduddv1 = vValue1.zw;
            mask = vMask;
        } 
        // Step forwards
        t += binaryStep;
        
        binaryStep *= 0.5;
    }
    
    return vBestPoint;
}

//--------------------------------------------------------------------------------------
// Intersect the ray with the texture bounding box so we know where to start.
// This is for the case where our eyepoint is outside of the box.
//--------------------------------------------------------------------------------------
float3 GetFirstSceneIntersection( float3 vRayO, float3 vRayDir )
{
    // Intersect the ray with the bounding box
    // ( y - vRayO.y ) / vRayDir.y = t

    float fMaxT = -1;
    float t;
    float3 vRayIntersection;

    // -X plane
    if( vRayDir.x > 0 )
    {
        t = ( 0 - vRayO.x ) / vRayDir.x;
        fMaxT = max( t, fMaxT );
    }

    // +X plane
    if( vRayDir.x < 0 )
    {
        t = ( 1 - vRayO.x ) / vRayDir.x;
        fMaxT = max( t, fMaxT );
    }

    // -Y plane
    if( vRayDir.y > 0 )
    {
        t = ( 0 - vRayO.y ) / vRayDir.y;
        fMaxT = max( t, fMaxT );
    }

    // +Y plane
    if( vRayDir.y < 0 )
    {
        t = ( 1 - vRayO.y ) / vRayDir.y;
        fMaxT = max( t, fMaxT );
    }

    // -Z plane
    if( vRayDir.z > 0 )
    {
        t = ( 0 - vRayO.z ) / vRayDir.z;
        fMaxT = max( t, fMaxT );
    }

    // +Z plane
    if( vRayDir.z < 0 )
    {
        t = ( 1 - vRayO.z ) / vRayDir.z;
        fMaxT = max( t, fMaxT );
    }

    vRayIntersection = vRayO + vRayDir * fMaxT;

    return vRayIntersection;
}

//--------------------------------------------------------------------------------------
// Cone step a using the detail map
//--------------------------------------------------------------------------------------
float3 ConeStepRayDetail( float3 vRayStart, float3 vRayEnd, inout float2 dduddv1, inout float4 mask, uniform bool bBisect )
{	
    float3 vRayDir = vRayEnd - vRayStart;
    
    float fMaxLength = length( vRayDir );
    vRayDir /= fMaxLength;
    
    float fIZ = sqrt( 1.0 - vRayDir.z * vRayDir.z );
    
    float4 vHeightSample1;
    
    float Height = SmoothSampleDetail( vRayStart.xy, vHeightSample1, mask );
    float fTotalStep = 0;
    float fStepSize = 0;
    int StepCounter = 0;
    
    float fMinStep = ( g_InvDetailMapSize *  g_InvDetailRepeat );
    
    // Linear step then bisect
    fStepSize = 4 * fMinStep;
    while( vRayStart.z > Height )
    {	
        // Step the ray
        fTotalStep += fStepSize;
        vRayStart += vRayDir * fStepSize;
        
        // Early out if necessary
        if( fTotalStep > fMaxLength || StepCounter > MAX_DETAIL_STEPS )
        {
            dduddv1 = vHeightSample1.zw;
            return vRayStart;
        }
            
        // Sample the texture
        Height = SmoothSampleDetail( vRayStart.xy, vHeightSample1, mask );
        
        StepCounter ++;
    }
    
    dduddv1 = vHeightSample1.zw;
    
    float3 vExactHit;
    if( bBisect )
    {
        // The previous hit was not inside the object, but this was one, so bisect
        float3 vRayDelta = fStepSize * vRayDir;
        vExactHit = BisectDetail( vRayStart - vRayDelta, vRayDelta, dduddv1, mask );
    }
    else
    {
        vExactHit = vRayStart;
    }
    
    return vExactHit;
}

//--------------------------------------------------------------------------------------
// Cone step a ray, then bisect
//--------------------------------------------------------------------------------------
float3 ConeStepRay( float3 vRayStart, float3 vRayEnd, inout float2 dduddv1, inout float4 mask, uniform bool bBisect, uniform bool bOutside, uniform bool bDetail )
{	
    float3 vRayDir = vRayEnd - vRayStart;
    float3 vOldStart = vRayStart;
    mask = 1;
    if( bOutside )
    {
        // if we're outside the box, find the first intersection point with the box and start there
        // Don't worry if vRayDir isn't normalized, it will still work out
        vRayStart = GetFirstSceneIntersection( vRayStart, vRayDir );
        vRayDir = vRayEnd - vRayStart;
    }
    
    float fMaxLength = length( vRayDir );
    vRayDir /= fMaxLength;
    
    // Do the actual cone step mapping
    float fIZ = sqrt( 1.0 - vRayDir.z * vRayDir.z );
    float4 vHeightSample = SmoothSample( vRayStart.xy );
    float fTotalStep = 0;
    float fStepSize = 0;
    int StepCounter = 0;
    while( vRayStart.z > vHeightSample.r )
    {
        // Calculate the step size
        float fConeRatio = vHeightSample.g * vHeightSample.g;
        float fStepRatio = 1.0 / ( ( fIZ / fConeRatio ) - vRayDir.z );
        fStepSize = max( g_InvMapSize, ( vRayStart.z - vHeightSample.r ) * fStepRatio );
        
        // Step the ray
        fTotalStep += fStepSize;
        vRayStart += vRayDir * fStepSize;
        
        // Early out if necessary
        if( fTotalStep > fMaxLength )
            return float3( -1, -1, -1 );
            
        if( StepCounter > MAX_CONE_STEPS )
            return float3( -1, -1, -1 );
            
        // Sample the texture
        vHeightSample = SmoothSample( vRayStart.xy );
        
        StepCounter ++;
    }
    
    // Do another cone step mapping if we're adding detail
    float3 vExactHit;
    dduddv1 = vHeightSample.zw;
    if( bDetail )
    {
        float3 vToPoint = vRayStart - vOldStart;
        vToPoint.z *= g_HeightRatio;
        float fLenSq = dot( vToPoint, vToPoint );
    
        if( fLenSq < g_DetailDistanceSq )
        {
            float3 vRayDelta = fStepSize * vRayDir;
            vExactHit = ConeStepRayDetail( vRayStart - vRayDelta, vRayEnd, dduddv1, mask, bBisect );
            
            return vExactHit;
        }
        else
        {
            mask =  g_txMask.SampleLevel( g_samLinearPoint, vRayStart.xy, 0 );
        }
    }
    
    // If we want an accurate intersection, use bisection to find the hit point
    if( bBisect )
    {
        // The previous hit was not inside the object, but this was one, so bisect
        float3 vRayDelta = fStepSize * vRayDir;
        vExactHit = Bisect( vRayStart - vRayDelta, vRayDelta, dduddv1 );
    }
    else
    {
        vExactHit = vRayStart;
    }
    
    return vExactHit;
}

//--------------------------------------------------------------------------------------
// Shoot a ray across the texture
//--------------------------------------------------------------------------------------
struct PS_OUTPUT
{
    float4 color : SV_TARGET;
    float  depth : SV_DEPTH;
};

PS_OUTPUT ShootRayPS( PS_RAYINPUT Input, uniform bool bOutside, uniform bool bDetail, uniform bool bShadow, uniform bool bOrtho )
{
    PS_OUTPUT Output;
    
    float2 dduddv1 = float2( 0, 0 );
    float3 vEyePt = g_vTextureEyePt.xzy;
    float4 mask;
    
    // Shoot rays differently for an orthographic projection vs a perspective projection
    if( bOrtho )
        vEyePt = Input.vTextureRayEnd + g_LightDirTex * 2.0;
    else
        vEyePt = g_vTextureEyePt.xzy;
        
    // Find the intersection point
    float3 vIntersection = ConeStepRay( vEyePt, Input.vTextureRayEnd, dduddv1, mask, true, bOutside, bDetail );
    
    float4 vColor = float4( 0, 0, 0, 0 );
    
    // If we hit anything vIntersection.r will be greater than or equal to zero
    if( vIntersection.r >= 0 )
    {	
        // Reconstruct the normal
        float3 vNormal;
        vNormal.xz = ( dduddv1.xy - 0.5 ) * 2;
        vNormal.y = sqrt( 1 - ( dduddv1.x * dduddv1.x ) - ( dduddv1.y * dduddv1.y ) ) * 0.5;
        vNormal = normalize( vNormal );
        
        // Add detail if requested
        if( bDetail )
        {
            float2 texDetail = float2( vIntersection.x, 1-vIntersection.y ) * g_DetailRepeat;
            float4 redgreen = g_txDetailGrad_RedGreen.SampleLevel( g_samWrap, texDetail, 0 );
            float4 bluealpha = g_txDetailGrad_BlueAlpha.SampleLevel( g_samWrap, texDetail, 0 );
        
            float2 dduddvDetail = redgreen.rg * mask.r + redgreen.ba * mask.g + bluealpha.rg * mask.b + bluealpha.ba * mask.a;
            float3 vNormal2;
            vNormal2.xz = ( dduddvDetail.xy - 0.5 ) * g_DetailMapSize * g_DetailRepeat * g_DetailHeight;
            vNormal2.y = 1 - g_DetailHeight;
            vNormal2.x *= -1;
            vNormal2 = normalize( vNormal2 );
        
            vNormal = normalize( vNormal + vNormal2 * 0.5 );
        }
        
        // Light
        float fLighting = saturate( dot( g_LightDir, vNormal ) );
        
        // Set color equal to white
        vColor = float4( 1, 1, 1, 1 );
        
        // Unless we're adding detail
        if( bDetail )
        {
            float2 texDetail = float2( vIntersection.x, vIntersection.y ) * g_DetailRepeat;
            float4 vColorDetail = g_txDetailDiffuse[0].SampleLevel( g_samWrap, texDetail, 0 ) * mask.r;
            vColorDetail +=       g_txDetailDiffuse[1].SampleLevel( g_samWrap, texDetail, 0 ) * mask.g;
            vColorDetail +=       g_txDetailDiffuse[2].SampleLevel( g_samWrap, texDetail, 0 ) * mask.b;
            vColorDetail +=       g_txDetailDiffuse[3].SampleLevel( g_samWrap, texDetail, 0 ) * mask.a;
            vColor = vColor * vColorDetail;
        }
        
        // Do we want shadows?
        float fShadow = 1;
        if( bShadow )
        {
            fShadow = CalculateShadowAmountRaycast( vIntersection.xzy );
        }
        
        vColor *= max( 0.1, fLighting * fShadow );
    }
    else
    {
        discard;
    }
    
    Output.color = vColor;
    
    // Convert the intersection point to a depth value
    float4 vProjPoint = mul( float4( vIntersection.xzy, 1 ), g_mTexToViewProj );
    Output.depth = vProjPoint.z / vProjPoint.w;
    
    return Output;
}



//--------------------------------------------------------------------------------------
// Renders scene to render target using D3D10 Techniques
//--------------------------------------------------------------------------------------
technique10 RenderTerrain_Inside
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, ShootRayVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, ShootRayPS( false, false, false, false ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderTerrain_Outside
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, ShootRayVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, ShootRayPS( true, false, false, false ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderTerrain_Ortho
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, ShootRayVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, ShootRayPS( true, false, false, true ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderTerrain_Inside_Detail
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, ShootRayVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, ShootRayPS( false, true, false, false ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderTerrain_Outside_Detail
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, ShootRayVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, ShootRayPS( true, true, false, false ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderTerrain_Ortho_Detail
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, ShootRayVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, ShootRayPS( true, true, false, true ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderTerrain_Inside_Shadow
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, ShootRayVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, ShootRayPS( false, false, true, false ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderTerrain_Outside_Shadow
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, ShootRayVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, ShootRayPS( true, false, true, false ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderTerrain_Ortho_Shadow
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, ShootRayVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, ShootRayPS( true, false, true, true ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderTerrain_Inside_Detail_Shadow
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, ShootRayVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, ShootRayPS( false, true, true, false ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderTerrain_Outside_Detail_Shadow
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, ShootRayVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, ShootRayPS( true, true, true, false ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderTerrain_Ortho_Detail_Shadow
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, ShootRayVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, ShootRayPS( true, true, true, false ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderGeometryTerrain
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, MeshVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, GeometryTerrainPS( false ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderGeometryTerrain_Shadow
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, MeshVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, GeometryTerrainPS( true ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderGeometryTerrain_Wire
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, MeshVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, GeometryTerrainPS( false ) ) );

        SetRasterizerState( Wireframe );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderMesh
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, MeshVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, MeshPS( false ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderMesh_Shadow
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, MeshVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, MeshPS( true ) ) );

        SetRasterizerState( Solid );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}

technique10 RenderTerrain_Wire
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, ShootRayVS( ) ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, RayTextureCoordsPS( ) ) );

        SetRasterizerState( Wireframe );
        SetBlendState( DisableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( EnableDepth, 0 );
    }
}