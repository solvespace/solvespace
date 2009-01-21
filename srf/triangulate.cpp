#include "../solvespace.h"

void SPolygon::UvTriangulateInto(SMesh *m) {
    if(l.n <= 0) return;

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
            if(sc->l.n < 1) continue;

            if(top->ContainsPointProjdToNormal(normal, sc->l.elem[0].p)) {
                sc->tag = 2;
                sc->MakeEdgesInto(&el);
            }
        }

        bool holesExist, mergedHole;
        do {
            holesExist = false;
            mergedHole = false;

            for(sc = l.First(); sc; sc = l.NextAfter(sc)) {
                if(sc->tag != 2) continue;

                holesExist = true;
                if(merged.BridgeToContour(sc, &el, &vl)) {
                    // Merged it in, so we're done with it.
                    sc->tag = 3;
                    mergedHole = true;
                } else {
                    // Looks like we can't merge it yet, but that can happen;
                    // we may have to merge the holes in a specific order.
                }
            }
            if(holesExist && !mergedHole) {
                dbp("holes exist, yet couldn't merge one");
                return;
            }
        } while(holesExist);

        merged.UvTriangulateInto(m);
        merged.l.Clear();
        el.Clear();
        vl.Clear();

        l.RemoveTagged();
    }
}

bool SContour::BridgeToContour(SContour *sc, 
                               SEdgeList *avoidEdges, List<Vector> *avoidPts)
{
    int thisp, scp;
   
    Vector a, b, *f;
    for(thisp = 0; thisp < l.n; thisp++) {
        a = l.elem[thisp].p;

        for(f = avoidPts->First(); f; f = avoidPts->NextAfter(f)) {
            if(f->Equals(a)) break;
        }
        if(f) continue;

        for(scp = 0; scp < (sc->l.n - 1); scp++) {
            b = sc->l.elem[scp].p;

            for(f = avoidPts->First(); f; f = avoidPts->NextAfter(f)) {
                if(f->Equals(b)) break;
            }
            if(f) continue;

            if(avoidEdges->AnyEdgeCrosses(a, b)) {
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
    int i, j;
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
    m->AddTriangle(&tr);

    // By deleting the point at bp, we may change the ear-ness of the points
    // on either side.
    l.elem[ap].ear = SPoint::UNKNOWN;
    l.elem[cp].ear = SPoint::UNKNOWN;

    l.ClearTags();
    l.elem[bp].tag = 1;
    l.RemoveTagged();
}

void SContour::UvTriangulateInto(SMesh *m) {
    int i;
    for(i = 0; i < l.n; i++) {
        (l.elem[i]).ear = IsEar(i) ? SPoint::EAR : SPoint::NOT_EAR;
    }

    while(l.n > 3) {
        for(i = 0; i < l.n; i++) {
            if(l.elem[i].ear == SPoint::UNKNOWN) {
                (l.elem[i]).ear = IsEar(i) ? SPoint::EAR : SPoint::NOT_EAR;
            }
        }
        for(i = 0; i < l.n; i++) {
            if(l.elem[i].ear == SPoint::EAR) {
                break;
            }
        }
        if(i >= l.n) {
            dbp("couldn't find an ear! fail");
            return;
        }
        ClipEarInto(m, i);
    }

    ClipEarInto(m, 0); // add the last triangle
}

