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
    Vector r = (a.ScaledBy(db/dab)).Plus(b.ScaledBy(-da/dab));
    return r;
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
        // Arbitrarily pick a side. This fails if two faces are coplanar.
        InsertHow(POS, tr, instead, flip, cpl);
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
    bool ispos[3], isneg[3], ison[3];
    ZERO(&ispos); ZERO(&isneg); ZERO(&ison);
    // Count vertices in the plane
    for(int i = 0; i < 3; i++) {
        if(fabs(dt[i] - d) < LENGTH_EPS) {
            inc++;
            ison[i] = true;
        } else if(dt[i] > d) {
            posc++;
            ispos[i] = true;
        } else {
            negc++;
            isneg[i] = true;
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
            // Two vertices in-plane, other above or below
            // XXX do edge bsp
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

    // Standardize so that a is on the plane
    if(posc == 1 && negc == 1 && inc == 1) {
        bool bpos;
        if       (ison[0]) { a = tr->a; b = tr->b; c = tr->c; bpos = ispos[1];
        } else if(ison[1]) { a = tr->b; b = tr->c; c = tr->a; bpos = ispos[2];
        } else if(ison[2]) { a = tr->c; b = tr->a; c = tr->b; bpos = ispos[0];
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

        return this;
    }

    // Standardize so that a is on one side, and b and c are on the other.
    if(posc == 2 && negc == 1) {
        if       (isneg[0]) {   a = tr->a; b = tr->b; c = tr->c;
        } else if(isneg[1]) {   a = tr->b; b = tr->c; c = tr->a;
        } else if(isneg[2]) {   a = tr->c; b = tr->a; c = tr->b;
        } else oops();

    } else if(posc == 1 && negc == 2) {
        if       (ispos[0]) {   a = tr->a; b = tr->b; c = tr->c;
        } else if(ispos[1]) {   a = tr->b; b = tr->c; c = tr->a;
        } else if(ispos[2]) {   a = tr->c; b = tr->a; c = tr->b;
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
    return this;
}

void SBsp3::DebugDraw(void) {
    if(!this) return;

    pos->DebugDraw();
    Vector norm = tri.Normal();
    glNormal3d(norm.x, norm.y, norm.z);

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
}

