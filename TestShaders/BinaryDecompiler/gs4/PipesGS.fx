// ParticlesGS.fx
// Copyright (c) 2005 Microsoft Corporation. All rights reserved.
//

struct VSPipeIn
{
    float3 pos          : POSITION;
    float3 norm         : NORMAL;
    float3 dir          : DIRECTION;
    float2 timerNtype   : TIMERNTYPE;
    float3 targetdir    : TARGETDIR;
    uint currentFace    : CURRENTFACE;
    uint leaves         : LEAVES;
    float pipelife      : PIPELIFE;
};
struct VSPipeOut
{
    float3 pos          : OUT_POSITION;
    float3 norm         : OUT_NORMAL;
    float3 dir          : OUT_DIRECTION;
    float2 timerNtype   : OUT_TIMERNTYPE;
    float3 targetdir    : OUT_TARGETDIR;
    uint currentFace    : OUT_CURRENTFACE;
    uint leaves         : OUT_LEAVES;
    float pipelife      : OUT_PIPELIFE;
};

cbuffer cb0
{
    float4x4 g_mWorldViewProj;
    float g_fGlobalTime;
    float g_fUndulate;
    float4 vMaterialSpec;
    float4 vMaterialDiff;
};

cbuffer cbUIUpdates
{   
    float g_fLifeSpan;
    float g_fLifeSpanVar;
    float g_fRadiusMin;
    float g_fRadiusMax;
    float g_fGrowTime;
    float g_fStepSize;
    float g_fTurnRate;
    float g_fTurnSpeed;
    float g_fLeafRate;
    float g_fShrinkTime;
    uint g_uMaxFaces;
};


Texture1D g_txRandom;
SamplerState g_samPoint
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Wrap;
};

//
//Input Buffers
//
Buffer<float3> g_adjBuffer;
Buffer<float4> g_triCenterBuffer;

//
// Explanation of different pipe sections
//
#define PT_START    0 //Start section - this section finds a location on the mesh and puts a GROW there
#define PT_GROW     1 //Grow section - this section spawns a new STATIC and a new GROW to continue growing
#define PT_SHRINK   2 //Shrink section - this section shrinks
#define PT_STATIC   3 //Static section - this section has grown and will stay indefinitly

//
// Sample a random direction from our random texture
//
float3 RandomDir(float fOffset)
{
    float tCoord = (g_fGlobalTime + fOffset) / 1024.0;
    return g_txRandom.SampleLevel( g_samPoint, tCoord, 0 );
}

float3 FixDir( float3 dir, float3 currentNorm )
{
    float3 left = cross( dir, currentNorm );
    float3 newDir = cross( currentNorm, left );
    return normalize(newDir);
}


////////////////////////////////
// Crawling Pipe Functions
////////////////////////////////

void GSCrawlHandleStart(VSPipeIn input, inout PointStream<VSPipeOut> PointOutputStreamCrawl )
{   
    VSPipeOut output;
	output.pos = input.pos;
	output.norm = input.norm;
	output.dir = input.dir;
	output.timerNtype = input.timerNtype;
	output.targetdir = input.targetdir;
	output.currentFace = input.currentFace;
	output.leaves = input.leaves;
	output.pipelife = input.pipelife;
    
    if(input.timerNtype.x == 0)
    {
        output.pos = g_triCenterBuffer.Load( input.currentFace*2 );
        output.norm = g_triCenterBuffer.Load( input.currentFace*2 + 1);
        output.targetdir = output.pos;
        
        float3 newDir = normalize(RandomDir( input.currentFace ));
        output.dir = FixDir( newDir, output.norm );
        
        float3 rand = normalize(RandomDir( input.currentFace + 100 ));
        float LifeVar = 10.0*rand.x;
        output.timerNtype.x = g_fLifeSpan+1.0;
        output.timerNtype.y = PT_GROW;
        output.currentFace = input.currentFace;
        output.pipelife = -LifeVar; 
    }
    else
        output.timerNtype.x --;
        
    PointOutputStreamCrawl.Append(output);
}

void GSCrawlPicNewTarget( VSPipeIn input, inout PointStream<VSPipeOut> PointOutputStreamCrawl )
{
    VSPipeOut output;
	output.pos = input.pos;
	output.norm = input.norm;
	output.dir = input.dir;
	output.timerNtype = input.timerNtype;
	output.targetdir = input.targetdir;
	output.currentFace = input.currentFace;
	output.leaves = input.leaves;
	output.pipelife = input.pipelife;

    output.pos = output.targetdir;
    
    float neighbor1 = g_adjBuffer.Load( output.currentFace*3     );
    float neighbor2 = g_adjBuffer.Load( output.currentFace*3 + 1 );
    float neighbor3 = g_adjBuffer.Load( output.currentFace*3 + 2 );
    
    float3 center1 = g_triCenterBuffer.Load( neighbor1*2 );
    float3 normal1 = g_triCenterBuffer.Load( neighbor1*2 + 1 );
    float3 dir1 = center1 - output.pos;
    
    float3 center2 = g_triCenterBuffer.Load( neighbor2*2 );
    float3 normal2 = g_triCenterBuffer.Load( neighbor2*2 + 1 );
    float3 dir2 = center2 - output.pos;
    
    float3 center3 = g_triCenterBuffer.Load( neighbor3*2 );
    float3 normal3 = g_triCenterBuffer.Load( neighbor3*2 + 1 );
    float3 dir3 = center3 - output.pos;
    
    float3 normRand = normalize( RandomDir( float(input.currentFace) ) );
    if( abs(normRand.x) < g_fTurnRate )
    {
        output.dir = RandomDir( 15 );
    }
    
    float d1 = dot( output.dir, normalize(dir1) );
    float d2 = dot( output.dir, normalize(dir2) );
    float d3 = dot( output.dir, normalize(dir3) );
    
    if( neighbor1 < 40000000 && d1 > d2 && d1 > d2 )
    {
    
        output.dir = FixDir( output.dir, normal1 );
        output.norm = normal1;
        output.currentFace = neighbor1;
        output.targetdir = center1;
    }
    
    else if( neighbor2 < 40000000 && d2 > d1 && d2 > d3 )
    {
        output.dir = FixDir( output.dir, normal2 );
        output.norm = normal2;
        output.currentFace = neighbor2;
        output.targetdir = center2;
    }
    
    else
    {
        output.dir = FixDir( output.dir, normal3 );
        output.norm = normal3;
        output.currentFace = neighbor3;
        output.targetdir = center3;
    }
    
    PointOutputStreamCrawl.Append( output );
}

void GSCrawlHandleGrow(VSPipeIn input, inout PointStream<VSPipeOut> PointOutputStreamCrawl)
{   
    float fLen = length( input.pos - input.targetdir );
    if( fLen < g_fStepSize )
    {
        GSCrawlPicNewTarget( input, PointOutputStreamCrawl );
    }
    else
    {
		VSPipeOut output;
		output.pos = input.pos;
		output.norm = input.norm;
		output.dir = input.dir;
		output.timerNtype = input.timerNtype;
		output.targetdir = input.targetdir;
		output.currentFace = input.currentFace;
		output.leaves = input.leaves;
		output.pipelife = input.pipelife;

        float distToTarget = length( output.targetdir - output.pos );
        float3 uncorrectedPos = output.pos + output.dir*distToTarget;
        float3 delta = output.targetdir - uncorrectedPos;
        output.dir = (uncorrectedPos + 0.5*delta) - output.pos;
        output.dir = normalize( output.dir );
        output.pos += output.dir*g_fStepSize;
        output.norm = FixDir( output.norm, output.dir );
        output.timerNtype.x = g_fLifeSpan;
        output.leaves = 0;  
    
        float3 normRand = normalize( RandomDir( output.currentFace ) );
        if( abs(normRand.x) < g_fLeafRate )
            output.leaves = abs(normRand.y)*2000;
    
        if( output.pipelife > g_fLifeSpan )
        {
            output.timerNtype.x = 0;
            output.pipelife = 0;
            output.timerNtype.y = PT_START;
            output.currentFace = abs(normRand.z)*g_uMaxFaces;
        }
    
        PointOutputStreamCrawl.Append(output);
    }
}

//
// Main pipe crawling function
//
[maxvertexcount(2)]
void GSCrawlPipesMain(point VSPipeIn input[1], inout PointStream<VSPipeOut> PointOutputStreamCrawl)
{
    if( PT_START == input[0].timerNtype.y )
    {
        GSCrawlHandleStart( input[0], PointOutputStreamCrawl );
    }
    else
    {       
        if( PT_GROW == input[0].timerNtype.y )
            GSCrawlHandleGrow( input[0], PointOutputStreamCrawl );
            
        //emit us as a static
        VSPipeOut output;
		output.pos = input[0].pos;
		output.norm = input[0].norm;
		output.dir = input[0].dir;
		output.timerNtype = input[0].timerNtype;
		output.targetdir = input[0].targetdir;
		output.currentFace = input[0].currentFace;
		output.leaves = input[0].leaves;
		output.pipelife = input[0].pipelife;

        output.timerNtype.y = PT_STATIC;
        output.timerNtype.x -= 1;
        output.pipelife ++;

        if(0 != output.timerNtype.x && output.pipelife < g_fLifeSpan)
            PointOutputStreamCrawl.Append( output );
    }
}
