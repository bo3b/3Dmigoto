#define cursor_pass     IniParams[5].w
#define cursor_window   IniParams[6].xy
#define cursor_hotspot  IniParams[6].zw
#define cursor_showing  IniParams[7].y
#define window_size     IniParams[7].zw

Texture2D<float4> StereoParams : register(t125);
Texture1D<float4> IniParams : register(t120);

Texture2D<float4> cursor_mask : register(t100);
Texture2D<float4> cursor_color : register(t101);

void main(
		out float4 pos : SV_Position0,
		out float2 texcoord : TEXCOORD0,
		uint vertex : SV_VertexID)
{
	uint mask_width, mask_height;
	uint color_width, color_height;
	float2 cursor_size;

	// For easy bailing:
	pos = 0;
	texcoord = 0;

	if (!cursor_showing)
		return;

	cursor_color.GetDimensions(color_width, color_height);
	cursor_mask.GetDimensions(mask_width, mask_height);

	if (color_width) {
		// Colour cursor, bail if we are in the black and white / inverted cursor pass:
		if (cursor_pass == 2)
			return;
		cursor_size = float2(color_width, color_height);
	} else {
		// Black and white / inverted cursor, bail if we are in the colour cursor pass:
		if (cursor_pass == 1)
			return;
		cursor_size = float2(mask_width, mask_height / 2);
	}

	pos.xy = cursor_window - cursor_hotspot;

	// Not using vertex buffers so manufacture our own coordinates.
	switch(vertex) {
		case 0:
			texcoord = float2(0, cursor_size.y);
			break;
		case 1:
			texcoord = float2(0, 0);
			pos.y += cursor_size.y;
			break;
		case 2:
			texcoord = float2(cursor_size.x, cursor_size.y);
			pos.x += cursor_size.x;
			break;
		case 3:
			texcoord = float2(cursor_size.x, 0);
			pos.xy += cursor_size;
			break;
		default:
			pos.xy = 0;
			break;
	};

	// Scale from pixels to clip space:
	pos.xy = (pos.xy / window_size * 2 - 1) * float2(1, -1);
	pos.zw = float2(0, 1);

	// Adjust stereo depth of pos here using whatever means you feel is
	// suitable for this game, e.g. with a suitable crosshair.hlsl you
	// could automatically adjust it from the depth buffer:
	//float2 mouse_pos = (cursor_window / window_size * 2 - 1);
	//pos.x += adjust_from_depth_buffer(mouse_pos.x, mouse_pos.y);
}
