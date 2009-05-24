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

void SMesh::MakeFromUnionOf(SMesh *a, SMesh *b) {
    SBsp3 *bspa = SBsp3::FromMesh(a);
    SBsp3 *bspb = SBsp3::FromMesh(b);

    flipNormal = false;
    keepCoplanar = false;
    AddAgainstBsp(b, bspa);

    flipNormal = false;
    keepCoplanar = true;
    AddAgainstBsp(a, bspb);
}

void SMesh::MakeFromDifferenceOf(SMesh *a, SMesh *b) {
    SBsp3 *bspa = SBsp3::FromMesh(a);
    SBsp3 *bspb = SBsp3::FromMesh(b);

    flipNormal = true;
    keepCoplanar = true;
    AddAgainstBsp(b, bspa);

    flipNormal = false;
    keepCoplanar = false;
    AddAgainstBsp(a, bspb);
}

void SMesh::MakeFromCopyOf(SMesh *a) {
    int i;
    for(i = 0; i < a->l.n; i++) {
        AddTriangle(&(a->l.elem[i]));
    }
}

void SMesh::MakeFromAssemblyOf(SMesh *a, SMesh *b) {
    MakeFromCopyOf(a);
    MakeFromCopyOf(b);
}

void SMesh::MakeFromTransformationOf(SMesh *a, Vector trans, Quaternion q) {
    STriangle *tr;
    for(tr = a->l.First(); tr; tr = a->l.NextAfter(tr)) {
        STriangle tt = *tr;
        tt.a = (q.Rotate(tt.a)).Plus(trans);
        tt.b = (q.Rotate(tt.b)).Plus(trans);
        tt.c = (q.Rotate(tt.c)).Plus(trans);
        AddTriangle(&tt);
    }
}

bool SMesh::IsEmpty(void) {
    return (l.n == 0);
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

void SKdNode::FindEdgeOn(Vector a, Vector b, int *n, int cnt,
                            bool coplanarIsInter, bool *inter, bool *fwd)
{
    if(gt && lt) {
        double ac = a.Element(which),
               bc = b.Element(which);
        if(ac < c + KDTREE_EPS ||
           bc < c + KDTREE_EPS)
        {
            lt->FindEdgeOn(a, b, n, cnt, coplanarIsInter, inter, fwd);
        }
        if(ac > c - KDTREE_EPS ||
           bc > c - KDTREE_EPS)
        {
            gt->FindEdgeOn(a, b, n, cnt, coplanarIsInter, inter, fwd);
        }
        return;
    }
    
    // We are a leaf node; so we iterate over all the triangles in our
    // linked list.
    STriangleLl *ll;
    for(ll = tris; ll; ll = ll->next) {
        STriangle *tr = ll->tri;

        if(tr->tag == cnt) continue;
        
        // Test if this triangle matches up with the given edge
        if((a.Equals(tr->b) && b.Equals(tr->a)) ||
           (a.Equals(tr->c) && b.Equals(tr->b)) ||
           (a.Equals(tr->a) && b.Equals(tr->c)))
        {
            (*n)++;
            // Record whether this triangle is front- or back-facing.
            if(tr->Normal().z > LENGTH_EPS) {
                *fwd = true;
            } else {
                *fwd = false;
            }
        } else if(((a.Equals(tr->a) && b.Equals(tr->b)) ||
                   (a.Equals(tr->b) && b.Equals(tr->c)) ||
                   (a.Equals(tr->c) && b.Equals(tr->a))))
        {
            // It's an edge of this triangle, okay.
        } else {
            // Check for self-intersection
            Vector n = (tr->Normal()).WithMagnitude(1);
            double d = (tr->a).Dot(n);
            double pa = a.Dot(n) - d, pb = b.Dot(n) - d;
            // It's an intersection if neither point lies in-plane,
            // and the edge crosses the plane (should handle in-plane
            // intersections separately but don't yet).
            if((pa < -LENGTH_EPS || pa > LENGTH_EPS) &&
               (pb < -LENGTH_EPS || pb > LENGTH_EPS) &&
               (pa*pb < 0))
            {
                // The edge crosses the plane of the triangle; now see if
                // it crosses inside the triangle.
                if(tr->ContainsPointProjd(b.Minus(a), a)) {
                    if(coplanarIsInter) {
                        *inter = true;
                    } else {
                        Vector p = Vector::AtIntersectionOfPlaneAndLine(
                                                n, d, a, b, NULL);
                        Vector ta = tr->a,
                               tb = tr->b,
                               tc = tr->c;
                        if((p.DistanceToLine(ta, tb.Minus(ta)) < LENGTH_EPS) ||
                           (p.DistanceToLine(tb, tc.Minus(tb)) < LENGTH_EPS) ||
                           (p.DistanceToLine(tc, ta.Minus(tc)) < LENGTH_EPS))
                        {
                            // Intersection lies on edge. This happens when
                            // our edge is from a triangle coplanar with
                            // another triangle in the mesh. We don't test
                            // the edge against triangles whose plane contains
                            // that edge, but we do end up testing against
                            // the coplanar triangle's neighbours, which we
                            // will intersect on their edges.
                        } else {
                            *inter = true;
                        }
                    }
                }
            }
        }

        // Ensure that we don't count this triangle twice if it appears
        // in two buckets of the kd tree.
        tr->tag = cnt;
    }
}

void SKdNode::SplitLinesAgainstTriangle(SEdgeList *sel, STriangle *tr) {
    SEdgeList seln;
    ZERO(&seln);

    Vector tn = tr->Normal().WithMagnitude(1);
    double td = tn.Dot(tr->a);

    // Consider front-facing triangles only
    if(tn.z > LENGTH_EPS) {
        // If the edge crosses our triangle's plane, then split into above
        // and below parts.
        SEdge *se;
        for(se = sel->l.First(); se; se = sel->l.NextAfter(se)) {
            double da = (se->a).Dot(tn) - td,
                   db = (se->b).Dot(tn) - td;
            if((da < -LENGTH_EPS && db > LENGTH_EPS) ||
               (db < -LENGTH_EPS && da > LENGTH_EPS))
            {
                Vector m = Vector::AtIntersectionOfPlaneAndLine(
                                        tn, td,
                                        se->a, se->b, NULL);
                seln.AddEdge(m, se->b);
                se->b = m;
            }
        }
        for(se = seln.l.First(); se; se = seln.l.NextAfter(se)) {
            sel->AddEdge(se->a, se->b);
        }
        seln.Clear();

        for(se = sel->l.First(); se; se = sel->l.NextAfter(se)) {
            Vector pt = ((se->a).Plus(se->b)).ScaledBy(0.5);
            double dt = pt.Dot(tn) - td;
            if(pt.Dot(tn) - td > -LENGTH_EPS) {
                // Edge is in front of or on our plane (remember, tn.z > 0)
                // so it is exempt from further splitting
                se->auxA = 1;
            } else {
                // Edge is behind our plane, needs further splitting
                se->auxA = 0;
            }
        }

        // Considering only the (x, y) coordinates, split the edge against our
        // triangle.
        Point2d a = (tr->a).ProjectXy(),
                b = (tr->b).ProjectXy(),
                c = (tr->c).ProjectXy();

        Point2d n[3] = { (b.Minus(a)).Normal().WithMagnitude(1),
                         (c.Minus(b)).Normal().WithMagnitude(1),
                         (a.Minus(c)).Normal().WithMagnitude(1)  };

        double d[3] = { n[0].Dot(b),
                        n[1].Dot(c),
                        n[2].Dot(a)  };

        // Split all of the edges where they intersect the triangle edges
        int i;
        for(i = 0; i < 3; i++) {
            for(se = sel->l.First(); se; se = sel->l.NextAfter(se)) {
                if(se->auxA) continue;

                Point2d ap = (se->a).ProjectXy(),
                        bp = (se->b).ProjectXy();
                double da = n[i].Dot(ap) - d[i],
                       db = n[i].Dot(bp) - d[i];
                if((da < -LENGTH_EPS && db > LENGTH_EPS) ||
                   (db < -LENGTH_EPS && da > LENGTH_EPS))
                {
                    double dab = (db - da);
                    Vector spl = ((se->a).ScaledBy( db/dab)).Plus(
                                  (se->b).ScaledBy(-da/dab));
                    seln.AddEdge(spl, se->b);
                    se->b = spl;
                }
            }
            for(se = seln.l.First(); se; se = seln.l.NextAfter(se)) {
                sel->AddEdge(se->a, se->b, 0);
            }
            seln.Clear();
        }
       
        for(se = sel->l.First(); se; se = sel->l.NextAfter(se)) {
            if(se->auxA) {
                // Lies above or on the triangle plane, so triangle doesn't
                // occlude it.
                se->tag = 0;
            } else {
                // Test the segment to see if it lies outside the triangle
                // (i.e., outside wrt at least one edge), and keep it only
                // then.
                Point2d pt = ((se->a).Plus(se->b).ScaledBy(0.5)).ProjectXy();
                se->tag = 1;
                for(i = 0; i < 3; i++) {
                    // If the test point lies on the boundary of our triangle,
                    // then we still discard the edge.
                    if(n[i].Dot(pt) - d[i] > LENGTH_EPS) se->tag = 0;
                }
            }
        }
        sel->l.RemoveTagged();
    }
}

void SKdNode::OcclusionTestLine(SEdge orig, SEdgeList *sel, int cnt) {
    if(gt && lt) {
        double ac = (orig.a).Element(which),
               bc = (orig.b).Element(which);
        // We can ignore triangles that are separated in x or y, but triangles
        // that are separated in z may still contribute
        if(ac < c + KDTREE_EPS ||
           bc < c + KDTREE_EPS ||
           which == 2)
        {
            lt->OcclusionTestLine(orig, sel, cnt);
        }
        if(ac > c - KDTREE_EPS ||
           bc > c - KDTREE_EPS ||
           which == 2)
        {
            gt->OcclusionTestLine(orig, sel, cnt);
        }
    } else {
        STriangleLl *ll;
        for(ll = tris; ll; ll = ll->next) {
            STriangle *tr = ll->tri;

            if(tr->tag == cnt) continue;

            SplitLinesAgainstTriangle(sel, tr);
            tr->tag = cnt;
        }
    }
}

//-----------------------------------------------------------------------------
// Report all naked edges of the mesh (i.e., edges that don't join up to
// a single anti-parallel edge of another triangle), and all edges that
// intersect another triangle. If coplanarIsInter, then edges coplanar with
// another triangle and within it are reported, otherwise not. We report
// in *inter and *leaky whether the mesh is self-intersecting or leaky
// (having naked edges) respectively.
//-----------------------------------------------------------------------------
void SKdNode::MakeNakedEdgesInto(SEdgeList *sel, bool coplanarIsInter,
                                        bool *inter, bool *leaky)
{
    if(inter) *inter = false;
    if(leaky) *leaky = false;

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
            bool thisIntersects = false, fwd;
            FindEdgeOn(a, b, &n, cnt, coplanarIsInter, &thisIntersects, &fwd);
            if(n != 1) {
                sel->AddEdge(a, b);
                if(leaky) *leaky = true;
            }
            if(thisIntersects) {
                sel->AddEdge(a, b);
                if(inter) *inter = true;
            }

            cnt++;
        }
    }

    m.Clear();
}

void SKdNode::MakeTurningEdgesInto(SEdgeList *sel) {
    SMesh m;
    ZERO(&m);
    ClearTags();
    MakeMeshInto(&m);

    int cnt = 1234;
    int i, j;
    for(i = 0; i < m.l.n; i++) {
        STriangle *tr = &(m.l.elem[i]);
        if(tr->Normal().z > LENGTH_EPS) continue;
        // So this is a back-facing triangle

        for(j = 0; j < 3; j++) {
            Vector a = (j == 0) ? tr->a : ((j == 1)  ? tr->b : tr->c);
            Vector b = (j == 0) ? tr->b : ((j == 1)  ? tr->c : tr->a);

            int n = 0;
            bool inter, fwd;
            FindEdgeOn(a, b, &n, cnt, true, &inter, &fwd);
            if(n == 1) {
                // and its neighbour is front-facing, so generate the edge.
                if(fwd) sel->AddEdge(a, b);
            }

            cnt++;
        }
    }

    m.Clear();
}

