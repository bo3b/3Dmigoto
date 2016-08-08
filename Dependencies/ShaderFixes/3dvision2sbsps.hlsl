Texture2D<float4> StereoParams : register(t125);
Texture1D<float4> IniParams : register(t120);
Texture2D<float4> t100 : register(t100);

void main(float4 pos : SV_Position0, out float4 result : SV_Target0)
{
	float4 stereo = StereoParams.Load(0);
	float mode = IniParams.Load(int2(7, 0)).x;

	float x = pos.x;
	float y = pos.y;
	float width, height;
	float x1 = 0, y1 = 0;

	t100.GetDimensions(width, height);

	if (mode == 0) { // Regular 3D Vision
		if (stereo.z == 1)
			x += width / 2;
	} else if (mode == 1) { // Regular 3D Vision with eyes swapped
		if (stereo.z == -1)
			x += width / 2;
	} else if (mode == 2 || mode == 3) { // Side by side
		x *= 2;
		x1 = 1;
		if (mode == 3) { // Swap eyes
			x += width / 2 * (x >= width / 2 ? -1 : 1);
		}
	} else if (mode == 4 || mode == 5) { // Top and bottom
		y *= 2;
		y1 = 1;
		if (y >= height) {
			y -= height;
			if (mode == 4) {
				x += width / 2;
			}
		} else if (mode == 5) {
			x += width / 2;
		}
	}

	result = t100.Load(float3(x, y, 0));
	if (x1 || y1)
		result = (result + t100.Load(float3(x + x1, y + y1, 0))) / 2;
	result.w = 1;
}
