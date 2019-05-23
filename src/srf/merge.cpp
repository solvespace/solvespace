//-----------------------------------------------------------------------------
// Routines to merge multiple coincident surfaces (each with their own trim
// curves) into a single surface, with all of the trim curves.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "../solvespace.h"

void SShell::MergeCoincidentSurfaces() {
    surface.ClearTags();

    int i, j;
    SSurface *si, *sj;

    for(i = 0; i < surface.n; i++) {
        si = &(surface[i]);
        if(si->tag) continue;
        // Let someone else clean up the empty surfaces; we can certainly merge
        // them, but we don't know how to calculate a reasonable bounding box.
        if(si->trim.IsEmpty())
            continue;
        // And for now we handle only coincident planes, so no sense wasting
        // time on other surfaces.
        if(si->degm != 1 || si->degn != 1) continue;

        SEdgeList sel = {};
        si->MakeEdgesInto(this, &sel, SSurface::MakeAs::XYZ);

        bool mergedThisTime, merged = false;
        do {
            mergedThisTime = false;

            for(j = i + 1; j < surface.n; j++) {
                sj = &(surface[j]);
                if(sj->tag) continue;
                if(!sj->CoincidentWith(si, /*sameNormal=*/true)) continue;
                if(!sj->color.Equals(si->color)) continue;
                // But we do merge surfaces with different face entities, since
                // otherwise we'd hardly ever merge anything.

                // This surface is coincident. But let's not merge coincident
                // surfaces if they contain disjoint contours; that just makes
                // the bounding box tests less effective, and possibly things
                // less robust.
                SEdgeList tel = {};
                sj->MakeEdgesInto(this, &tel, SSurface::MakeAs::XYZ);
                if(!sel.ContainsEdgeFrom(&tel)) {
                    tel.Clear();
                    continue;
                }
                tel.Clear();

                sj->tag = 1;
                merged = true;
                mergedThisTime = true;
                sj->MakeEdgesInto(this, &sel, SSurface::MakeAs::XYZ);
                sj->trim.Clear();

                // All the references to this surface get replaced with the
                // new srf
                SCurve *sc;
                for(sc = curve.First(); sc; sc = curve.NextAfter(sc)) {
                    if(sc->surfA == sj->h) sc->surfA = si->h;
                    if(sc->surfB == sj->h) sc->surfB = si->h;
                }
            }

            // If this iteration merged a contour onto ours, then we have to
            // go through the surfaces again; that might have made a new
            // surface touch us.
        } while(mergedThisTime);

        if(merged) {
            sel.CullExtraneousEdges();
            si->trim.Clear();
            si->TrimFromEdgeList(&sel, /*asUv=*/false);

            // And we must choose control points such that all the trims lie
            // with u and v in [0, 1], so that the bbox tests work.
            Vector u, v, n;
            si->TangentsAt(0.5, 0.5, &u, &v);
            u = u.WithMagnitude(1);
            v = v.WithMagnitude(1);
            n = si->NormalAt(0.5, 0.5).WithMagnitude(1);
            v = (n.Cross(u)).WithMagnitude(1);

            double umax = VERY_NEGATIVE, umin = VERY_POSITIVE,
                   vmax = VERY_NEGATIVE, vmin = VERY_POSITIVE;
            SEdge *se;
            for(se = sel.l.First(); se; se = sel.l.NextAfter(se)) {
                double ut = (se->a).Dot(u), vt = (se->a).Dot(v);
                umax = max(umax, ut);
                vmax = max(vmax, vt);
                umin = min(umin, ut);
                vmin = min(vmin, vt);
            }

            // An interesting problem here; the real curve could extend
            // slightly beyond the bounding box of the piecewise linear
            // bits. Not a problem for us, but some apps won't import STEP
            // in that case. So give a bit of extra room; in theory just
            // a chord tolerance, but more can't hurt.
            double muv = max((umax - umin), (vmax - vmin));
            double tol = muv/50 + 3*SS.ChordTolMm();
            umax += tol;
            vmax += tol;
            umin -= tol;
            vmin -= tol;

            // We move in the +v direction as v goes from 0 to 1, and in the
            // +u direction as u goes from 0 to 1. So our normal ends up
            // pointed the same direction.
            double nt = (si->ctrl[0][0]).Dot(n);
            si->ctrl[0][0] =
                Vector::From(umin, vmin, nt).ScaleOutOfCsys(u, v, n);
            si->ctrl[0][1] =
                Vector::From(umin, vmax, nt).ScaleOutOfCsys(u, v, n);
            si->ctrl[1][1] =
                Vector::From(umax, vmax, nt).ScaleOutOfCsys(u, v, n);
            si->ctrl[1][0] =
                Vector::From(umax, vmin, nt).ScaleOutOfCsys(u, v, n);
        }
        sel.Clear();
    }

    surface.RemoveTagged();
}

