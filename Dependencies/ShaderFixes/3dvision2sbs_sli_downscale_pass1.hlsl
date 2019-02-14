Texture2D<float4> StereoParams : register(t125);
Texture1D<float4> IniParams : register(t120);
Texture2D<float4> t100 : register(t100);

#define mode IniParams[0].x

void main(float4 pos : SV_Position0, out float4 result : SV_Target0)
{
	int x = pos.x;
	int y = pos.y;

	if (mode >= 4) {
		// TAB or Line Interlaced
		y *= 2;
		result = (t100.Load(int3(x, y    , 0)) +
		          t100.Load(int3(x, y + 1, 0))) / 2;
	} else {
		// SBS
		x *= 2;
		result = (t100.Load(int3(x    , y, 0)) +
		          t100.Load(int3(x + 1, y, 0))) / 2;
	}
	result.w = 1;
}
