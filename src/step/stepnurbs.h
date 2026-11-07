// Exact NURBS -> native Bezier operations. No STEPcode or foreign kernel.
#ifndef SOLVESPACE_STEP_NURBS_H
#define SOLVESPACE_STEP_NURBS_H

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "srf/surface.h"

namespace SolveSpace {
namespace Step {
namespace Nurbs {

struct Error {
    int entity;
    bool unsupported;
    std::string message;
};

const size_t MAX_CONTROLS = 4096;
const size_t MAX_PATCHES = 4096;

struct Knots {
    int entity, degree;
    size_t controls;
    std::vector<double> full; // normalized to the active finite domain [0,1]
    std::vector<double> breaks;

    Knots(int entity, int degree, size_t controls, const std::vector<int> &multiplicities,
          const std::vector<double> &values);
};

struct CurveSpan {
    SBezier bezier;
    double first, last;
};

struct SurfacePatch {
    SSurface surface;
    double u0, u1, v0, v1;
    int iu, iv;
};

void CheckWeights(int entity, const std::vector<double> &weights);
std::vector<CurveSpan> Curve(const Knots &knots, const std::vector<Vector> &controls,
                             const std::vector<double> &weights);
std::vector<SurfacePatch> Surface(const Knots &u, const Knots &v,
                                 const std::vector<std::vector<Vector>> &controls,
                                 const std::vector<std::vector<double>> &weights);

// These restrict existing rational polynomials, never fit new ones.
SBezier Restrict(const SBezier &bezier, double first, double last);
SBezier IsoCurve(const SSurface &surface, bool constantU, bool atFinish);
void Transpose(SSurface *surface);

// All matches on the finite curve, including endpoints. No extrapolation.
std::vector<double> Parameters(const SBezier &bezier, Vector point,
                               double tolerance = LENGTH_EPS);
std::vector<double> SortedCuts(const SBezier &bezier, std::vector<double> cuts);

// Recognition for truthful straight-edge references only. Collinear controls
// need not be ordered, but the rational curve itself must traverse the segment
// monotonically. Neither the curve nor its parameterization is replaced.
bool IsMonotoneLine(const SBezier &bezier, Vector start, Vector finish, double tolerance);

// Used only to split exact boundaries at exact native patch seams. Subdivision
// and numerical root refinement do not replace either input curve. Ambiguous
// or unresolvable intersections fail explicitly, with a bounded work budget.
std::vector<std::pair<double, double>> Intersections(int entity, const SBezier &a,
                                                    const SBezier &b);

} // namespace Nurbs
} // namespace Step
} // namespace SolveSpace

#endif
