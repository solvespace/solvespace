//-----------------------------------------------------------------------------
// Routines to merge multiple coincident surfaces (each with their own trim
// curves) into a single surface, with all of the trim curves.
//-----------------------------------------------------------------------------
#include "../solvespace.h"

void SShell::MergeCoincidentSurfaces(void) {
    surface.ClearTags();

    int i, j;
    SSurface *si, *sj;

    for(i = 0; i < surface.n; i++) {
        si = &(surface.elem[i]);
        if(si->tag) continue;
        // Let someone else clean up the empty surfaces; we can certainly merge
        // them, but we don't know how to calculate a reasonable bounding box.
        if(si->trim.n == 0) continue;

        SEdgeList sel;
        ZERO(&sel);

        bool merged = false;

        for(j = i + 1; j < surface.n; j++) {
            sj = &(surface.elem[j]);
            if(sj->tag) continue;
            if(!sj->CoincidentWith(si, true)) continue;
            if(sj->color != si->color) continue;
            // But we do merge surfaces with different face entities, since
            // otherwise we'd hardly ever merge anything.

            // This surface is coincident, so it gets merged.
            sj->tag = 1;
            merged = true;
            sj->MakeEdgesInto(this, &sel, false);
            sj->trim.Clear();

            // All the references to this surface get replaced with the new srf
            SCurve *sc;
            for(sc = curve.First(); sc; sc = curve.NextAfter(sc)) {
                if(sc->surfA.v == sj->h.v) sc->surfA = si->h;
                if(sc->surfB.v == sj->h.v) sc->surfB = si->h;
            }
        }

        if(merged) {
            si->MakeEdgesInto(this, &sel, false);
            sel.CullExtraneousEdges();
            si->trim.Clear();
            si->TrimFromEdgeList(&sel, false);

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
    }

    surface.RemoveTagged();
}

