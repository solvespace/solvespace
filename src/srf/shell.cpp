//-----------------------------------------------------------------------------
// Anything involving NURBS shells (i.e., shells); except
// for the real math, which is in ratpoly.cpp.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "../solvespace.h"

typedef struct {
    hSCurve     hc;
    hSSurface   hs;
} TrimLine;


void SShell::MakeFromExtrusionOf(SBezierLoopSet *sbls, Vector t0, Vector t1, RgbaColor color)
{
    // Make the extrusion direction consistent with respect to the normal
    // of the sketch we're extruding.
    if((t0.Minus(t1)).Dot(sbls->normal) < 0) {
        swap(t0, t1);
    }

    // Define a coordinate system to contain the original sketch, and get
    // a bounding box in that csys
    Vector n = sbls->normal.ScaledBy(-1);
    Vector u = n.Normal(0), v = n.Normal(1);
    Vector orig = sbls->point;
    double umax = VERY_NEGATIVE, umin = VERY_POSITIVE;
    sbls->GetBoundingProjd(u, orig, &umin, &umax);
    double vmax = VERY_NEGATIVE, vmin = VERY_POSITIVE;
    sbls->GetBoundingProjd(v, orig, &vmin, &vmax);
    // and now fix things up so that all u and v lie between 0 and 1
    orig = orig.Plus(u.ScaledBy(umin));
    orig = orig.Plus(v.ScaledBy(vmin));
    u = u.ScaledBy(umax - umin);
    v = v.ScaledBy(vmax - vmin);

    // So we can now generate the top and bottom surfaces of the extrusion,
    // planes within a translated (and maybe mirrored) version of that csys.
    SSurface s0, s1;
    s0 = SSurface::FromPlane(orig.Plus(t0), u, v);
    s0.color = color;
    s1 = SSurface::FromPlane(orig.Plus(t1).Plus(u), u.ScaledBy(-1), v);
    s1.color = color;
    hSSurface hs0 = surface.AddAndAssignId(&s0),
              hs1 = surface.AddAndAssignId(&s1);

    // Now go through the input curves. For each one, generate its surface
    // of extrusion, its two translated trim curves, and one trim line. We
    // go through by loops so that we can assign the lines correctly.
    SBezierLoop *sbl;
    for(sbl = sbls->l.First(); sbl; sbl = sbls->l.NextAfter(sbl)) {
        SBezier *sb;
        List<TrimLine> trimLines = {};

        for(sb = sbl->l.First(); sb; sb = sbl->l.NextAfter(sb)) {
            // Generate the surface of extrusion of this curve, and add
            // it to the list
            SSurface ss = SSurface::FromExtrusionOf(sb, t0, t1);
            ss.color = color;
            hSSurface hsext = surface.AddAndAssignId(&ss);

            // Translate the curve by t0 and t1 to produce two trim curves
            SCurve sc = {};
            sc.isExact = true;
            sc.exact = sb->TransformedBy(t0, Quaternion::IDENTITY, 1.0);
            (sc.exact).MakePwlInto(&(sc.pts));
            sc.surfA = hs0;
            sc.surfB = hsext;
            hSCurve hc0 = curve.AddAndAssignId(&sc);

            sc = {};
            sc.isExact = true;
            sc.exact = sb->TransformedBy(t1, Quaternion::IDENTITY, 1.0);
            (sc.exact).MakePwlInto(&(sc.pts));
            sc.surfA = hs1;
            sc.surfB = hsext;
            hSCurve hc1 = curve.AddAndAssignId(&sc);

            STrimBy stb0, stb1;
            // The translated curves trim the flat top and bottom surfaces.
            stb0 = STrimBy::EntireCurve(this, hc0, /*backwards=*/false);
            stb1 = STrimBy::EntireCurve(this, hc1, /*backwards=*/true);
            (surface.FindById(hs0))->trim.Add(&stb0);
            (surface.FindById(hs1))->trim.Add(&stb1);

            // The translated curves also trim the surface of extrusion.
            stb0 = STrimBy::EntireCurve(this, hc0, /*backwards=*/true);
            stb1 = STrimBy::EntireCurve(this, hc1, /*backwards=*/false);
            (surface.FindById(hsext))->trim.Add(&stb0);
            (surface.FindById(hsext))->trim.Add(&stb1);

            // And form the trim line
            Vector pt = sb->Finish();
            sc = {};
            sc.isExact = true;
            sc.exact = SBezier::From(pt.Plus(t0), pt.Plus(t1));
            (sc.exact).MakePwlInto(&(sc.pts));
            hSCurve hl = curve.AddAndAssignId(&sc);
            // save this for later
            TrimLine tl;
            tl.hc = hl;
            tl.hs = hsext;
            trimLines.Add(&tl);
        }

        int i;
        for(i = 0; i < trimLines.n; i++) {
            TrimLine *tl = &(trimLines[i]);
            SSurface *ss = surface.FindById(tl->hs);

            TrimLine *tlp = &(trimLines[WRAP(i-1, trimLines.n)]);

            STrimBy stb;
            stb = STrimBy::EntireCurve(this, tl->hc, /*backwards=*/true);
            ss->trim.Add(&stb);
            stb = STrimBy::EntireCurve(this, tlp->hc, /*backwards=*/false);
            ss->trim.Add(&stb);

            (curve.FindById(tl->hc))->surfA = ss->h;
            (curve.FindById(tlp->hc))->surfB = ss->h;
        }
        trimLines.Clear();
    }
}

bool SShell::CheckNormalAxisRelationship(SBezierLoopSet *sbls, Vector pt, Vector axis, double da, double dx)
// Check that the direction of revolution/extrusion ends up parallel to the normal of
// the sketch, on the side of the axis where the sketch is.
{
    SBezierLoop *sbl;
    Vector pto;
    double md = VERY_NEGATIVE;
    for(sbl = sbls->l.First(); sbl; sbl = sbls->l.NextAfter(sbl)) {
        SBezier *sb;
        for(sb = sbl->l.First(); sb; sb = sbl->l.NextAfter(sb)) {
            // Choose the point farthest from the axis; we'll get garbage
            // if we choose a point that lies on the axis, for example.
            // (And our surface will be self-intersecting if the sketch
            // spans the axis, so don't worry about that.)
            for(int i = 0; i <= sb->deg; i++) {
                Vector p = sb->ctrl[i];
                double d = p.DistanceToLine(pt, axis);
                if(d > md) {
                    md  = d;
                    pto = p;
                }
            }
        }
    }
    Vector ptc = pto.ClosestPointOnLine(pt, axis),
           up = axis.Cross(pto.Minus(ptc)).ScaledBy(da),
           vp = up.Plus(axis.ScaledBy(dx));
   
    return (vp.Dot(sbls->normal) > 0);
}

// sketch must not contain the axis of revolution as a non-construction line for helix
void SShell::MakeFromHelicalRevolutionOf(SBezierLoopSet *sbls, Vector pt, Vector axis,
                                         RgbaColor color, Group *group, double angles,
                                         double anglef, double dists, double distf) {
    int i0 = surface.n; // number of pre-existing surfaces
    SBezierLoop *sbl;
    // for testing - hard code the axial distance, and number of sections.
    // distance will need to be parameters in the future.
    double dist  = distf - dists;
    int sections = (int)(fabs(anglef - angles) / (PI / 2) + 1);
    double wedge = (anglef - angles) / sections;
    int startMapping = Group::REMAP_LATHE_START, endMapping = Group::REMAP_LATHE_END;

    if(CheckNormalAxisRelationship(sbls, pt, axis, anglef-angles, distf-dists)) {
        swap(angles, anglef);
        swap(dists, distf);
        dist  = -dist;
        wedge = -wedge;
        swap(startMapping, endMapping);
    }

    // Define a coordinate system to contain the original sketch, and get
    // a bounding box in that csys
    Vector n = sbls->normal.ScaledBy(-1);
    Vector u = n.Normal(0), v = n.Normal(1);
    Vector orig = sbls->point;
    double umax = VERY_NEGATIVE, umin = VERY_POSITIVE;
    sbls->GetBoundingProjd(u, orig, &umin, &umax);
    double vmax = VERY_NEGATIVE, vmin = VERY_POSITIVE;
    sbls->GetBoundingProjd(v, orig, &vmin, &vmax);
    // and now fix things up so that all u and v lie between 0 and 1
    orig = orig.Plus(u.ScaledBy(umin));
    orig = orig.Plus(v.ScaledBy(vmin));
    u    = u.ScaledBy(umax - umin);
    v    = v.ScaledBy(vmax - vmin);

    // So we can now generate the end caps of the extrusion within
    // a translated and rotated (and maybe mirrored) version of that csys.
    SSurface s0, s1;
    s0 = SSurface::FromPlane(orig.RotatedAbout(pt, axis, angles).Plus(axis.ScaledBy(dists)),
                             u.RotatedAbout(axis, angles), v.RotatedAbout(axis, angles));
    s0.color = color;

    hEntity face0 = group->Remap(Entity::NO_ENTITY, startMapping);
    s0.face = face0.v;

    s1 = SSurface::FromPlane(
        orig.Plus(u).RotatedAbout(pt, axis, anglef).Plus(axis.ScaledBy(distf)),
        u.ScaledBy(-1).RotatedAbout(axis, anglef), v.RotatedAbout(axis, anglef));
    s1.color = color;

    hEntity face1 = group->Remap(Entity::NO_ENTITY, endMapping);
    s1.face = face1.v;

    hSSurface hs0 = surface.AddAndAssignId(&s0);
    hSSurface hs1 = surface.AddAndAssignId(&s1);

    // Now we actually build and trim the swept surfaces. One loop at a time.
    for(sbl = sbls->l.First(); sbl; sbl = sbls->l.NextAfter(sbl)) {
        int i, j;
        SBezier *sb;
        List<std::vector<hSSurface>> hsl = {};

        // This is where all the NURBS are created and Remapped to the generating curve
        for(sb = sbl->l.First(); sb; sb = sbl->l.NextAfter(sb)) {
            std::vector<hSSurface> revs(sections);
            for(j = 0; j < sections; j++) {
                if((dist == 0) && sb->deg == 1 &&
                   (sb->ctrl[0]).DistanceToLine(pt, axis) < LENGTH_EPS &&
                   (sb->ctrl[1]).DistanceToLine(pt, axis) < LENGTH_EPS) {
                    // This is a line on the axis of revolution; it does
                    // not contribute a surface.
                    revs[j].v = 0;
                } else {
                    SSurface ss = SSurface::FromRevolutionOf(
                        sb, pt, axis, angles + (wedge)*j, angles + (wedge) * (j + 1),
                        dists + j * dist / sections, dists + (j + 1) * dist / sections);
                    ss.color = color;
                    if(sb->entity != 0) {
                        hEntity he;
                        he.v          = sb->entity;
                        hEntity hface = group->Remap(he, Group::REMAP_LINE_TO_FACE);
                        if(SK.entity.FindByIdNoOops(hface) != NULL) {
                            ss.face = hface.v;
                        }
                    }
                    revs[j] = surface.AddAndAssignId(&ss);
                }
            }
            hsl.Add(&revs);
        }
        // Still the same loop. Need to create trim curves
        for(i = 0; i < sbl->l.n; i++) {
            std::vector<hSSurface> revs = hsl[i], revsp = hsl[WRAP(i - 1, sbl->l.n)];

            sb = &(sbl->l[i]);

            // we will need the grid t-values for this entire row of surfaces
            List<double> t_values;
            t_values = {};
            if (revs[0].v) { 
                double ps = 0.0;
                t_values.Add(&ps);
                (surface.FindById(revs[0]))->MakeTriangulationGridInto(
                        &t_values, 0.0, 1.0, true, 0);
            }
            // we generate one more curve than we did surfaces
            for(j = 0; j <= sections; j++) {
                SCurve sc;
                Quaternion qs = Quaternion::From(axis, angles + wedge * j);
                // we want Q*(x - p) + p = Q*x + (p - Q*p)
                Vector ts =
                    pt.Minus(qs.Rotate(pt)).Plus(axis.ScaledBy(dists + j * dist / sections));

                // If this input curve generated a surface, then trim that
                // surface with the rotated version of the input curve.
                if(revs[0].v) { // not d[j] because crash on j==sections
                    sc         = {};
                    sc.isExact = true;
                    sc.exact   = sb->TransformedBy(ts, qs, 1.0);
                    // make the PWL for the curve based on t value list
                    for(int x = 0; x < t_values.n; x++) {
                        SCurvePt scpt;
                        scpt.tag    = 0;
                        scpt.p      = sc.exact.PointAt(t_values[x]);
                        scpt.vertex = (x == 0) || (x == (t_values.n - 1));
                        sc.pts.Add(&scpt);
                    }

                    // the surfaces already exists so trim with this curve
                    if(j < sections) {
                        sc.surfA = revs[j];
                    } else {
                        sc.surfA = hs1; // end cap
                    }

                    if(j > 0) {
                        sc.surfB = revs[j - 1];
                    } else {
                        sc.surfB = hs0; // staring cap
                    }

                    hSCurve hcb = curve.AddAndAssignId(&sc);

                    STrimBy stb;
                    stb = STrimBy::EntireCurve(this, hcb, /*backwards=*/true);
                    (surface.FindById(sc.surfA))->trim.Add(&stb);
                    stb = STrimBy::EntireCurve(this, hcb, /*backwards=*/false);
                    (surface.FindById(sc.surfB))->trim.Add(&stb);
                } else if(j == 0) { // curve was on the rotation axis and is shared by the end caps.
                    sc         = {};
                    sc.isExact = true;
                    sc.exact   = sb->TransformedBy(ts, qs, 1.0);
                    (sc.exact).MakePwlInto(&(sc.pts));
                    sc.surfA    = hs1; // end cap
                    sc.surfB    = hs0; // staring cap
                    hSCurve hcb = curve.AddAndAssignId(&sc);

                    STrimBy stb;
                    stb = STrimBy::EntireCurve(this, hcb, /*backwards=*/true);
                    (surface.FindById(sc.surfA))->trim.Add(&stb);
                    stb = STrimBy::EntireCurve(this, hcb, /*backwards=*/false);
                    (surface.FindById(sc.surfB))->trim.Add(&stb);
                }

                // And if this input curve and the one after it both generated
                // surfaces, then trim both of those by the appropriate
                // curve based on the control points.
                if((j < sections) && revs[j].v && revsp[j].v) {
                    SSurface *ss = surface.FindById(revs[j]);

                    sc         = {};
                    sc.isExact = true;
                    sc.exact   = SBezier::From(ss->ctrl[0][0], ss->ctrl[0][1], ss->ctrl[0][2]);
                    sc.exact.weight[1] = ss->weight[0][1];
                    double max_dt = 0.5;
                    if (sc.exact.deg > 1) max_dt = 0.125;
                    (sc.exact).MakePwlInto(&(sc.pts), 0.0, max_dt);
                    sc.surfA = revs[j];
                    sc.surfB = revsp[j];

                    hSCurve hcc = curve.AddAndAssignId(&sc);

                    STrimBy stb;
                    stb = STrimBy::EntireCurve(this, hcc, /*backwards=*/false);
                    (surface.FindById(sc.surfA))->trim.Add(&stb);
                    stb = STrimBy::EntireCurve(this, hcc, /*backwards=*/true);
                    (surface.FindById(sc.surfB))->trim.Add(&stb);
                }
            }
            t_values.Clear();
        }

        hsl.Clear();
    }

    if(dist == 0) {
        MakeFirstOrderRevolvedSurfaces(pt, axis, i0);
    }
}

void SShell::MakeFromRevolutionOf(SBezierLoopSet *sbls, Vector pt, Vector axis, RgbaColor color,
                                  Group *group) {
    int i0 = surface.n; // number of pre-existing surfaces
    SBezierLoop *sbl;

    if(CheckNormalAxisRelationship(sbls, pt, axis, 1.0, 0.0)) {
        axis = axis.ScaledBy(-1);
    }

    // Now we actually build and trim the surfaces.
    for(sbl = sbls->l.First(); sbl; sbl = sbls->l.NextAfter(sbl)) {
        int i, j;
        SBezier *sb;
        List<std::vector<hSSurface>> hsl = {};

        for(sb = sbl->l.First(); sb; sb = sbl->l.NextAfter(sb)) {
            std::vector<hSSurface> revs(4);
            for(j = 0; j < 4; j++) {
                if(sb->deg == 1 &&
                    (sb->ctrl[0]).DistanceToLine(pt, axis) < LENGTH_EPS &&
                    (sb->ctrl[1]).DistanceToLine(pt, axis) < LENGTH_EPS)
                {
                    // This is a line on the axis of revolution; it does
                    // not contribute a surface.
                    revs[j].v = 0;
                } else {
                    SSurface ss = SSurface::FromRevolutionOf(sb, pt, axis, (PI / 2) * j,
                                                             (PI / 2) * (j + 1), 0.0, 0.0);
                    ss.color = color;
                    if(sb->entity != 0) {
                        hEntity he;
                        he.v = sb->entity;
                        hEntity hface = group->Remap(he, Group::REMAP_LINE_TO_FACE);
                        if(SK.entity.FindByIdNoOops(hface) != NULL) {
                            ss.face = hface.v;
                        }
                    }
                    revs[j] = surface.AddAndAssignId(&ss);
                }
            }
            hsl.Add(&revs);
        }

        for(i = 0; i < sbl->l.n; i++) {
            std::vector<hSSurface> revs  = hsl[i],
                     revsp = hsl[WRAP(i-1, sbl->l.n)];

            sb   = &(sbl->l[i]);

            for(j = 0; j < 4; j++) {
                SCurve sc;
                Quaternion qs = Quaternion::From(axis, (PI/2)*j);
                // we want Q*(x - p) + p = Q*x + (p - Q*p)
                Vector ts = pt.Minus(qs.Rotate(pt));

                // If this input curve generate a surface, then trim that
                // surface with the rotated version of the input curve.
                if(revs[j].v) {
                    sc = {};
                    sc.isExact = true;
                    sc.exact = sb->TransformedBy(ts, qs, 1.0);
                    (sc.exact).MakePwlInto(&(sc.pts));
                    sc.surfA = revs[j];
                    sc.surfB = revs[WRAP(j-1, 4)];

                    hSCurve hcb = curve.AddAndAssignId(&sc);

                    STrimBy stb;
                    stb = STrimBy::EntireCurve(this, hcb, /*backwards=*/true);
                    (surface.FindById(sc.surfA))->trim.Add(&stb);
                    stb = STrimBy::EntireCurve(this, hcb, /*backwards=*/false);
                    (surface.FindById(sc.surfB))->trim.Add(&stb);
                }

                // And if this input curve and the one after it both generated
                // surfaces, then trim both of those by the appropriate
                // circle.
                if(revs[j].v && revsp[j].v) {
                    SSurface *ss = surface.FindById(revs[j]);

                    sc = {};
                    sc.isExact = true;
                    sc.exact = SBezier::From(ss->ctrl[0][0],
                                             ss->ctrl[0][1],
                                             ss->ctrl[0][2]);
                    sc.exact.weight[1] = ss->weight[0][1];
                    (sc.exact).MakePwlInto(&(sc.pts));
                    sc.surfA = revs[j];
                    sc.surfB = revsp[j];

                    hSCurve hcc = curve.AddAndAssignId(&sc);

                    STrimBy stb;
                    stb = STrimBy::EntireCurve(this, hcc, /*backwards=*/false);
                    (surface.FindById(sc.surfA))->trim.Add(&stb);
                    stb = STrimBy::EntireCurve(this, hcc, /*backwards=*/true);
                    (surface.FindById(sc.surfB))->trim.Add(&stb);
                }
            }
        }

        hsl.Clear();
    }

    MakeFirstOrderRevolvedSurfaces(pt, axis, i0);
}

void SShell::MakeFirstOrderRevolvedSurfaces(Vector pt, Vector axis, int i0) {
    int i;

    for(i = i0; i < surface.n; i++) {
        SSurface *srf = &(surface[i]);

        // Revolution of a line; this is potentially a plane, which we can
        // rewrite to have degree (1, 1).
        if(srf->degm == 1 && srf->degn == 2) {
            // close start, far start, far finish
            Vector cs, fs, ff;
            double d0, d1;
            d0 = (srf->ctrl[0][0]).DistanceToLine(pt, axis);
            d1 = (srf->ctrl[1][0]).DistanceToLine(pt, axis);

            if(d0 > d1) {
                cs = srf->ctrl[1][0];
                fs = srf->ctrl[0][0];
                ff = srf->ctrl[0][2];
            } else {
                cs = srf->ctrl[0][0];
                fs = srf->ctrl[1][0];
                ff = srf->ctrl[1][2];
            }

            // origin close, origin far
            Vector oc = cs.ClosestPointOnLine(pt, axis),
                   of = fs.ClosestPointOnLine(pt, axis);

            if(oc.Equals(of)) {
                // This is a plane, not a (non-degenerate) cone.
                Vector oldn = srf->NormalAt(0.5, 0.5);

                Vector u = fs.Minus(of), v;

                v = (axis.Cross(u)).WithMagnitude(1);

                double vm = (ff.Minus(of)).Dot(v);
                v = v.ScaledBy(vm);

                srf->degm = 1;
                srf->degn = 1;
                srf->ctrl[0][0] = of;
                srf->ctrl[0][1] = of.Plus(u);
                srf->ctrl[1][0] = of.Plus(v);
                srf->ctrl[1][1] = of.Plus(u).Plus(v);
                srf->weight[0][0] = 1;
                srf->weight[0][1] = 1;
                srf->weight[1][0] = 1;
                srf->weight[1][1] = 1;

                if(oldn.Dot(srf->NormalAt(0.5, 0.5)) < 0) {
                    swap(srf->ctrl[0][0], srf->ctrl[1][0]);
                    swap(srf->ctrl[0][1], srf->ctrl[1][1]);
                }
                continue;
            }

            if(fabs(d0 - d1) < LENGTH_EPS) {
                // This is a cylinder; so transpose it so that we'll recognize
                // it as a surface of extrusion.
                SSurface sn = *srf;

                // Transposing u and v flips the normal, so reverse u to
                // flip it again and put it back where we started.
                sn.degm = 2;
                sn.degn = 1;
                int dm, dn;
                for(dm = 0; dm <= 1; dm++) {
                    for(dn = 0; dn <= 2; dn++) {
                        sn.ctrl  [dn][dm] = srf->ctrl  [1-dm][dn];
                        sn.weight[dn][dm] = srf->weight[1-dm][dn];
                    }
                }

                *srf = sn;
                continue;
            }
        }
    }
}

void SShell::MakeFromCopyOf(SShell *a) {
    ssassert(this != a, "Can't make from copy of self");
    MakeFromTransformationOf(a,
        Vector::From(0, 0, 0), Quaternion::IDENTITY, 1.0);
}

void SShell::MakeFromTransformationOf(SShell *a,
                                      Vector t, Quaternion q, double scale)
{
    booleanFailed = false;
    surface.ReserveMore(a->surface.n);
    for(SSurface &s : a->surface) {
        SSurface n;
        n = SSurface::FromTransformationOf(&s, t, q, scale, /*includingTrims=*/true);
        surface.Add(&n); // keeping the old ID
    }

    curve.ReserveMore(a->curve.n);
    for(SCurve &c : a->curve) {
        SCurve n;
        n = SCurve::FromTransformationOf(&c, t, q, scale);
        curve.Add(&n); // keeping the old ID
    }
}

void SShell::MakeEdgesInto(SEdgeList *sel) {
    for(SSurface &s : surface) {
        s.MakeEdgesInto(this, sel, SSurface::MakeAs::XYZ);
    }
}

void SShell::MakeSectionEdgesInto(Vector n, double d, SEdgeList *sel, SBezierList *sbl)
{
    for(SSurface &s : surface) {
        if(s.CoincidentWithPlane(n, d)) {
            s.MakeSectionEdgesInto(this, sel, sbl);
        }
    }
}

void SShell::TriangulateInto(SMesh *sm) {
#pragma omp parallel for
    for(int i=0; i<surface.n; i++) {
        SSurface *s = &surface[i];
        SMesh m;
        s->TriangulateInto(this, &m);
        #pragma omp critical
        sm->MakeFromCopyOf(&m);
        m.Clear();
    }
}

bool SShell::IsEmpty() const {
    return surface.IsEmpty();
}

void SShell::Clear() {
    for(SSurface &s : surface) {
        s.Clear();
    }
    surface.Clear();

    for(SCurve &c : curve) {
        c.Clear();
    }
    curve.Clear();
}
