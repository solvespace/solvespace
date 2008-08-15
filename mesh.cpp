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
    int maxTriangles = (l.n - start) + 10;

    STriMeta meta = l.elem[start].meta;

    STriangle *tout = (STriangle *)AllocTemporary(maxTriangles*sizeof(*tout));
    int toutc = 0;

    Vector n, *conv = (Vector *)AllocTemporary(maxTriangles*3*sizeof(*conv));
    int convc = 0;

    int start0 = start;

    int i, j;
    for(i = start; i < l.n; i++) {
        STriangle *tr = &(l.elem[i]);
        if(tr->MinAltitude() < LENGTH_EPS) {
            tr->tag = 1;
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

            if(bDot < 0) return; // XXX, shouldn't happen
        }

        for(i = 0; i < convc - 2; i++) {
            STriangle tr = STriangle::From(meta, conv[0], conv[i+1], conv[i+2]);
            if(tr.MinAltitude() > LENGTH_EPS) {
                tout[toutc++] = tr;
            }
        }
    }

    l.n = start0;
    for(i = 0; i < toutc; i++) {
        AddTriangle(&(tout[i]));
    }
    FreeTemporary(tout);
    FreeTemporary(conv);
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

    // For the actual output, take the union.
    flipNormal = false;
    keepCoplanar = false;
    AddAgainstBsp(srcb, bspa);

    flipNormal = false;
    keepCoplanar = true;
    AddAgainstBsp(srca, bspb);
    
    // And we're successful if the intersection was empty.
    return (error->l.n == 0);
}

void SMesh::MakeFromCopy(SMesh *a) {
    int i;
    for(i = 0; i < a->l.n; i++) {
        AddTriangle(&(a->l.elem[i]));
    }
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

#define KDTREE_EPS (20*LENGTH_EPS) // nice and sloppy

STriangleLl *STriangleLl::Alloc(void) 
    { return (STriangleLl *)AllocTemporary(sizeof(STriangleLl)); }
SKdNode *SKdNode::Alloc(void) 
    { return (SKdNode *)AllocTemporary(sizeof(SKdNode)); }

SKdNode *SKdNode::From(SMesh *m) {
    int i;
    STriangle *tra = (STriangle *)AllocTemporary((m->l.n) * sizeof(*tra));

    for(i = 0; i < m->l.n; i++) {
        tra[i] = m->l.elem[i];
    }

    srand(0);
    int n = m->l.n;
    while(n > 1) {
        int k = rand() % n;
        n--;
        SWAP(STriangle, tra[k], tra[n]);
    }

    STriangleLl *tll = NULL;
    for(i = 0; i < m->l.n; i++) {
        STriangleLl *tn = STriangleLl::Alloc();
        tn->tri = &(tra[i]);
        tn->next = tll;
        tll = tn;
    }

    return SKdNode::From(tll, 0);
}

SKdNode *SKdNode::From(STriangleLl *tll, int which) {
    SKdNode *ret = Alloc();

    if(!tll) goto leaf;

    int i;
    int gtc[3] = { 0, 0, 0 }, ltc[3] = { 0, 0, 0 }, allc = 0;
    double badness[3];
    double split[3];
    for(i = 0; i < 3; i++) {
        int tcnt = 0;
        STriangleLl *ll;
        for(ll = tll; ll; ll = ll->next) {
            STriangle *tr = ll->tri;
            split[i] += (ll->tri->a).Element(i);
            split[i] += (ll->tri->b).Element(i);
            split[i] += (ll->tri->c).Element(i);
            tcnt++;
        }
        split[i] /= (tcnt*3);

        for(ll = tll; ll; ll = ll->next) {
            STriangle *tr = ll->tri;
            
            double a = (tr->a).Element(i),
                   b = (tr->b).Element(i),
                   c = (tr->c).Element(i);
            
            if(a < split[i] + KDTREE_EPS ||
               b < split[i] + KDTREE_EPS ||
               c < split[i] + KDTREE_EPS)
            {
                ltc[i]++;
            }
            if(a > split[i] - KDTREE_EPS ||
               b > split[i] - KDTREE_EPS ||
               c > split[i] - KDTREE_EPS)
            {
                gtc[i]++;
            }
            if(i == 0) allc++;
        }
        badness[i] = pow((double)ltc[i], 4) + pow((double)gtc[i], 4);
    }
    if(badness[0] < badness[1] && badness[0] < badness[2]) {
        which = 0;
    } else if(badness[1] < badness[2]) {
        which = 1;
    } else {
        which = 2;
    }

    if(allc < 10) goto leaf;
    if(allc == gtc[which] || allc == ltc[which]) goto leaf;

    STriangleLl *ll;
    STriangleLl *lgt = NULL, *llt = NULL;
    for(ll = tll; ll; ll = ll->next) {
        STriangle *tr = ll->tri;
        
        double a = (tr->a).Element(which),
               b = (tr->b).Element(which),
               c = (tr->c).Element(which);
        
        if(a < split[which] + KDTREE_EPS ||
           b < split[which] + KDTREE_EPS ||
           c < split[which] + KDTREE_EPS)
        {
            STriangleLl *n = STriangleLl::Alloc();
            *n = *ll;
            n->next = llt;
            llt = n;
        }
        if(a > split[which] - KDTREE_EPS ||
           b > split[which] - KDTREE_EPS ||
           c > split[which] - KDTREE_EPS)
        {
            STriangleLl *n = STriangleLl::Alloc();
            *n = *ll;
            n->next = lgt;
            lgt = n;
        }
    }

    ret->which = which;
    ret->c = split[which];
    ret->gt = SKdNode::From(lgt, (which + 1) % 3);
    ret->lt = SKdNode::From(llt, (which + 1) % 3);
    return ret;

leaf:
//    dbp("leaf: allc=%d gtc=%d ltc=%d which=%d", allc, gtc[which], ltc[which], which);
    ret->tris = tll;
    return ret;
}

void SKdNode::ClearTags(void) {
    if(gt && lt) {
        gt->ClearTags();
        lt->ClearTags();
    } else {
        STriangleLl *ll;
        for(ll = tris; ll; ll = ll->next) {
            ll->tri->tag = 0;
        }
    }
}

void SKdNode::AddTriangle(STriangle *tr) {
    if(gt && lt) {
        double ta = (tr->a).Element(which),
               tb = (tr->b).Element(which),
               tc = (tr->c).Element(which);
        if(ta < c + KDTREE_EPS ||
           tb < c + KDTREE_EPS ||
           tc < c + KDTREE_EPS)
        {
            lt->AddTriangle(tr);
        }
        if(ta > c - KDTREE_EPS ||
           tb > c - KDTREE_EPS ||
           tc > c - KDTREE_EPS)
        {
            gt->AddTriangle(tr);
        }
    } else {
        STriangleLl *tn = STriangleLl::Alloc();
        tn->tri = tr;
        tn->next = tris;
        tris = tn;
    }
}

void SKdNode::MakeMeshInto(SMesh *m) {
    if(gt) gt->MakeMeshInto(m);
    if(lt) lt->MakeMeshInto(m);

    STriangleLl *ll;
    for(ll = tris; ll; ll = ll->next) {
        if(ll->tri->tag) continue;

        m->AddTriangle(ll->tri);
        ll->tri->tag = 1;
    }
}

void SKdNode::SnapToVertex(Vector v, SMesh *extras) {
    if(gt && lt) {
        double vc = v.Element(which);
        if(vc < c + KDTREE_EPS) {
            lt->SnapToVertex(v, extras);
        }
        if(vc > c - KDTREE_EPS) {
            gt->SnapToVertex(v, extras);
        }
        // Nothing bad happens if the triangle to be split appears in both
        // branches; the first call will split the triangle, so that the
        // second call will do nothing, because the modified triangle will
        // already contain v
    } else {
        STriangleLl *ll;
        for(ll = tris; ll; ll = ll->next) {
            STriangle *tr = ll->tri;

            // Do a cheap bbox test first
            int k;
            bool mightHit = true;

            for(k = 0; k < 3; k++) {
                if((tr->a).Element(k) < v.Element(k) - KDTREE_EPS &&
                   (tr->b).Element(k) < v.Element(k) - KDTREE_EPS &&
                   (tr->c).Element(k) < v.Element(k) - KDTREE_EPS)
                {
                    mightHit = false;
                    break;
                }
                if((tr->a).Element(k) > v.Element(k) + KDTREE_EPS &&
                   (tr->b).Element(k) > v.Element(k) + KDTREE_EPS &&
                   (tr->c).Element(k) > v.Element(k) + KDTREE_EPS)
                {
                    mightHit = false;
                    break;
                }
            }
            if(!mightHit) continue;

            if(tr->a.Equals(v)) { tr->a = v; continue; }
            if(tr->b.Equals(v)) { tr->b = v; continue; }
            if(tr->c.Equals(v)) { tr->c = v; continue; }

            if(v.OnLineSegment(tr->a, tr->b)) {
                STriangle nt = STriangle::From(tr->meta, tr->a, v, tr->c);
                extras->AddTriangle(&nt);
                tr->a = v;
                continue;
            }
            if(v.OnLineSegment(tr->b, tr->c)) {
                STriangle nt = STriangle::From(tr->meta, tr->b, v, tr->a);
                extras->AddTriangle(&nt);
                tr->b = v;
                continue;
            }
            if(v.OnLineSegment(tr->c, tr->a)) {
                STriangle nt = STriangle::From(tr->meta, tr->c, v, tr->b);
                extras->AddTriangle(&nt);
                tr->c = v;
                continue;
            }
        }
    }
}

void SKdNode::SnapToMesh(SMesh *m) {
    int i, j, k;
    for(i = 0; i < m->l.n; i++) {
        STriangle *tr = &(m->l.elem[i]);
        for(j = 0; j < 3; j++) {
            Vector v = ((j == 0) ? tr->a : 
                       ((j == 1) ? tr->b :
                                   tr->c));

            SMesh extra;
            ZERO(&extra);
            SnapToVertex(v, &extra);

            for(k = 0; k < extra.l.n; k++) {
                STriangle *tra = (STriangle *)AllocTemporary(sizeof(*tra));
                *tra = extra.l.elem[k];
                AddTriangle(tra);
            }
            extra.Clear();
        }
    }
}

void SKdNode::FindEdgeOn(Vector a, Vector b, int *n, int *nOther,
                            STriMeta m, int cnt)
{
    if(gt && lt) {
        double ac = a.Element(which),
               bc = b.Element(which);
        if(ac < c + KDTREE_EPS ||
           bc < c + KDTREE_EPS)
        {
            lt->FindEdgeOn(a, b, n, nOther, m, cnt);
        }
        if(ac > c - KDTREE_EPS ||
           bc > c - KDTREE_EPS)
        {
            gt->FindEdgeOn(a, b, n, nOther, m, cnt);
        }
    } else {
        STriangleLl *ll;
        for(ll = tris; ll; ll = ll->next) {
            STriangle *tr = ll->tri;

            if(tr->tag == cnt) continue;
            
            if((a.EqualsExactly(tr->b) && b.EqualsExactly(tr->a)) ||
               (a.EqualsExactly(tr->c) && b.EqualsExactly(tr->b)) ||
               (a.EqualsExactly(tr->a) && b.EqualsExactly(tr->c)))
            {
                (*n)++;
                if(tr->meta.face != m.face) {
                    if(tr->meta.color == m.color &&
                       tr->meta.face != 0 && m.face != 0)
                    {
                        hEntity hf0 = { tr->meta.face },
                                hf1 = { m.face };
                        Entity *f0 = SS.GetEntity(hf0),
                               *f1 = SS.GetEntity(hf1);

                        Vector n0 = f0->FaceGetNormalNum().WithMagnitude(1),
                               n1 = f1->FaceGetNormalNum().WithMagnitude(1);

                        if(n0.Equals(n1) || n0.Equals(n1.ScaledBy(-1))) {
                            // faces are coincident, skip
                            // (If the planes are parallel, and the edge
                            // lies in both planes, then they're also
                            // coincident.)
                        } else {
                            (*nOther)++;
                        }
                    } else {
                        (*nOther)++;
                    }
                }
            }

            tr->tag = cnt;
        }
    }
}

void SKdNode::MakeCertainEdgesInto(SEdgeList *sel, bool emphasized) {
    SMesh m;
    ZERO(&m);
    ClearTags();
    MakeMeshInto(&m);

    int cnt = 1234;
    int i, j;
    for(i = 0; i < m.l.n; i++) {
        STriangle *tr = &(m.l.elem[i]);

        for(j = 0; j < 3; j++) {
            Vector a = (j == 0) ? tr->a : ((j == 1)  ? tr->b : tr->c);
            Vector b = (j == 0) ? tr->b : ((j == 1)  ? tr->c : tr->a);

            int n = 0, nOther = 0;
            FindEdgeOn(a, b, &n, &nOther, tr->meta, cnt++);
            if(n != 1) {
                if(!emphasized) {
                    if(n == 0) sel->AddEdge(a, b);
                } else {
//                    dbp("hanging: n=%d (%.3f %.3f %.3f)  (%.3f %.3f %.3f)", 
//                        n, CO(a), CO(b));
                }
            }
            if(nOther > 0) {
                if(emphasized) sel->AddEdge(a, b);
            }
        }
    }

    m.Clear();
}

