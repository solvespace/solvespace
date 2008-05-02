#include "solvespace.h"

bool SEdgeList::AssemblePolygon(SPolygon *dest, SEdge *errorAt) {
    dest->Clear();
    l.ClearTags();

    for(;;) {
        Vector first, last;
        int i;
        for(i = 0; i < l.n; i++) {
            if(!l.elem[i].tag) {
                first = l.elem[i].a;
                last = l.elem[i].b;
                l.elem[i].tag = 1;
                break;
            }
        }
        if(i >= l.n) {
            return true;
        }

        dest->AddEmptyContour();
        dest->AddPoint(first);
        dest->AddPoint(last);
        do {
            for(i = 0; i < l.n; i++) {
                SEdge *se = &(l.elem[i]);
                if(se->tag) continue;

                if(se->a.Equals(last)) {
                    dest->AddPoint(se->b);
                    last = se->b;
                    se->tag = 1;
                    break;
                }
                if(se->b.Equals(last)) {
                    dest->AddPoint(se->a);
                    last = se->a;
                    se->tag = 1;
                    break;
                }
            }
            if(i >= l.n) {
                // Couldn't assemble a closed contour; mark where.
                errorAt->a = first;
                errorAt->b = last;
                return false;
            }

        } while(!last.Equals(first));
    }
}

void SPolygon::Clear(void) {
    int i;
    for(i = 0; i < l.n; i++) {
        (l.elem[i]).l.Clear();
    }
    l.Clear();
}

void SPolygon::AddEmptyContour(void) {
    SContour c;
    memset(&c, 0, sizeof(c));
    l.Add(&c);
}

void SPolygon::AddPoint(Vector p) {
    if(l.n < 1) oops();

    SPoint sp;
    sp.tag = 0;
    sp.p = p;

    // Add to the last contour in the list
    (l.elem[l.n-1]).l.Add(&sp);
}

void SPolygon::MakeEdgesInto(SEdgeList *el) {
    int i;
    for(i = 0; i < l.n; i++) {
        (l.elem[i]).MakeEdgesInto(el);
    }
}

Vector SPolygon::Normal(void) {
    if(l.n < 1) return Vector::MakeFrom(0, 0, 0);
    return (l.elem[0]).Normal();
}

void SContour::MakeEdgesInto(SEdgeList *el) {
    int i;
    for(i = 0; i < (l.n-1); i++) {
        SEdge e;
        e.a = l.elem[i].p;
        e.b = l.elem[i+1].p;
        el->l.Add(&e);
    }
}

Vector SContour::Normal(void) {
    if(l.n < 3) return Vector::MakeFrom(0, 0, 0);

    Vector u = (l.elem[0].p).Minus(l.elem[1].p);

    Vector v;
    double dot = 2;
    // Find the edge in the contour that's closest to perpendicular to the 
    // first edge, since that will give best numerical stability.
    for(int i = 1; i < (l.n-1); i++) {
        Vector vt = (l.elem[i].p).Minus(l.elem[i+1].p);
        double dott = fabs(vt.Dot(u)/(u.Magnitude()*vt.Magnitude()));
        if(dott < dot) {
            dot = dott;
            v = vt;
        }
    }
    return (u.Cross(v)).WithMagnitude(1);
}

