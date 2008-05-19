#include "solvespace.h"

static int ByDouble(const void *av, const void *bv) {
    const double *a = (const double *)av;
    const double *b = (const double *)bv;
    if(*a == *b) {
        return 0;
    } else if(*a > *b) {
        return 1;
    } else {
        return -1;
    }
}

bool SEdgeList::AssemblePolygon(SPolygon *dest, SEdge *errorAt) {
    dest->Clear();

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
                if(errorAt) {
                    errorAt->a = first;
                    errorAt->b = last;
                }
                return false;
            }

        } while(!last.Equals(first));
    }
}

void SEdgeList::CopyBreaking(SEdgeList *dest) {
    int i, j, k;
    
    for(i = 0; i < l.n; i++) {
        SEdge *ei = &(l.elem[i]);
        Vector p0i = ei->a;
        Vector dpi = (ei->b).Minus(ei->a);

        double inter[100];
        int inters = 0;
        for(j = 0; j < l.n; j++) {
            if(i == j) continue;
            SEdge *ej = &(l.elem[j]);
            Vector p0j = ej->a;
            Vector dpj = (ej->b).Minus(ej->a);

            // Find the intersection, if any
            Vector dn = dpi.Cross(dpj);
            if(dn.Magnitude() < 0.001) continue; // parallel, non-intersecting
            Vector dni = dn.Cross(dpi);
            Vector dnj = dn.Cross(dpj);
            double tj =  ((p0i.Minus(p0j)).Dot(dni))/(dpj.Dot(dni));
            double ti = -((p0i.Minus(p0j)).Dot(dnj))/(dpi.Dot(dnj));
            // could also test for skew, but assume it's all in plane so not

            if(ti <= 0  || ti >= 1) continue;
            if(tj < -0.001 || tj > 1.001) continue;

            inter[inters++] = ti;
        }
        inter[inters++] = 0;
        inter[inters++] = 1;
        qsort(inter, inters, sizeof(inter[0]), ByDouble);

        for(k = 1; k < inters; k++) {
            SEdge ne;
            ne.tag = 0;
            ne.a = p0i.Plus(dpi.ScaledBy(inter[k-1]));
            ne.b = p0i.Plus(dpi.ScaledBy(inter[k]));
            dest->l.Add(&ne);
        }
    }
}

void SEdgeList::CullDuplicates(void) {
    int i, j;
    for(i = 0; i < l.n; i++) {
        SEdge *se = &(l.elem[i]);
        if(se->tag) continue;

        if(((se->a).Minus(se->b)).Magnitude() < 0.01) {
            se->tag = 1;
            continue;
        }

        for(j = i+1; j < l.n; j++) {
            SEdge *st = &(l.elem[j]);
            if(st->tag) continue;

            if(((se->a).Equals(st->a) && (se->b).Equals(st->b)) ||
               ((se->a).Equals(st->b) && (se->b).Equals(st->a)))
            {
                // This is an exact duplicate, so mark it as unused now.
                st->tag = 1;
                break;
            }
        }
    }
}

bool SEdgeList::BooleanOp(int op, bool inA, bool inB) {
    if(op == UNION) {
        return inA || inB;
    } else if(op == DIFF) {
        return inA && (!inB);
    } else if(op == INTERSECT) {
        return inA && inB;
    } else oops();
}

void SEdgeList::CullForBoolean(int op, SPolygon *a, SPolygon *b) {
    int i;
    for(i = 0; i < l.n; i++) {
        SEdge *se = &(l.elem[i]);
        if(se->tag) continue;

        Vector tp = ((se->a).Plus(se->b)).ScaledBy(0.5);
        Vector nudge = ((se->a).Minus(se->b)).Cross(a->normal);
        nudge = nudge.WithMagnitude(.01);
        Vector tp1 = tp.Plus(nudge);
        Vector tp2 = tp.Minus(nudge);
       
        bool inf1 = BooleanOp(op, a->ContainsPoint(tp1), b->ContainsPoint(tp1));
        bool inf2 = BooleanOp(op, a->ContainsPoint(tp2), b->ContainsPoint(tp2));

        if((inf1 && inf2) || (!inf1 && !inf2)) {
            // The "in polygon" state doesn't change as you cross the edge;
            // so it doesn't lie on the output polygon.
            se->tag = 1;
        }
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

Vector SPolygon::ComputeNormal(void) {
    if(l.n < 1) return Vector::MakeFrom(0, 0, 0);
    return (l.elem[0]).ComputeNormal();
}

bool SPolygon::ContainsPoint(Vector p) {
    bool inside = false;
    int i;
    for(i = 0; i < l.n; i++) {
        SContour *sc = &(l.elem[i]);
        if(sc->ContainsPointProjdToNormal(normal, p)) {
            inside = !inside;
        }
    }
    return inside;
}

void SPolygon::IntersectAgainstPlane(SEdgeList *dest, Vector p0, Vector n) {
    if(l.n == 0 || (l.elem[0].l.n == 0)) return;
    double od = normal.Dot(l.elem[0].l.elem[0].p);
    double d = n.Dot(p0);

    Vector u = (normal.Cross(n));
    if(u.Magnitude() < 0.001) {
        if(n.Dot(normal) < 0) od = -od;
        if(fabs(od - d) < 0.001) {
            // The planes are coincident; so the intersection is a copy of
            // this polygon.
            MakeEdgesInto(dest);
        }
        return;
    }

    u = u.WithMagnitude(1);
    Vector v = normal.Cross(u);

    Vector lp = Vector::AtIntersectionOfPlanes(n, d, normal, od);
    double vp = v.Dot(lp);

    double inter[100];
    int inters = 0;
    int i;
    for(i = 0; i < l.n; i++) {
        SContour *sc = &(l.elem[i]);
        // The 0.01 is because I mishandle the case where the intersection
        // plane goes through a vertex
        sc->IntersectAgainstPlane(inter, &inters, u, v, vp);
    }
    qsort(inter, inters, sizeof(inter[0]), ByDouble);
    for(i = 0; i < inters; i += 2) {
        SEdge se;
        se.tag = 0;
        se.a = lp.Plus(u.ScaledBy(inter[i]));
        se.b = lp.Plus(u.ScaledBy(inter[i+1]));
        dest->l.Add(&se);
    }
}

void SPolygon::CopyBreaking(SPolyhedron *dest, SPolyhedron *against, int how) {
    if(l.n == 0 || (l.elem[0].l.n == 0)) return;
    Vector p0 = l.elem[0].l.elem[0].p;

    SEdgeList el; ZERO(&el);
    int i;
    for(i = 0; i < against->l.n; i++) {
        SPolygon *pb = &(against->l.elem[i]);
        pb->IntersectAgainstPlane(&el, p0, normal);
    }
    el.CullDuplicates();

    SPolygon inter; ZERO(&inter);
    bool worked = el.AssemblePolygon(&inter, NULL);
    inter.normal = normal;

    SPolygon res; ZERO(&res);
    if(how == 0) {
        this->Boolean(&res, SEdgeList::DIFF, &inter);
        res.normal = normal;
    } else if(how == 1) {
        this->Boolean(&res, SEdgeList::INTERSECT, &inter);
        res.normal = normal.ScaledBy(-1);
    } else oops();

    if(res.l.n > 0) {
        dest->l.Add(&res);
    }

    el.l.Clear();
    inter.Clear();
}

void SPolygon::FixContourDirections(void) {
    // Outside curve looks counterclockwise, projected against our normal.
    int i, j;
    for(i = 0; i < l.n; i++) {
        SContour *sc = &(l.elem[i]);
        if(sc->l.n < 1) continue;
        Vector pt = (sc->l.elem[0]).p;

        bool outer = true;
        for(j = 0; j < l.n; j++) {
            if(i == j) continue;
            SContour *sct = &(l.elem[j]);
            if(sct->ContainsPointProjdToNormal(normal, pt)) {
                outer = !outer;
            }
        }
   
        bool clockwise = sc->IsClockwiseProjdToNormal(normal);
        if(clockwise && outer || (!clockwise && !outer)) {
            sc->Reverse();
        }
    }
}

bool SPolygon::Boolean(SPolygon *dest, int op, SPolygon *b) {
    SEdgeList el;
    ZERO(&el);
    this->MakeEdgesInto(&el);
    b->MakeEdgesInto(&el);
    
    SEdgeList br;
    ZERO(&br);
    el.CopyBreaking(&br);

    br.CullDuplicates();
    br.CullForBoolean(op, this, b);

    SEdge e;
    bool ret = br.AssemblePolygon(dest, &e);
    if(!ret) {
        br.l.ClearTags();
        br.CullDuplicates();
        br.CullForBoolean(op, this, b);
    }

    br.l.Clear();
    el.l.Clear();
    return ret;
}

void SContour::MakeEdgesInto(SEdgeList *el) {
    int i;
    for(i = 0; i < (l.n-1); i++) {
        SEdge e;
        e.tag = 0;
        e.a = l.elem[i].p;
        e.b = l.elem[i+1].p;
        el->l.Add(&e);
    }
}

Vector SContour::ComputeNormal(void) {
    Vector n = Vector::MakeFrom(0, 0, 0);

    for(int i = 0; i < l.n - 2; i++) {
        Vector u = (l.elem[i+1].p).Minus(l.elem[i+0].p).WithMagnitude(1);
        Vector v = (l.elem[i+2].p).Minus(l.elem[i+1].p).WithMagnitude(1);
        Vector nt = u.Cross(v);
        if(nt.Magnitude() > n.Magnitude()) {
            n = nt;
        }
    }
    return n.WithMagnitude(1);
}

bool SContour::IsClockwiseProjdToNormal(Vector n) {
    // Degenerate things might happen as we draw; doesn't really matter
    // what we do then.
    if(n.Magnitude() < 0.01) return true;

    // An arbitrary 2d coordinate system that has n as its normal
    Vector u = n.Normal(0);
    Vector v = n.Normal(1);

    double area = 0;
    for(int i = 0; i < (l.n - 1); i++) {
        double u0 = (l.elem[i  ].p).Dot(u);
        double v0 = (l.elem[i  ].p).Dot(v);
        double u1 = (l.elem[i+1].p).Dot(u);
        double v1 = (l.elem[i+1].p).Dot(v);

        area += ((v0 + v1)/2)*(u1 - u0);
    }

    return (area < 0);
}

bool SContour::ContainsPointProjdToNormal(Vector n, Vector p) {
    Vector u = n.Normal(0);
    Vector v = n.Normal(1);

    double up = p.Dot(u);
    double vp = p.Dot(v);

    bool inside = false;
    for(int i = 0; i < (l.n - 1); i++) {
        double ua = (l.elem[i  ].p).Dot(u);
        double va = (l.elem[i  ].p).Dot(v);
        // The curve needs to be exactly closed; approximation is death.
        double ub = (l.elem[(i+1)%(l.n-1)].p).Dot(u);
        double vb = (l.elem[(i+1)%(l.n-1)].p).Dot(v);

        if ((((va <= vp) && (vp < vb)) ||
             ((vb <= vp) && (vp < va))) &&
            (up < (ub - ua) * (vp - va) / (vb - va) + ua))
        {
          inside = !inside;
        }
    }

    return inside;
}


void SContour::IntersectAgainstPlane(double *inter, int *inters,
                                     Vector u, Vector v, double vp)
{
    for(int i = 0; i < (l.n - 1); i++) {
        double ua = (l.elem[i  ].p).Dot(u);
        double va = (l.elem[i  ].p).Dot(v);
        double ub = (l.elem[(i+1)%(l.n-1)].p).Dot(u);
        double vb = (l.elem[(i+1)%(l.n-1)].p).Dot(v);

        double u0, v0, du, dv;

        if(va < vb) {
            u0 = ua; v0 = va;
            du = (ub - ua); dv = (vb - va);
        } else {
            u0 = ub; v0 = vb;
            du = (ua - ub); dv = (va - vb);
        }

        if(dv == 0) continue; 

        double t = (vp - v0)/dv;
        if(t >= 0 && t < 1) {
            double ui = u0 + t*du;
            // Our line v = vp intersects the edge; record the u value
            inter[(*inters)++] = ui;
        }
    }
}

void SContour::Reverse(void) {
    int i;
    for(i = 0; i < (l.n / 2); i++) {
        int i2 = (l.n - 1) - i;
        SPoint t = l.elem[i2];
        l.elem[i2] = l.elem[i];
        l.elem[i] = t;
    }
}

void SPolyhedron::AddFace(SPolygon *p) {
    l.Add(p);
}

void SPolyhedron::Clear(void) {
    int i;
    for(i = 0; i < l.n; i++) {
        (l.elem[i]).Clear();
    }
    l.Clear();
}

void SPolyhedron::IntersectAgainstPlane(SEdgeList *d, Vector p0, Vector n) {
    int i;
    for(i = 0; i < l.n; i++) {
        SPolygon *sp = &(l.elem[i]);
        sp->IntersectAgainstPlane(d, p0, n);
    }
}

bool SPolyhedron::Boolean(SPolyhedron *dest, int op, SPolyhedron *b) {
    int i;
    dbp(">>>");

    for(i = 0; i < l.n; i++) {
        SPolygon *sp = &(l.elem[i]);
        sp->CopyBreaking(dest, b, 0);
    }

    for(i = 0; i < b->l.n; i++) {
        SPolygon *sp = &(b->l.elem[i]);
        sp->CopyBreaking(dest, this, 1);
    }

    return true;
}

