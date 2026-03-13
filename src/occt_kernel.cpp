//-----------------------------------------------------------------------------
// OpenCASCADE Technology (OCCT) integration for SolveSpace.
//
// This file is compiled only when HAVE_OPENCASCADE is defined.
//
// Copyright 2024 The SolveSpace contributors.
//-----------------------------------------------------------------------------
#include "solvespace.h"

#ifdef HAVE_OPENCASCADE

// ---- OCCT geometry -------------------------------------------------------
#include <Geom_BSplineCurve.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2dAPI_PointsToBSpline.hxx>
#include <GeomConvert.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Trsf.hxx>
#include <Precision.hxx>
#include <ShapeAnalysis_Surface.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColgp_Array1OfPnt2d.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColStd_Array2OfReal.hxx>

// ---- OCCT topology -------------------------------------------------------
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepLib.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Wire.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>

// ---- OCCT Boolean ops ----------------------------------------------------
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>

// ---- OCCT shape healing --------------------------------------------------
#include <ShapeFix_Shape.hxx>

// ---- OCCT STEP export ----------------------------------------------------
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>

// ---- OCCT IGES export ----------------------------------------------------
#include <IGESControl_Writer.hxx>

namespace SolveSpace {

// ============================================================================
// Internal helpers
// ============================================================================

// Number of UV samples used when computing a 2-D pcurve for a trim edge.
static const int PCURVE_SAMPLES = 16;

//-----------------------------------------------------------------------------
// Convert a SolveSpace rational Bezier curve (SBezier) to an OCCT
// Geom_BSplineCurve.  A Bezier curve of degree d is a B-spline with a single
// span: knots = {0,1} and multiplicities = {d+1, d+1}.
//-----------------------------------------------------------------------------
static Handle(Geom_BSplineCurve) SBezierToGeomCurve(const SBezier &sb) {
    const int n = sb.deg + 1;           // number of poles

    TColgp_Array1OfPnt   poles(1, n);
    TColStd_Array1OfReal weights(1, n);

    bool isRational = false;
    for(int i = 0; i < n; i++) {
        poles  .SetValue(i + 1, gp_Pnt(sb.ctrl[i].x, sb.ctrl[i].y, sb.ctrl[i].z));
        weights.SetValue(i + 1, sb.weight[i]);
        if(fabs(sb.weight[i] - 1.0) > 1e-10) isRational = true;
    }

    // Clamped Bezier knot vector.
    TColStd_Array1OfReal    knots(1, 2);
    TColStd_Array1OfInteger mults(1, 2);
    knots.SetValue(1, 0.0); knots.SetValue(2, 1.0);
    mults.SetValue(1, n);   mults.SetValue(2, n);

    if(isRational) {
        return new Geom_BSplineCurve(poles, weights, knots, mults, sb.deg);
    } else {
        return new Geom_BSplineCurve(poles, knots, mults, sb.deg);
    }
}

//-----------------------------------------------------------------------------
// Convert a SolveSpace rational Bezier patch (SSurface) to an OCCT
// Geom_BSplineSurface.  Like the curve case, a single-span Bezier patch uses
// clamped knot vectors {0,1} in both directions.
//-----------------------------------------------------------------------------
static Handle(Geom_BSplineSurface) SSurfaceToGeomSurface(const SSurface &ss) {
    const int m = ss.degm + 1;          // poles in u
    const int n = ss.degn + 1;          // poles in v

    TColgp_Array2OfPnt   poles(1, m, 1, n);
    TColStd_Array2OfReal weights(1, m, 1, n);

    bool isRational = false;
    for(int i = 0; i < m; i++) {
        for(int j = 0; j < n; j++) {
            const Vector &p = ss.ctrl[i][j];
            const double  w = ss.weight[i][j];
            poles  .SetValue(i + 1, j + 1, gp_Pnt(p.x, p.y, p.z));
            weights.SetValue(i + 1, j + 1, w);
            if(fabs(w - 1.0) > 1e-10) isRational = true;
        }
    }

    // Clamped Bezier knot vectors.
    TColStd_Array1OfReal    uKnots(1, 2), vKnots(1, 2);
    TColStd_Array1OfInteger uMults(1, 2), vMults(1, 2);
    uKnots.SetValue(1, 0.0); uKnots.SetValue(2, 1.0);
    vKnots.SetValue(1, 0.0); vKnots.SetValue(2, 1.0);
    uMults.SetValue(1, m);   uMults.SetValue(2, m);
    vMults.SetValue(1, n);   vMults.SetValue(2, n);

    if(isRational) {
        return new Geom_BSplineSurface(
            poles, weights, uKnots, vKnots, uMults, vMults, ss.degm, ss.degn);
    } else {
        return new Geom_BSplineSurface(
            poles, uKnots, vKnots, uMults, vMults, ss.degm, ss.degn);
    }
}

//-----------------------------------------------------------------------------
// Compute the 2-D parameter-space (pcurve) representation of a trim curve.
//
// The trim curve is given as a SBezier in 3-D space.  We sample it at
// PCURVE_SAMPLES evenly spaced parameter values, project each 3-D sample onto
// the OCCT surface using ShapeAnalysis_Surface, and fit a degree-3 B-spline
// through the resulting UV points.
//
// The resulting pcurve is required by BRepBuilderAPI_MakeEdge to properly
// anchor an edge on a parametric surface face.
//-----------------------------------------------------------------------------
static Handle(Geom2d_BSplineCurve) ComputePCurve(
        const SBezier &sb,
        const Handle(Geom_BSplineSurface) &geomSurf)
{
    TColgp_Array1OfPnt2d uvPts(1, PCURVE_SAMPLES);

    Handle(ShapeAnalysis_Surface) sas = new ShapeAnalysis_Surface(geomSurf);

    for(int i = 0; i < PCURVE_SAMPLES; i++) {
        const double t   = (double)i / (PCURVE_SAMPLES - 1);
        const Vector p3d = sb.PointAt(t);
        const gp_Pnt gpPt(p3d.x, p3d.y, p3d.z);

        gp_Pnt2d uv = sas->ValueOfUV(gpPt, Precision::Confusion());
        uvPts.SetValue(i + 1, uv);
    }

    const int degree = std::min(3, PCURVE_SAMPLES - 1);
    Geom2dAPI_PointsToBSpline approx(uvPts, degree, degree);
    if(!approx.IsDone()) return Handle(Geom2d_BSplineCurve)();
    return approx.Curve();
}

//-----------------------------------------------------------------------------
// Build an OCCT TopoDS_Wire from a SBezierLoop for the given surface.
//
// Each SBezier in the loop becomes a TopoDS_Edge constructed from a 2-D
// pcurve on the surface (so that OCCT can properly trim the face).  As a
// safety fallback, if the pcurve computation fails the 3-D curve is used
// directly.
//-----------------------------------------------------------------------------
static TopoDS_Wire BuildOCCTWire(
        const SBezierLoop &loop,
        const Handle(Geom_BSplineSurface) &geomSurf)
{
    BRepBuilderAPI_MakeWire mkWire;

    for(const SBezier *sb = loop.l.First(); sb; sb = loop.l.NextAfter(sb)) {
        Handle(Geom2d_BSplineCurve) pcurve = ComputePCurve(*sb, geomSurf);

        TopoDS_Edge edge;
        if(!pcurve.IsNull()) {
            // Build the edge from the 2-D pcurve on the surface; OCCT will
            // derive the 3-D representation automatically.
            BRepBuilderAPI_MakeEdge mkEdge(pcurve, geomSurf, 0.0, 1.0);
            if(mkEdge.IsDone()) {
                edge = mkEdge.Edge();

                // Also register the exact 3-D B-spline for tessellation /
                // visualisation quality.
                Handle(Geom_BSplineCurve) curve3D = SBezierToGeomCurve(*sb);
                BRep_Builder builder;
                builder.UpdateEdge(edge, curve3D, Precision::Confusion());
                builder.SameRange(edge, Standard_False);
                builder.SameParameter(edge, Standard_False);
            }
        }

        // Fallback: 3-D only (the face may be non-manifold but still usable
        // for export).
        if(edge.IsNull()) {
            Handle(Geom_BSplineCurve) curve3D = SBezierToGeomCurve(*sb);
            BRepBuilderAPI_MakeEdge mkEdge(curve3D, 0.0, 1.0);
            if(mkEdge.IsDone()) edge = mkEdge.Edge();
        }

        if(!edge.IsNull()) mkWire.Add(edge);
    }

    if(!mkWire.IsDone()) return TopoDS_Wire();
    return mkWire.Wire();
}

//-----------------------------------------------------------------------------
// Convert a SolveSpace SShell to an OCCT TopoDS_Shape.
//
// Each SSurface with trim curves becomes an OCCT TopoDS_Face.  The faces are
// stitched together with BRepBuilderAPI_Sewing; if the result is a closed
// shell it is promoted to a solid.  ShapeFix_Shape is run at the end to
// correct any minor topology issues.
//-----------------------------------------------------------------------------
static TopoDS_Shape SShellToOCCTShape(SShell *shell) {
    BRepBuilderAPI_Sewing sewing(Precision::Confusion() * 10.0);

    for(SSurface &ss : shell->surface) {
        if(ss.trim.IsEmpty()) continue;

        Handle(Geom_BSplineSurface) geomSurf = SSurfaceToGeomSurface(ss);

        // Collect all trim curves for this surface as exact SBeziers.
        SBezierList sbl = {};
        ss.MakeSectionEdgesInto(shell, /*sel=*/NULL, &sbl);

        // Group the trim curves into outer loops + their holes.
        SBezierLoopSetSet sblss = {};
        SPolygon          spxyz = {};
        bool     allClosed = false;
        SEdge    notClosedAt;
        sblss.FindOuterFacesFrom(&sbl, &spxyz, &ss,
                                 SS.ExportChordTolMm(),
                                 &allClosed, &notClosedAt,
                                 /*allCoplanar=*/NULL, /*notCoplanarAt=*/NULL,
                                 /*openContours=*/NULL);

        for(SBezierLoopSet *sbls = sblss.l.First();
                            sbls;
                            sbls = sblss.l.NextAfter(sbls))
        {
            BRepBuilderAPI_MakeFace mkFace(geomSurf, Precision::Confusion());

            for(SBezierLoop *loop = sbls->l.First();
                             loop;
                             loop = sbls->l.NextAfter(loop))
            {
                TopoDS_Wire wire = BuildOCCTWire(*loop, geomSurf);
                if(!wire.IsNull()) mkFace.Add(wire);
            }

            if(mkFace.IsDone()) sewing.Add(mkFace.Face());
        }

        sblss.Clear();
        spxyz.Clear();
        sbl.Clear();
    }

    sewing.Perform();
    TopoDS_Shape shape = sewing.SewedShape();
    if(shape.IsNull()) return shape;

    // Promote a closed shell to a solid so that Boolean operations work.
    if(shape.ShapeType() == TopAbs_SHELL) {
        BRepBuilderAPI_MakeSolid mkSolid;
        mkSolid.Add(TopoDS::Shell(shape));
        if(mkSolid.IsDone()) shape = mkSolid.Solid();
    }

    // Heal minor topology defects.
    ShapeFix_Shape fixer(shape);
    fixer.Perform();
    return fixer.Shape();
}

//-----------------------------------------------------------------------------
// Convert an OCCT TopoDS_Shape back to a SolveSpace SShell.
//
// Only B-spline surfaces that fit within SolveSpace's single-span, degree ≤ 3
// constraint are converted; other surface types are approximated using
// GeomConvert.  Trim curves are extracted from the face boundary edges and
// converted to SBezier curves via the SCurve mechanism.
//
// Returns true if at least one surface was converted successfully.
//-----------------------------------------------------------------------------
static bool OCCTShapeToSShell(const TopoDS_Shape &shape, SShell *shell) {
    if(shape.IsNull()) return false;

    int convertedFaces = 0;

    TopExp_Explorer faceExp(shape, TopAbs_FACE);
    for(; faceExp.More(); faceExp.Next()) {
        const TopoDS_Face &face = TopoDS::Face(faceExp.Current());
        const bool faceReversed = (face.Orientation() == TopAbs_REVERSED);

        TopLoc_Location loc;
        Handle(Geom_Surface) geomSurf = BRep_Tool::Surface(face, loc);
        if(geomSurf.IsNull()) continue;

        // Try to obtain (or convert to) a single-span B-spline surface.
        Handle(Geom_BSplineSurface) bspline =
            Handle(Geom_BSplineSurface)::DownCast(geomSurf);
        if(bspline.IsNull()) {
            try {
                bspline = GeomConvert::SurfaceToBSplineSurface(geomSurf);
            } catch(...) {}
        }
        if(bspline.IsNull()) continue;

        const int uDeg = bspline->UDegree();
        const int vDeg = bspline->VDegree();

        // SolveSpace surfaces are single-span Bezier patches of degree ≤ 3.
        if(uDeg > 3 || vDeg > 3) continue;
        if(bspline->NbUKnots() - 1 != 1 || bspline->NbVKnots() - 1 != 1) continue;

        const int m = uDeg + 1;   // poles in u
        const int n = vDeg + 1;   // poles in v
        if(m > 4 || n > 4) continue;

        SSurface ss = {};
        ss.degm = uDeg;
        ss.degn = vDeg;

        const TColgp_Array2OfPnt &poles = bspline->Poles();
        TColStd_Array2OfReal weights(1, bspline->NbUPoles(),
                                     1, bspline->NbVPoles());
        if(bspline->IsURational() || bspline->IsVRational()) {
            bspline->Weights(weights);
        } else {
            weights.Init(1.0);
        }

        const gp_Trsf trsf = loc.IsIdentity()
                           ? gp_Trsf()
                           : loc.Transformation();

        for(int i = 0; i < m; i++) {
            for(int j = 0; j < n; j++) {
                gp_Pnt pt = poles.Value(i + 1, j + 1);
                if(!loc.IsIdentity()) pt.Transform(trsf);

                // If the face orientation is reversed, swap u-parameter
                // direction to keep outward normals consistent.
                const int ii = faceReversed ? (m - 1 - i) : i;
                ss.ctrl[ii][j]   = Vector::From(pt.X(), pt.Y(), pt.Z());
                ss.weight[ii][j] = weights.Value(i + 1, j + 1);
            }
        }

        // Extract trim curves from the face wire edges.
        TopExp_Explorer edgeExp(face, TopAbs_EDGE);
        for(; edgeExp.More(); edgeExp.Next()) {
            const TopoDS_Edge &edge = TopoDS::Edge(edgeExp.Current());
            Standard_Real first = 0.0, last = 1.0;
            Handle(Geom_Curve) curve3D = BRep_Tool::Curve(edge, first, last);
            if(curve3D.IsNull()) continue;

            Handle(Geom_BSplineCurve) bsplCurve =
                Handle(Geom_BSplineCurve)::DownCast(curve3D);
            if(bsplCurve.IsNull()) {
                try {
                    bsplCurve = GeomConvert::CurveToBSplineCurve(curve3D);
                } catch(...) {}
            }
            if(bsplCurve.IsNull()) continue;

            // Only single-span degree ≤ 3 curves fit in SBezier.
            const int deg = bsplCurve->Degree();
            if(deg > 3) continue;
            if(bsplCurve->NbKnots() - 1 != 1) continue;

            const int np = deg + 1;
            if(np > 4) continue;   // guard the fixed-size ctrl/weight arrays
            const TColgp_Array1OfPnt &cPoles = bsplCurve->Poles();
            TColStd_Array1OfReal cWeights(1, bsplCurve->NbPoles());
            if(bsplCurve->IsRational()) {
                bsplCurve->Weights(cWeights);
            } else {
                cWeights.Init(1.0);
            }

            // Build a new SCurve holding the exact SBezier.
            SCurve sc = {};
            sc.isExact       = true;
            sc.exact.deg     = deg;
            // Source is INTERSECTION since it comes from a Boolean result.
            sc.source        = SCurve::Source::INTERSECTION;

            for(int k = 0; k < np; k++) {
                gp_Pnt pt = cPoles.Value(k + 1);
                if(!loc.IsIdentity()) pt.Transform(trsf);
                sc.exact.ctrl[k]   = Vector::From(pt.X(), pt.Y(), pt.Z());
                sc.exact.weight[k] = cWeights.Value(k + 1);
            }

            hSCurve hc = shell->curve.AddAndAssignId(&sc);

            // Associate the trim with the surface.
            STrimBy stb = STrimBy::EntireCurve(shell, hc, /*backwards=*/false);
            ss.trim.Add(&stb);
        }

        shell->surface.AddAndAssignId(&ss);
        convertedFaces++;
    }

    return convertedFaces > 0;
}

// ============================================================================
// Public API
// ============================================================================

bool OCCTExportStepFile(const Platform::Path &filename, SShell *shell) {
    if(!shell || shell->surface.IsEmpty()) return false;

    TopoDS_Shape shape = SShellToOCCTShape(shell);
    if(shape.IsNull()) return false;

    // Write AP214 (the most widely supported STEP flavour).
    Interface_Static::SetCVal("write.step.schema", "AP214");
    Interface_Static::SetCVal("write.step.product.name", "SolveSpace Model");

    STEPControl_Writer writer;
    const IFSelect_ReturnStatus xfer =
        writer.Transfer(shape, STEPControl_AsIs);
    if(xfer != IFSelect_RetDone) return false;

    return writer.Write(filename.raw.c_str()) == IFSelect_RetDone;
}

bool OCCTExportIgesFile(const Platform::Path &filename, SShell *shell) {
    if(!shell || shell->surface.IsEmpty()) return false;

    TopoDS_Shape shape = SShellToOCCTShape(shell);
    if(shape.IsNull()) return false;

    IGESControl_Writer writer;
    writer.AddShape(shape);
    writer.ComputeModel();
    return writer.Write(filename.raw.c_str()) != 0;
}

// Internal helper shared by all three Boolean operations.
static bool OCCTBoolean(SShell *result, SShell *a, SShell *b, int opType) {
    TopoDS_Shape shapeA = SShellToOCCTShape(a);
    TopoDS_Shape shapeB = SShellToOCCTShape(b);
    if(shapeA.IsNull() || shapeB.IsNull()) return false;

    TopoDS_Shape shapeResult;
    switch(opType) {
        case 0: {
            BRepAlgoAPI_Fuse op(shapeA, shapeB);
            if(!op.IsDone()) return false;
            shapeResult = op.Shape();
            break;
        }
        case 1: {
            BRepAlgoAPI_Cut op(shapeA, shapeB);
            if(!op.IsDone()) return false;
            shapeResult = op.Shape();
            break;
        }
        default: {
            BRepAlgoAPI_Common op(shapeA, shapeB);
            if(!op.IsDone()) return false;
            shapeResult = op.Shape();
            break;
        }
    }

    if(shapeResult.IsNull()) return false;
    return OCCTShapeToSShell(shapeResult, result);
}

bool OCCTBooleanUnion(SShell *result, SShell *a, SShell *b) {
    return OCCTBoolean(result, a, b, 0);
}

bool OCCTBooleanDifference(SShell *result, SShell *a, SShell *b) {
    return OCCTBoolean(result, a, b, 1);
}

bool OCCTBooleanIntersection(SShell *result, SShell *a, SShell *b) {
    return OCCTBoolean(result, a, b, 2);
}

} // namespace SolveSpace

#endif // HAVE_OPENCASCADE
