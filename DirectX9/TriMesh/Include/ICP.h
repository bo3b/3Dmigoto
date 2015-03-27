#ifndef ICP_H
#define ICP_H
/*
Szymon Rusinkiewicz
Princeton University

ICP.h
Routines for doing ICP.
*/

#include "TriMesh.h"
#include "XForm.h"
#include "KDtree.h"


namespace trimesh {

// Determine which points on s1 and s2 overlap the other, filling in o1 and o2
// Also fills in maxdist, if it is <= 0 on input
extern void compute_overlaps(TriMesh *s1, TriMesh *s2,
			     const xform &xf1, const xform &xf2,
			     const KDtree *kd1, const KDtree *kd2,
			     ::std::vector<float> &o1, ::std::vector<float> &o2,
			     float &maxdist, int verbose);

// Do ICP.  Aligns mesh s2 to s1, updating xf2 with the new transform.
// Returns alignment error, or -1 on failure.
// Pass in 0 for maxdist to figure it out...
// Pass in vector<float>() for weights to figure it out...
extern float ICP(TriMesh *s1, TriMesh *s2,
		 const xform &xf1, xform &xf2,
		 const KDtree *kd1, const KDtree *kd2,
		 ::std::vector<float> &weights1, ::std::vector<float> &weights2,
		 float maxdist = 0.0f, int verbose = 0,
		 bool do_scale = false, bool do_affine = false);

// Easier-to-use interface to ICP
extern float ICP(TriMesh *s1, TriMesh *s2, const xform &xf1, xform &xf2,
		 int verbose = 0,
		 bool do_scale = false, bool do_affine = false);

}; // namespace trimesh

#endif
