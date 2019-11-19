
//From http://www.nvidia.com/content/GTC-2010/pdfs/2260_GTC2010.pdf

RWStructuredBuffer<float> g_data;
#define GROUP_DIM_X 128
groupshared float sharedData[GROUP_DIM_X];

[numthreads( GROUP_DIM_X, 1, 1)]
void main( uint3 threadIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID)
{ 
   // each thread loads one element from global to shared mem
   unsigned int tid = threadIdx.x;
   unsigned int i = groupIdx.x*GROUP_DIM_X + threadIdx.x;
   sharedData[tid] = g_data[i];
   GroupMemoryBarrierWithGroupSync();

   // do reduction in shared mem
   for(unsigned int s=1; s < GROUP_DIM_X; s *= 2) {
      if (tid % (2*s) == 0) {
         sharedData[tid] += sharedData[tid + s];
      }
      GroupMemoryBarrierWithGroupSync();
   }

   // write result for this block to global mem
   if (tid == 0)
      g_data[groupIdx.x] = sharedData[0];
}
