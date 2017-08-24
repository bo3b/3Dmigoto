Texture2D<float4> StereoParams : register(t125);
Texture1D<float4> IniParams : register(t120);

Texture2D<float4> cursor_mask : register(t100);
Texture2D<float4> cursor_color : register(t101);

// Draw a black and white, and possibly inverted cursor,
// e.g. use "Windows Standard", "Windows Inverted" or "Windows Black" to test
float4 draw_cursor_bw(float2 texcoord)
{
	uint mask_width, mask_height;
	float4 result;

	cursor_mask.GetDimensions(mask_width, mask_height);

	// Black and white cursor - "the upper half is the icon AND bitmask and
	// the lower half is the icon XOR bitmask".
	uint xor = cursor_mask.Load(float3(texcoord, 0)).x;
	uint and = cursor_mask.Load(float3(texcoord.x, texcoord.y + mask_height / 2, 0)).x;

	result.xyz = xor;
	result.w = and ^ xor;

	return result;
}

// Draw a colour cursor, e.g. use "Windows Default" to test
float4 draw_cursor_color(float2 texcoord)
{
	float4 result;
	float mask;

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

float4 draw_cursor(float2 texcoord)
{
	uint color_width, color_height;

	cursor_color.GetDimensions(color_width, color_height);

	if (color_width)
		return draw_cursor_color(texcoord);
	else
		return draw_cursor_bw(texcoord);
}

void main(float4 pos : SV_Position0, float2 texcoord : TEXCOORD0, out float4 result : SV_Target0)
{
	result = draw_cursor(texcoord);
}
