//-----------------------------------------------------------------------------
// Binary space partitioning tree, used to represent a volume in 3-space
// bounded by a triangle mesh. These are used to compute Boolean operations
// on meshes. These aren't used for anything relating to an SShell of
// ratpoly surfaces.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

SBsp2 *SBsp2::Alloc(void) { return (SBsp2 *)AllocTemporary(sizeof(SBsp2)); }
SBsp3 *SBsp3::Alloc(void) { return (SBsp3 *)AllocTemporary(sizeof(SBsp3)); }

SBsp3 *SBsp3::FromMesh(SMesh *m) {
    SBsp3 *bsp3 = NULL;
    int i;

    SMesh mc = {};
    for(i = 0; i < m->l.n; i++) {
        mc.AddTriangle(&(m->l.elem[i]));
    }

    srand(0); // Let's be deterministic, at least!
    int n = mc.l.n;
    while(n > 1) {
        int k = rand() % n;
        n--;
        swap(mc.l.elem[k], mc.l.elem[n]);
    }

    for(i = 0; i < mc.l.n; i++) {
        bsp3 = InsertOrCreate(bsp3, &(mc.l.elem[i]), NULL);
    }

    mc.Clear();
    return bsp3;
}

Vector SBsp3::IntersectionWith(Vector a, Vector b) {
    double da = a.Dot(n) - d;
    double db = b.Dot(n) - d;
    if(da*db > 0) oops();

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

void SBsp3::InsertHow(int how, STriangle *tr, SMesh *instead) {
    switch(how) {
        case POS:
            if(instead && !pos) goto alt;
            pos = InsertOrCreate(pos, tr, instead);
            break;

        case NEG:
            if(instead && !neg) goto alt;
            neg = InsertOrCreate(neg, tr, instead);
            break;

        case COPLANAR: {
            if(instead) goto alt;
            SBsp3 *m = Alloc();
            m->n = n;
            m->d = d;
            m->tri = *tr;
            m->more = more;
            more = m;
            break;
        }
        default: oops();
    }
    return;

alt:
    if(how == POS && !(instead->flipNormal)) {
        instead->AddTriangle(tr->meta, tr->a, tr->b, tr->c);
    } else if(how == NEG && instead->flipNormal) {
        instead->AddTriangle(tr->meta, tr->c, tr->b, tr->a);
    } else if(how == COPLANAR) {
        if(edges) {
            edges->InsertTriangle(tr, instead, this);
        } else {
            // I suppose this actually is allowed to happen, if the coplanar
            // face is the leaf, and all of its neighbors are earlier in tree?
            InsertInPlane(false, tr, instead);
        }
    } else {
        instead->atLeastOneDiscarded = true;
    }
}

void SBsp3::InsertConvexHow(int how, STriMeta meta, Vector *vertex, int n,
                            SMesh *instead)
{
    switch(how) {
        case POS:
            if(pos) {
                pos = pos->InsertConvex(meta, vertex, n, instead);
                return;
            }
            break;

        case NEG:
            if(neg) {
                neg = neg->InsertConvex(meta, vertex, n, instead);
                return;
            }
            break;

        default: oops();
    }
    int i;
    for(i = 0; i < n - 2; i++) {
        STriangle tr = STriangle::From(meta,
                                       vertex[0], vertex[i+1], vertex[i+2]);
        InsertHow(how, &tr, instead);
    }
}

SBsp3 *SBsp3::InsertConvex(STriMeta meta, Vector *vertex, int cnt,
                           SMesh *instead)
{
    Vector e01 = (vertex[1]).Minus(vertex[0]);
    Vector e12 = (vertex[2]).Minus(vertex[1]);
    Vector out = e01.Cross(e12);

#define MAX_VERTICES 50
    if(cnt+1 >= MAX_VERTICES) goto triangulate;

    int i;
    Vector on[2];
    bool isPos[MAX_VERTICES];
    bool isNeg[MAX_VERTICES];
    bool isOn[MAX_VERTICES];
    int posc, negc, onc; posc = negc = onc = 0;
    for(i = 0; i < cnt; i++) {
        double dt = n.Dot(vertex[i]);
        isPos[i] = isNeg[i] = isOn[i] = false;
        if(fabs(dt - d) < LENGTH_EPS) {
            isOn[i] = true;
            if(onc < 2) {
                on[onc] = vertex[i];
            }
            onc++;
        } else if(dt > d) {
            isPos[i] = true;
            posc++;
        } else {
            isNeg[i] = true;
            negc++;
        }
    }
    if(onc != 2 && onc != 1 && onc != 0) goto triangulate;

    if(onc == 2) {
        if(!instead) {
            SEdge se = SEdge::From(on[0], on[1]);
            edges = SBsp2::InsertOrCreateEdge(edges, &se, n, out);
        }
    }

    if(posc == 0) {
        InsertConvexHow(NEG, meta, vertex, cnt, instead);
        return this;
    }
    if(negc == 0) {
        InsertConvexHow(POS, meta, vertex, cnt, instead);
        return this;
    }

    Vector vpos[MAX_VERTICES];
    Vector vneg[MAX_VERTICES];
    int npos, nneg; npos = nneg = 0;

    Vector inter[2];
    int inters; inters = 0;

    for(i = 0; i < cnt; i++) {
        int ip = WRAP((i + 1), cnt);

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
            Vector vi = IntersectionWith(vertex[i], vertex[ip]);
            vpos[npos++] = vi;
            vneg[nneg++] = vi;

            if(inters >= 2) goto triangulate; // XXX shouldn't happen but does
            inter[inters++] = vi;
        }
    }
    if(npos > cnt + 1 || nneg > cnt + 1) oops();

    if(!instead) {
        if(inters == 2) {
            SEdge se = SEdge::From(inter[0], inter[1]);
            edges = SBsp2::InsertOrCreateEdge(edges, &se, n, out);
        } else if(inters == 1 && onc == 1) {
            SEdge se = SEdge::From(inter[0], on[0]);
            edges = SBsp2::InsertOrCreateEdge(edges, &se, n, out);
        } else if(inters == 0 && onc == 2) {
            // We already handled this on-plane existing edge
        } else {
            goto triangulate;
        }
    }
    if(nneg < 3 || npos < 3) goto triangulate; // XXX

    InsertConvexHow(NEG, meta, vneg, nneg, instead);
    InsertConvexHow(POS, meta, vpos, npos, instead);
    return this;

triangulate:
    // We don't handle the special case for this; do it as triangles
    SBsp3 *r = this;
    for(i = 0; i < cnt - 2; i++) {
        STriangle tr = STriangle::From(meta,
                                       vertex[0], vertex[i+1], vertex[i+2]);
        r = InsertOrCreate(r, &tr, instead);
    }
    return r;
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
    double dt[3] = { (tr->a).Dot(n), (tr->b).Dot(n), (tr->c).Dot(n) };

    int inc = 0, posc = 0, negc = 0;
    bool isPos[3] = {}, isNeg[3] = {}, isOn[3] = {};
    // Count vertices in the plane
    for(int i = 0; i < 3; i++) {
        if(fabs(dt[i] - d) < LENGTH_EPS) {
            inc++;
            isOn[i] = true;
        } else if(dt[i] > d) {
            posc++;
            isPos[i] = true;
        } else {
            negc++;
            isNeg[i] = true;
        }
    }

    // All vertices in-plane
    if(inc == 3) {
        InsertHow(COPLANAR, tr, instead);
        return;
    }

    // No split required
    if(posc == 0 || negc == 0) {
        if(inc == 2) {
            Vector a, b;
            if     (!isOn[0]) { a = tr->b; b = tr->c; }
            else if(!isOn[1]) { a = tr->c; b = tr->a; }
            else if(!isOn[2]) { a = tr->a; b = tr->b; }
            else oops();
            if(!instead) {
                SEdge se = SEdge::From(a, b);
                edges = SBsp2::InsertOrCreateEdge(edges, &se, n, tr->Normal());
            }
        }

        if(posc > 0) {
            InsertHow(POS, tr, instead);
        } else {
            InsertHow(NEG, tr, instead);
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
        } else oops();

        Vector bPc = IntersectionWith(b, c);
        STriangle btri = STriangle::From(tr->meta, a, b, bPc);
        STriangle ctri = STriangle::From(tr->meta, c, a, bPc);

        if(bpos) {
            InsertHow(POS, &btri, instead);
            InsertHow(NEG, &ctri, instead);
        } else {
            InsertHow(POS, &ctri, instead);
            InsertHow(NEG, &btri, instead);
        }

        if(!instead) {
            SEdge se = SEdge::From(a, bPc);
            edges = SBsp2::InsertOrCreateEdge(edges, &se, n, tr->Normal());
        }

        return;
    }

    if(posc == 2 && negc == 1) {
        // Standardize so that a is on one side, and b and c are on the other.
        if       (isNeg[0]) {   a = tr->a; b = tr->b; c = tr->c;
        } else if(isNeg[1]) {   a = tr->b; b = tr->c; c = tr->a;
        } else if(isNeg[2]) {   a = tr->c; b = tr->a; c = tr->b;
        } else oops();

    } else if(posc == 1 && negc == 2) {
        if       (isPos[0]) {   a = tr->a; b = tr->b; c = tr->c;
        } else if(isPos[1]) {   a = tr->b; b = tr->c; c = tr->a;
        } else if(isPos[2]) {   a = tr->c; b = tr->a; c = tr->b;
        } else oops();
    } else oops();

    Vector aPb = IntersectionWith(a, b);
    Vector cPa = IntersectionWith(c, a);

    STriangle alone = STriangle::From(tr->meta, a, aPb, cPa);
    Vector quad[4] = { aPb, b, c, cPa };

    if(posc == 2 && negc == 1) {
        InsertConvexHow(POS, tr->meta, quad, 4, instead);
        InsertHow(NEG, &alone, instead);
    } else {
        InsertConvexHow(NEG, tr->meta, quad, 4, instead);
        InsertHow(POS, &alone, instead);
    }
    if(!instead) {
        SEdge se = SEdge::From(aPb, cPa);
        edges = SBsp2::InsertOrCreateEdge(edges, &se, n, alone.Normal());
    }

    return;
}

void SBsp3::GenerateInPaintOrder(SMesh *m) {

    // Doesn't matter which branch we take if the normal has zero z
    // component, so don't need a separate case for that.
    if(n.z < 0) {
        if(pos) pos->GenerateInPaintOrder(m);
    } else {
        if(neg) neg->GenerateInPaintOrder(m);
    }

    SBsp3 *flip = this;
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

void SBsp3::DebugDraw(void) {

    if(pos) pos->DebugDraw();
    Vector norm = tri.Normal();
    glNormal3d(norm.x, norm.y, norm.z);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glBegin(GL_TRIANGLES);
        ssglVertex3v(tri.a);
        ssglVertex3v(tri.b);
        ssglVertex3v(tri.c);
    glEnd();

    glDisable(GL_LIGHTING);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    ssglDepthRangeOffset(2);
    glBegin(GL_TRIANGLES);
        ssglVertex3v(tri.a);
        ssglVertex3v(tri.b);
        ssglVertex3v(tri.c);
    glEnd();

    glDisable(GL_LIGHTING);
    glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    glPointSize(10);
    ssglDepthRangeOffset(2);
    glBegin(GL_TRIANGLES);
        ssglVertex3v(tri.a);
        ssglVertex3v(tri.b);
        ssglVertex3v(tri.c);
    glEnd();

    ssglDepthRangeOffset(0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    if(more) more->DebugDraw();
    if(neg) neg->DebugDraw();

    if(edges) edges->DebugDraw(n, d);
}

/////////////////////////////////

Vector SBsp2::IntersectionWith(Vector a, Vector b) {
    double da = a.Dot(no) - d;
    double db = b.Dot(no) - d;
    if(da*db > 0) oops();

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
    oops();
}

void SBsp2::InsertTriangleHow(int how, STriangle *tr, SMesh *m, SBsp3 *bsp3) {
    switch(how) {
        case POS:
            if(pos) {
                pos->InsertTriangle(tr, m, bsp3);
            } else {
                bsp3->InsertInPlane(true, tr, m);
            }
            break;

        case NEG:
            if(neg) {
                neg->InsertTriangle(tr, m, bsp3);
            } else {
                bsp3->InsertInPlane(false, tr, m);
            }
            break;

        default: oops();
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
            InsertTriangleHow(POS, tr, m, bsp3);
        } else {
            InsertTriangleHow(NEG, tr, m, bsp3);
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
        } else oops();

        Vector bPc = IntersectionWith(b, c);
        STriangle btri = STriangle::From(tr->meta, a, b, bPc);
        STriangle ctri = STriangle::From(tr->meta, c, a, bPc);

        if(bpos) {
            InsertTriangleHow(POS, &btri, m, bsp3);
            InsertTriangleHow(NEG, &ctri, m, bsp3);
        } else {
            InsertTriangleHow(POS, &ctri, m, bsp3);
            InsertTriangleHow(NEG, &btri, m, bsp3);
        }

        return;
    }

    if(posc == 2 && negc == 1) {
        // Standardize so that a is on one side, and b and c are on the other.
        if       (isNeg[0]) {   a = tr->a; b = tr->b; c = tr->c;
        } else if(isNeg[1]) {   a = tr->b; b = tr->c; c = tr->a;
        } else if(isNeg[2]) {   a = tr->c; b = tr->a; c = tr->b;
        } else oops();

    } else if(posc == 1 && negc == 2) {
        if       (isPos[0]) {   a = tr->a; b = tr->b; c = tr->c;
        } else if(isPos[1]) {   a = tr->b; b = tr->c; c = tr->a;
        } else if(isPos[2]) {   a = tr->c; b = tr->a; c = tr->b;
        } else oops();
    } else oops();

    Vector aPb = IntersectionWith(a, b);
    Vector cPa = IntersectionWith(c, a);

    STriangle alone = STriangle::From(tr->meta, a,   aPb, cPa);
    STriangle quad1 = STriangle::From(tr->meta, aPb, b,   c  );
    STriangle quad2 = STriangle::From(tr->meta, aPb, c,   cPa);

    if(posc == 2 && negc == 1) {
        InsertTriangleHow(POS, &quad1, m, bsp3);
        InsertTriangleHow(POS, &quad2, m, bsp3);
        InsertTriangleHow(NEG, &alone, m, bsp3);
    } else {
        InsertTriangleHow(NEG, &quad1, m, bsp3);
        InsertTriangleHow(NEG, &quad2, m, bsp3);
        InsertTriangleHow(POS, &alone, m, bsp3);
    }

    return;
}

void SBsp2::DebugDraw(Vector n, double d) {
    if(fabs((edge.a).Dot(n) - d) > LENGTH_EPS) oops();
    if(fabs((edge.b).Dot(n) - d) > LENGTH_EPS) oops();

    ssglLineWidth(10);
    glBegin(GL_LINES);
        ssglVertex3v(edge.a);
        ssglVertex3v(edge.b);
    glEnd();
    if(pos) pos->DebugDraw(n, d);
    if(neg) neg->DebugDraw(n, d);
    if(more) more->DebugDraw(n, d);
    ssglLineWidth(1);
}

