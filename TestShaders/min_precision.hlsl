// Compile with fxc /T ps_5_0 /Fo min_precision.bin

// HLSL scalar types:
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb509646(v=vs.85).aspx

// HLSL Vector types:
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb509707(v=vs.85).aspx

// NOTE: New types in Windows 8
// Declarations are the same, load instructions differ:
Texture2D<min16float>   min16float_tex : register(t39);
Texture2D<min10float>   min10float_tex : register(t40);
Texture2D<min16int>     min16int_tex   : register(t41);
Texture2D<min12int>     min12int_tex   : register(t42);
Texture2D<min16uint>    min16uint_tex  : register(t43);

void main(out float4 output : SV_Target0)
{
	output = 0;

	// Use all textures to ensure the compiler doesn't optimise them out,
	// and to see what the load instructions look like:

	// Requires Windows 8:
	output += min16float_tex.Load(0);
	output += min10float_tex.Load(0);
	output += min16int_tex.Load(0);
	output += min12int_tex.Load(0);
	output += min16uint_tex.Load(0);
}
