#ifndef COLOR_H
#define COLOR_H
/*
Szymon Rusinkiewicz
Princeton University

Color.h
Random class for encapsulating colors...

Due to all the possible colorspace variants, here's the documentation of
what is actually implemented for the conversions:
 - CIE 1931 2-degree observer
 - ITU-R BT.709 primaries
 - D65 illuminant (5nm tabulated - not the rounded version in Rec. 709)
 - CIELAB uses a linear adaptation from D65 to equal-intensity white so that
    1,1,1 in RGB maps to 100,0,0 in CIELAB
 - "RGB" means linearly-scaled RGB with the above illuminant and primaries
 - RGB-sRGB conversion uses the full part-linear, part-power-law function
 - HSV is single-hexcone, sRGB
 - Y'CbCr is JFIF-standard (Rec. 601 scaling, full excursion) starting from sRGB
 - Range of [0..1] for all spaces except CIELAB and hue in HSV
*/

#include "Vec.h"
#ifndef M_TWOPIf
# define M_TWOPIf 6.2831855f
#endif


namespace trimesh {

class Color : public Vec<3,float> {
public:
	Color()
		{}
	Color(const Vec<3,float> &v_) : Vec<3,float>(v_)
		{}
	Color(const Vec<3,double> &v_) : Vec<3,float>((float)v_[0], (float)v_[1], (float)v_[2])
		{}
	Color(float r, float g, float b) : Vec<3,float>(r,g,b)
		{}
	Color(double r, double g, double b) : Vec<3,float>((float)r, (float)g, (float)b)
		{}
	explicit Color(const float *rgb) : Vec<3,float>(rgb[0], rgb[1], rgb[2])
		{}
	explicit Color(const double *rgb) : Vec<3,float>((float)rgb[0], (float)rgb[1], (float)rgb[2])
		{}

	// Implicit conversion from float would be bad, so we have an
	// explicit constructor and an assignment statement.
	explicit Color(float c) : Vec<3,float>(c,c,c)
		{}
	explicit Color(double c) : Vec<3,float>((float)c, (float)c, (float)c)
		{}
	Color &operator = (float c)
		{ return *this = Color(c); }
	Color &operator = (double c)
		{ return *this = Color(c); }

	// Assigning from ints divides by 255
	Color(int r, int g, int b)
	{
		const float mult = 1.0f / 255.0f;
		*this = Color(mult*r, mult*g, mult*b);
	}
	explicit Color(const int *rgb)
		{ *this = Color(rgb[0], rgb[1], rgb[2]); }
	explicit Color(const unsigned char *rgb)
		{ *this = Color(rgb[0], rgb[1], rgb[2]); }
	explicit Color(int c)
		{ *this = Color(c,c,c); }
	Color &operator = (int c)
		{ return *this = Color(c); }

	// Named colors
	static Color black()
		{ return Color(0.0f, 0.0f, 0.0f); }
	static Color white()
		{ return Color(1.0f, 1.0f, 1.0f); }
	static Color red()
		{ return Color(1.0f, 0.0f, 0.0f); }
	static Color green()
		{ return Color(0.0f, 1.0f, 0.0f); }
	static Color blue()
		{ return Color(0.0f, 0.0f, 1.0f); }
	static Color yellow()
		{ return Color(1.0f, 1.0f, 0.0f); }
	static Color cyan()
		{ return Color(0.0f, 1.0f, 1.0f); }
	static Color magenta()
		{ return Color(1.0f, 0.0f, 1.0f); }
	static Color orange()
		{ return Color(238, 127, 45); }  // Princeton orange

	// 3x3 color transform - matrix given in *row-major* order
	const Color col_transform(float m11, float m12, float m13,
				  float m21, float m22, float m23,
				  float m31, float m32, float m33) const
	{
		return Color(m11*v[0]+m12*v[1]+m13*v[2],
			     m21*v[0]+m22*v[1]+m23*v[2],
			     m31*v[0]+m32*v[1]+m33*v[2]);
	}

private:
	const Color hsv2srgb() const
	{
		using namespace ::std;

		// From FvDFH
		float H = v[0], S = v[1], V = v[2];
		if (S <= 0.0f)
			return Color(V,V,V);
		H = fmod(H, M_TWOPIf);
		if (H < 0.0f)
			H += M_TWOPIf;
		H *= 6.0f / M_TWOPIf;
		int i = int(floor(H));
		float f = H - i;
		float p = V * (1.0f - S);
		float q = V * (1.0f - (S*f));
		float t = V * (1.0f - (S*(1.0f-f)));
		switch(i) {
			case 0: return Color(V, t, p);
			case 1: return Color(q, V, p);
			case 2: return Color(p, V, t);
			case 3: return Color(p, q, V);
			case 4: return Color(t, p, V);
			default: return Color(V, p, q);
		}
	}
	const Color srgb2hsv() const
	{
		using ::std::min;
		using ::std::max;

		float V = max(max(v[0], v[1]), v[2]);
		float diff = V - min(min(v[0], v[1]), v[2]);
		float S = diff / V;
		float H = 0.0f;
		if (S == 0.0f)
			return Color(H, S, V);
		if (V == v[0])
			H = (v[1] - v[2]) / diff;
		else if (V == v[1])
			H = (v[2] - v[0]) / diff + 2.0f;
		else
			H = (v[0] - v[1]) / diff + 4.0f;
		H *= M_TWOPIf / 6.0f;
		if (H < 0.0f)
			H += M_TWOPIf;
		return Color(H, S, V);
	}

	static inline float cielab_nonlinearity(float x)
	{
		using namespace ::std;

		if (x > 216.0f / 24389.0f)
			return (float) cbrt(x);
		else
			return 4.0f / 29.0f + (841.0f / 108.0f) * x;
	}
	static inline float inv_cielab_nonlinearity(float x)
	{
		if (x > (6.0f / 29.0f))
			return cube(x);
		else
			return (x - 4.0f / 29.0f) * (108.0f / 841.0f);
	}
	const Color xyz2cielab() const
	{
		float fx = cielab_nonlinearity(v[0] * (1.0f / 0.95042966f));
		float fy = cielab_nonlinearity(v[1]);
		float fz = cielab_nonlinearity(v[2] * (1.0f / 1.08880057f));
		return Color(116.0f * fy - 16.0f,
			     500.0f * (fx - fy),
			     200.0f * (fy - fz));
	}
	const Color cielab2xyz() const
	{
		float fy = (v[0] + 16.0f) * (1.0f / 116.0f);
		float fx = fy + v[1] * 0.002f;
		float fz = fy - v[2] * 0.005f;
		return Color(0.95042966f * inv_cielab_nonlinearity(fx),
			     inv_cielab_nonlinearity(fy),
			     1.08880057f * inv_cielab_nonlinearity(fz));
	}

	const Color xyz2rgb() const
	{
		return col_transform(3.24083023f, -1.53731690f, -0.49858927f,
 				    -0.96922932f,  1.87593979f,  0.04155444f,
				     0.05564529f, -0.20403272f,  1.05726046f);
	}
	const Color rgb2xyz() const
	{
		return col_transform(0.41240858f, 0.35758962f, 0.18043146f,
				     0.21264817f, 0.71517924f, 0.07217259f,
				     0.01933165f, 0.11919654f, 0.95027238f);
	}

	static inline float srgb_nonlinearity(float x)
	{
		using namespace ::std;

		if (x > 0.0031308f)
			return 1.055f * pow(x, 1.0f/2.4f) - 0.055f;
		else
			return x * 12.92f;
	}
	static inline float inv_srgb_nonlinearity(float x)
	{
		using namespace ::std;

		if (x > (0.0031308f * 12.92f))
			return pow((x + 0.055f) * (1.0f / 1.055f), 2.4f);
		else
			return x * (1.0f / 12.92f);
	}
	const Color rgb2srgb() const
	{
		return Color(srgb_nonlinearity(v[0]),
			     srgb_nonlinearity(v[1]),
			     srgb_nonlinearity(v[2]));
	}
	const Color srgb2rgb() const
	{
		return Color(inv_srgb_nonlinearity(v[0]),
			     inv_srgb_nonlinearity(v[1]),
			     inv_srgb_nonlinearity(v[2]));
	}

	const Color srgb2ycbcr() const
	{
		return Color(0.0f, 0.5f, 0.5f) + col_transform(
			0.299f, 0.587f, 0.114f,
			-0.16873589f, -0.33126411f, 0.5f,
			0.5f, -0.41868759f, -0.08131241f);
	}
	const Color ycbcr2srgb() const
	{
		return Color(v[0], v[1] - 0.5f, v[2] - 0.5f).col_transform(
			     1.0f, 0.0f, 1.402f,
			     1.0f, -0.34413629f, -0.71413629f,
			     1.0f, 1.772f, 0.0f);
	}

public:
	enum Colorspace { CIELAB, XYZ, RGB, SRGB, YCBCR, HSV };
	const Color convert(Colorspace src, Colorspace dst) const
	{
		if (src == dst)
			return Color(*this);
		if (src == HSV)
			return Color::hsv(v[0],v[1],v[2]).convert(SRGB, dst);
		else if (dst == HSV)
			return convert(src, SRGB).srgb2hsv();
		// Else we have a natural order in which to convert things
		int srcnum = int(src), dstnum = int(dst);
		if (srcnum < dstnum) switch (src) {
			case CIELAB:
				return (dst == XYZ) ? cielab2xyz() :
					cielab2xyz().convert(XYZ, dst);
			case XYZ:
				return (dst == RGB) ? xyz2rgb() :
					xyz2rgb().convert(RGB, dst);
			case RGB:
				return (dst == SRGB) ? rgb2srgb() :
					rgb2srgb().convert(SRGB, dst);
			default:
				return srgb2ycbcr();
		} else switch (src) {
			case YCBCR:
				return (dst == SRGB) ? ycbcr2srgb() :
					ycbcr2srgb().convert(SRGB, dst);
			case SRGB:
				return (dst == RGB) ? srgb2rgb() :
					srgb2rgb().convert(RGB, dst);
			case RGB:
				return (dst == XYZ) ? rgb2xyz() :
					rgb2xyz().convert(XYZ, dst);
			default:
				return xyz2cielab();
		}
	}

	// Linear to nonlinear - raises values to the power of 1/g
	const Color gamma(float g) const
	{
		using namespace ::std;

		float g1 = 1.0f / g;
		return Color(pow(v[0],g1), pow(v[1],g1), pow(v[2],g1));
	}

	// Just apply the nonlinearity, not full colorspace conversion
	const Color gamma(Colorspace dst) const
	{
		switch (dst) {
			case CIELAB:
				return Color(cielab_nonlinearity(v[0]),
					     cielab_nonlinearity(v[1]),
					     cielab_nonlinearity(v[2]));
			case SRGB:
			case YCBCR:
				return Color(srgb_nonlinearity(v[0]),
					     srgb_nonlinearity(v[1]),
					     srgb_nonlinearity(v[2]));
			default:
				return Color(*this);
		}
	}

	// Nonlinear to linear - raises values to the power of g
	const Color ungamma(float g) const
	{
		using namespace ::std;

		return Color(pow(v[0],g), pow(v[1],g), pow(v[2],g));
	}

	const Color ungamma(Colorspace dst) const
	{
		switch (dst) {
			case CIELAB:
				return Color(inv_cielab_nonlinearity(v[0]),
					     inv_cielab_nonlinearity(v[1]),
					     inv_cielab_nonlinearity(v[2]));
			case SRGB:
			case YCBCR:
				return Color(inv_srgb_nonlinearity(v[0]),
					     inv_srgb_nonlinearity(v[1]),
					     inv_srgb_nonlinearity(v[2]));
			default:
				return Color(*this);
		}
	}

	// For backwards compatibility with earlier versions of Color.h,
	// this stays as a static method.  New code should use convert().
	static Color hsv(float h, float s, float v)
	{
		return Color(h,s,v).hsv2srgb();
	}
};

}; // namespace trimesh

#endif
