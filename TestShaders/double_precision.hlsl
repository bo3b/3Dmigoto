// Compile with fxc /T ps_5_0 /Fo double_precision.bin

// HLSL scalar types:
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb509646(v=vs.85).aspx

// HLSL Vector types:
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb509707(v=vs.85).aspx

// No double4 - too large for a Texture2D (maybe a structured buffer?):
Texture2D<double>       double_tex  : register(t36);
Texture2D<double1>      double1_tex : register(t37);
Texture2D<double2>      double2_tex : register(t38);

Texture2D<float>        float_tex   : register(t0);
Texture2D<uint>         uint_tex    : register(t5);
Texture2D<int>          sint_tex    : register(t10);

void main(uint2 in_test : TEXCOORD0, out float4 output : SV_Target0)
{
	double val = asdouble(in_test.x, in_test.y);

	// Excercises dtof:
	val += (float)double_tex.Load(0);

	val += double1_tex.Load(0);
	val += double2_tex.Load(0).x;

	// Excercises dadd:
	val += double2_tex.Load(0).x + double_tex.Load(0);

	// Excercises ftod:
	val += (double)float_tex.Load(0) + double_tex.Load(0);

	// Excercises dmul:
	val += double2_tex.Load(1).x * double_tex.Load(1);

	// Excercises ddiv and globalFlags enable11_1DoubleExtensions:
	val += double2_tex.Load(2).x / double_tex.Load(2);

	// Excercises drcp:
	val += rcp(double_tex.Load(3));

	// Excercises dmax:
	val += max(double2_tex.Load(4).x, double_tex.Load(4));

	// Excercises dmin:
	val += min(double2_tex.Load(5).x, double_tex.Load(5));

	// Excercises deq:
	val += double2_tex.Load(6).x == double_tex.Load(6);

	// Excercises dne:
	val += double2_tex.Load(7).x != double_tex.Load(7);

	// Excercises dge:
	val += double2_tex.Load(7).x >= double_tex.Load(7);

	// Excercises dlt:
	val += double2_tex.Load(8).x < double_tex.Load(8);

	// Excercises dfma:
	val += fma(double2_tex.Load(9).x, double2_tex.Load(9).y, double_tex.Load(9));

	// Excercises dmovc:
	val += double2_tex.Load(10).x ? double2_tex.Load(10).y : double_tex.Load(10);

	// Excercises itod:
	val += sint_tex.Load(0);

	// Excercises dtoi:
	val += (int)double_tex.Load(11);

	// Excercises utod:
	val += uint_tex.Load(0);

	// Excercises dtou:
	val += (uint)double_tex.Load(11);


	// Excercises double literal strings (XXX: UNSUPPORTED):
	// val += mad(0.5l, double2_tex.Load(9).x, 0.5l);

	// TODO: Excercise dmov, dfma, literal double strings

	output = (float)val;
}
