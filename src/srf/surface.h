//-----------------------------------------------------------------------------
// Functions relating to rational polynomial surfaces, which are trimmed by
// curves (either rational polynomial curves, or piecewise linear
// approximations to curves of intersection that can't be represented
// exactly in ratpoly form), and assembled into watertight shells.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_SURFACE_H
#define SOLVESPACE_SURFACE_H

#include "dsc.h"
#include "sedge.h"
#include "smesh.h"
#include "group.h"
#include "sbezier.h"
#include "scurve.h"
#include "ssurface.h"
#include "sshell.h"
#include "sbspuv.h"

namespace SolveSpace {

// Utility functions, Bernstein polynomials of order 1-3 and their derivatives.
double Bernstein(int k, int deg, double t);
double BernsteinDerivative(int k, int deg, double t);

class SBezierList;
class SSurface;
class SCurvePt;


// Now the data structures to represent a shell of trimmed rational polynomial
// surfaces.



}

#endif

