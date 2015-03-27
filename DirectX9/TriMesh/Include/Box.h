#ifndef BOX_H
#define BOX_H
/*
Szymon Rusinkiewicz
Princeton University

Box.h
Templated axis-aligned bounding boxes - meant to be used with Vec.h
*/

#include "Vec.h"
#include "strutil.h"
#include <iostream>
#include <fstream>


namespace trimesh {

template <size_t D, class T = float>
class Box {
private:
	typedef Vec<D,T> Point;

public:
	Point min, max;
	bool valid;

	// Construct as empty
	Box() : valid(false)
		{}

	// Construct from a single point
	Box(const Point &p) : min(p), max(p), valid(true)
		{}

	// Mark invalid
	void clear()
		{ valid = false; }

	// Return center point, (vector) diagonal, and (scalar) radius
	Point center() const
	{
		if (valid)
			return T(0.5) * (min+max);
		else
			return Vec<D,T>();
	}
	Point size() const
	{
		if (valid)
			return max - min;
		else
			return Vec<D,T>();
	}
	T radius() const
	{
		if (valid)
			return T(0.5) * dist(min, max);
		else
			return T(0);
	}

	// Grow a bounding box to encompass a point
	Box<D,T> &operator += (const Point &p)
	{
		if (valid) {
			min.min(p);
			max.max(p);
		} else {
			min = p;
			max = p;
			valid = true;
		}
		return *this;
	}
	Box<D,T> &operator += (const Box<D,T> &b)
	{
		if (valid) {
			min.min(b.min);
			max.max(b.max);
		} else {
			min = b.min;
			max = b.max;
			valid = true;
		}
		return *this;
	}

	friend const Box<D,T> operator + (const Box<D,T> &b, const Point &p)
		{ return Box<D,T>(b) += p; }
	friend const Box<D,T> operator + (const Point &p, const Box<D,T> &b)
		{ return Box<D,T>(b) += p; }
	friend const Box<D,T> operator + (const Box<D,T> &b1, const Box<D,T> &b2)
		{ return Box<D,T>(b1) += b2; }

	// Read a Box from a file.
	bool read(const ::std::string &filename)
	{
		using namespace ::std;
		fstream f(filename.c_str());
		Box<D,T> B;
		f >> B;
		f.close();
		if (f.good()) {
			*this = B;
			return true;
		}
		return false;
	}

	// Write a Box to a file
	bool write(const ::std::string &filename) const
	{
		using namespace ::std;
		ofstream f(filename.c_str());
		f << *this;
		f.close();
		return f.good();
	}

	// iostream operators
	friend ::std::ostream &operator << (::std::ostream &os, const Box<D,T> &b)
	{
		using namespace ::std;
		const size_t n = b.min.size();
		for (size_t i = 0; i < n-1; i++)
			os << b.min[i] << " ";
		os << b.min[n-1] << endl;
		for (size_t i = 0; i < n-1; i++)
			os << b.max[i] << " ";
		os << b.max[n-1] << endl;
		return os;
	}
	friend ::std::istream &operator >> (::std::istream &is, Box<D,T> &b)
	{
		using namespace ::std;
		const size_t n = b.min.size();
		for (size_t i = 0; i < n; i++)
			is >> b.min[i];
		for (size_t i = 0; i < n; i++)
			is >> b.max[i];
		for (size_t i = 0; i < n; i++)
			if (b.min[i] > b.max[i])
				swap(b.min[i], b.max[i]);
		b.valid = is.good();
		return is;
	}
};

typedef Box<3,float> box;
typedef Box<2,float> box2;
typedef Box<3,float> box3;
typedef Box<4,float> box4;
typedef Box<2,int> ibox2;
typedef Box<3,int> ibox3;
typedef Box<4,int> ibox4;


// Generate a .bbox filename from an input (scan) filename
static inline ::std::string bboxname(const ::std::string &filename)
{
	return replace_ext(filename, "bbox");
}

}; // namespace trimesh

#endif
