#include "solvespace.h"

void SMesh::Clear(void) {
    l.Clear();
}

void SMesh::AddTriangle(STriMeta meta, Vector n, Vector a, Vector b, Vector c) {
    Vector ab = b.Minus(a), bc = c.Minus(b);
    Vector np = ab.Cross(bc);
    if(np.Magnitude() < 1e-10) {
        // ugh; gl sometimes tesselates to collinear triangles
        return;
    }
    if(np.Dot(n) > 0) {
        AddTriangle(meta, a, b, c);
    } else {
        AddTriangle(meta, c, b, a);
    }
}
void SMesh::AddTriangle(STriMeta meta, Vector a, Vector b, Vector c) {
    STriangle t; ZERO(&t);
    t.meta = meta;
    t.a = a;
    t.b = b;
    t.c = c;
    AddTriangle(&t);
}
void SMesh::AddTriangle(STriangle *st) {
    l.Add(st);
}

void SMesh::DoBounding(Vector v, Vector *vmax, Vector *vmin) {
    vmax->x = max(vmax->x, v.x);
    vmax->y = max(vmax->y, v.y);
    vmax->z = max(vmax->z, v.z);

    vmin->x = min(vmin->x, v.x);
    vmin->y = min(vmin->y, v.y);
    vmin->z = min(vmin->z, v.z);
}
void SMesh::GetBounding(Vector *vmax, Vector *vmin) {
    int i;
    *vmin = Vector::From( 1e12,  1e12,  1e12);
    *vmax = Vector::From(-1e12, -1e12, -1e12);
    for(i = 0; i < l.n; i++) {
        STriangle *st = &(l.elem[i]);
        DoBounding(st->a, vmax, vmin);
        DoBounding(st->b, vmax, vmin);
        DoBounding(st->c, vmax, vmin);
    }
}

void SMesh::Simplify(int start) {
#define MAX_TRIANGLES 2000
    if(l.n - start > MAX_TRIANGLES) oops();

    STriMeta meta = l.elem[start].meta;

    STriangle tout[MAX_TRIANGLES];
    int toutc = 0;

    Vector n, conv[MAX_TRIANGLES*3];
    int convc = 0;

    int start0 = start;

    int i, j;
    for(i = start; i < l.n; i++) {
        STriangle *tr = &(l.elem[i]);
        if((tr->Normal()).Magnitude() < LENGTH_EPS*LENGTH_EPS) {
            tr->tag = 0;
        } else {
            tr->tag = 0;
        }
    }

    for(;;) {
        bool didAdd;
        convc = 0;
        for(i = start; i < l.n; i++) {
            STriangle *tr = &(l.elem[i]);
            if(tr->tag) continue;

            tr->tag = 1;
            n = (tr->Normal()).WithMagnitude(1);
            conv[convc++] = tr->a;
            conv[convc++] = tr->b;
            conv[convc++] = tr->c;

            start = i+1;
            break;
        }
        if(i >= l.n) break;

        do {
            didAdd = false;

            for(j = 0; j < convc; j++) {
                Vector a = conv[WRAP((j-1), convc)],
                       b = conv[j], 
                       d = conv[WRAP((j+1), convc)],
                       e = conv[WRAP((j+2), convc)];

                Vector c;
                for(i = start; i < l.n; i++) {
                    STriangle *tr = &(l.elem[i]);
                    if(tr->tag) continue;

                    if((tr->a).Equals(d) && (tr->b).Equals(b)) {
                        c = tr->c;
                    } else if((tr->b).Equals(d) && (tr->c).Equals(b)) {
                        c = tr->a;
                    } else if((tr->c).Equals(d) && (tr->a).Equals(b)) {
                        c = tr->b;
                    } else {
                        continue;
                    }
                    // The vertex at C must be convex; but the others must
                    // be tested
                    Vector ab = b.Minus(a);
                    Vector bc = c.Minus(b);
                    Vector cd = d.Minus(c);
                    Vector de = e.Minus(d);

                    double bDot = (ab.Cross(bc)).Dot(n);
                    double dDot = (cd.Cross(de)).Dot(n);

                    bDot /= min(ab.Magnitude(), bc.Magnitude());
                    dDot /= min(cd.Magnitude(), de.Magnitude());
                   
                    if(fabs(bDot) < LENGTH_EPS && fabs(dDot) < LENGTH_EPS) {
                        conv[WRAP((j+1), convc)] = c;
                        // and remove the vertex at j, which is a dup
                        memmove(conv+j, conv+j+1, 
                                          (convc - j - 1)*sizeof(conv[0]));
                        convc--;
                    } else if(fabs(bDot) < LENGTH_EPS && dDot > 0) {
                        conv[j] = c;
                    } else if(fabs(dDot) < LENGTH_EPS && bDot > 0) {
                        conv[WRAP((j+1), convc)] = c;
                    } else if(bDot > 0 && dDot > 0) {
                        // conv[j] is unchanged, conv[j+1] goes to [j+2]
                        memmove(conv+j+2, conv+j+1,
                                            (convc - j - 1)*sizeof(conv[0]));
                        conv[j+1] = c;
                        convc++;
                    } else {
                        continue;
                    }

                    didAdd = true;
                    tr->tag = 1;
                    break;
                }
            }
        } while(didAdd);

        // I need to debug why this is required; sometimes the above code
        // still generates a convex polygon
        for(i = 0; i < convc; i++) {
            Vector a = conv[WRAP((i-1), convc)],
                   b = conv[i], 
                   c = conv[WRAP((i+1), convc)];
            Vector ab = b.Minus(a);
            Vector bc = c.Minus(b);
            double bDot = (ab.Cross(bc)).Dot(n);
            bDot /= min(ab.Magnitude(), bc.Magnitude());

            if(bDot < 0) oops();
        }

        for(i = 0; i < convc - 2; i++) {
            STriangle tr = STriangle::From(meta, conv[0], conv[i+1], conv[i+2]);
            if((tr.Normal()).Magnitude() > LENGTH_EPS*LENGTH_EPS) {
                tout[toutc++] = tr;
            }
        }
    }

    l.n = start0;
    for(i = 0; i < toutc; i++) {
        AddTriangle(&(tout[i]));
    }
}

void SMesh::AddAgainstBsp(SMesh *srcm, SBsp3 *bsp3) {
    int i;

    for(i = 0; i < srcm->l.n; i++) {
        STriangle *st = &(srcm->l.elem[i]);
        int pn = l.n;
        atLeastOneDiscarded = false;
        bsp3->Insert(st, this);
        if(!atLeastOneDiscarded && (l.n != (pn+1))) {
            l.n = pn;
            if(flipNormal) {
                AddTriangle(st->meta, st->c, st->b, st->a);
            } else {
                AddTriangle(st->meta, st->a, st->b, st->c);
            }
        }
        if(l.n - pn > 1) {
            Simplify(pn);
        }
    }
}

void SMesh::MakeFromUnion(SMesh *a, SMesh *b) {
    SBsp3 *bspa = SBsp3::FromMesh(a);
    SBsp3 *bspb = SBsp3::FromMesh(b);

    flipNormal = false;
    keepCoplanar = false;
    AddAgainstBsp(b, bspa);

    flipNormal = false;
    keepCoplanar = true;
    AddAgainstBsp(a, bspb);
}

void SMesh::MakeFromDifference(SMesh *a, SMesh *b) {
    SBsp3 *bspa = SBsp3::FromMesh(a);
    SBsp3 *bspb = SBsp3::FromMesh(b);

    flipNormal = true;
    keepCoplanar = true;
    AddAgainstBsp(b, bspa);

    flipNormal = false;
    keepCoplanar = false;
    AddAgainstBsp(a, bspb);
}

bool SMesh::MakeFromInterferenceCheck(SMesh *srca, SMesh *srcb, SMesh *error) {
    SBsp3 *bspa = SBsp3::FromMesh(srca);
    SBsp3 *bspb = SBsp3::FromMesh(srcb);

    error->Clear();
    error->flipNormal = true;
    error->keepCoplanar = false;

    error->AddAgainstBsp(srcb, bspa);
    error->AddAgainstBsp(srca, bspb);
    // Now we have a list of all the triangles (or fragments thereof) from
    // A that lie inside B, or vice versa. That's the interference, and
    // we report it so that it can be flagged.
    
    // But as far as the actual model, we just copy everything over.
    int i;
    for(i = 0; i < srca->l.n; i++) {
        AddTriangle(&(srca->l.elem[i]));
    }
    for(i = 0; i < srcb->l.n; i++) {
        AddTriangle(&(srcb->l.elem[i]));
    }
    return (error->l.n == 0);
}

DWORD SMesh::FirstIntersectionWith(Point2d mp) {
    Vector p0 = Vector::From(mp.x, mp.y, 0);
    Vector gn = Vector::From(0, 0, 1);

    double maxT = -1e12;
    DWORD face = 0;

    int i;
    for(i = 0; i < l.n; i++) {
        STriangle tr = l.elem[i];
        tr.a = SS.GW.ProjectPoint3(tr.a);
        tr.b = SS.GW.ProjectPoint3(tr.b);
        tr.c = SS.GW.ProjectPoint3(tr.c);

        Vector n = tr.Normal();

        if(n.Dot(gn) < LENGTH_EPS) continue; // back-facing or on edge

        if(tr.ContainsPointProjd(gn, p0)) {
            // Let our line have the form r(t) = p0 + gn*t
            double t = -(n.Dot((tr.a).Minus(p0)))/(n.Dot(gn));
            if(t > maxT) {
                maxT = t;
                face = tr.meta.face;
            }
        }
    }
    return face;
}

SBsp2 *SBsp2::Alloc(void) { return (SBsp2 *)AllocTemporary(sizeof(SBsp2)); }
SBsp3 *SBsp3::Alloc(void) { return (SBsp3 *)AllocTemporary(sizeof(SBsp3)); }

SBsp3 *SBsp3::FromMesh(SMesh *m) {
    SBsp3 *bsp3 = NULL;
    int i;

    SMesh mc; ZERO(&mc);
    for(i = 0; i < m->l.n; i++) {
        mc.AddTriangle(&(m->l.elem[i]));
    }

    srand(0); // Let's be deterministic, at least!
    int n = mc.l.n;
    while(n > 1) {
        int k = rand() % n;
        n--;
        SWAP(STriangle, mc.l.elem[k], mc.l.elem[n]);
    }

    for(i = 0; i < mc.l.n; i++) {
        bsp3 = bsp3->Insert(&(mc.l.elem[i]), NULL);
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
    bool sameNormal;
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
            pos = pos->Insert(tr, instead);
            break;

        case NEG:
            if(instead && !neg) goto alt;
            neg = neg->Insert(tr, instead);
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
    int posc = 0, negc = 0, onc = 0;
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
            edges = edges->InsertEdge(&se, n, out);
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
    int npos = 0, nneg = 0;

    Vector inter[2];
    int inters = 0;

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

            if(inters >= 2) oops();
            inter[inters++] = vi;
        }
    }
    if(npos > cnt + 1 || nneg > cnt + 1) oops();

    if(!instead) {
        if(inters == 2) {
            SEdge se = SEdge::From(inter[0], inter[1]);
            edges = edges->InsertEdge(&se, n, out);
        } else if(inters == 1 && onc == 1) {
            SEdge se = SEdge::From(inter[0], on[0]);
            edges = edges->InsertEdge(&se, n, out);
        } else if(inters == 0 && onc == 2) {
            // We already handled this on-plane existing edge
        } else oops();
    }
    if(nneg < 3 || npos < 3) oops();

    InsertConvexHow(NEG, meta, vneg, nneg, instead);
    InsertConvexHow(POS, meta, vpos, npos, instead);
    return this;

triangulate:
    // We don't handle the special case for this; do it as triangles
    SBsp3 *r = this;
    for(i = 0; i < cnt - 2; i++) {
        STriangle tr = STriangle::From(meta,
                                       vertex[0], vertex[i+1], vertex[i+2]);
        r = r->Insert(&tr, instead);
    }
    return r;
}

SBsp3 *SBsp3::Insert(STriangle *tr, SMesh *instead) {
    if(!this) {
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
        InsertHow(COPLANAR, tr, instead);
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
                SEdge se = SEdge::From(a, b);
                edges = edges->InsertEdge(&se, n, tr->Normal());
            }
        }

        if(posc > 0) {
            InsertHow(POS, tr, instead);
        } else {
            InsertHow(NEG, tr, instead);
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
    glxDepthRangeOffset(2);
    glBegin(GL_TRIANGLES);
        glxVertex3v(tri.a);
        glxVertex3v(tri.b);
        glxVertex3v(tri.c);
    glEnd();

    glDisable(GL_LIGHTING);
    glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    glPointSize(10);
    glxDepthRangeOffset(2);
    glBegin(GL_TRIANGLES);
        glxVertex3v(tri.a);
        glxVertex3v(tri.b);
        glxVertex3v(tri.c);
    glEnd(); 

    glxDepthRangeOffset(0);
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
        r->no = ((r->np).Cross((nedge->b).Minus(nedge->a))).WithMagnitude(1);
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
        m->no = ((m->np).Cross((nedge->b).Minus(nedge->a))).WithMagnitude(1);
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

        SEdge ea = SEdge::From(nedge->a, aPb);
        SEdge eb = SEdge::From(aPb, nedge->b);

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

