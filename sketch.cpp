#include "solvespace.h"

const hEntity  Entity::FREE_IN_3D = { 0 };
const hEntity  Entity::NO_ENTITY = { 0 };
const hParam   Param::NO_PARAM = { 0 };
#define NO_PARAM (Param::NO_PARAM)

const hGroup Group::HGROUP_REFERENCES = { 1 };
const hRequest Request::HREQUEST_REFERENCE_XY = { 1 };
const hRequest Request::HREQUEST_REFERENCE_YZ = { 2 };
const hRequest Request::HREQUEST_REFERENCE_ZX = { 3 };

void Group::AddParam(IdList<Param,hParam> *param, hParam hp, double v) {
    Param pa;
    memset(&pa, 0, sizeof(pa));
    pa.h = hp;
    pa.val = v;

    param->Add(&pa);
}

void Group::MenuGroup(int id) {
    Group g;
    memset(&g, 0, sizeof(g));
    g.visible = true;

    SS.GW.GroupSelection();
#define gs (SS.GW.gs)

    switch(id) {
        case GraphicsWindow::MNU_GROUP_3D:
            g.type = DRAWING_3D;
            g.name.strcpy("draw-in-3d");
            break;

        case GraphicsWindow::MNU_GROUP_WRKPL:
            g.type = DRAWING_WORKPLANE;
            g.name.strcpy("draw-in-plane");
            if(gs.points == 1 && gs.n == 1) {
                g.subtype = WORKPLANE_BY_POINT_ORTHO;

                Vector u = SS.GW.projRight, v = SS.GW.projUp;
                u = u.ClosestOrtho();
                v = v.Minus(u.ScaledBy(v.Dot(u)));
                v = v.ClosestOrtho();

                g.wrkpl.q = Quaternion::MakeFrom(u, v);
                g.wrkpl.origin = gs.point[0];
            } else if(gs.points == 1 && gs.lineSegments == 2 && gs.n == 3) {
                g.subtype = WORKPLANE_BY_LINE_SEGMENTS;

                g.wrkpl.origin = gs.point[0];
                g.wrkpl.entityB = gs.entity[0];
                g.wrkpl.entityC = gs.entity[1];

                Vector ut = SS.GetEntity(g.wrkpl.entityB)->VectorGetNum();
                Vector vt = SS.GetEntity(g.wrkpl.entityC)->VectorGetNum();
                ut = ut.WithMagnitude(1);
                vt = vt.WithMagnitude(1);

                if(fabs(SS.GW.projUp.Dot(vt)) < fabs(SS.GW.projUp.Dot(ut))) {
                    SWAP(Vector, ut, vt);
                    g.wrkpl.swapUV = true;
                }
                if(SS.GW.projRight.Dot(ut) < 0) g.wrkpl.negateU = true;
                if(SS.GW.projUp.   Dot(vt) < 0) g.wrkpl.negateV = true;
            } else {
                Error("Bad selection for new drawing in workplane.");
                return;
            }
            SS.GW.ClearSelection();
            break;

        case GraphicsWindow::MNU_GROUP_EXTRUDE:
            g.type = EXTRUDE;
            g.opA = SS.GW.activeGroup;
            g.wrkpl.entityB = SS.GW.ActiveWorkplane();
            g.subtype = ONE_SIDED;
            g.name.strcpy("extrude");
            break;

        case GraphicsWindow::MNU_GROUP_ROT:
            g.type = ROTATE;
            g.opA = SS.GW.activeGroup;
            g.exprA = Expr::FromConstant(3)->DeepCopyKeep();
            g.subtype = ONE_SIDED;
            g.name.strcpy("rotate");
            break;

        case GraphicsWindow::MNU_GROUP_TRANS:
            g.type = TRANSLATE;
            g.opA = SS.GW.activeGroup;
            g.exprA = Expr::FromConstant(3)->DeepCopyKeep();
            g.subtype = ONE_SIDED;
            g.name.strcpy("translate");
            break;

        default: oops();
    }

    SS.group.AddAndAssignId(&g);
    SS.GenerateAll(SS.GW.solving == GraphicsWindow::SOLVE_ALWAYS);
    SS.GW.activeGroup = g.h;
    if(g.type == DRAWING_WORKPLANE) {
        SS.GetGroup(g.h)->activeWorkplane = g.h.entity(0);
    }
    SS.GW.AnimateOntoWorkplane();
    TextWindow::ScreenSelectGroup(0, g.h.v);
    SS.TW.Show();
}

char *Group::DescriptionString(void) {
    static char ret[100];
    if(name.str[0]) {
        sprintf(ret, "g%03x-%s", h.v, name.str);
    } else {
        sprintf(ret, "g%03x-(unnamed)", h.v);
    }
    return ret;
}

void Group::Generate(IdList<Entity,hEntity> *entity,
                     IdList<Param,hParam> *param)
{
    Vector gn = (SS.GW.projRight).Cross(SS.GW.projUp);
    Vector gp = SS.GW.projRight.Plus(SS.GW.projUp);
    gn = gn.WithMagnitude(200/SS.GW.scale);
    gp = gp.WithMagnitude(200/SS.GW.scale);
    int a, i;
    switch(type) {
        case DRAWING_3D:
            break;

        case DRAWING_WORKPLANE: {
            Quaternion q;
            if(subtype == WORKPLANE_BY_LINE_SEGMENTS) {
                Vector u = SS.GetEntity(wrkpl.entityB)->VectorGetNum();
                Vector v = SS.GetEntity(wrkpl.entityC)->VectorGetNum();
                u = u.WithMagnitude(1);
                Vector n = u.Cross(v);
                v = (n.Cross(u)).WithMagnitude(1);

                if(wrkpl.swapUV) SWAP(Vector, u, v);
                if(wrkpl.negateU) u = u.ScaledBy(-1);
                if(wrkpl.negateV) v = v.ScaledBy(-1);
                q = Quaternion::MakeFrom(u, v);
            } else if(subtype == WORKPLANE_BY_POINT_ORTHO) {
                // Already given, numerically.
                q = wrkpl.q;
            } else oops();

            Entity normal;
            memset(&normal, 0, sizeof(normal));
            normal.type = Entity::NORMAL_N_COPY;
            normal.numNormal = q;
            normal.point[0] = h.entity(2);
            normal.group = h;
            normal.h = h.entity(1);
            entity->Add(&normal);

            Entity point;
            memset(&point, 0, sizeof(point));
            point.type = Entity::POINT_N_COPY;
            point.numPoint = SS.GetEntity(wrkpl.origin)->PointGetNum();
            point.group = h;
            point.h = h.entity(2);
            entity->Add(&point);

            Entity wp;
            memset(&wp, 0, sizeof(wp));
            wp.type = Entity::WORKPLANE;
            wp.normal = normal.h;
            wp.point[0] = point.h;
            wp.group = h;
            wp.h = h.entity(0);
            entity->Add(&wp);
            break;
        }

        case EXTRUDE:
            AddParam(param, h.param(0), gn.x);
            AddParam(param, h.param(1), gn.y);
            AddParam(param, h.param(2), gn.z);
            int ai, af;
            if(subtype == ONE_SIDED) {
                ai = 0; af = 2;
            } else if(subtype == TWO_SIDED) {
                ai = -1; af = 1;
            } else oops();
            for(i = 0; i < entity->n; i++) {
                Entity *e = &(entity->elem[i]);
                if(e->group.v != opA.v) continue;

                hEntity he = e->h; e = NULL;
                // As soon as I call CopyEntity, e may become invalid! That
                // adds entities, which may cause a realloc.
                CopyEntity(he, ai,
                    h.param(0), h.param(1), h.param(2),
                    NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                    true);
                CopyEntity(he, af,
                    h.param(0), h.param(1), h.param(2),
                    NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                    true);
                MakeExtrusionLines(he, ai, af);
            }
            break;

        case TRANSLATE: {
            // The translation vector
            AddParam(param, h.param(0), gp.x);
            AddParam(param, h.param(1), gp.y);
            AddParam(param, h.param(2), gp.z);

            int n = (int)(exprA->Eval());
            for(a = 0; a < n; a++) {
                for(i = 0; i < entity->n; i++) {
                    Entity *e = &(entity->elem[i]);
                    if(e->group.v != opA.v) continue;

                    CopyEntity(e->h, a*2 - (subtype == ONE_SIDED ? 0 : (n-1)),
                        h.param(0), h.param(1), h.param(2),
                        NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                        true);
                }
            }
            break;
        }
        case ROTATE:
            // The translation vector
            AddParam(param, h.param(0), gp.x);
            AddParam(param, h.param(1), gp.y);
            AddParam(param, h.param(2), gp.z);
            // The rotation quaternion
            AddParam(param, h.param(3), 1);
            AddParam(param, h.param(4), 0);
            AddParam(param, h.param(5), 0);
            AddParam(param, h.param(6), 0);

            for(i = 0; i < entity->n; i++) {
                Entity *e = &(entity->elem[i]);
                if(e->group.v != opA.v) continue;

                CopyEntity(e->h, 0,
                    h.param(0), h.param(1), h.param(2),
                    h.param(3), h.param(4), h.param(5), h.param(6),
                    false);
            }
            break;

        default: oops();
    }
}

void Group::GenerateEquations(IdList<Equation,hEquation> *l) {
    Equation eq;
    if(type == ROTATE) {
        // Normalize the quaternion
        ExprQuaternion q = {
            Expr::FromParam(h.param(3)),
            Expr::FromParam(h.param(4)),
            Expr::FromParam(h.param(5)),
            Expr::FromParam(h.param(6)) };
        eq.e = (q.Magnitude())->Minus(Expr::FromConstant(1));
        eq.h = h.equation(0);
        l->Add(&eq);
    } else if(type == EXTRUDE) {
        if(wrkpl.entityB.v != Entity::FREE_IN_3D.v) {
            Entity *w = SS.GetEntity(wrkpl.entityB);
            ExprVector u = w->Normal()->NormalExprsU();
            ExprVector v = w->Normal()->NormalExprsV();
            ExprVector extruden = {
                Expr::FromParam(h.param(0)),
                Expr::FromParam(h.param(1)),
                Expr::FromParam(h.param(2)) };
            eq.e = u.Dot(extruden);
            eq.h = h.equation(0);
            l->Add(&eq);
            eq.e = v.Dot(extruden);
            eq.h = h.equation(1);
            l->Add(&eq);
        }
    }
}

hEntity Group::Remap(hEntity in, int copyNumber) {
    int i;
    for(i = 0; i < remap.n; i++) {
        EntityMap *em = &(remap.elem[i]);
        if(em->input.v == in.v && em->copyNumber == copyNumber) {
            // We already have a mapping for this entity.
            return h.entity(em->h.v);
        }
    }
    // We don't have a mapping yet, so create one.
    EntityMap em;
    em.input = in;
    em.copyNumber = copyNumber;
    remap.AddAndAssignId(&em);
    return h.entity(em.h.v);
}

void Group::MakeExtrusionLines(hEntity in, int ai, int af) {
    Entity *ep = SS.GetEntity(in);
    if(!(ep->IsPoint())) return;

    Entity en;
    memset(&en, 0, sizeof(en));
    en.point[0] = Remap(ep->h, ai);
    en.point[1] = Remap(ep->h, af);
    en.group = h;
    en.h = Remap(ep->h, 10);
    en.type = Entity::LINE_SEGMENT;
    // And then this line segment gets added
    SS.entity.Add(&en);
}

void Group::CopyEntity(hEntity in, int a, hParam dx, hParam dy, hParam dz,
                       hParam qw, hParam qvx, hParam qvy, hParam qvz,
                       bool transOnly)
{
    Entity *ep = SS.GetEntity(in);
    
    Entity en;
    memset(&en, 0, sizeof(en));
    en.type = ep->type;
    en.h = Remap(ep->h, a);
    en.group = h;
    en.construction = ep->construction;

    switch(ep->type) {
        case Entity::WORKPLANE:
            // Don't copy these.
            return;

        case Entity::LINE_SEGMENT:  
            en.point[0] = Remap(ep->point[0], a);
            en.point[1] = Remap(ep->point[1], a);
            break;

        case Entity::CUBIC:
            en.point[0] = Remap(ep->point[0], a);
            en.point[1] = Remap(ep->point[1], a);
            en.point[2] = Remap(ep->point[2], a);
            en.point[3] = Remap(ep->point[3], a);
            break;

        case Entity::CIRCLE:
            en.point[0] = Remap(ep->point[0], a);
            en.normal   = Remap(ep->normal, a);
            en.distance = Remap(ep->distance, a);
            break;

        case Entity::ARC_OF_CIRCLE:
            en.point[0] = Remap(ep->point[0], a);
            en.point[1] = Remap(ep->point[1], a);
            en.point[2] = Remap(ep->point[2], a);
            en.normal   = Remap(ep->normal, a);
            break;

        case Entity::POINT_N_COPY:
        case Entity::POINT_N_TRANS:
        case Entity::POINT_N_ROT_TRANS:
        case Entity::POINT_IN_3D:
        case Entity::POINT_IN_2D:
            if(transOnly) {
                en.type = Entity::POINT_N_TRANS;
                en.param[0] = dx;
                en.param[1] = dy;
                en.param[2] = dz;
            } else {
                en.type = Entity::POINT_N_ROT_TRANS;
                en.param[0] = dx;
                en.param[1] = dy;
                en.param[2] = dz;
                en.param[3] = qw;
                en.param[4] = qvx;
                en.param[5] = qvy;
                en.param[6] = qvz;
            }
            en.numPoint = ep->PointGetNum();
            en.timesApplied = a;
            break;

        case Entity::NORMAL_N_COPY:
        case Entity::NORMAL_N_ROT:
        case Entity::NORMAL_IN_3D:
        case Entity::NORMAL_IN_2D:
            if(transOnly) {
                en.type = Entity::NORMAL_N_COPY;
            } else {
                en.type = Entity::NORMAL_N_ROT;
                en.param[0] = qw;
                en.param[1] = qvx;
                en.param[2] = qvy;
                en.param[3] = qvz;
            }
            en.numNormal = ep->NormalGetNum();
            en.point[0] = Remap(ep->point[0], a);
            en.timesApplied = a;
            break;

        case Entity::DISTANCE_N_COPY:
        case Entity::DISTANCE:
            en.type = Entity::DISTANCE_N_COPY;
            en.numDistance = ep->DistanceGetNum();
            break;

        default:
            oops();
    }
    SS.entity.Add(&en);
}

SMesh *Group::PreviousGroupMesh(void) {
    int i;
    for(i = 0; i < SS.group.n; i++) {
        Group *g = &(SS.group.elem[i]);
        if(g->h.v == h.v) break;
    }
    if(i == 0 || i >= SS.group.n) oops();
    return &(SS.group.elem[i-1].mesh);
}

void Group::MakePolygons(void) {
    poly.Clear();

    SEdgeList edges;
    ZERO(&edges);
    SMesh outm;
    ZERO(&outm);

    if(type == DRAWING_3D || type == DRAWING_WORKPLANE ||
       type == ROTATE || type == TRANSLATE)
    {
        int i;
        for(i = 0; i < SS.entity.n; i++) {
            Entity *e = &(SS.entity.elem[i]);
            if(e->group.v != h.v) continue;

            e->GenerateEdges(&edges);
        }
        SEdge error;
        if(edges.AssemblePolygon(&poly, &error)) {
            polyError.yes = false;
            poly.normal = poly.ComputeNormal();
            poly.FixContourDirections();
        } else {
            polyError.yes = true;
            polyError.notClosedAt = error;
            poly.Clear();
        }
    } else if(type == EXTRUDE) {
        int i;
        Group *src = SS.GetGroup(opA);
        Vector translate = Vector::MakeFrom(
            SS.GetParam(h.param(0))->val,
            SS.GetParam(h.param(1))->val,
            SS.GetParam(h.param(2))->val
        );
        Vector tbot, ttop;
        if(subtype == ONE_SIDED) {
            tbot = Vector::MakeFrom(0, 0, 0); ttop = translate.ScaledBy(2);
        } else {
            tbot = translate.ScaledBy(-1); ttop = translate.ScaledBy(1);
        }
        bool flipBottom = translate.Dot(src->poly.normal) > 0;

        // Get a triangulation of the source poly; this is not a closed mesh.
        SMesh srcm; ZERO(&srcm);
        (src->poly).TriangulateInto(&srcm);

        // Do the bottom; that has normal pointing opposite from translate
        for(i = 0; i < srcm.l.n; i++) {
            STriangle *st = &(srcm.l.elem[i]);
            Vector at = (st->a).Plus(tbot), 
                   bt = (st->b).Plus(tbot),
                   ct = (st->c).Plus(tbot);
            if(flipBottom) {
                outm.AddTriangle(ct, bt, at);
            } else {
                outm.AddTriangle(at, bt, ct);
            }
        }
        // And the top; that has the normal pointing the same dir as translate
        for(i = 0; i < srcm.l.n; i++) {
            STriangle *st = &(srcm.l.elem[i]);
            Vector at = (st->a).Plus(ttop), 
                   bt = (st->b).Plus(ttop),
                   ct = (st->c).Plus(ttop);
            if(flipBottom) {
                outm.AddTriangle(at, bt, ct);
            } else {
                outm.AddTriangle(ct, bt, at);
            }
        }
        srcm.Clear();
        // Get the source polygon to extrude, and break it down to edges
        edges.Clear();
        (src->poly).MakeEdgesInto(&edges);

        // The sides; these are quads, represented as two triangles.
        for(i = 0; i < edges.l.n; i++) {
            SEdge *edge = &(edges.l.elem[i]);
            Vector abot = (edge->a).Plus(tbot), bbot = (edge->b).Plus(tbot);
            Vector atop = (edge->a).Plus(ttop), btop = (edge->b).Plus(ttop);
            if(flipBottom) {
                outm.AddTriangle(bbot, abot, atop);
                outm.AddTriangle(bbot, atop, btop);
            } else {
                outm.AddTriangle(abot, bbot, atop);
                outm.AddTriangle(bbot, btop, atop);
            }
        }
    }
    edges.Clear();

    // So our group's mesh appears in outm. Combine this with the previous
    // group's mesh, using the requested operation.
    mesh.Clear();
    SMesh *a = PreviousGroupMesh();
    if(meshCombine == COMBINE_AS_UNION) {
        mesh.MakeFromUnion(a, &outm);
    } else {
        mesh.MakeFromDifference(a, &outm);
    }
    outm.Clear();
}

void Group::Draw(void) {
    // Show this even if the group is not visible. It's already possible
    // to show or hide just this with the "show solids" flag.

    if(type == DRAWING_3D || type == DRAWING_WORKPLANE) {
        GLfloat mpf[] = { 0.1f, 0.1f, 0.1f, 1.0 };
        glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mpf);
    } else {
        GLfloat mpf[] = { 0.4f, 0.4f, 0.4f, 1.0 };
        glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mpf);
    }
    // The back faces are drawn in red; should never seem them, since we
    // draw closed shells, so that's a debugging aid.
    GLfloat mpb[] = { 1.0f, 0.1f, 0.1f, 1.0 };
    glMaterialfv(GL_BACK, GL_AMBIENT_AND_DIFFUSE, mpb);

    glEnable(GL_LIGHTING);
    if(SS.GW.showShaded) glxFillMesh(&mesh);
    glDisable(GL_LIGHTING);

    if(SS.GW.showMesh) glxDebugMesh(&mesh);

    if(!SS.GW.showShaded) return;
    if(polyError.yes) {
        glxColor4d(1, 0, 0, 0.2);
        glLineWidth(10);
        glBegin(GL_LINES);
            glxVertex3v(polyError.notClosedAt.a);
            glxVertex3v(polyError.notClosedAt.b);
        glEnd();
        glLineWidth(1);
        glxColor3d(1, 0, 0);
        glPushMatrix();
            glxTranslatev(polyError.notClosedAt.b);
            glxOntoWorkplane(SS.GW.projRight, SS.GW.projUp);
            glxWriteText("not closed contour!");
        glPopMatrix();
    } else {
        glxColor4d(0, 1.0, 1.0, 0.05);
        glPolygonOffset(-1, -1);
        glxFillPolygon(&poly);
        glPolygonOffset(0, 0);
    }
}

hParam Request::AddParam(IdList<Param,hParam> *param, hParam hp) {
    Param pa;
    memset(&pa, 0, sizeof(pa));
    pa.h = hp;
    param->Add(&pa);
    return hp;
}

void Request::Generate(IdList<Entity,hEntity> *entity,
                       IdList<Param,hParam> *param)
{
    int points = 0;
    int params = 0;
    int et = 0;
    bool hasNormal = false;
    bool hasDistance = false;
    int i;

    Entity e;
    memset(&e, 0, sizeof(e));
    switch(type) {
        case Request::WORKPLANE:
            et = Entity::WORKPLANE;
            points = 1;
            hasNormal = true;
            break;

        case Request::DATUM_POINT:
            et = 0;
            points = 1;
            break;

        case Request::LINE_SEGMENT:
            et = Entity::LINE_SEGMENT;
            points = 2;
            break;

        case Request::CIRCLE:
            et = Entity::CIRCLE;
            points = 1;
            params = 1;
            hasNormal = true;
            hasDistance = true;
            break;

        case Request::ARC_OF_CIRCLE:
            et = Entity::ARC_OF_CIRCLE;
            points = 3;
            hasNormal = true;
            break;

        case Request::CUBIC:
            et = Entity::CUBIC;
            points = 4; 
            break;

        default: oops();
    }

    // Generate the entity that's specific to this request.
    e.type = et;
    e.group = group;
    e.workplane = workplane;
    e.construction = construction;
    e.h = h.entity(0);

    // And generate entities for the points
    for(i = 0; i < points; i++) {
        Entity p;
        memset(&p, 0, sizeof(p));
        p.workplane = workplane;
        // points start from entity 1, except for datum point case
        p.h = h.entity(i+(et ? 1 : 0));
        p.group = group;

        if(workplane.v == Entity::FREE_IN_3D.v) {
            p.type = Entity::POINT_IN_3D;
            // params for x y z
            p.param[0] = AddParam(param, h.param(16 + 3*i + 0));
            p.param[1] = AddParam(param, h.param(16 + 3*i + 1));
            p.param[2] = AddParam(param, h.param(16 + 3*i + 2));
        } else {
            p.type = Entity::POINT_IN_2D;
            // params for u v
            p.param[0] = AddParam(param, h.param(16 + 3*i + 0));
            p.param[1] = AddParam(param, h.param(16 + 3*i + 1));
        }
        entity->Add(&p);
        e.point[i] = p.h;
    }
    if(hasNormal) {
        Entity n;
        memset(&n, 0, sizeof(n));
        n.workplane = workplane;
        n.h = h.entity(32);
        n.group = group;
        if(workplane.v == Entity::FREE_IN_3D.v) {
            n.type = Entity::NORMAL_IN_3D;
            n.param[0] = AddParam(param, h.param(32+0));
            n.param[1] = AddParam(param, h.param(32+1));
            n.param[2] = AddParam(param, h.param(32+2));
            n.param[3] = AddParam(param, h.param(32+3));
        } else {
            n.type = Entity::NORMAL_IN_2D;
            // and this is just a copy of the workplane quaternion,
            // so no params required
        }
        if(points < 1) oops();
        // The point determines where the normal gets displayed on-screen;
        // it's entirely cosmetic.
        n.point[0] = e.point[0];
        entity->Add(&n);
        e.normal = n.h;
    }
    if(hasDistance) {
        Entity d;
        memset(&d, 0, sizeof(d));
        d.workplane = workplane;
        d.h = h.entity(64);
        d.group = group;
        d.type = Entity::DISTANCE;
        d.param[0] = AddParam(param, h.param(64));
        entity->Add(&d);
        e.distance = d.h;
    }
    // And generate any params not associated with the point that
    // we happen to need.
    for(i = 0; i < params; i++) {
        e.param[i] = AddParam(param, h.param(i));
    }

    if(et) entity->Add(&e);
}

char *Request::DescriptionString(void) {
    static char ret[100];
    if(name.str[0]) {
        sprintf(ret, "r%03x-%s", h.v, name.str);
    } else {
        sprintf(ret, "r%03x-(unnamed)", h.v);
    }
    return ret;
}

