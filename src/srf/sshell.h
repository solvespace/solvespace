//
// Created by benjamin on 9/27/18.
//

#ifndef SOLVESPACE_SSHELL_H
#define SOLVESPACE_SSHELL_H

#include "sbezier.h"
#include "ssurface.h"
#include "scurve.h"
#include "smesh.h"

namespace SolveSpace{

// TODO fix circular dependency
class Group;

class SShell {
public:
    IdList<SCurve,hSCurve>      curve;
    IdList<SSurface,hSSurface>  surface;

    bool                        booleanFailed;

    void MakeFromExtrusionOf(SBezierLoopSet *sbls, Vector t0, Vector t1,
                             RgbaColor color);
    void MakeFromRevolutionOf(SBezierLoopSet *sbls, Vector pt, Vector axis,
                              RgbaColor color, Group *group);

    void MakeFromUnionOf(SShell *a, SShell *b);
    void MakeFromDifferenceOf(SShell *a, SShell *b);
    void MakeFromBoolean(SShell *a, SShell *b, SSurface::CombineAs type);
    void CopyCurvesSplitAgainst(bool opA, SShell *agnst, SShell *into);
    void CopySurfacesTrimAgainst(SShell *sha, SShell *shb, SShell *into, SSurface::CombineAs type);
    void MakeIntersectionCurvesAgainst(SShell *against, SShell *into);
    void MakeClassifyingBsps(SShell *useCurvesFrom);
    void AllPointsIntersecting(Vector a, Vector b, List<SInter> *il,
                               bool asSegment, bool trimmed, bool inclTangent);
    void MakeCoincidentEdgesInto(SSurface *proto, bool sameNormal,
                                 SEdgeList *el, SShell *useCurvesFrom);
    void RewriteSurfaceHandlesForCurves(SShell *a, SShell *b);
    void CleanupAfterBoolean();

    // Definitions when classifying regions of a surface; it is either inside,
    // outside, or coincident (with parallel or antiparallel normal) with a
    // shell.
    enum class Class : uint32_t {
        INSIDE     = 100,
        OUTSIDE    = 200,
        COINC_SAME = 300,
        COINC_OPP  = 400
    };
    static const double DOTP_TOL;
    Class ClassifyRegion(Vector edge_n, Vector inter_surf_n,
                         Vector edge_surf_n) const;

    bool ClassifyEdge(Class *indir, Class *outdir,
                      Vector ea, Vector eb,
                      Vector p, Vector edge_n_in,
                      Vector edge_n_out, Vector surf_n);

    void MakeFromCopyOf(SShell *a);
    void MakeFromTransformationOf(SShell *a,
                                  Vector trans, Quaternion q, double scale);
    void MakeFromAssemblyOf(SShell *a, SShell *b);
    void MergeCoincidentSurfaces();

    void TriangulateInto(SMesh *sm);
    void MakeEdgesInto(SEdgeList *sel);
    void MakeSectionEdgesInto(Vector n, double d, SEdgeList *sel, SBezierList *sbl);
    bool IsEmpty() const;
    void RemapFaces(Group *g, int remap);
    void Clear();

};

}

#endif //SOLVESPACE_SSHELL_H
