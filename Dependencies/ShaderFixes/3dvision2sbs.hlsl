Texture2D<float4> StereoParams : register(t125);
Texture1D<float4> IniParams : register(t120);

#define mode IniParams[0].x

#ifdef VERTEX_SHADER
void main(
		out float4 pos : SV_Position0,
		uint vertex : SV_VertexID)
{
	float4 stereo = StereoParams.Load(0);

	// Not using vertex buffers so manufacture our own coordinates.
	switch(vertex) {
		case 0:
			pos.xy = float2(-1, -1);
			break;
		case 1:
			pos.xy = float2(-1, 1);
			break;
		case 2:
			pos.xy = float2(1, -1);
			break;
		case 3:
			pos.xy = float2(1, 1);
			break;
		default:
			pos.xy = 0;
			break;
	};
	pos.zw = float2(0, 1);
}
#endif /* VERTEX_SHADER */

#ifdef PIXEL_SHADER
Texture2D<float4> t100 : register(t100);

void main(float4 pos : SV_Position0, out float4 result : SV_Target0)
{
	float4 stereo = StereoParams.Load(0);

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
		x = int(x);
		x *= 2;
		x1 = 1;
		if (mode == 3) { // Swap eyes
			x += width / 2 * (x >= width / 2 ? -1 : 1);
		}
	} else if (mode == 4 || mode == 5) { // Top and bottom
		y = int(y);
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
	} else if (mode == 6 || mode == 7) {
		int side = y - (int)floor(y /2.0) * (int)2; // chooses the side for sampling if y is even side is always 0, else it is always 1
		if (mode == 6) {
			if (side == 0) { // left side of the reverse blited image
				y1 = 1;
			} else {  // right side of the reverse blited image
				y1 = -1;
				x = x + width / 2;
			}
		}
		else if (mode == 7) { // swap eyes
			if (side == 0) { // right side of the reverse blited image
				y1 = 1;
				x = x + width / 2;
			} else {  // left side of the reverse blited image
				y1 = -1;
			}
		}
	}

	result = t100.Load(float3(x, y, 0));
	if (x1 || y1)
		result = (result + t100.Load(float3(x + x1, y + y1, 0))) / 2;
	result.w = 1;
}
#endif /* PIXEL_SHADER */
