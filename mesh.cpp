#include "solvespace.h"

void SMesh::Clear(void) {
    l.Clear();
}

void SMesh::AddTriangle(Vector n, Vector a, Vector b, Vector c) {
    Vector ab = b.Minus(a), bc = c.Minus(b);
    Vector np = ab.Cross(bc);
    if(np.Dot(n) > 0) {
        AddTriangle(a, b, c);
    } else {
        AddTriangle(c, b, a);
    }
}

void SMesh::AddTriangle(Vector a, Vector b, Vector c) {
    STriangle t; ZERO(&t);
    t.a = a;
    t.b = b;
    t.c = c;
    l.Add(&t);
}

SBsp2 *SBsp2::Alloc(void) { return (SBsp2 *)AllocTemporary(sizeof(SBsp2)); }
SBsp3 *SBsp3::Alloc(void) { return (SBsp3 *)AllocTemporary(sizeof(SBsp3)); }

SBsp3 *SBsp3::FromMesh(SMesh *m) {
    int i;
    SBsp3 *ret = NULL;
    for(i = 0; i < m->l.n; i++) {
        ret = ret->Insert(&(m->l.elem[i]), NULL, false, false);
    }
    return ret;
}

Vector SBsp3::IntersectionWith(Vector a, Vector b) {
    double da = a.Dot(n) - d;
    double db = b.Dot(n) - d;
    if(da*db > 0) oops();

    double dab = (db - da);
    return (a.ScaledBy(db/dab)).Plus(b.ScaledBy(-da/dab));
}

void SBsp3::InsertInPlane(bool pos2, STriangle *tr,
                          SMesh *m, bool flip, bool cpl)
{
    Vector tc = ((tr->a).Plus(tr->b).Plus(tr->c)).ScaledBy(1.0/3);

    bool onFace = false;
    bool sameNormal;

    SBsp3 *ll = this;
    while(ll) {
        if((ll->tri).ContainsPoint(tc)) {
            onFace = true;
            sameNormal = (tr->Normal()).Dot((ll->tri).Normal()) > 0;
            break;
        }
        ll = ll->more;
    }
    
    if(flip) {
        if(cpl) oops();
        if(!pos2 && (!onFace || !sameNormal)) {
            m->AddTriangle(tr->c, tr->b, tr->a);
        }
    } else {
        if(pos2 || (onFace && sameNormal && cpl)) {
            m->AddTriangle(tr->a, tr->b, tr->c);
        }
    }
}

void SBsp3::InsertHow(int how, STriangle *tr,
                      SMesh *instead, bool flip, bool cpl)
{
    switch(how) {
        case POS:
            if(instead && !pos) goto alt;
            pos = pos->Insert(tr, instead, flip, cpl);
            break;

        case NEG:
            if(instead && !neg) goto alt;
            neg = neg->Insert(tr, instead, flip, cpl);
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
    if(how == POS && !flip) {
        instead->AddTriangle(tr->a, tr->b, tr->c);
    }
    if(how == NEG && flip) {
        instead->AddTriangle(tr->c, tr->b, tr->a);
    }
    if(how == COPLANAR) {
        if(edges) {
            edges->InsertTriangle(tr, instead, this, flip, cpl);
        } else {
            // I suppose this actually is allowed to happen, if the coplanar
            // face is the leaf, and all of its neighbors are earlier in tree?
            InsertInPlane(true, tr, instead, flip, cpl);
        }
    }
}

SBsp3 *SBsp3::Insert(STriangle *tr, SMesh *instead, bool flip, bool cpl) {
    if(!this) {
        // Brand new node; so allocate for it, and fill us in.
        SBsp3 *r = Alloc();
        r->n = tr->Normal();
        r->d = (tr->a).Dot(r->n);
        r->tri = *tr;
        return r;
    }

    double dt[3] = { (tr->a).Dot(n), (tr->b).Dot(n), (tr->c).Dot(n) };

    int inc = 0, posc = 0, negc = 0;
    bool isPos[3], isNeg[3], isOn[3];
    ZERO(&isPos); ZERO(&isNeg); ZERO(&isOn);
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
        InsertHow(COPLANAR, tr, instead, flip, cpl);
        return this;
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
                SEdge se = { 0, a, b };
                edges = edges->InsertEdge(&se, n, tr->Normal());
            }
        }

        if(posc > 0) {
            InsertHow(POS, tr, instead, flip, cpl);
        } else {
            InsertHow(NEG, tr, instead, flip, cpl);
        }
        return this;
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
        STriangle btri = { 0, a, b, bPc };
        STriangle ctri = { 0, c, a, bPc };

        if(bpos) {
            InsertHow(POS, &btri, instead, flip, cpl);
            InsertHow(NEG, &ctri, instead, flip, cpl);
        } else {
            InsertHow(POS, &ctri, instead, flip, cpl);
            InsertHow(NEG, &btri, instead, flip, cpl);
        }

        if(!instead) {
            SEdge se = { 0, a, bPc };
            edges = edges->InsertEdge(&se, n, tr->Normal());
        }

        return this;
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

    STriangle alone = { 0, a,   aPb, cPa };
    STriangle quad1 = { 0, aPb, b,   c   };
    STriangle quad2 = { 0, aPb, c,   cPa };

    if(posc == 2 && negc == 1) {
        InsertHow(POS, &quad1, instead, flip, cpl);
        InsertHow(POS, &quad2, instead, flip, cpl);
        InsertHow(NEG, &alone, instead, flip, cpl);
    } else {
        InsertHow(NEG, &quad1, instead, flip, cpl);
        InsertHow(NEG, &quad2, instead, flip, cpl);
        InsertHow(POS, &alone, instead, flip, cpl);
    }
    if(!instead) {
        SEdge se = { 0, aPb, cPa };
        edges = edges->InsertEdge(&se, n, alone.Normal());
    }

    return this;
}

void SBsp3::DebugDraw(void) {
    if(!this) return;

    pos->DebugDraw();
    Vector norm = tri.Normal();
    glNormal3d(norm.x, norm.y, norm.z);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glBegin(GL_TRIANGLES);
        glxVertex3v(tri.a);
        glxVertex3v(tri.b);
        glxVertex3v(tri.c);
    glEnd();

    glDisable(GL_LIGHTING);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glPolygonOffset(-1, 0);
    glBegin(GL_TRIANGLES);
        glxVertex3v(tri.a);
        glxVertex3v(tri.b);
        glxVertex3v(tri.c);
    glEnd();

    glDisable(GL_LIGHTING);
    glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    glPointSize(10);
    glPolygonOffset(-1, 0);
    glBegin(GL_TRIANGLES);
        glxVertex3v(tri.a);
        glxVertex3v(tri.b);
        glxVertex3v(tri.c);
    glEnd(); 

    glPolygonOffset(0, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    more->DebugDraw();
    neg->DebugDraw();

    edges->DebugDraw(n, d);
}

/////////////////////////////////

Vector SBsp2::IntersectionWith(Vector a, Vector b) {
    double da = a.Dot(no) - d;
    double db = b.Dot(no) - d;
    if(da*db > 0) oops();

    double dab = (db - da);
    return (a.ScaledBy(db/dab)).Plus(b.ScaledBy(-da/dab));
}

SBsp2 *SBsp2::InsertEdge(SEdge *nedge, Vector nnp, Vector out) {
    if(!this) {
        // Brand new node; so allocate for it, and fill us in.
        SBsp2 *r = Alloc();
        r->np = nnp;
        r->no = (r->np).Cross((nedge->b).Minus(nedge->a));
        if(out.Dot(r->no) < 0) {
            r->no = (r->no).ScaledBy(-1);
        }
        r->d = (nedge->a).Dot(r->no);
        r->edge = *nedge;
        return r;
    }

    double dt[2] = { (nedge->a).Dot(no), (nedge->b).Dot(no) };

    bool isPos[2], isNeg[2], isOn[2];
    ZERO(&isPos); ZERO(&isNeg); ZERO(&isOn);
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
        pos = pos->InsertEdge(nedge, nnp, out);
        return this;
    }
    if((isNeg[0] && isNeg[1])||(isNeg[0] && isOn[1])||(isOn[0] && isNeg[1])) {
        neg = neg->InsertEdge(nedge, nnp, out);
        return this;
    }
    if(isOn[0] && isOn[1]) {
        SBsp2 *m = Alloc();

        m->np = nnp;
        m->no = (m->np).Cross((nedge->b).Minus(nedge->a));
        if(out.Dot(m->no) < 0) {
            m->no = (m->no).ScaledBy(-1);
        }
        m->d = (nedge->a).Dot(m->no);
        m->edge = *nedge;

        m->more = more;
        more = m;
        return this;
    }
    if((isPos[0] && isNeg[1]) || (isNeg[0] && isPos[1])) {
        Vector aPb = IntersectionWith(nedge->a, nedge->b);

        SEdge ea = { 0, nedge->a, aPb };
        SEdge eb = { 0, aPb, nedge->b };

        if(isPos[0]) {
            pos = pos->InsertEdge(&ea, nnp, out);
            neg = neg->InsertEdge(&eb, nnp, out);
        } else {
            neg = neg->InsertEdge(&ea, nnp, out);
            pos = pos->InsertEdge(&eb, nnp, out);
        }
        return this;
    }
    oops();
}

void SBsp2::InsertTriangleHow(int how, STriangle *tr,
                              SMesh *m, SBsp3 *bsp3, bool flip, bool cpl)
{
    switch(how) {
        case POS:
            if(pos) {
                pos->InsertTriangle(tr, m, bsp3, flip, cpl);
            } else {
                bsp3->InsertInPlane(true, tr, m, flip, cpl);
            }
            break;

        case NEG:
            if(neg) {
                neg->InsertTriangle(tr, m, bsp3, flip, cpl);
            } else {
                bsp3->InsertInPlane(false, tr, m, flip, cpl);
            }
            break;

        default: oops();
    }
}

void SBsp2::InsertTriangle(STriangle *tr,
                           SMesh *m, SBsp3 *bsp3, bool flip, bool cpl)
{
    double dt[3] = { (tr->a).Dot(no), (tr->b).Dot(no), (tr->c).Dot(no) };

    bool isPos[3], isNeg[3], isOn[3];
    int inc = 0, posc = 0, negc = 0;
    ZERO(&isPos); ZERO(&isNeg); ZERO(&isOn);
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
            InsertTriangleHow(POS, tr, m, bsp3, flip, cpl);
        } else {
            InsertTriangleHow(NEG, tr, m, bsp3, flip, cpl);
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
        STriangle btri = { 0, a, b, bPc };
        STriangle ctri = { 0, c, a, bPc };

        if(bpos) {
            InsertTriangleHow(POS, &btri, m, bsp3, flip, cpl);
            InsertTriangleHow(NEG, &ctri, m, bsp3, flip, cpl);
        } else {
            InsertTriangleHow(POS, &ctri, m, bsp3, flip, cpl);
            InsertTriangleHow(NEG, &btri, m, bsp3, flip, cpl);
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

    STriangle alone = { 0, a,   aPb, cPa };
    STriangle quad1 = { 0, aPb, b,   c   };
    STriangle quad2 = { 0, aPb, c,   cPa };

    if(posc == 2 && negc == 1) {
        InsertTriangleHow(POS, &quad1, m, bsp3, flip, cpl);
        InsertTriangleHow(POS, &quad2, m, bsp3, flip, cpl);
        InsertTriangleHow(NEG, &alone, m, bsp3, flip, cpl);
    } else {
        InsertTriangleHow(NEG, &quad1, m, bsp3, flip, cpl);
        InsertTriangleHow(NEG, &quad2, m, bsp3, flip, cpl);
        InsertTriangleHow(POS, &alone, m, bsp3, flip, cpl);
    }

    return;
}

void SBsp2::DebugDraw(Vector n, double d) {
    if(!this) return;

    if(fabs((edge.a).Dot(n) - d) > LENGTH_EPS) oops();
    if(fabs((edge.b).Dot(n) - d) > LENGTH_EPS) oops();

    glLineWidth(10);
    glBegin(GL_LINES);
        glxVertex3v(edge.a);
        glxVertex3v(edge.b);
    glEnd();
    pos->DebugDraw(n, d);
    neg->DebugDraw(n, d);
    more->DebugDraw(n, d);
    glLineWidth(1);
}

