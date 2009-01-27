#include "solvespace.h"

void SShell::MakeFromUnionOf(SShell *a, SShell *b) {
    MakeFromBoolean(a, b, AS_UNION);
}

void SShell::MakeFromDifferenceOf(SShell *a, SShell *b) {
    MakeFromBoolean(a, b, AS_DIFFERENCE);
}

SCurve SCurve::MakeCopySplitAgainst(SShell *against) {
    SCurve ret;
    ret = *this;
    ZERO(&(ret.pts));

    Vector *p;
    for(p = pts.First(); p; p = pts.NextAfter(p)) {
        ret.pts.Add(p);
    }
    return ret;
}

void SShell::CopyCurvesSplitAgainst(SShell *against, SShell *into) {
    SCurve *sc;
    for(sc = curve.First(); sc; sc = curve.NextAfter(sc)) {
        SCurve scn = sc->MakeCopySplitAgainst(against);
        hSCurve hsc = into->curve.AddAndAssignId(&scn);
        // And note the new ID so that we can rewrite the trims appropriately
        sc->newH = hsc;
    }
}

void SShell::MakeEdgeListUseNewCurveIds(SEdgeList *el) {
    SEdge *se;
    for(se = el->l.First(); se; se = el->l.NextAfter(se)) {
        hSCurve oldh = { se->auxA };
        SCurve *osc = curve.FindById(oldh);
        se->auxA = osc->newH.v;
        // auxB is the direction, which is unchanged
    }
}

void SSurface::TrimFromEdgeList(SEdgeList *el) {
    el->l.ClearTags();
   
    STrimBy stb;
    ZERO(&stb);
    for(;;) {
        // Find an edge, any edge; we'll start from there.
        SEdge *se;
        for(se = el->l.First(); se; se = el->l.NextAfter(se)) {
            if(se->tag) continue;
            break;    
        }
        if(!se) break;
        se->tag = 1;
        stb.start = se->a;
        stb.finish = se->b;
        stb.curve.v = se->auxA;
        stb.backwards = se->auxB ? true : false;

        // Find adjoining edges from the same curve; those should be
        // merged into a single trim.
        bool merged;
        do {
            merged = false;
            for(se = el->l.First(); se; se = el->l.NextAfter(se)) {
                if(se->tag)                         continue;
                if(se->auxA != stb.curve.v)         continue;
                if(( se->auxB && !stb.backwards) ||
                   (!se->auxB &&  stb.backwards))   continue;

                if((se->a).Equals(stb.finish)) {
                    stb.finish = se->b;
                    se->tag = 1;
                    merged = true;
                } else if((se->b).Equals(stb.start)) {
                    stb.start = se->a;
                    se->tag = 1;
                    merged = true;
                }
            }
        } while(merged);

        // And add the merged trim, with xyz (not uv like the polygon) pts
        stb.start  = PointAt(stb.start.x,  stb.start.y);
        stb.finish = PointAt(stb.finish.x, stb.finish.y);
        trim.Add(&stb);
    }
}

//-----------------------------------------------------------------------------
// Trim this surface against the specified shell, in the way that's appropriate
// for the specified Boolean operation type (and which operand we are). We
// also need a pointer to the shell that contains our own surface, since that
// contains our original trim curves.
//-----------------------------------------------------------------------------
SSurface SSurface::MakeCopyTrimAgainst(SShell *against, SShell *shell,
                                       int type, bool opA)
{
    SSurface ret;
    // The returned surface is identical, just the trim curves change
    ret = *this;
    ZERO(&(ret.trim));

    SEdgeList el;
    ZERO(&el);
    MakeEdgesInto(shell, &el, true);
    shell->MakeEdgeListUseNewCurveIds(&el);

    ret.TrimFromEdgeList(&el);

    el.Clear();
    return ret;
}

void SShell::CopySurfacesTrimAgainst(SShell *against, SShell *into,
                                     int type, bool opA)
{
    SSurface *ss;
    for(ss = surface.First(); ss; ss = surface.NextAfter(ss)) {
        SSurface ssn;
        ssn = ss->MakeCopyTrimAgainst(against, this, type, opA);
        into->surface.AddAndAssignId(&ssn);
    }
}

void SShell::MakeIntersectionCurvesAgainst(SShell *agnst, SShell *into) {
    SSurface *sa;
    for(sa = agnst->surface.First(); sa; sa = agnst->surface.NextAfter(sa)) {
        SSurface *sb;
        for(sb = surface.First(); sb; sb = surface.NextAfter(sb)) {
            // Intersect every surface from our shell against every surface
            // from agnst; this will add zero or more curves to the curve
            // list for into.
            sa->IntersectAgainst(sb, into);
        }
    }
}

void SShell::CleanupAfterBoolean(void) {
    SSurface *ss;
    for(ss = surface.First(); ss; ss = surface.NextAfter(ss)) {
        (ss->orig).Clear();
        (ss->inside).Clear();
        (ss->onSameNormal).Clear();
        (ss->onFlipNormal).Clear();
    }
}

void SShell::MakeFromBoolean(SShell *a, SShell *b, int type) {
    // Copy over all the original curves, splitting them so that a
    // piecwise linear segment never crosses a surface from the other
    // shell.
    a->CopyCurvesSplitAgainst(b, this);
    b->CopyCurvesSplitAgainst(a, this);

    // Generate the intersection curves for each surface in A against all
    // the surfaces in B
    a->MakeIntersectionCurvesAgainst(b, this);

    // Then trim and copy the surfaces
    a->CopySurfacesTrimAgainst(b, this, type, true);
    b->CopySurfacesTrimAgainst(a, this, type, false);

    // And clean up the piecewise linear things we made as a calculation aid
    a->CleanupAfterBoolean();
    b->CleanupAfterBoolean();
}

