//-----------------------------------------------------------------------------
// Binary space partitioning tree, used to represent a volume in 3-space
// bounded by a triangle mesh. These are used to compute Boolean operations
// on meshes. These aren't used for anything relating to an SShell of
// ratpoly surfaces.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

SBsp2 *SBsp2::Alloc() { return (SBsp2 *)AllocTemporary(sizeof(SBsp2)); }
SBsp3 *SBsp3::Alloc() { return (SBsp3 *)AllocTemporary(sizeof(SBsp3)); }

SBsp3 *SBsp3::FromMesh(const SMesh *m) {
    SMesh mc = {};
    for(auto const &elt : m->l) { mc.AddTriangle(&elt); }

    srand(0); // Let's be deterministic, at least!
    int n = mc.l.n;
    while(n > 1) {
        int k = rand() % n;
        n--;
        swap(mc.l[k], mc.l[n]);
    }

    SBsp3 *bsp3 = NULL;
    for(auto &elt : mc.l) { bsp3 = InsertOrCreate(bsp3, &elt, NULL); }
    mc.Clear();
    return bsp3;
}

Vector SBsp3::IntersectionWith(Vector a, Vector b) const {
    double da = a.Dot(n) - d;
    double db = b.Dot(n) - d;
    ssassert(da*db < 0, "Expected segment to intersect BSP node");

    double dab = (db - da);
    return (a.ScaledBy(db/dab)).Plus(b.ScaledBy(-da/dab));
}

void SBsp3::InsertInPlane(bool pos2, STriangle *tr, SMesh *m) {
    Vector tc = ((tr->a).Plus(tr->b).Plus(tr->c)).ScaledBy(1.0/3);

    bool onFace = false;
    bool sameNormal = false;
    double maxNormalMag = -1;

    Vector lln, trn = tr->Normal();

    SBsp3 *ll = this;
    while(ll) {
        if((ll->tri).ContainsPoint(tc)) {
            onFace = true;
            // If the mesh contains almost-zero-area triangles, and we're
            // just on the edge of one of those, then don't trust its normal.
            lln = (ll->tri).Normal();
            if(lln.Magnitude() > maxNormalMag) {
                sameNormal = trn.Dot(lln) > 0;
                maxNormalMag = lln.Magnitude();
            }
        }
        ll = ll->more;
    }

    if(m->flipNormal && ((!pos2 && !onFace) ||
                                   (onFace && !sameNormal && m->keepCoplanar)))
    {
        m->AddTriangle(tr->meta, tr->c, tr->b, tr->a);
    } else if(!(m->flipNormal) && ((pos2 && !onFace) ||
                                   (onFace && sameNormal && m->keepCoplanar)))
    {
        m->AddTriangle(tr->meta, tr->a, tr->b, tr->c);
    } else {
        m->atLeastOneDiscarded = true;
    }
}

void SBsp3::InsertHow(BspClass how, STriangle *tr, SMesh *instead) {
    switch(how) {
        case BspClass::POS:
            if(instead && !pos) goto alt;
            pos = InsertOrCreate(pos, tr, instead);
            break;

        case BspClass::NEG:
            if(instead && !neg) goto alt;
            neg = InsertOrCreate(neg, tr, instead);
            break;

        case BspClass::COPLANAR: {
            if(instead) goto alt;
            SBsp3 *m = Alloc();
            m->n = n;
            m->d = d;
            m->tri = *tr;
            m->more = more;
            more = m;
            break;
        }
    }
    return;

alt:
    if(how == BspClass::POS && !(instead->flipNormal)) {
        instead->AddTriangle(tr->meta, tr->a, tr->b, tr->c);
    } else if(how == BspClass::NEG && instead->flipNormal) {
        instead->AddTriangle(tr->meta, tr->c, tr->b, tr->a);
    } else if(how == BspClass::COPLANAR) {
        if(edges) {
            edges->InsertTriangle(tr, instead, this);
        } else {
            // I suppose this actually is allowed to happen, if the coplanar
            // face is the leaf, and all of its neighbors are earlier in tree?
            InsertInPlane(/*pos2=*/false, tr, instead);
        }
    } else {
        instead->atLeastOneDiscarded = true;
    }
}

class BspUtil {
public:
    SBsp3      *bsp;

    size_t      onc;
    size_t      posc;
    size_t      negc;
    bool       *isPos;
    bool       *isNeg;
    bool       *isOn;

    // triangle operations
    STriangle  *tr;
    STriangle  *btri; // also as alone
    STriangle  *ctri;

    // convex operations
    Vector     *on;
    size_t      npos;
    size_t      nneg;
    Vector     *vpos; // also as quad
    Vector     *vneg;

    static BspUtil *Alloc() {
        return (BspUtil *)AllocTemporary(sizeof(BspUtil));
    }

    void AllocOn() {
        on = (Vector *)AllocTemporary(sizeof(Vector) * 2);
    }

    void AllocTriangle() {
        btri = (STriangle *)AllocTemporary(sizeof(STriangle));
    }

    void AllocTriangles() {
        btri = (STriangle *)AllocTemporary(sizeof(STriangle) * 2);
        ctri = &btri[1];
    }

    void AllocQuad() {
        vpos = (Vector *)AllocTemporary(sizeof(Vector) * 4);
    }

    void AllocClassify(size_t size) {
        // Allocate a one big piece is faster than a small ones.
        isPos = (bool *)AllocTemporary(sizeof(bool) * size * 3);
        isNeg = &isPos[size];
        isOn  = &isNeg[size];
    }

    void AllocVertices(size_t size) {
        vpos = (Vector *)AllocTemporary(sizeof(Vector) * size * 2);
        vneg = &vpos[size];
    }

    void ClassifyTriangle(STriangle *tri, SBsp3 *node) {
        tr   = tri;
        bsp  = node;
        onc  = 0;
        posc = 0;
        negc = 0;

        AllocClassify(3);

        double dt[3] = { (tr->a).Dot(bsp->n), (tr->b).Dot(bsp->n), (tr->c).Dot(bsp->n) };
        double d = bsp->d;
        // Count vertices in the plane
        for(int i = 0; i < 3; i++) {
            if(dt[i] > d + LENGTH_EPS) {
                posc++;
                isPos[i] = true;
            } else if(dt[i] < d - LENGTH_EPS) {
                negc++;
                isNeg[i] = true;
            } else {
                onc++;
                isOn[i] = true;
            }
        }
    }

    bool ClassifyConvex(Vector *vertex, size_t cnt, SBsp3 *node, bool insertEdge) {
        bsp  = node;
        onc  = 0;
        posc = 0;
        negc = 0;

        AllocClassify(cnt);
        AllocOn();

        for(size_t i = 0; i < cnt; i++) {
            double dt = bsp->n.Dot(vertex[i]);
            isPos[i] = isNeg[i] = isOn[i] = false;
            if(fabs(dt - bsp->d) < LENGTH_EPS) {
                isOn[i] = true;
                if(onc < 2) {
                    on[onc] = vertex[i];
                }
                onc++;
            } else if(dt > bsp->d) {
                isPos[i] = true;
                posc++;
            } else {
                isNeg[i] = true;
                negc++;
            }
        }

        if(onc != 2 && onc != 1 && onc != 0) return false;
        if(onc == 2) {
            if(insertEdge) {
                Vector e01 = (vertex[1]).Minus(vertex[0]);
                Vector e12 = (vertex[2]).Minus(vertex[1]);
                Vector out = e01.Cross(e12);
                SEdge se = SEdge::From(on[0], on[1]);
                bsp->edges = SBsp2::InsertOrCreateEdge(bsp->edges, &se, bsp->n, out);
            }
        }
        return true;
    }

    bool ClassifyConvexVertices(Vector *vertex, size_t cnt, bool insertEdges) {
        Vector inter[2];
        int inters = 0;

        npos = 0;
        nneg = 0;

        // Enlarge vertices list to consider two intersections
        AllocVertices(cnt + 4);

        for(size_t i = 0; i < cnt; i++) {
            size_t ip = WRAP((i + 1), cnt);

            if(isPos[i]) {
                vpos[npos++] = vertex[i];
            }
            if(isNeg[i]) {
                vneg[nneg++] = vertex[i];
            }
            if(isOn[i]) {
                vneg[nneg++] = vertex[i];
                vpos[npos++] = vertex[i];
            }
            if((isPos[i] && isNeg[ip]) || (isNeg[i] && isPos[ip])) {
                Vector vi = bsp->IntersectionWith(vertex[i], vertex[ip]);
                vpos[npos++] = vi;
                vneg[nneg++] = vi;

                if(inters >= 2) return false; // triangulate: XXX shouldn't happen but does
                inter[inters++] = vi;
            }
        }
        ssassert(npos <= cnt + 1 && nneg <= cnt + 1, "Impossible");

        if(insertEdges) {
            Vector e01 = (vertex[1]).Minus(vertex[0]);
            Vector e12 = (vertex[2]).Minus(vertex[1]);
            Vector out = e01.Cross(e12);
            if(inters == 2) {
                SEdge se = SEdge::From(inter[0], inter[1]);
                bsp->edges = SBsp2::InsertOrCreateEdge(bsp->edges, &se, bsp->n, out);
            } else if(inters == 1 && onc == 1) {
                SEdge se = SEdge::From(inter[0], on[0]);
                bsp->edges = SBsp2::InsertOrCreateEdge(bsp->edges, &se, bsp->n, out);
            } else if(inters == 0 && onc == 2) {
                // We already handled this on-plane existing edge
            } else {
                return false; //triangulate;
            }
        }
        if(nneg < 3 || npos < 3) return false; // triangulate; // XXX

        return true;
    }

    void ProcessEdgeInsert() {
        ssassert(onc == 2, "Impossible");

        Vector a, b;
        if     (!isOn[0]) { a = tr->b; b = tr->c; }
        else if(!isOn[1]) { a = tr->c; b = tr->a; }
        else if(!isOn[2]) { a = tr->a; b = tr->b; }
        else ssassert(false, "Impossible");

        SEdge se = SEdge::From(a, b);
        bsp->edges = SBsp2::InsertOrCreateEdge(bsp->edges, &se, bsp->n, tr->Normal());
    }

    bool SplitIntoTwoTriangles(bool insertEdge) {
        ssassert(posc == 1 && negc == 1 && onc == 1, "Impossible");

        bool bpos;
        Vector a, b, c;

        // Standardize so that a is on the plane
        if       (isOn[0]) { a = tr->a; b = tr->b; c = tr->c; bpos = isPos[1];
        } else if(isOn[1]) { a = tr->b; b = tr->c; c = tr->a; bpos = isPos[2];
        } else if(isOn[2]) { a = tr->c; b = tr->a; c = tr->b; bpos = isPos[0];
        } else ssassert(false, "Impossible");

        AllocTriangles();
        Vector bPc = bsp->IntersectionWith(b, c);
        *btri = STriangle::From(tr->meta, a, b, bPc);
        *ctri = STriangle::From(tr->meta, c, a, bPc);

        if(insertEdge) {
            SEdge se = SEdge::From(a, bPc);
            bsp->edges = SBsp2::InsertOrCreateEdge(bsp->edges, &se, bsp->n, tr->Normal());
        }

        return bpos;
    }

    bool SplitIntoTwoPieces(bool insertEdge) {
        Vector a, b, c;
        if(posc == 2 && negc == 1) {
            // Standardize so that a is on one side, and b and c are on the other.
            if       (isNeg[0]) {   a = tr->a; b = tr->b; c = tr->c;
            } else if(isNeg[1]) {   a = tr->b; b = tr->c; c = tr->a;
            } else if(isNeg[2]) {   a = tr->c; b = tr->a; c = tr->b;
            } else ssassert(false, "Impossible");
        } else if(posc == 1 && negc == 2) {
            if       (isPos[0]) {   a = tr->a; b = tr->b; c = tr->c;
            } else if(isPos[1]) {   a = tr->b; b = tr->c; c = tr->a;
            } else if(isPos[2]) {   a = tr->c; b = tr->a; c = tr->b;
            } else ssassert(false, "Impossible");
        } else ssassert(false, "Impossible");

        Vector aPb = bsp->IntersectionWith(a, b);
        Vector cPa = bsp->IntersectionWith(c, a);
        AllocTriangle();
        AllocQuad();

        *btri = STriangle::From(tr->meta, a, aPb, cPa);

        vpos[0] = aPb;
        vpos[1] = b;
        vpos[2] = c;
        vpos[3] = cPa;

        if(insertEdge) {
            SEdge se = SEdge::From(aPb, cPa);
            bsp->edges = SBsp2::InsertOrCreateEdge(bsp->edges, &se, bsp->n, btri->Normal());
        }

        return posc == 2 && negc == 1;
    }

    static SBsp3 *Triangulate(SBsp3 *bsp, const STriMeta &meta, Vector *vertex,
                              size_t cnt, SMesh *instead) {
        for(size_t i = 0; i < cnt - 2; i++) {
            STriangle tr = STriangle::From(meta, vertex[0], vertex[i + 1], vertex[i + 2]);
            bsp = SBsp3::InsertOrCreate(bsp, &tr, instead);
        }
        return bsp;
    }
};

void SBsp3::InsertConvexHow(BspClass how, STriMeta meta, Vector *vertex, size_t n,
                            SMesh *instead) {
    switch(how) {
        case BspClass::POS:
            if(pos) {
                pos = pos->InsertConvex(meta, vertex, n, instead);
                return;
            }
            break;

        case BspClass::NEG:
            if(neg) {
                neg = neg->InsertConvex(meta, vertex, n, instead);
                return;
            }
            break;

        default: ssassert(false, "Unexpected BSP insert type");
    }

    for(size_t i = 0; i < n - 2; i++) {
        STriangle tr = STriangle::From(meta,
                                       vertex[0], vertex[i+1], vertex[i+2]);
        InsertHow(how, &tr, instead);
    }
}

SBsp3 *SBsp3::InsertConvex(STriMeta meta, Vector *vertex, size_t cnt, SMesh *instead) {
    BspUtil *u = BspUtil::Alloc();
    if(u->ClassifyConvex(vertex, cnt, this, !instead)) {
        if(u->posc == 0) {
            InsertConvexHow(BspClass::NEG, meta, vertex, cnt, instead);
            return this;
        }
        if(u->negc == 0) {
            InsertConvexHow(BspClass::POS, meta, vertex, cnt, instead);
            return this;
        }

        if(u->ClassifyConvexVertices(vertex, cnt, !instead)) {
            InsertConvexHow(BspClass::NEG, meta, u->vneg, u->nneg, instead);
            InsertConvexHow(BspClass::POS, meta, u->vpos, u->npos, instead);
            return this;
        }
    }

    // We don't handle the special case for this; do it as triangles
    return BspUtil::Triangulate(this, meta, vertex, cnt, instead);
}

SBsp3 *SBsp3::InsertOrCreate(SBsp3 *where, STriangle *tr, SMesh *instead) {
    if(where == NULL) {
        if(instead) {
            if(instead->flipNormal) {
                instead->atLeastOneDiscarded = true;
            } else {
                instead->AddTriangle(tr->meta, tr->a, tr->b, tr->c);
            }
            return NULL;
        }

        // Brand new node; so allocate for it, and fill us in.
        SBsp3 *r = Alloc();
        r->n = (tr->Normal()).WithMagnitude(1);
        r->d = (tr->a).Dot(r->n);
        r->tri = *tr;
        return r;
    }
    where->Insert(tr, instead);
    return where;
}

void SBsp3::Insert(STriangle *tr, SMesh *instead) {
    BspUtil *u = BspUtil::Alloc();
    u->ClassifyTriangle(tr, this);

    // All vertices in-plane
    if(u->onc == 3) {
        InsertHow(BspClass::COPLANAR, tr, instead);
        return;
    }

    // No split required
    if(u->posc == 0 || u->negc == 0) {
        if(!instead && u->onc == 2) {
            u->ProcessEdgeInsert();
        }

        if(u->posc > 0) {
            InsertHow(BspClass::POS, tr, instead);
        } else {
            InsertHow(BspClass::NEG, tr, instead);
        }
        return;
    }

    // The polygon must be split into two triangles, one above, one below.
    if(u->posc == 1 && u->negc == 1 && u->onc == 1) {
        if(u->SplitIntoTwoTriangles(!instead)) {
            InsertHow(BspClass::POS, u->btri, instead);
            InsertHow(BspClass::NEG, u->ctri, instead);
        } else {
            InsertHow(BspClass::POS, u->ctri, instead);
            InsertHow(BspClass::NEG, u->btri, instead);
        }
        return;
    }

    // The polygon must be split into two pieces: a triangle and a quad.
    if(u->SplitIntoTwoPieces(!instead)) {
        InsertConvexHow(BspClass::POS, tr->meta, u->vpos, 4, instead);
        InsertHow(BspClass::NEG, u->btri, instead);
    } else {
        InsertConvexHow(BspClass::NEG, tr->meta, u->vpos, 4, instead);
        InsertHow(BspClass::POS, u->btri, instead);
    }
}

void SBsp3::GenerateInPaintOrder(SMesh *m) const {
    // Doesn't matter which branch we take if the normal has zero z
    // component, so don't need a separate case for that.
    if(n.z < 0) {
        if(pos) pos->GenerateInPaintOrder(m);
    } else {
        if(neg) neg->GenerateInPaintOrder(m);
    }

    const SBsp3 *flip = this;
    while(flip) {
        m->AddTriangle(&(flip->tri));
        flip = flip->more;
    }

    if(n.z < 0) {
        if(neg) neg->GenerateInPaintOrder(m);
    } else {
        if(pos) pos->GenerateInPaintOrder(m);
    }
}

/////////////////////////////////

Vector SBsp2::IntersectionWith(Vector a, Vector b) const {
    double da = a.Dot(no) - d;
    double db = b.Dot(no) - d;
    ssassert(da*db < 0, "Expected segment to intersect BSP node");

    double dab = (db - da);
    return (a.ScaledBy(db/dab)).Plus(b.ScaledBy(-da/dab));
}

SBsp2 *SBsp2::InsertOrCreateEdge(SBsp2 *where, SEdge *nedge, Vector nnp, Vector out) {
    if(where == NULL) {
        // Brand new node; so allocate for it, and fill us in.
        SBsp2 *r = Alloc();
        r->np = nnp;
        r->no = ((r->np).Cross((nedge->b).Minus(nedge->a))).WithMagnitude(1);
        if(out.Dot(r->no) < 0) {
            r->no = (r->no).ScaledBy(-1);
        }
        r->d = (nedge->a).Dot(r->no);
        r->edge = *nedge;
        return r;
    }
    where->InsertEdge(nedge, nnp, out);
    return where;
}

void SBsp2::InsertEdge(SEdge *nedge, Vector nnp, Vector out) {

    double dt[2] = { (nedge->a).Dot(no), (nedge->b).Dot(no) };

    bool isPos[2] = {}, isNeg[2] = {}, isOn[2] = {};
    for(int i = 0; i < 2; i++) {
        if(fabs(dt[i] - d) < LENGTH_EPS) {
            isOn[i] = true;
        } else if(dt[i] > d) {
            isPos[i] = true;
        } else {
            isNeg[i] = true;
        }
    }

    if((isPos[0] && isPos[1])||(isPos[0] && isOn[1])||(isOn[0] && isPos[1])) {
        pos = InsertOrCreateEdge(pos, nedge, nnp, out);
        return;
    }
    if((isNeg[0] && isNeg[1])||(isNeg[0] && isOn[1])||(isOn[0] && isNeg[1])) {
        neg = InsertOrCreateEdge(neg, nedge, nnp, out);
        return;
    }
    if(isOn[0] && isOn[1]) {
        SBsp2 *m = Alloc();

        m->np = nnp;
        m->no = ((m->np).Cross((nedge->b).Minus(nedge->a))).WithMagnitude(1);
        if(out.Dot(m->no) < 0) {
            m->no = (m->no).ScaledBy(-1);
        }
        m->d = (nedge->a).Dot(m->no);
        m->edge = *nedge;

        m->more = more;
        more = m;
        return;
    }
    if((isPos[0] && isNeg[1]) || (isNeg[0] && isPos[1])) {
        Vector aPb = IntersectionWith(nedge->a, nedge->b);

        SEdge ea = SEdge::From(nedge->a, aPb);
        SEdge eb = SEdge::From(aPb, nedge->b);

        if(isPos[0]) {
            pos = InsertOrCreateEdge(pos, &ea, nnp, out);
            neg = InsertOrCreateEdge(neg, &eb, nnp, out);
        } else {
            neg = InsertOrCreateEdge(neg, &ea, nnp, out);
            pos = InsertOrCreateEdge(pos, &eb, nnp, out);
        }
        return;
    }
    ssassert(false, "Impossible");
}

void SBsp2::InsertTriangleHow(BspClass how, STriangle *tr, SMesh *m, SBsp3 *bsp3) {
    switch(how) {
        case BspClass::POS:
            if(pos) {
                pos->InsertTriangle(tr, m, bsp3);
            } else {
                bsp3->InsertInPlane(/*pos2=*/true, tr, m);
            }
            break;

        case BspClass::NEG:
            if(neg) {
                neg->InsertTriangle(tr, m, bsp3);
            } else {
                bsp3->InsertInPlane(/*pos2=*/false, tr, m);
            }
            break;

        default: ssassert(false, "Unexpected BSP insert type");
    }
}

void SBsp2::InsertTriangle(STriangle *tr, SMesh *m, SBsp3 *bsp3) {
    double dt[3] = { (tr->a).Dot(no), (tr->b).Dot(no), (tr->c).Dot(no) };

    bool isPos[3] = {}, isNeg[3] = {}, isOn[3] = {};
    int inc = 0, posc = 0, negc = 0;
    for(int i = 0; i < 3; i++) {
        if(fabs(dt[i] - d) < LENGTH_EPS) {
            isOn[i] = true;
            inc++;
        } else if(dt[i] > d) {
            isPos[i] = true;
            posc++;
        } else {
            isNeg[i] = true;
            negc++;
        }
    }

    if(inc == 3) {
        // All vertices on-line; so it's a degenerate triangle, to ignore.
        return;
    }

    // No split required
    if(posc == 0 || negc == 0) {
        if(posc > 0) {
            InsertTriangleHow(BspClass::POS, tr, m, bsp3);
        } else {
            InsertTriangleHow(BspClass::NEG, tr, m, bsp3);
        }
        return;
    }

    // The polygon must be split into two pieces, one above, one below.
    Vector a, b, c;

    if(posc == 1 && negc == 1 && inc == 1) {
        bool bpos;
        // Standardize so that a is on the plane
        if       (isOn[0]) { a = tr->a; b = tr->b; c = tr->c; bpos = isPos[1];
        } else if(isOn[1]) { a = tr->b; b = tr->c; c = tr->a; bpos = isPos[2];
        } else if(isOn[2]) { a = tr->c; b = tr->a; c = tr->b; bpos = isPos[0];
        } else ssassert(false, "Impossible");

        Vector bPc = IntersectionWith(b, c);
        STriangle btri = STriangle::From(tr->meta, a, b, bPc);
        STriangle ctri = STriangle::From(tr->meta, c, a, bPc);

        if(bpos) {
            InsertTriangleHow(BspClass::POS, &btri, m, bsp3);
            InsertTriangleHow(BspClass::NEG, &ctri, m, bsp3);
        } else {
            InsertTriangleHow(BspClass::POS, &ctri, m, bsp3);
            InsertTriangleHow(BspClass::NEG, &btri, m, bsp3);
        }

        return;
    }

    if(posc == 2 && negc == 1) {
        // Standardize so that a is on one side, and b and c are on the other.
        if       (isNeg[0]) {   a = tr->a; b = tr->b; c = tr->c;
        } else if(isNeg[1]) {   a = tr->b; b = tr->c; c = tr->a;
        } else if(isNeg[2]) {   a = tr->c; b = tr->a; c = tr->b;
        } else ssassert(false, "Impossible");

    } else if(posc == 1 && negc == 2) {
        if       (isPos[0]) {   a = tr->a; b = tr->b; c = tr->c;
        } else if(isPos[1]) {   a = tr->b; b = tr->c; c = tr->a;
        } else if(isPos[2]) {   a = tr->c; b = tr->a; c = tr->b;
        } else ssassert(false, "Impossible");
    } else ssassert(false, "Impossible");

    Vector aPb = IntersectionWith(a, b);
    Vector cPa = IntersectionWith(c, a);

    STriangle alone = STriangle::From(tr->meta, a,   aPb, cPa);
    STriangle quad1 = STriangle::From(tr->meta, aPb, b,   c  );
    STriangle quad2 = STriangle::From(tr->meta, aPb, c,   cPa);

    if(posc == 2 && negc == 1) {
        InsertTriangleHow(BspClass::POS, &quad1, m, bsp3);
        InsertTriangleHow(BspClass::POS, &quad2, m, bsp3);
        InsertTriangleHow(BspClass::NEG, &alone, m, bsp3);
    } else {
        InsertTriangleHow(BspClass::NEG, &quad1, m, bsp3);
        InsertTriangleHow(BspClass::NEG, &quad2, m, bsp3);
        InsertTriangleHow(BspClass::POS, &alone, m, bsp3);
    }

    return;
}
