// This structured buffer is divided up into 8 sections of up to 512 entries
// each plus a special section at the end (at offset 0x4000).

// The final section contains 8 integers. When looking at the sky these are all
// 5, while when looking down at the city it's more like 99, 117, 135, 120, 83,
// 72, 77, 67.

// I believe that these 8 numbers are the lengths of the first 8 sections and
// represent the number of lights active in a part of the screen.

// The first 8 sections contain integers which at first glance appear to be
// *almost* sequential, but closer examination shows that some numbers are
// skipped. Only the top n numbers of each section should be considered -
// anything below that is stale data from previous frames.

// I believe that these numbers are indexes into the other structured buffers
// and correspond to specific lights in the scene.

// The 8 sections represent a larger grid on the screen, divided up like so
// (confirmed through experimentation):

// -------------------------
// |     |     |     |     |
// |  0  |  1  |  2  |  3  |
// |     |     |     |     |
// -------------------------
// |     |     |     |     |
// |  4  |  5  |  7  |  8  |
// |     |     |     |     |
// -------------------------

// Each of those large tiles will be broken up into 10 x 22 smaller tiles in
// this shader.

// The problem is that once we move lights into their stereo position they
// might now be in different tiles, which will cause them to clip along the
// edges between tiles because the new tile won't even consider drawing them.
// This buffer is populated by the CPU via a Map() call, so we can't fix it in
// a previous shader - instead we have to deal with the problem here.
//
// dcl_resource_structured t10, 4
StructuredBuffer<uint> LightsIn : register(t10);

// This structured buffer will replace the t10 buffer passed from the CPU - We
// add a new loop to the shader to merge lists from neighboring tiles together:
RWStructuredBuffer<uint> LightsOut : register(u2);


Texture2D<float4> StereoParams : register(t125);
Texture1D<float4> IniParams : register(t120);


  [numthreads(4, 2, 1)]
void main( uint3 tid : SV_DispatchThreadID )
{
  float4 stereo = StereoParams.Load(0);

  // x is this tile, y is the tile being merged (or current if just copying)
  // Left eye merges from the right, right eye merges from the left
  // XXX: There might still be some circumstances where we need to improve
  // this, e.g. small monitors where the large tiles are smaller than the max
  // separation, or when using large amounts of pop-out
  uint2 tile_x = uint2(tid.x, min(max(tid.x + stereo.z, 0), 3));

  // Tile IDs to copy from and to:
  uint2 tile_idx = tid.y * 4 + tile_x;

  // Offset of each tile's list of lights in this row:
  uint2 lights_list_off = tile_idx << 9;

  // Number of lights to copy to this tile from each source tile:
  uint2 num_lights_ptr = tile_idx + 4096;
  uint2 num_lights = 0;
  num_lights.x = LightsIn[num_lights_ptr.x];
  if (tile_x.x != tile_x.y)
    num_lights.y = LightsIn[num_lights_ptr.y];

  // Iterate over both lists merging them together:
  uint3 light_idx = 0;
  for (light_idx.z = 0; light_idx.z < 512; light_idx.z++) {
    // Stop once all source lists are exhausted:
    bool2 finished_lists = light_idx.xy >= num_lights;
    if (all(finished_lists))
      break;

    // Load the lights from the original buffers from unfinished source lists:
    uint3 cur_light_ptr = lights_list_off.xyx + light_idx.xyz;
    uint2 light_id = 0x7fffffff; // INT_MAX
    if (!finished_lists.x)
      light_id.x = LightsIn[cur_light_ptr.x];
    if (!finished_lists.y)
      light_id.y = LightsIn[cur_light_ptr.y];

    // Append the smaller light ID to the destination list:
    uint val = min(light_id.x, light_id.y);
    LightsOut[cur_light_ptr.z] = val;

    // Increment pointers on any source lists that match the minimum value:
    light_idx.xy += light_id == val;
  }

  // Write the new list length:
  LightsOut[num_lights_ptr.x] = light_idx.z;
}
