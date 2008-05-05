#include "solvespace.h"

const hEntity  Entity::FREE_IN_3D = { 0 };

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

    switch(id) {
        case GraphicsWindow::MNU_GROUP_DRAWING:
            g.type = DRAWING;
            g.name.strcpy("drawing");
            break;

        case GraphicsWindow::MNU_GROUP_EXTRUDE:
            g.type = EXTRUDE;
            g.opA.v = 2;
            g.name.strcpy("extrude");
            break;

        default: oops();
    }

    SS.group.AddAndAssignId(&g);
    SS.GenerateAll(SS.GW.solving == GraphicsWindow::SOLVE_ALWAYS);
    SS.GW.activeGroup = g.h;
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
    int i;
    switch(type) {
        case DRAWING:
            return;

        case EXTRUDE:
            AddParam(param, h.param(0), 50);
            AddParam(param, h.param(1), 50);
            AddParam(param, h.param(2), 50);
            for(i = 0; i < entity->n; i++) {
                Entity *e = &(entity->elem[i]);
                if(e->group.v != opA.v) continue;

                CopyEntity(e->h, 0, h.param(0), h.param(1), h.param(2), true);
            }
            break;

        default: oops();
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

void Group::CopyEntity(hEntity in, int a, hParam dx, hParam dy, hParam dz,
                       bool isExtrusion)
{
    Entity *ep = SS.GetEntity(in);
    
    Entity en;
    memset(&en, 0, sizeof(en));
    en.type = ep->type;
    en.h = Remap(ep->h, a);
    en.group = h;

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
            en.param[0] = ep->param[0]; // XXX make numerical somehow later
            break;

        case Entity::POINT_IN_3D:
        case Entity::POINT_IN_2D:
            en.type = Entity::POINT_XFRMD;
            en.param[0] = dx;
            en.param[1] = dy;
            en.param[2] = dz;
            en.numPoint = ep->PointGetNum();

            if(isExtrusion) {
                if(a != 0) oops();
                SS.entity.Add(&en);
                en.point[0] = ep->h;
                en.point[1] = en.h;
                en.h = Remap(ep->h, 1);
                en.type = Entity::LINE_SEGMENT;
                // And then this line segment gets added
            }
            break;

        case Entity::NORMAL_IN_3D:
        case Entity::NORMAL_IN_2D:
            en.type = Entity::NORMAL_XFRMD;
            en.numNormal = ep->NormalGetNum();
            en.point[0] = Remap(ep->point[0], a);
            break;

        default:
            oops();
    }
    SS.entity.Add(&en);
}

void Group::MakePolygons(void) {
    faces.Clear();
    if(type == DRAWING) {
        edges.l.Clear();
        int i;
        for(i = 0; i < SS.entity.n; i++) {
            Entity *e = &(SS.entity.elem[i]);
            if(e->group.v != h.v) continue;

            e->GenerateEdges(&edges);
        }
        SPolygon poly;
        memset(&poly, 0, sizeof(poly));
        SEdge error;
        if(edges.AssemblePolygon(&poly, &error)) {
            polyError.yes = false;
            faces.Add(&poly);
        } else {
            polyError.yes = true;
            polyError.notClosedAt = error;
            poly.Clear();
        }
    } else if(type == EXTRUDE) {
        Vector translate;
        translate.x = SS.GetParam(h.param(0))->val;
        translate.y = SS.GetParam(h.param(1))->val;
        translate.z = SS.GetParam(h.param(2))->val;

        edges.l.Clear();
        Group *src = SS.GetGroup(opA);
        if(src->faces.n != 1) return;

        (src->faces.elem[0]).MakeEdgesInto(&edges);

        SPolygon poly;
        SEdge error;

        // The bottom
        memset(&poly, 0, sizeof(poly));
        if(!edges.AssemblePolygon(&poly, &error)) oops();
        faces.Add(&poly);

        // The sides
        int i;
        for(i = 0; i < edges.l.n; i++) {
            SEdge *edge = &(edges.l.elem[i]);
            memset(&poly, 0, sizeof(poly));
            poly.AddEmptyContour();
            poly.AddPoint(edge->a);
            poly.AddPoint(edge->b);
            poly.AddPoint((edge->b).Plus(translate));
            poly.AddPoint((edge->a).Plus(translate));
            poly.AddPoint(edge->a);
            faces.Add(&poly);
            edge->a = (edge->a).Plus(translate);
            edge->b = (edge->b).Plus(translate);
        }

        // The top
        memset(&poly, 0, sizeof(poly));
        if(!edges.AssemblePolygon(&poly, &error)) oops();
        faces.Add(&poly);
    }
}

void Group::Draw(void) {
    if(!visible) return;

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
        int i;
        glEnable(GL_LIGHTING);
        GLfloat vec[] = { 0, 0, 0.5, 1.0 };
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, vec);
        for(i = 0; i < faces.n; i++) {
            glxFillPolygon(&(faces.elem[i]));
        }
        glDisable(GL_LIGHTING);
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
        n.h = h.entity(16);
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

