// Compile with fxc /T ps_5_0 /Fo resource_types.bin

// HLSL scalar types:
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb509646(v=vs.85).aspx

// HLSL Vector types:
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb509707(v=vs.85).aspx

Texture2D<float>        float_tex   : register(t0); // Same as float1
Texture2D<float1>       float1_tex  : register(t1);
Texture2D<float2>       float2_tex  : register(t2);
Texture2D<float3>       float3_tex  : register(t3);
Texture2D<float4>       float4_tex  : register(t4);

Texture2D<uint>         uint_tex    : register(t5);
Texture2D<uint1>        uint1_tex   : register(t6);
Texture2D<uint2>        uint2_tex   : register(t7);
Texture2D<uint3>        uint3_tex   : register(t8);
Texture2D<uint4>        uint4_tex   : register(t9);

Texture2D<int>          sint_tex    : register(t10);
Texture2D<int1>         sint1_tex   : register(t11);
Texture2D<int2>         sint2_tex   : register(t12);
Texture2D<int3>         sint3_tex   : register(t13);
Texture2D<int4>         sint4_tex   : register(t14);

Texture2D<unorm float>  unorm_tex   : register(t15);
Texture2D<unorm float1> unorm1_tex  : register(t16);
Texture2D<unorm float2> unorm2_tex  : register(t17);
Texture2D<unorm float3> unorm3_tex  : register(t18);
Texture2D<unorm float4> unorm4_tex  : register(t19);

Texture2D<snorm float>  snorm_tex   : register(t20);
Texture2D<snorm float1> snorm1_tex  : register(t21);
Texture2D<snorm float2> snorm2_tex  : register(t22);
Texture2D<snorm float3> snorm3_tex  : register(t23);
Texture2D<snorm float4> snorm4_tex  : register(t24);

// bool becomes uint
Texture2D<bool>         bool_tex    : register(t25);
Texture2D<bool1>        bool1_tex   : register(t26);
Texture2D<bool2>        bool2_tex   : register(t27);
Texture2D<bool3>        bool3_tex   : register(t28);
Texture2D<bool4>        bool4_tex   : register(t29);

// dword becomes uint
// No dword1-4:
Texture2D<dword>        dword_tex   : register(t30);

// half is for language compatibility only - will become regular float:
Texture2D<half>         half_tex    : register(t31);
Texture2D<half1>        half1_tex   : register(t32);
Texture2D<half2>        half2_tex   : register(t33);
Texture2D<half3>        half3_tex   : register(t34);
Texture2D<half4>        half4_tex   : register(t35);

// No double4 - too large for a Texture2D (maybe a structured buffer?):
Texture2D<double>       double_tex  : register(t36);
Texture2D<double1>      double1_tex : register(t37);
Texture2D<double2>      double2_tex : register(t38);

// NOTE: New types in Windows 8
// Declarations are the same, load instructions differ:
Texture2D<min16float>   min16float_tex : register(t39);
Texture2D<min10float>   min10float_tex : register(t40);
Texture2D<min16int>     min16int_tex   : register(t41);
Texture2D<min12int>     min12int_tex   : register(t42);
Texture2D<min16uint>    min16uint_tex  : register(t43);

typedef vector<int, 1> v1;
Texture2D<v1> vector_tex : register(t44);

// XXX: Is it possible to have mixed types in anything other than a structured
// buffer?

void main(out float4 output : SV_Target0)
{
	output = 0;

	// Use all textures to ensure the compiler doesn't optimise them out,
	// and to see what the load instructions look like:

	// All become float4 textures in the bytecode, only the signature
	// sections differentiate these:
	output += float_tex.Load(0);
	output += float1_tex.Load(0);
	output += float4(float2_tex.Load(0), 0, 0);
	output += float4(float3_tex.Load(0), 0);
	output += float4_tex.Load(0);

	output += uint_tex.Load(0);
	output += uint1_tex.Load(0);
	output += float4(uint2_tex.Load(0), 0, 0);
	output += float4(uint3_tex.Load(0), 0);
	output += uint4_tex.Load(0);

	output += sint_tex.Load(0);
	output += sint1_tex.Load(0);
	output += float4(sint2_tex.Load(0), 0, 0);
	output += float4(sint3_tex.Load(0), 0);
	output += sint4_tex.Load(0);

	output += unorm_tex.Load(0);
	output += unorm1_tex.Load(0);
	output += float4(unorm2_tex.Load(0), 0, 0);
	output += float4(unorm3_tex.Load(0), 0);
	output += unorm4_tex.Load(0);

	output += snorm_tex.Load(0);
	output += snorm1_tex.Load(0);
	output += float4(snorm2_tex.Load(0), 0, 0);
	output += float4(snorm3_tex.Load(0), 0);
	output += snorm4_tex.Load(0);

	output += bool_tex.Load(0);
	output += bool1_tex.Load(0);
	output += float4(bool2_tex.Load(0), 0, 0);
	output += float4(bool3_tex.Load(0), 0);
	output += bool4_tex.Load(0);

	output += dword_tex.Load(0);

	output += half_tex.Load(0);
	output += half1_tex.Load(0);
	output += float4(half2_tex.Load(0), 0, 0);
	output += float4(half3_tex.Load(0), 0);
	output += half4_tex.Load(0);

	output += (float)double_tex.Load(0);
	output += (float)double1_tex.Load(0);
	output += float4(double2_tex.Load(0), 0, 0);

	// Requires Windows 8:
	output += min16float_tex.Load(0);
	output += min10float_tex.Load(0);
	output += min16int_tex.Load(0);
	output += min12int_tex.Load(0);
	output += min16uint_tex.Load(0);

	output += vector_tex.Load(0);
}
