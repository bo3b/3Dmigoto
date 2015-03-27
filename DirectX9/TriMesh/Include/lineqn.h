#ifndef LINEQN_H
#define LINEQN_H
/*
Szymon Rusinkiewicz
Princeton University

lineqn.h
Solution of systems of linear equations and eigenvalue decomposition.
Some are patterned after the code in Numerical Recipes, but with a bit of
the fancy C++ template thing happening.
*/


// Windows defines min and max as macros, which prevents us from
// using the type-safe versions from std::
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

#include <cmath>
#include <algorithm>
#include <limits>


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

// LU decomposition
template <class T, int N>
static inline bool ludcmp(T a[N][N], int indx[N], T *d = NULL)
{
	using namespace ::std;
	T vv[N];

	if (d)
		*d = T(1);
	for (int i = 0; i < N; i++) {
		T big = T(0);
		for (int j = 0; j < N; j++) {
			T tmp = fabs(a[i][j]);
			if (tmp > big)
				big = tmp;
		}
		if (big == T(0))
			return false;
		vv[i] = T(1) / big;
	}
	for (int j = 0; j < N; j++) {
		for (int i = 0; i < j; i++) {
			T sum = a[i][j];
			for (int k = 0; k < i; k++)
				sum -= a[i][k]*a[k][j];
			a[i][j]=sum;
		}
		T big = T(0);
		int imax = j;
		for (int i = j; i < N; i++) {
			T sum = a[i][j];
			for (int k = 0; k < j; k++)
				sum -= a[i][k]*a[k][j];
			a[i][j] = sum;
			T tmp = vv[i] * fabs(sum);
			if (tmp > big) {
				big = tmp;
				imax = i;
			}
		}
		if (imax != j) {
			for (int k = 0; k < N; k++)
				swap(a[imax][k], a[j][k]);
			if (d)
				*d = -(*d);
			vv[imax] = vv[j];
		}
		indx[j] = imax;
		if (unlikely(a[j][j] == T(0)))
			return false;
		if (j != N-1) {
			T tmp = T(1) / a[j][j];
			for (int i = j+1; i < N; i++)
				a[i][j] *= tmp;
		}
	}
	return true;
}


// Backsubstitution after ludcmp
template <class T, int N>
static inline void lubksb(T a[N][N], int indx[N], T b[N])
{
	int ii = -1;
	for (int i = 0; i < N; i++) {
		int ip = indx[i];
		T sum = b[ip];
		b[ip] = b[i];
		if (ii != -1)
			for (int j = ii; j < i; j++)
				sum -= a[i][j] * b[j];
		else if (sum)
			ii = i;
		b[i] = sum;
	}
	for (int i = N-1; i >= 0; i--) {
		T sum = b[i];
		for (int j = i+1; j < N; j++)
			sum -= a[i][j] * b[j];
		b[i] = sum / a[i][i];
	}
}


// Perform LDL^T decomposition of a symmetric positive definite matrix.
// Like Cholesky, but no square roots.  Overwrites lower triangle of matrix.
template <class T, int N>
static inline bool ldltdc(T A[N][N], T rdiag[N])
{
	T v[N-1];
	for (int i = 0; i < N; i++) {
		for (int k = 0; k < i; k++)
			v[k] = A[i][k] * rdiag[k];
		for (int j = i; j < N; j++) {
			T sum = A[i][j];
			for (int k = 0; k < i; k++)
				sum -= v[k] * A[j][k];
			if (i == j) {
				if (unlikely(sum <= T(0)))
					return false;
				rdiag[i] = T(1) / sum;
			} else {
				A[j][i] = sum;
			}
		}
	}

	return true;
}


// Solve Ax=B after ldltdc
template <class T, int N>
static inline void ldltsl(T A[N][N], T rdiag[N], T B[N], T x[N])
{
	for (int i = 0; i < N; i++) {
		T sum = B[i];
		for (int k = 0; k < i; k++)
			sum -= A[i][k] * x[k];
		x[i] = sum * rdiag[i];
	}
	for (int i = N - 1; i >= 0; i--) {
		T sum = 0;
		for (int k = i + 1; k < N; k++)
			sum += A[k][i] * x[k];
		x[i] -= sum * rdiag[i];
	}
}


// Eigenvector decomposition for real, symmetric matrices,
// a la Bowdler et al. / EISPACK / JAMA
// Entries of d are eigenvalues, sorted smallest to largest.
// A changed in-place to have its columns hold the corresponding eigenvectors.
// Note that A must be completely filled in on input.
template <class T, int N>
static inline void eigdc(T A[N][N], T d[N])
{
	using namespace ::std;

	// Householder
	T e[N];
	for (int j = 0; j < N; j++) {
		d[j] = A[N-1][j];
		e[j] = T(0);
	}
	for (int i = N-1; i > 0; i--) {
		T scale = T(0);
		for (int k = 0; k < i; k++)
			scale += fabs(d[k]);
		if (scale == T(0)) {
			e[i] = d[i-1];
			for (int j = 0; j < i; j++) {
				d[j] = A[i-1][j];
				A[i][j] = A[j][i] = T(0);
			}
			d[i] = T(0);
		} else {
			T h(0);
			T invscale = T(1) / scale;
			for (int k = 0; k < i; k++) {
				d[k] *= invscale;
				h += sqr(d[k]);
			}
			T f = d[i-1];
			T g = (f > T(0)) ? -sqrt(h) : sqrt(h);
			e[i] = scale * g;
			h -= f * g;
			d[i-1] = f - g;
			for (int j = 0; j < i; j++)
				e[j] = T(0);
			for (int j = 0; j < i; j++) {
				f = d[j];
				A[j][i] = f;
				g = e[j] + f * A[j][j];
				for (int k = j+1; k < i; k++) {
					g += A[k][j] * d[k];
					e[k] += A[k][j] * f;
				}
				e[j] = g;
			}
			f = T(0);
			T invh = T(1) / h;
			for (int j = 0; j < i; j++) {
				e[j] *= invh;
				f += e[j] * d[j];
			}
			T hh = f / (h + h);
			for (int j = 0; j < i; j++)
				e[j] -= hh * d[j];
			for (int j = 0; j < i; j++) {
				f = d[j];
				g = e[j];
				for (int k = j; k < i; k++)
					A[k][j] -= f * e[k] + g * d[k];
				d[j] = A[i-1][j];
				A[i][j] = T(0);
			}
			d[i] = h;
		}
	}

	for (int i = 0; i < N-1; i++) {
		A[N-1][i] = A[i][i];
		A[i][i] = T(1);
		T h = d[i+1];
		if (h != T(0)) {
			T invh = T(1) / h;
			for (int k = 0; k <= i; k++)
				d[k] = A[k][i+1] * invh;
			for (int j = 0; j <= i; j++) {
				T g = T(0);
				for (int k = 0; k <= i; k++)
					g += A[k][i+1] * A[k][j];
				for (int k = 0; k <= i; k++)
					A[k][j] -= g * d[k];
			}
		}
		for (int k = 0; k <= i; k++)
			A[k][i+1] = T(0);
	}
	for (int j = 0; j < N; j++) {
		d[j] = A[N-1][j];
		A[N-1][j] = T(0);
	}
	A[N-1][N-1] = T(1);

	// QL
	for (int i = 1; i < N; i++)
		e[i-1] = e[i];
	e[N-1] = T(0);
	T f = T(0), tmp = T(0);
	const T eps = ::std::numeric_limits<T>::epsilon();
	for (int l = 0; l < N; l++) {
		tmp = max(tmp, fabs(d[l]) + fabs(e[l]));
		int m = l;
		while (m < N) {
			if (fabs(e[m]) <= eps * tmp)
				break;
			m++;
		}
		if (m > l) {
			do {
				T g = d[l];
				T p = (d[l+1] - g) / (e[l] + e[l]);
				T r = T(hypot(p, T(1)));
				if (p < T(0))
					r = -r;
				d[l] = e[l] / (p + r);
				d[l+1] = e[l] * (p + r);
				T dl1 = d[l+1];
				T h = g - d[l];
				for (int i = l+2; i < N; i++)
					d[i] -= h; 
				f += h;
				p = d[m];   
				T c = T(1), c2 = T(1), c3 = T(1);
				T el1 = e[l+1], s = T(0), s2 = T(0);
				for (int i = m - 1; i >= l; i--) {
					c3 = c2;
					c2 = c;
					s2 = s;
					g = c * e[i];
					h = c * p;
					r = T(hypot(p, e[i]));
					e[i+1] = s * r;
					s = e[i] / r;
					c = p / r;
					p = c * d[i] - s * g;
					d[i+1] = h + s * (c * g + s * d[i]);
					for (int k = 0; k < N; k++) {
						h = A[k][i+1];
						A[k][i+1] = s * A[k][i] + c * h;
						A[k][i] = c * A[k][i] - s * h;
					}
				}
				p = -s * s2 * c3 * el1 * e[l] / dl1;
				e[l] = s * p;
				d[l] = c * p;
			} while (fabs(e[l]) > eps * tmp);
		}
		d[l] += f;
		e[l] = T(0);
	}

	// Sort
	for (int i = 0; i < N-1; i++) {
		int k = i;
		T p = d[i];
		for (int j = i+1; j < N; j++) {
			if (d[j] < p) {
				k = j;
				p = d[j];
			}
		}
		if (k == i)
			continue;
		d[k] = d[i];
		d[i] = p;
		for (int j = 0; j < N; j++)
			swap(A[j][i], A[j][k]);
	}
}


// x <- A * d * A' * b
template <class T, int N>
static inline void eigmult(T A[N][N],
			   T d[N],
			   T b[N],
			   T x[N])
{
	T e[N];
	for (int i = 0; i < N; i++) {
		e[i] = T(0);
		for (int j = 0; j < N; j++)
			e[i] += A[j][i] * b[j];
		e[i] *= d[i];
	}
	for (int i = 0; i < N; i++) {
		x[i] = T(0);
		for (int j = 0; j < N; j++)
			x[i] += A[i][j] * e[j];
	}
}

}; // namespace trimesh

#endif
