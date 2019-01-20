Texture2D<float4> StereoParams : register(t125);
Texture1D<float4> IniParams : register(t120);
Texture2D<float4> t100 : register(t100);

#define mode IniParams[0].x

void main(float4 pos : SV_Position0, out float4 result : SV_Target0)
{
	float4 stereo = StereoParams.Load(0);

	float x = pos.x;
	float y = pos.y;
	float width, height;
	float x1 = 0, y1 = 0;

	t100.GetDimensions(width, height);

	if (mode == 2 || mode == 3) { // Side by side
		if (mode == 3) { // Swap eyes
			x += width / 2 * (x >= width / 2 ? -1 : 1);
		}
	} else if (mode == 4 || mode == 5) { // Top and bottom
		if (y >= height) {
			y -= height;
			if (mode == 4) {
				x += width / 2;
			}
		} else if (mode == 5) {
			x += width / 2;
		}
	} else if (mode == 6 || mode == 7) {
		int side = y - (int)floor(y /2.0) * (int)2; // chooses the side for sampling if y is even side is always 0, else it is always 1
		y /= 2;
		if (mode == 6) {
			if (side == 0) { // left side of the reverse blited image
			} else {  // right side of the reverse blited image
				x = x + width / 2;
			}
		}
		else if (mode == 7) { // swap eyes
			if (side == 0) { // right side of the reverse blited image
				x = x + width / 2;
			} else {  // left side of the reverse blited image
			}
		}
	}

	result = t100.Load(float3(x, y, 0));
	result.w = 1;
}
