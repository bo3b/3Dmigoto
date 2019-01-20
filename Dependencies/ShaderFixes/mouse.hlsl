#define cursor_window   IniParams[0].xy
#define cursor_hotspot  IniParams[0].zw
#define window_size     IniParams[1].xy
#define cursor_showing  IniParams[1].z
#define cursor_pass     IniParams[1].w

Texture2D<float4> StereoParams : register(t125);
Texture1D<float4> IniParams : register(t120);

Texture2D<float4> cursor_mask : register(t100);
Texture2D<float4> cursor_color : register(t101);

struct vs2ps {
	float4 pos : SV_Position0;
	float2 texcoord : TEXCOORD0;
};

#ifdef VERTEX_SHADER
void main(out vs2ps output, uint vertex : SV_VertexID)
{
	uint mask_width, mask_height;
	uint color_width, color_height;
	float2 cursor_size;

	// For easy bailing:
	output.pos = 0;
	output.texcoord = 0;

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

	output.pos.xy = cursor_window - cursor_hotspot;

	// Not using vertex buffers so manufacture our own coordinates.
	switch(vertex) {
		case 0:
			output.texcoord = float2(0, cursor_size.y);
			break;
		case 1:
			output.texcoord = float2(0, 0);
			output.pos.y += cursor_size.y;
			break;
		case 2:
			output.texcoord = float2(cursor_size.x, cursor_size.y);
			output.pos.x += cursor_size.x;
			break;
		case 3:
			output.texcoord = float2(cursor_size.x, 0);
			output.pos.xy += cursor_size;
			break;
		default:
			output.pos.xy = 0;
			break;
	};

	// Scale from pixels to clip space:
	output.pos.xy = (output.pos.xy / window_size * 2 - 1) * float2(1, -1);
	output.pos.zw = float2(0, 1);

	// Adjust stereo depth of pos here using whatever means you feel is
	// suitable for this game, e.g. with a suitable crosshair.hlsl you
	// could automatically adjust it from the depth buffer:
	//float2 mouse_pos = (cursor_window / window_size * 2 - 1);
	//output.pos.x += adjust_from_depth_buffer(mouse_pos.x, mouse_pos.y);
}
#endif /* VERTEX_SHADER */

#ifdef PIXEL_SHADER
// Draw a black and white, and possibly inverted cursor,
// e.g. use "Windows Standard", "Windows Inverted" or "Windows Black" to test
float4 draw_cursor_bw(float2 texcoord, float2 dimensions)
{
	float4 result;

	if (any(texcoord < 0 || texcoord >= dimensions))
		return float4(0, 0, 0, 1);

	// Black and white cursor - "the upper half is the icon AND bitmask and
	// the lower half is the icon XOR bitmask".
	uint xor = cursor_mask.Load(float3(texcoord, 0)).x;
	uint and = cursor_mask.Load(float3(texcoord.x, texcoord.y + dimensions.y, 0)).x;

	result.xyz = xor;
	result.w = and ^ xor;

	return result;
}

// Draw a colour cursor, e.g. use "Windows Default" to test
float4 draw_cursor_color(float2 texcoord, float2 dimensions)
{
	float4 result;
	float mask;

	if (any(texcoord < 0 || texcoord >= dimensions))
		return 0;

	result = cursor_color.Load(float3(texcoord, 0));
	mask = cursor_mask.Load(float3(texcoord, 0)).x;

	// We may or may not have an alpha channel in the color bitmap, but
	// as far as I can tell Windows doesn't expose an API to check if the
	// cursor has an alpha channel or not, so we have no good way to know
	// (32bpp does not imply alpha). People on stackoverflow are scanning
	// the entire alpha channel looking for non-zero values to fudge this.
	//
	// If the alpha is 0 it may either mean there is an alpha channel and
	// this pixel should be fully transparent, or that there is no alpha
	// channel and this pixel should only use the AND mask for alpha. Let's
	// assume that alpha=0 means no alpha channel, which should work
	// provided the mask blanks out those pixels as well.
	//
	// If later we find a way to detect this in 3DMigoto we can use
	// Format=DXGI_FORMAT_B8G8R8X8_UNORM_SRGB to indicate there is no alpha
	// channel, which will cause the read here to return 1 for opaque.

	if (!result.w)
		result.w = 1;

	if (mask)
		result.w = 0;

	return result;
}

float4 smooth_cursor_bw(float2 texcoord)
{
	uint mask_width, mask_height;
	float4 px1, px2, px3, px4, tmp1, tmp2;
	float2 coord1, coord2;

	cursor_mask.GetDimensions(mask_width, mask_height);
	float2 dimensions = float2(mask_width, mask_height / 2);

	// Subtracting 0.5 here to sample at the edge of each pixel instead of
	// the center - when not scaling the cursor this will make it just as
	// sharp as if we hadn't applied an interpolation filter at all, but
	// still gives us a smooth result when we are scaling. Note that this
	// can make some of the sample locations negative, which we handle in
	// draw_cursor_bw:
	texcoord -= 0.5;

	coord1 = floor(texcoord);
	coord2 = min(ceil(texcoord), dimensions - 1);

	// Because of the special handling we have to do with the mask, we
	// can't use a SamplerState to interpolate the cursor image, so
	// implement a simple linear filter with loads instead:
	px1 = draw_cursor_bw(coord1, dimensions);
	px2 = draw_cursor_bw(float2(coord2.x, coord1.y), dimensions);
	px3 = draw_cursor_bw(float2(coord1.x, coord2.y), dimensions);
	px4 = draw_cursor_bw(coord2, dimensions);

	tmp1 = lerp(px1, px2, frac(texcoord.x));
	tmp2 = lerp(px3, px4, frac(texcoord.x));
	return lerp(tmp1, tmp2, frac(texcoord.y));
}

float4 smooth_cursor_color(float2 texcoord, float2 dimensions)
{
	float4 px1, px2, px3, px4, tmp1, tmp2;
	float2 coord1, coord2;

	// Subtracting 0.5 here to sample at the edge of each pixel instead of
	// the center - when not scaling the cursor this will make it just as
	// sharp as if we hadn't applied an interpolation filter at all, but
	// still gives us a smooth result when we are scaling. Note that this
	// can make some of the sample locations negative, which we handle in
	// draw_cursor_color:
	texcoord -= 0.5;

	coord1 = floor(texcoord);
	coord2 = min(ceil(texcoord), dimensions - 1);

	// Because of the special handling we have to do with the mask, we
	// can't use a SamplerState to interpolate the cursor image, so
	// implement a simple linear filter with loads instead:
	px1 = draw_cursor_color(coord1, dimensions);
	px2 = draw_cursor_color(float2(coord2.x, coord1.y), dimensions);
	px3 = draw_cursor_color(float2(coord1.x, coord2.y), dimensions);
	px4 = draw_cursor_color(coord2, dimensions);

	tmp1 = lerp(px1, px2, frac(texcoord.x));
	tmp2 = lerp(px3, px4, frac(texcoord.x));
	return lerp(tmp1, tmp2, frac(texcoord.y));
}

float4 draw_cursor(float2 texcoord)
{
	uint color_width, color_height;

	cursor_color.GetDimensions(color_width, color_height);

	if (color_width)
		return smooth_cursor_color(texcoord, float2(color_width, color_height));
	else
		return smooth_cursor_bw(texcoord);
}

void main(vs2ps input, out float4 result : SV_Target0)
{
	result = draw_cursor(input.texcoord);
}
#endif /* PIXEL_SHADER */
