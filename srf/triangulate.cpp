#include "../solvespace.h"

void SPolygon::UvTriangulateInto(SMesh *m, SSurface *srf) {
    if(l.n <= 0) return;

    SDWORD in = GetMilliseconds();

    normal = Vector::From(0, 0, 1);

    while(l.n > 0) {
        FixContourDirections();
        l.ClearTags();

        // Find a top-level contour, and start with that. Then build bridges
        // in order to merge all its islands into a single contour.
        SContour *top;
        for(top = l.First(); top; top = l.NextAfter(top)) {
            if(top->timesEnclosed == 0) {
                break;
            }
        }
        if(!top) {
            dbp("polygon has no top-level contours?");
            return;
        }

        // Start with the outer contour
        SContour merged;
        ZERO(&merged);
        top->tag = 1;
        top->CopyInto(&merged);
        (merged.l.n)--;

        // List all of the edges, for testing whether bridges work.
        SEdgeList el;
        ZERO(&el);
        top->MakeEdgesInto(&el);
        List<Vector> vl;
        ZERO(&vl);

        // And now find all of its holes; 
        SContour *sc;
        for(sc = l.First(); sc; sc = l.NextAfter(sc)) {
            if(sc->timesEnclosed != 1) continue;
            if(sc->l.n < 2) continue;

            // Test the midpoint of an edge. Our polygon may not be self-
            // intersecting, but two countours may share a vertex; so a
            // vertex could be on the edge of another polygon, in which
            // case ContainsPointProjdToNormal returns indeterminate.
            Vector tp = ((sc->l.elem[0].p).Plus(sc->l.elem[1].p)).ScaledBy(0.5);
            if(top->ContainsPointProjdToNormal(normal, tp)) {
                sc->tag = 2;
                sc->MakeEdgesInto(&el);
                sc->FindPointWithMinX();
            }
        }

//        dbp("finished finding holes: %d ms", GetMilliseconds() - in);
        for(;;) {
            double xmin = 1e10;
            SContour *scmin = NULL;

            for(sc = l.First(); sc; sc = l.NextAfter(sc)) {
                if(sc->tag != 2) continue;

                if(sc->xminPt.x < xmin) {
                    xmin = sc->xminPt.x;
                    scmin = sc;
                }
            }
            if(!scmin) break;

            if(!merged.BridgeToContour(scmin, &el, &vl)) {
                dbp("couldn't merge our hole");
                return;
            }
//            dbp("   bridged to contour: %d ms", GetMilliseconds() - in);
            scmin->tag = 3;
        }
//        dbp("finished merging holes: %d ms", GetMilliseconds() - in);

        merged.UvTriangulateInto(m, srf);
//        dbp("finished ear clippping: %d ms", GetMilliseconds() - in);
        merged.l.Clear();
        el.Clear();
        vl.Clear();

        l.RemoveTagged();
    }
}

bool SContour::BridgeToContour(SContour *sc, 
                               SEdgeList *avoidEdges, List<Vector> *avoidPts)
{
    int i, j;

    // Start looking for a bridge on our new hole near its leftmost (min x)
    // point.
    int sco = 0;
    for(i = 0; i < (sc->l.n - 1); i++) {
        if((sc->l.elem[i].p).EqualsExactly(sc->xminPt)) {
            sco = i;
        }
    }

    // And start looking on our merged contour at whichever point is nearest
    // to the leftmost point of the new segment.
    int thiso = 0;
    double dmin = 1e10;
    for(i = 0; i < l.n; i++) {
        Vector p = l.elem[i].p;
        double d = (p.Minus(sc->xminPt)).MagSquared();
        if(d < dmin) {
            dmin = d;
            thiso = i;
        }
    }

    int thisp, scp;
   
    Vector a, b, *f;
    for(i = 0; i < l.n; i++) {
        thisp = WRAP(i+thiso, l.n);
        a = l.elem[thisp].p;

        for(f = avoidPts->First(); f; f = avoidPts->NextAfter(f)) {
            if(f->Equals(a)) break;
        }
        if(f) continue;

        for(j = 0; j < (sc->l.n - 1); j++) {
            scp = WRAP(j+sco, (sc->l.n - 1));
            b = sc->l.elem[scp].p;

            for(f = avoidPts->First(); f; f = avoidPts->NextAfter(f)) {
                if(f->Equals(b)) break;
            }
            if(f) continue;

            if(avoidEdges->AnyEdgeCrossings(a, b) > 0) {
                // doesn't work, bridge crosses an existing edge
            } else {
                goto haveEdge;
            }
        }
    }
    // Tried all the possibilities, didn't find an edge
    return false;

haveEdge:
    SContour merged;
    ZERO(&merged);
    for(i = 0; i < l.n; i++) {
        merged.AddPoint(l.elem[i].p);
        if(i == thisp) {
            // less than or equal; need to duplicate the join point
            for(j = 0; j <= (sc->l.n - 1); j++) {
                int jp = WRAP(j + scp, (sc->l.n - 1));
                merged.AddPoint((sc->l.elem[jp]).p);
            }
            // and likewise duplicate join point for the outer curve
            merged.AddPoint(l.elem[i].p);
        }
    }

    // and future bridges mustn't cross our bridge, and it's tricky to get
    // things right if two bridges come from the same point
    avoidEdges->AddEdge(a, b);
    avoidPts->Add(&a);
    avoidPts->Add(&b);

    l.Clear();
    l = merged.l;
    return true;
}

bool SContour::IsEar(int bp) {
    int ap = WRAP(bp-1, l.n),
        cp = WRAP(bp+1, l.n);

    STriangle tr;
    ZERO(&tr);
    tr.a = l.elem[ap].p;
    tr.b = l.elem[bp].p;
    tr.c = l.elem[cp].p;

    if((tr.a).Equals(tr.c)) {
        // This is two coincident and anti-parallel edges. Zero-area, so
        // won't generate a real triangle, but we certainly can clip it.
        return true;
    }

    Vector n = Vector::From(0, 0, -1);
    if((tr.Normal()).Dot(n) < LENGTH_EPS) {
        // This vertex is reflex, or between two collinear edges; either way,
        // it's not an ear.
        return false;
    }

    // Accelerate with an axis-aligned bounding box test
    Vector maxv = tr.a, minv = tr.a;
    (tr.b).MakeMaxMin(&maxv, &minv);
    (tr.c).MakeMaxMin(&maxv, &minv);

    int i;
    for(i = 0; i < l.n; i++) {
        if(i == ap || i == bp || i == cp) continue;
        
        Vector p = l.elem[i].p;
        if(p.OutsideAndNotOn(maxv, minv)) continue;

        // A point on the edge of the triangle is considered to be inside,
        // and therefore makes it a non-ear; but a point on the vertex is
        // "outside", since that's necessary to make bridges work.
        if(p.EqualsExactly(tr.a)) continue;
        if(p.EqualsExactly(tr.b)) continue;
        if(p.EqualsExactly(tr.c)) continue;
            
        if(tr.ContainsPointProjd(n, p)) {
            return false;
        }
    }
    return true;
}

void SContour::ClipEarInto(SMesh *m, int bp) {
    int ap = WRAP(bp-1, l.n),
        cp = WRAP(bp+1, l.n);
    
    STriangle tr;
    ZERO(&tr);
    tr.a = l.elem[ap].p;
    tr.b = l.elem[bp].p;
    tr.c = l.elem[cp].p;
    if(tr.Normal().MagSquared() < LENGTH_EPS*LENGTH_EPS) {
        // A vertex with more than two edges will cause us to generate
        // zero-area triangles, which must be culled.
    } else {
        m->AddTriangle(&tr);
    }

    // By deleting the point at bp, we may change the ear-ness of the points
    // on either side.
    l.elem[ap].ear = SPoint::UNKNOWN;
    l.elem[cp].ear = SPoint::UNKNOWN;

    l.ClearTags();
    l.elem[bp].tag = 1;
    l.RemoveTagged();
}

void SContour::UvTriangulateInto(SMesh *m, SSurface *srf) {
    int i;

    // Clean the original contour by removing any zero-length edges.
    l.ClearTags();
    for(i = 1; i < l.n; i++) {
       if((l.elem[i].p).Equals(l.elem[i-1].p)) {
            l.elem[i].tag = 1;
        }
    }
    l.RemoveTagged();

    // Now calculate the ear-ness of each vertex
    for(i = 0; i < l.n; i++) {
        (l.elem[i]).ear = IsEar(i) ? SPoint::EAR : SPoint::NOT_EAR;
    }

    bool toggle = false;
    while(l.n > 3) {
        // Some points may have changed ear-ness, so recalculate
        for(i = 0; i < l.n; i++) {
            if(l.elem[i].ear == SPoint::UNKNOWN) {
                (l.elem[i]).ear = IsEar(i) ? SPoint::EAR : SPoint::NOT_EAR;
            }
        }

        int bestEar = -1;
        double bestChordTol = VERY_POSITIVE;
        // Alternate the starting position so we generate strip-like
        // triangulations instead of fan-like
        toggle = !toggle;
        int offset = toggle ? -1 : 0;
        for(i = 0; i < l.n; i++) {
            int ear = WRAP(i+offset, l.n);
            if(l.elem[ear].ear == SPoint::EAR) {
                if(!srf) {
                    bestEar = ear;
                    break;
                }
                // If we are triangulating a curved surface, then try to
                // clip ears that have a small chord tolerance from the
                // surface.
                Vector prev = l.elem[WRAP((i+offset-1), l.n)].p,
                       next = l.elem[WRAP((i+offset+1), l.n)].p;
                double tol = srf->ChordToleranceForEdge(prev, next);
                if(tol < bestChordTol - LENGTH_EPS) {
                    bestEar = ear;
                    bestChordTol = tol;
                }
                if(bestChordTol < 0.1*(SS.chordTol / SS.GW.scale)) {
                    break;
                }
            }
        }
        if(bestEar < 0) {
            dbp("couldn't find an ear! fail");
            return;
        }
        ClipEarInto(m, bestEar);
    }

    ClipEarInto(m, 0); // add the last triangle
}

double SSurface::ChordToleranceForEdge(Vector a, Vector b) {
    Vector as = PointAt(a.x, a.y), bs = PointAt(b.x, b.y);

    double worst = VERY_NEGATIVE;
    int i;
    for(i = 1; i <= 3; i++) {
        Vector p  = a. Plus((b. Minus(a )).ScaledBy(i/4.0)),
               ps = as.Plus((bs.Minus(as)).ScaledBy(i/4.0));

        Vector pps = PointAt(p.x, p.y);
        worst = max(worst, (pps.Minus(ps)).MagSquared());
    }
    return sqrt(worst);
}


