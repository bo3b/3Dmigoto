#ifndef VEC_H
#define VEC_H
/*
Szymon Rusinkiewicz
Princeton University

Vec.h
Class for a constant-length vector

Supports the following operations:

	vec v1;			// Initialized to (0, 0, 0)
	vec v2(1.23f);		// Initialized to (1.23f, 1.23f, 1.23f)
	vec v3(1, 2, 3);	// Initialized to (1, 2, 3)
	vec v4(v3);		// Copy constructor

	float farray[3];
	vec v5 = vec(farray);	// Explicit: "v4 = farray" won't work

	Vec<3,double> vd;	// The "vec" used above is Vec<3,float>
	point p1, p2, p3;	// Same as vec

	v3 = v1 + v2;		// Also -, *, /  (all componentwise)
	v3 = 3.5f * v1;		// Also vec * scalar, vec / scalar
				// NOTE: scalar has to be the same type:
				// it won't work to do double * vec<float>
	v1 = min(v2, v3);	// Componentwise min/max
	v1 = sin(v2);		// Componentwise - all the usual functions...
	swap(v1, v2);		// In-place swap

	v3 = v1 DOT v2;		// Actually operator^
	v3 = v1 CROSS v2;	// Actually operator%

	float f = v1[0];	// Subscript
	float *fp = v1;		// Implicit conversion to float *

	f = len(v1);		// Length (also len2 == squared length)
	f = dist(p1, p2);	// Distance (also dist2 == squared distance)
	normalize(v1);		// Normalize (i.e., make it unit length)
				// normalize(vec(0,0,0)) => vec(1,0,0)
	v1 = trinorm(p1,p2,p3); // Normal of triangle (area-weighted)

	cout << v1 << endl;	// iostream output in the form (1,2,3)
	cin >> v2;		// iostream input using the same syntax

Also defines the utility functions sqr, cube, sgn, fract, clamp, mix,
step, smoothstep, faceforward, reflect, refract, and angle
*/


// Windows defines min and max as macros, which prevents us from using the
// type-safe versions from std::, as well as interfering with method defns.
// Also define NOMINMAX, which prevents future bad definitions.
#ifdef min
# undef min
#endif
#ifdef max
# undef max
#endif
#ifndef NOMINMAX
# define NOMINMAX
#endif
#ifndef _USE_MATH_DEFINES
 #define _USE_MATH_DEFINES
#endif

#include <cstddef>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <iostream>
#include <algorithm>


// Let gcc optimize conditional branches a bit better...
#ifndef likely
#  if !defined(__GNUC__) || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
#    define likely(x) (x)
#    define unlikely(x) (x)
#  else
#    define likely(x)   (__builtin_expect((x), 1))
#    define unlikely(x) (__builtin_expect((x), 0))
#  endif
#endif


namespace trimesh {

using ::std::size_t;


// Boost-like compile-time assertion checking
template <bool X> struct VEC_STATIC_ASSERTION_FAILURE;
template <> struct VEC_STATIC_ASSERTION_FAILURE<true>
	{ void operator () () {} };
#define VEC_STATIC_CHECK(expr) VEC_STATIC_ASSERTION_FAILURE<bool(expr)>()


// Vec class declaration
template <size_t D, class T = float>
class Vec {
public:
	// Types
	typedef T value_type;
	typedef value_type *pointer;
	typedef const value_type *const_pointer;
	typedef value_type &reference;
	typedef const value_type &const_reference;
	typedef value_type *iterator;
	typedef const value_type *const_iterator;
	typedef ::std::reverse_iterator<iterator> reverse_iterator;
	typedef ::std::reverse_iterator<const_iterator> const_reverse_iterator;
	typedef ::std::size_t size_type;
	typedef ::std::ptrdiff_t difference_type;

protected:
	// The internal representation: standard array
	T v[D];

public:
	// Constructor for no arguments.  Everything initialized to 0.
	Vec() { for (size_type i = 0; i < D; i++) v[i] = T(0); }

	// Uninitialized constructor - meant mostly for internal use
#define VEC_UNINITIALIZED ((void *) 0)
	Vec(void *) {}

	// Constructor for one argument - default value.  Explicit.
	explicit Vec(const T &x)
		{ for (size_type i = 0; i < D; i++) v[i] = x; }

	// Constructors for 2-4 arguments
	Vec(const T &x, const T &y)
		{ VEC_STATIC_CHECK(D == 2); v[0] = x; v[1] = y; }
	Vec(const T &x, const T &y, const T &z)
		{ VEC_STATIC_CHECK(D == 3); v[0] = x; v[1] = y; v[2] = z; }
	Vec(const T &x, const T &y, const T &z, const T &w)
		{ VEC_STATIC_CHECK(D == 4); v[0] = x; v[1] = y; v[2] = z; v[3] = w; }

	// Constructor from anything that can be accessed using []
	// Pretty aggressive, so marked as explicit.
	template <class S> explicit Vec(const S &x)
		{ for (size_type i = 0; i < D; i++) v[i] = x[i]; }

	// Using default copy constructor, assignment operator, and destructor

	// Array reference - no bounds checking
	reference operator [] (size_type i)
		{ return v[i]; }
	reference operator [] (int i)
		{ return v[i]; }
	const_reference operator [] (size_type i) const
		{ return v[i]; }
	const_reference operator [] (int i) const
		{ return v[i]; }

	// Array reference with bounds checking
	reference at(size_type i)
	{
		if (i >= D)
			throw ::std::out_of_range("Vec::at");
		return v[i];
	}
	const_reference at(size_type i) const
	{
		if (i >= D)
			throw ::std::out_of_range("Vec::at");
		return v[i];
	}

	// Other accessors, for compatibility with std::array
	reference front()
		{ return v[0]; }
	const_reference front() const
		{ return v[0]; }
	reference back()
		{ return v[D-1]; }
	const_reference back() const
		{ return v[D-1]; }

	// Conversion to pointer
	operator T * ()
		{ return v; }
	operator const T * ()
		{ return v; }
	operator const T * () const
		{ return v; }
	pointer data()
		{ return v; }
	const_pointer data() const
		{ return v; }

	// Iterators
	iterator begin()
		{ return v; }
	const_iterator begin() const
		{ return v; }
	const_iterator cbegin() const
		{ return v; }
	iterator end()
		{ return begin() + D; }
	const_iterator end() const
		{ return begin() + D; }
	const_iterator cend() const
		{ return begin() + D; }
	reverse_iterator rbegin()
		{ return reverse_iterator(end()); }
	const_reverse_iterator rbegin() const
		{ return const_reverse_iterator(end()); }
	const_reverse_iterator crbegin() const
		{ return const_reverse_iterator(end()); }
	reverse_iterator rend()
		{ return reverse_iterator(begin()); }
	const_reverse_iterator rend() const
		{ return const_reverse_iterator(begin()); }
	const_reverse_iterator crend() const
		{ return const_reverse_iterator(begin()); }

	// Capacity
	size_type size() const
		{ return D; }
	size_type max_size() const
		{ return D; }

	// empty() and clear() - check for all zero or set to zero
	bool empty() const
	{
		for (size_type i = 0; i < D; i++)
			if (v[i]) return false;
		return true;
	}
	void clear()
		{ for (size_type i = 0; i < D; i++) v[i] = T(0); }

	// Set all elements to some constant
	void fill(const value_type &x)
	{
		for (size_type i = 0; i < D; i++)
			v[i] = x;
	}
	Vec<D,T> &operator = (const value_type &x)
	{
		for (size_type i = 0; i < D; i++)
			v[i] = x;
		return *this;
	}

	// Member operators
	Vec<D,T> &operator += (const Vec<D,T> &x)
	{
		for (size_type i = 0; i < D; i++)
#pragma omp atomic
			v[i] += x[i];
		return *this;
	}
	Vec<D,T> &operator -= (const Vec<D,T> &x)
	{
		for (size_type i = 0; i < D; i++)
#pragma omp atomic
			v[i] -= x[i];
		return *this;
	}
	Vec<D,T> &operator *= (const Vec<D,T> &x)
	{
		for (size_type i = 0; i < D; i++)
#pragma omp atomic
			v[i] *= x[i];
		return *this;
	}
	Vec<D,T> &operator *= (const T &x)
	{
		for (size_type i = 0; i < D; i++)
#pragma omp atomic
			v[i] *= x;
		return *this;
	}
	Vec<D,T> &operator /= (const Vec<D,T> &x)
	{
		for (size_type i = 0; i < D; i++)
#pragma omp atomic
			v[i] /= x[i];
		return *this;
	}
	Vec<D,T> &operator /= (const T &x)
	{
		for (size_type i = 0; i < D; i++)
#pragma omp atomic
			v[i] /= x;
		return *this;
	}

	// Set each component to min/max of this and the other vector
	Vec<D,T> &min(const Vec<D,T> &x)
	{
#pragma omp critical
		for (size_type i = 0; i < D; i++)
			if (x[i] < v[i]) v[i] = x[i];
		return *this;
	}
	Vec<D,T> &max(const Vec<D,T> &x)
	{
#pragma omp critical
		for (size_type i = 0; i < D; i++)
			if (x[i] > v[i]) v[i] = x[i];
		return *this;
	}

	// Swap with another vector.  (Also exists as a global function.)
	void swap(Vec<D,T> &x)
	{
		using namespace ::std;
#pragma omp critical
		for (size_type i = 0; i < D; i++) swap(v[i], x[i]);
	}

	// Outside of class: + - * / % ^ << >> == != < > <= >=

	// Dot product with another vector (also exists as an operator)
	value_type dot(const Vec<D,T> &x) const
	{
		value_type total = v[0] * x[0];
		for (size_type i = 1; i < D; i++)
			total += v[i] * x[i];
		return total;
	}

	// Cross product with another vector (also exists as an operator)
	Vec<3,T> cross(const Vec<3,T> &x) const
	{
		VEC_STATIC_CHECK(D == 3);
		return Vec<3,T>(v[1]*x[2] - v[2]*x[1],
				v[2]*x[0] - v[0]*x[2],
				v[0]*x[1] - v[1]*x[0]);
	}

	// Some partial compatibility with std::valarray, plus generalizations
	value_type sum() const
	{
		value_type total = v[0];
		for (size_type i = 1; i < D; i++)
			total += v[i];
		return total;
	}
	value_type sumabs() const
	{
		using namespace ::std;
		value_type total = fabs(v[0]);
		for (size_type i = 1; i < D; i++)
			total += fabs(v[i]);
		return total;
	}
	value_type avg() const
		{ return sum() / D; }
	value_type mean() const
		{ return sum() / D; }
	value_type product() const
	{
		value_type total = v[0];
		for (size_type i = 1; i < D; i++)
			total *= v[i];
		return total;
	}
	value_type min() const
	{
		value_type m = v[0];
		for (size_type i = 1; i < D; i++)
			if (v[i] < m)
				m = v[i];
		return m;
	}
	value_type max() const
	{
		value_type m = v[0];
		for (size_type i = 1; i < D; i++)
			if (v[i] > m)
				m = v[i];
		return m;
	}
	Vec<D,T> apply(value_type func(value_type)) const
	{
		Vec<D,T> result(VEC_UNINITIALIZED);
		for (size_type i = 0; i < D; i++)
			result[i] = func(v[i]);
		return result;
	}
	Vec<D,T> apply(value_type func(const value_type&)) const
	{
		Vec<D,T> result(VEC_UNINITIALIZED);
		for (size_type i = 0; i < D; i++)
			result[i] = func(v[i]);
		return result;
	}
	Vec<D,T> cshift(int n) const
	{
		Vec<D,T> result(VEC_UNINITIALIZED);
		if (n < 0)
			n = (n % D) + D;
		for (size_type i = 0; i < D; i++)
			result[i] = v[(i+n)%D];
		return result;
	}
	Vec<D,T> shift(int n) const
	{
		using namespace ::std;
		if (abs(n) >= D)
			return Vec<D,T>();
		Vec<D,T> result; // Must be initialized to zero
		size_type start = n < 0 ? -n : 0;
		size_type stop = n > 0 ? D - n : D;
		for (size_type i = start; i < stop; i++)
			result[i] = v[i+n];
		return result;
	}

	// TODO for C++11: std::get()
}; // class Vec


// Shorthands for particular flavors of Vecs
typedef Vec<3,float> vec;
typedef Vec<3,float> point;
typedef Vec<2,float> vec2;
typedef Vec<3,float> vec3;
typedef Vec<4,float> vec4;
typedef Vec<2,int> ivec2;
typedef Vec<3,int> ivec3;
typedef Vec<4,int> ivec4;


// Nonmember operators that take two Vecs
template <size_t D, class T>
static inline const Vec<D,T> operator + (const Vec<D,T> &v1, const Vec<D,T> &v2)
{
	using namespace ::std;
	Vec<D,T> result(VEC_UNINITIALIZED);
	for (size_t i = 0; i < D; i++)
		result[i] = v1[i] + v2[i];
	return result;
}

template <size_t D, class T>
static inline const Vec<D,T> operator - (const Vec<D,T> &v1, const Vec<D,T> &v2)
{
	using namespace ::std;
	Vec<D,T> result(VEC_UNINITIALIZED);
	for (size_t i = 0; i < D; i++)
		result[i] = v1[i] - v2[i];
	return result;
}

template <size_t D, class T>
static inline const Vec<D,T> operator * (const Vec<D,T> &v1, const Vec<D,T> &v2)
{
	using namespace ::std;
	Vec<D,T> result(VEC_UNINITIALIZED);
	for (size_t i = 0; i < D; i++)
		result[i] = v1[i] * v2[i];
	return result;
}

template <size_t D, class T>
static inline const Vec<D,T> operator / (const Vec<D,T> &v1, const Vec<D,T> &v2)
{
	using namespace ::std;
	Vec<D,T> result(VEC_UNINITIALIZED);
	for (size_t i = 0; i < D; i++)
		result[i] = v1[i] / v2[i];
	return result;
}


// Dot product
template <size_t D, class T>
static inline const T operator ^ (const Vec<D,T> &v1, const Vec<D,T> &v2)
{
	using namespace ::std;
	T sum = v1[0] * v2[0];
	for (size_t i = 1; i < D; i++)
		sum += v1[i] * v2[i];
	return sum;
}
#define DOT ^


// Cross product - only in 3 dimensions
template <class T>
static inline const Vec<3,T> operator % (const Vec<3,T> &v1, const Vec<3,T> &v2)
{
	return Vec<3,T>(v1[1]*v2[2] - v1[2]*v2[1],
			v1[2]*v2[0] - v1[0]*v2[2],
			v1[0]*v2[1] - v1[1]*v2[0]);
}
#define CROSS %


// Component-wise equality and inequality (#include the usual caveats
// about comparing floats for equality...)
template <size_t D, class T>
static inline bool operator == (const Vec<D,T> &v1, const Vec<D,T> &v2)
{
	using namespace ::std;
	for (size_t i = 0; i < D; i++)
		if (v1[i] != v2[i])
			return false;
	return true;
}

template <size_t D, class T>
static inline bool operator != (const Vec<D,T> &v1, const Vec<D,T> &v2)
{
	using namespace ::std;
	for (size_t i = 0; i < D; i++)
		if (v1[i] != v2[i])
			return true;
	return false;
}


// Comparison by lexicographical ordering - not necessarily useful on its own,
// but necessary in order to put Vecs in sets, maps, etc.
template <size_t D, class T>
static inline bool operator < (const Vec<D,T> &v1, const Vec<D,T> &v2)
{
	using namespace ::std;
	for (size_t i = 0; i < D; i++) {
		if (v1[i] < v2[i])
			return true;
		else if (v1[i] > v2[i])
			return false;
	}
	return false;
}

template <size_t D, class T>
static inline bool operator > (const Vec<D,T> &v1, const Vec<D,T> &v2)
{
	return v2 < v1;
}

template <size_t D, class T>
static inline bool operator <= (const Vec<D,T> &v1, const Vec<D,T> &v2)
{
	return !(v2 < v1);
}

template <size_t D, class T>
static inline bool operator >= (const Vec<D,T> &v1, const Vec<D,T> &v2)
{
	return !(v1 < v2);
}


// Unary operators
template <size_t D, class T>
static inline const Vec<D,T> &operator + (const Vec<D,T> &v)
{
	return v;
}

template <size_t D, class T>
static inline const Vec<D,T> operator - (const Vec<D,T> &v)
{
	using namespace ::std;
	Vec<D,T> result(VEC_UNINITIALIZED);
	for (size_t i = 0; i < D; i++)
		result[i] = -v[i];
	return result;
}

template <size_t D, class T>
static inline bool operator ! (const Vec<D,T> &v)
{
	return v.empty();
}


// Vec/scalar operators
template <size_t D, class T>
static inline const Vec<D,T> operator * (const T &x, const Vec<D,T> &v)
{
	using namespace ::std;
	Vec<D,T> result(VEC_UNINITIALIZED);
	for (size_t i = 0; i < D; i++)
		result[i] = x * v[i];
	return result;
}

template <size_t D, class T>
static inline const Vec<D,T> operator * (const Vec<D,T> &v, const T &x)
{
	using namespace ::std;
	Vec<D,T> result(VEC_UNINITIALIZED);
	for (size_t i = 0; i < D; i++)
		result[i] = v[i] * x;
	return result;
}

template <size_t D, class T>
static inline const Vec<D,T> operator / (const T &x, const Vec<D,T> &v)
{
	using namespace ::std;
	Vec<D,T> result(VEC_UNINITIALIZED);
	for (size_t i = 0; i < D; i++)
		result[i] = x / v[i];
	return result;
}

template <size_t D, class T>
static inline const Vec<D,T> operator / (const Vec<D,T> &v, const T &x)
{
	using namespace ::std;
	Vec<D,T> result(VEC_UNINITIALIZED);
	for (size_t i = 0; i < D; i++)
		result[i] = v[i] / x;
	return result;
}


// iostream operators
template <size_t D, class T>
static inline ::std::ostream &operator << (::std::ostream &os, const Vec<D,T> &v)

{
	using namespace ::std;
	os << "(";
	for (size_t i = 0; i < D-1; i++)
		os << v[i] << ", ";
	return os << v[D-1] << ")";
}

template <size_t D, class T>
static inline ::std::istream &operator >> (::std::istream &is, Vec<D,T> &v)
{
	using namespace ::std;
	char c1 = 0, c2 = 0;

	is >> c1;
	if (c1 == '(' || c1 == '[') {
		is >> v[0] >> ws >> c2;
		for (size_t i = 1; i < D; i++) {
			if (c2 == ',')
				is >> v[i] >> ws >> c2;
			else
				is.setstate(ios::failbit);
		}
	}

	if (c1 == '(' && c2 != ')')
		is.setstate(ios::failbit);
	else if (c1 == '[' && c2 != ']')
		is.setstate(ios::failbit);

	return is;
}


// Utility functions for square and cube, to go along with sqrt and cbrt
template <class T>
static inline T sqr(const T &x)
{
	return x*x;
}

template <class T>
static inline T cube(const T &x)
{
	return x*x*x;
}


// Sign of a scalar.  Note that sgn(0) == 1.
template <class T>
static inline T sgn(const T &x)
{
	return (x < T(0)) ? T(-1) : T(1);
}


// Utility functions based on GLSL
template <class T>
static inline T fract(const T &x)
{
	return x - floor(x);
}

template <class T>
static inline T clamp(const T &x, const T &a, const T &b)
{
	return x > a ? x < b ? x : b : a;  // returns a on NaN
}

template <class T, class S>
static inline T mix(const T &x, const T &y, const S &a)
{
	return (S(1)-a) * x + a * y;
}

template <class T>
static inline T step(const T &a, const T &x)
{
	return x < a ? T(0) : T(1);
}

template <class T>
static inline T smoothstep(const T &a, const T &b, const T &x)
{
	if (b <= a) return step(x,a);
	T t = (x - a) / (b - a);
	return t <= T(0) ? T(0) : t >= T(1) ? T(1) : t * t * (T(3) - T(2) * t);
}

template <size_t D, class T>
static inline T faceforward(const Vec<D,T> &N, const Vec<D,T> &I,
			    const Vec<D,T> &Nref)
{
	return ((Nref DOT I) < T(0)) ? N : -N;
}

template <size_t D, class T>
static inline T reflect(const Vec<D,T> &I, const Vec<D,T> &N)
{
	return I - (T(2) * (N DOT I)) * N;
}

template <size_t D, class T>
static inline T refract(const Vec<D,T> &I, const Vec<D,T> &N,
			const T &eta)
{
	using namespace ::std;
	T NdotI = N DOT I;
	T k = T(1) - sqr(eta) * (T(1) - sqr(NdotI));
	return (k < T(0)) ? T(0) : eta * I - (eta * NdotI * sqrt(k)) * N;
}


// Squared length
template <size_t D, class T>
static inline const T len2(const Vec<D,T> &v)
{
	using namespace ::std;
	T l2 = v[0] * v[0];
	for (size_t i = 1; i < D; i++)
		l2 += v[i] * v[i];
	return l2;
}


// Length
template <size_t D, class T>
static inline const T len(const Vec<D,T> &v)
{
	using namespace ::std;
	return sqrt(len2(v));
}


// Squared distance
template <size_t D, class T>
static inline const T dist2(const Vec<D,T> &v1, const Vec<D,T> &v2)
{
	using namespace ::std;
	T d2 = sqr(v2[0]-v1[0]);
	for (size_t i = 1; i < D; i++)
		d2 += sqr(v2[i]-v1[i]);
	return d2;
}


// Distance
template <size_t D, class T>
static inline const T dist(const Vec<D,T> &v1, const Vec<D,T> &v2)
{
	using namespace ::std;
	return sqrt(dist2(v1,v2));
}


// In-place normalization to unit length
template <size_t D, class T>
static inline Vec<D,T> normalize(Vec<D,T> &v)
{
	using namespace ::std;
	T l = len(v);
	if (unlikely(l <= T(0))) {
		v[0] = T(1);
		for (size_t i = 1; i < D; i++)
			v[i] = T(0);
		return v;
	}

	l = T(1) / l;
	for (size_t i = 0; i < D; i++)
		v[i] *= l;

	return v;
}


// Area-weighted triangle face normal
template <class T>
static inline T trinorm(const T &v0, const T &v1, const T &v2)
{
	return (typename T::value_type) 0.5 * ((v1 - v0) CROSS (v2 - v0));
}


// Angle between two vectors
template <size_t D, class T>
static inline const T angle(const Vec<D,T> &v1, const Vec<D,T> &v2)
{
	using namespace ::std;
	return atan2(len(v1 CROSS v2), v1 DOT v2);
}


}; // namespace trimesh


// POSIX / C99 compatibility functions for MSVS
#ifdef _WIN32
/*#ifdef cbrt
# undef cbrt
#endif
inline float cbrt(float x)
{
	using namespace ::std;
	return (x < 0.0f) ? -pow(-x, 1.0f / 3.0f) : pow(x, 1.0f / 3.0f);
}
inline double cbrt(double x)
{
	using namespace ::std;
	return (x < 0.0) ? -pow(-x, 1.0 / 3.0) : pow(x, 1.0 / 3.0);
}
inline long double cbrt(long double x)
{
	using namespace ::std;
	return (x < 0.0L) ? -pow(-x, 1.0L / 3.0L) : pow(x, 1.0L / 3.0L);
}
#ifdef round
# undef round
#endif
inline float round(float x)
{
	return (x < 0.0f) ? float(int(x - 0.5f)) : float(int(x + 0.5f));
}
inline double round(double x)
{
	return (x < 0.0f) ? double(int(x - 0.5)) : double(int(x + 0.5));
}
inline long double round(long double x)
{
	return (x < 0.0f) ? (long double)(int(x - 0.5L)) : (long double)(int(x + 0.5L));
}
#ifdef trunc
# undef trunc
#endif
inline float trunc(float x)
{
	return (x < 0.0f) ? float(int(x)) : float(int(x));
}
inline double trunc(double x)
{
	return (x < 0.0f) ? double(int(x)) : double(int(x));
}
inline long double trunc(long double x)
{
	return (x < 0.0f) ? (long double)(int(x)) : (long double)(int(x));
}*/
#endif // _WIN32


// Generic macros for declaring 1-, 2-, and 3- argument
// componentwise functions on Vecs.
#define VEC_DECLARE_ONEARG(name) \
 template < ::std::size_t D, class T > \
 static inline trimesh::Vec<D,T> name(const trimesh::Vec<D,T> &v) \
 { \
	using namespace ::std; \
	using namespace ::trimesh; \
	Vec<D,T> result(VEC_UNINITIALIZED); \
	for (size_t i = 0; i < D; i++) \
		result[i] = name(v[i]); \
	return result; \
 }

// Vector-scalar, scalar-vector, and componentwise vector-vector versions
#define VEC_DECLARE_TWOARG_VS(name) \
 template < ::std::size_t D, class T > \
 static inline trimesh::Vec<D,T> name(const trimesh::Vec<D,T> &v, const T &a) \
 { \
	using namespace ::std; \
	using namespace ::trimesh; \
	Vec<D,T> result(VEC_UNINITIALIZED); \
	for (size_t i = 0; i < D; i++) \
		result[i] = name(v[i], a); \
	return result; \
 }
#define VEC_DECLARE_TWOARG_SV(name) \
 template < ::std::size_t D, class T > \
 static inline trimesh::Vec<D,T> name(const T &a, const trimesh::Vec<D,T> &v) \
 { \
	using namespace ::std; \
	using namespace ::trimesh; \
	Vec<D,T> result(VEC_UNINITIALIZED); \
	for (size_t i = 0; i < D; i++) \
		result[i] = name(a, v[i]); \
	return result; \
 }
#define VEC_DECLARE_TWOARG_VV(name) \
 template < ::std::size_t D, class T > \
 static inline trimesh::Vec<D,T> name(const trimesh::Vec<D,T> &v, const trimesh::Vec<D,T> &w) \
 { \
	using namespace ::std; \
	using namespace ::trimesh; \
	Vec<D,T> result(VEC_UNINITIALIZED); \
	for (size_t i = 0; i < D; i++) \
		result[i] = name(v[i], w[i]); \
	return result; \
 }

#define VEC_DECLARE_THREEARG_VSS(name) \
 template < ::std::size_t D, class T > \
 static inline trimesh::Vec<D,T> name(const trimesh::Vec<D,T> &v, const T &a, const T &b) \
 { \
	using namespace ::std; \
	using namespace ::trimesh; \
	Vec<D,T> result(VEC_UNINITIALIZED); \
	for (size_t i = 0; i < D; i++) \
		result[i] = name(v[i], a, b); \
	return result; \
 }
#define VEC_DECLARE_THREEARG_SSV(name) \
 template < ::std::size_t D, class T > \
 static inline trimesh::Vec<D,T> name(const T &a, const T &b, const trimesh::Vec<D,T> &v) \
 { \
	using namespace ::std; \
	using namespace ::trimesh; \
	Vec<D,T> result(VEC_UNINITIALIZED); \
	for (size_t i = 0; i < D; i++) \
		result[i] = name(a, b, v[i]); \
	return result; \
 }
#define VEC_DECLARE_THREEARG_VVV(name) \
 template < ::std::size_t D, class T > \
 static inline trimesh::Vec<D,T> name(const trimesh::Vec<D,T> &v, const trimesh::Vec<D,T> &w, const trimesh::Vec<D,T> &x) \
 { \
	using namespace ::std; \
	using namespace ::trimesh; \
	Vec<D,T> result(VEC_UNINITIALIZED); \
	for (size_t i = 0; i < D; i++) \
		result[i] = name(v[i], w[i], x[i]); \
	return result; \
 }


// The following is the list of functions in C89 and C++98, with the exception
// of frexp, ldexp, and modf (which have irregular calling conventions).
// They are supposed to be in namespace std, but Visual Studio and some
// older compilers also declare them in the global namespace.
// In the name of compatibility, we (reluctantly) do likewise.
VEC_DECLARE_ONEARG(acos)
VEC_DECLARE_ONEARG(asin)
VEC_DECLARE_ONEARG(atan)
VEC_DECLARE_TWOARG_VV(atan2)
VEC_DECLARE_ONEARG(ceil)
VEC_DECLARE_ONEARG(cos)
VEC_DECLARE_ONEARG(cosh)
VEC_DECLARE_ONEARG(exp)
VEC_DECLARE_ONEARG(fabs)
VEC_DECLARE_ONEARG(floor)
VEC_DECLARE_TWOARG_VS(fmod)
VEC_DECLARE_TWOARG_VV(fmod)
VEC_DECLARE_ONEARG(log)
VEC_DECLARE_ONEARG(log10)
VEC_DECLARE_TWOARG_VS(pow)
VEC_DECLARE_TWOARG_SV(pow)
VEC_DECLARE_TWOARG_VV(pow)
VEC_DECLARE_ONEARG(sin)
VEC_DECLARE_ONEARG(sinh)
VEC_DECLARE_ONEARG(sqrt)
VEC_DECLARE_ONEARG(tan)
VEC_DECLARE_ONEARG(tanh)
namespace std {
	using ::acos;
	using ::asin;
	using ::atan;
	using ::atan2;
	using ::ceil;
	using ::cos;
	using ::cosh;
	using ::exp;
	using ::fabs;
	using ::floor;
	using ::fmod;
	using ::log;
	using ::log10;
	using ::pow;
	using ::sin;
	using ::sinh;
	using ::sqrt;
	using ::tan;
	using ::tanh;
};


// These are only in namespace std.
namespace std {
	VEC_DECLARE_TWOARG_VS(min)
	VEC_DECLARE_TWOARG_SV(min)
	VEC_DECLARE_TWOARG_VV(min)
	VEC_DECLARE_TWOARG_VS(max)
	VEC_DECLARE_TWOARG_SV(max)
	VEC_DECLARE_TWOARG_VV(max)

	// Swap two Vecs.  Not atomic, unlike class method.
	template <size_t D, class T>
	static inline void swap(const ::trimesh::Vec<D,T> &v1, const ::trimesh::Vec<D,T> &v2)
	{
		for (size_t i = 0; i < D; i++)
			swap(v1[i], v2[i]);
	}
};


// These are POSIX and are commonly used.  Global namespace.
// Compatibility versions of these for MSVC are above.
VEC_DECLARE_ONEARG(cbrt)
VEC_DECLARE_ONEARG(round)
VEC_DECLARE_ONEARG(trunc)


// These are new functions declared in namespace trimesh.
namespace trimesh {
	VEC_DECLARE_ONEARG(sqr)
	VEC_DECLARE_ONEARG(cube)
	VEC_DECLARE_ONEARG(sgn)
	VEC_DECLARE_ONEARG(fract)
	VEC_DECLARE_THREEARG_VSS(clamp)
	VEC_DECLARE_THREEARG_VVV(clamp)
	VEC_DECLARE_TWOARG_SV(step)
	VEC_DECLARE_TWOARG_VV(step)
	VEC_DECLARE_THREEARG_SSV(smoothstep)
	VEC_DECLARE_THREEARG_VVV(smoothstep)
};


#undef VEC_DECLARE_ONEARG
#undef VEC_DECLARE_TWOARG_VS
#undef VEC_DECLARE_TWOARG_SV
#undef VEC_DECLARE_TWOARG_VV
#undef VEC_DECLARE_THREEARG_VSS
#undef VEC_DECLARE_THREEARG_SSV
#undef VEC_DECLARE_THREEARG_VVV


// Both valarrays and GLSL use abs() on a vector to mean fabs().
// Let's do the same...
template < ::std::size_t D, class T >
static inline trimesh::Vec<D,T> abs(const trimesh::Vec<D,T> &v)
{
	return fabs(v);
}
namespace std {
	using ::abs;
};


#endif
