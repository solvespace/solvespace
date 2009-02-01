#include "solvespace.h"

static int I, N;

void SShell::MakeFromUnionOf(SShell *a, SShell *b) {
    MakeFromBoolean(a, b, AS_UNION);
}

void SShell::MakeFromDifferenceOf(SShell *a, SShell *b) {
    MakeFromBoolean(a, b, AS_DIFFERENCE);
}

static Vector LineStart, LineDirection;
static int ByTAlongLine(const void *av, const void *bv)
{
    Vector *a = (Vector *)av,
           *b = (Vector *)bv;
    
    double ta = (a->Minus(LineStart)).DivPivoting(LineDirection),
           tb = (b->Minus(LineStart)).DivPivoting(LineDirection);

    return (ta > tb) ? 1 : -1;
}
SCurve SCurve::MakeCopySplitAgainst(SShell *agnstA, SShell *agnstB) {
    SCurve ret;
    ret = *this;
    ret.interCurve = false;
    ZERO(&(ret.pts));

    Vector *p = pts.First();
    if(!p) oops();
    Vector prev = *p;
    ret.pts.Add(p);
    p = pts.NextAfter(p);

    for(; p; p = pts.NextAfter(p)) {
        List<Vector> il;
        ZERO(&il);

        // Find all the intersections with the two passed shells
        if(agnstA) agnstA->AllPointsIntersecting(prev, *p, &il);
        if(agnstB) agnstB->AllPointsIntersecting(prev, *p, &il);

        // If any intersections exist, sort them in order along the
        // line and add them to the curve.
        if(il.n > 0) {
            LineStart = prev;
            LineDirection = p->Minus(prev);
            qsort(il.elem, il.n, sizeof(il.elem[0]), ByTAlongLine);

            Vector *pi;
            for(pi = il.First(); pi; pi = il.NextAfter(pi)) {
                ret.pts.Add(pi);
            }
        }

        ret.pts.Add(p);
        prev = *p;
    }
    return ret;
}

void SShell::CopyCurvesSplitAgainst(SShell *aga, SShell *agb, SShell *into) {
    SCurve *sc;
    for(sc = curve.First(); sc; sc = curve.NextAfter(sc)) {
        SCurve scn = sc->MakeCopySplitAgainst(aga, agb);
        hSCurve hsc = into->curve.AddAndAssignId(&scn);
        // And note the new ID so that we can rewrite the trims appropriately
        sc->newH = hsc;
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
SSurface SSurface::MakeCopyTrimAgainst(SShell *agnst, SShell *parent,
                                       SShell *into,
                                       int type, bool opA)
{
    SSurface ret;
    // The returned surface is identical, just the trim curves change
    ret = *this;
    ZERO(&(ret.trim));

    // First, build a list of the existing trim curves; update them to use
    // the split curves.
    STrimBy *stb;
    for(stb = trim.First(); stb; stb = trim.NextAfter(stb)) {
        STrimBy stn = *stb;
        stn.curve = (parent->curve.FindById(stn.curve))->newH;
        ret.trim.Add(&stn);
    }

    // Build up our original trim polygon
    SEdgeList orig;
    ZERO(&orig);
    ret.MakeEdgesInto(into, &orig, true);
    ret.trim.Clear();


    // And now intersect the other shell against us
    SEdgeList inter;
    ZERO(&inter);

    SSurface *ss;
    for(ss = agnst->surface.First(); ss; ss = agnst->surface.NextAfter(ss)) {
        SCurve *sc;
        for(sc = into->curve.First(); sc; sc = into->curve.NextAfter(sc)) {
            if(!(sc->interCurve)) continue;
            if(opA) {
                if(sc->surfB.v != h.v || sc->surfA.v != ss->h.v) continue;
            } else {
                if(sc->surfA.v != h.v || sc->surfB.v != ss->h.v) continue;
            }
           
            int i;
            for(i = 1; i < sc->pts.n; i++) {
                Vector a = sc->pts.elem[i-1],
                       b = sc->pts.elem[i];

                Point2d auv, buv;
                ss->ClosestPointTo(a, &(auv.x), &(auv.y));
                ss->ClosestPointTo(b, &(buv.x), &(buv.y));

                int c = ss->bsp->ClassifyEdge(auv, buv);
                if(c == SBspUv::INSIDE) {
                    Vector ta = Vector::From(0, 0, 0);
                    Vector tb = Vector::From(0, 0, 0);
                    ClosestPointTo(a, &(ta.x), &(ta.y));
                    ClosestPointTo(b, &(tb.x), &(tb.y));

                    Vector tn = NormalAt(ta.x, ta.y);
                    Vector sn = ss->NormalAt(auv.x, auv.y);

                    if((tn.Cross(b.Minus(a))).Dot(sn) > 0) {
                        inter.AddEdge(ta, tb, sc->h.v, 0);
                    } else { 
                        inter.AddEdge(tb, ta, sc->h.v, 1);
                    }
                }
            }
        }
    }

    SEdgeList final;
    ZERO(&final);

    if(I == 2) dbp("INTERBSP: %d", inter.l.n);
    SBspUv *interbsp = SBspUv::From(&inter);
    if(I == 2) dbp("INTEROVER");


    SEdge *se;
    N = 0;
    for(se = orig.l.First(); se; se = orig.l.NextAfter(se)) {
        int c = interbsp->ClassifyEdge(se->a.ProjectXy(), se->b.ProjectXy());

        if(I == 2) dbp("edge from %.3f %.3f %.3f to %.3f %.3f %.3f",
            CO(PointAt(se->a.x, se->a.y)), CO(PointAt(se->b.x, se->b.y)));

        if(c == SBspUv::OUTSIDE) {
            if(I == 2) dbp("   keep");
            final.AddEdge(se->a, se->b, se->auxA, se->auxB);
        } else {
            if(I == 2) dbp("    don't keep, %d", c);
        }
        N++;
    }

    for(se = inter.l.First(); se; se = inter.l.NextAfter(se)) {

        if(I == 2) {
            Vector mid = (se->a).Plus(se->b).ScaledBy(0.5);
            Vector arrow = (se->b).Minus(se->a);
            SWAP(double, arrow.x, arrow.y);
            arrow.x *= -1;
            arrow = arrow.WithMagnitude(0.03);
            arrow = arrow.Plus(mid);

            SS.nakedEdges.AddEdge(PointAt(se->a.x, se->a.y),
                                  PointAt(se->b.x, se->b.y));
            SS.nakedEdges.AddEdge(PointAt(mid.x, mid.y),
                                  PointAt(arrow.x, arrow.y));
        }

        int c = bsp->ClassifyEdge(se->a.ProjectXy(), se->b.ProjectXy());
        if(c == SBspUv::INSIDE) {
            final.AddEdge(se->b, se->a, se->auxA, !se->auxB);
        }
    }

    for(se = final.l.First(); se; se = final.l.NextAfter(se)) {
    }

    ret.TrimFromEdgeList(&final);

    final.Clear();
    inter.Clear();
    orig.Clear();
    return ret;
}

void SShell::CopySurfacesTrimAgainst(SShell *against, SShell *into,
                                     int type, bool opA)
{
    SSurface *ss;
    for(ss = surface.First(); ss; ss = surface.NextAfter(ss)) {
        SSurface ssn;
        ssn = ss->MakeCopyTrimAgainst(against, this, into, type, opA);
        into->surface.AddAndAssignId(&ssn);
        I++;
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
            sa->IntersectAgainst(sb, agnst, this, into);
        }
    }
}

void SShell::CleanupAfterBoolean(void) {
    SSurface *ss;
    for(ss = surface.First(); ss; ss = surface.NextAfter(ss)) {
    }
}

void SShell::MakeFromBoolean(SShell *a, SShell *b, int type) {
    a->MakeClassifyingBsps();
    b->MakeClassifyingBsps();

    // Copy over all the original curves, splitting them so that a
    // piecwise linear segment never crosses a surface from the other
    // shell.
    a->CopyCurvesSplitAgainst(b, NULL, this);
    b->CopyCurvesSplitAgainst(a, NULL, this);

    // Generate the intersection curves for each surface in A against all
    // the surfaces in B (which is all of the intersection curves).
    a->MakeIntersectionCurvesAgainst(b, this);

    if(a->surface.n == 0 || b->surface.n == 0) {
        // Then trim and copy the surfaces
        I = 100;
        a->CopySurfacesTrimAgainst(b, this, type, true);
        b->CopySurfacesTrimAgainst(a, this, type, false);
    } else {
        I = -1;
        a->CopySurfacesTrimAgainst(b, this, type, true);
        b->CopySurfacesTrimAgainst(a, this, type, false);
    }

    // And clean up the piecewise linear things we made as a calculation aid
    a->CleanupAfterBoolean();
    b->CleanupAfterBoolean();
}

//-----------------------------------------------------------------------------
// All of the BSP routines that we use to perform and accelerate polygon ops.
//-----------------------------------------------------------------------------
void SShell::MakeClassifyingBsps(void) {
    SSurface *ss;
    for(ss = surface.First(); ss; ss = surface.NextAfter(ss)) {
        ss->MakeClassifyingBsp(this);
    }
}

void SSurface::MakeClassifyingBsp(SShell *shell) {
    SEdgeList el;
    ZERO(&el);

    MakeEdgesInto(shell, &el, true);
    bsp = SBspUv::From(&el);

    el.Clear();
}

SBspUv *SBspUv::Alloc(void) {
    return (SBspUv *)AllocTemporary(sizeof(SBspUv));
}

static int ByLength(const void *av, const void *bv)
{
    SEdge *a = (SEdge *)av,
          *b = (SEdge *)bv;
    
    double la = (a->a).Minus(a->b).Magnitude(),
           lb = (b->a).Minus(b->b).Magnitude();

    // Sort in descending order, longest first. This improves numerical
    // stability for the normals.
    return (la < lb) ? 1 : -1;
}
SBspUv *SBspUv::From(SEdgeList *el) {
    SEdgeList work;
    ZERO(&work);

    SEdge *se;
    for(se = el->l.First(); se; se = el->l.NextAfter(se)) {
        work.AddEdge(se->a, se->b, se->auxA, se->auxB);
    }
    qsort(work.l.elem, work.l.n, sizeof(work.l.elem[0]), ByLength);

    SBspUv *bsp = NULL;
    for(se = work.l.First(); se; se = work.l.NextAfter(se)) {
        bsp = bsp->InsertEdge((se->a).ProjectXy(), (se->b).ProjectXy());
    }

    work.Clear();
    return bsp;
}

SBspUv *SBspUv::InsertEdge(Point2d ea, Point2d eb) {
    if(I == 2) {
        dbp("insert edge %.3f %.3f to %.3f %.3f", ea.x, ea.y, eb.x, eb.y);
    }

    if(!this) {
        SBspUv *ret = Alloc();
        ret->a = ea;
        ret->b = eb;
        return ret;
    }

    Point2d n = ((b.Minus(a)).Normal()).WithMagnitude(1);
    double d = a.Dot(n);

    double dea = ea.Dot(n) - d,
           deb = eb.Dot(n) - d;

    if(fabs(dea) < LENGTH_EPS && fabs(deb) < LENGTH_EPS) {
        // Line segment is coincident with this one, store in same node
        SBspUv *m = Alloc();
        m->a = ea;
        m->b = eb;
        m->more = more;
        more = m;
    } else if(fabs(dea) < LENGTH_EPS) {
        // Point A lies on this lie, but point B does not
        if(deb > 0) {
            pos = pos->InsertEdge(ea, eb);
        } else {
            neg = neg->InsertEdge(ea, eb);
        }
    } else if(fabs(deb) < LENGTH_EPS) {
        // Point B lies on this lie, but point A does not
        if(dea > 0) {
            pos = pos->InsertEdge(ea, eb);
        } else {
            neg = neg->InsertEdge(ea, eb);
        }
    } else if(dea > 0 && deb > 0) {
        pos = pos->InsertEdge(ea, eb);
    } else if(dea < 0 && deb < 0) {
        neg = neg->InsertEdge(ea, eb);
    } else {
        // New edge crosses this one; we need to split.
        double t = (d - n.Dot(ea)) / (n.Dot(eb.Minus(ea)));
        Point2d pi = ea.Plus((eb.Minus(ea)).ScaledBy(t));
        if(dea > 0) {
            pos = pos->InsertEdge(ea, pi);
            neg = neg->InsertEdge(pi, eb);
        } else {
            neg = neg->InsertEdge(ea, pi);
            pos = pos->InsertEdge(pi, eb);
        }
    }
    return this;
}

int SBspUv::ClassifyPoint(Point2d p, Point2d eb) {
    if(!this) return OUTSIDE;

    Point2d n = ((b.Minus(a)).Normal()).WithMagnitude(1);
    double d = a.Dot(n);

    double dp = p.Dot(n) - d;

    if(I == 2 && N == 5) {
        dbp("point %.3f %.3f has d=%.3f", p.x, p.y, dp);
    }

    if(fabs(dp) < LENGTH_EPS) {
        if(I == 2 && N == 5) dbp("   on line");
        SBspUv *f = this;
        while(f) {
            Point2d ba = (f->b).Minus(f->a);
            if(p.DistanceToLine(f->a, ba, true) < LENGTH_EPS) {
                if(eb.DistanceToLine(f->a, ba, false) < LENGTH_EPS) {
                    if(ba.Dot(eb.Minus(p)) > 0) {
                        return EDGE_PARALLEL;
                    } else {
                        return EDGE_ANTIPARALLEL;
                    }
                } else {
                    return EDGE_OTHER;
                }
            }
            f = f->more;
        }
        // Pick arbitrarily which side to send it down, doesn't matter
        return neg ? neg->ClassifyPoint(p, eb) : OUTSIDE;
    } else if(dp > 0) {
        if(I == 2 && N == 5) dbp("   pos");
        return pos ? pos->ClassifyPoint(p, eb) : INSIDE;
    } else {
        if(I == 2 && N == 5) dbp("   neg");
        return neg ? neg->ClassifyPoint(p, eb) : OUTSIDE;
    }
}

int SBspUv::ClassifyEdge(Point2d ea, Point2d eb) {
    return ClassifyPoint((ea.Plus(eb)).ScaledBy(0.5), eb);
}

