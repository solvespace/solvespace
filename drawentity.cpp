#include "solvespace.h"

void Entity::LineDrawOrGetDistance(Vector a, Vector b) {
    if(dogd.drawing) {
        // glPolygonOffset works only on polys, not lines, so do it myself
        Vector adj = SS.GW.projRight.Cross(SS.GW.projUp);
        adj = adj.ScaledBy(5/SS.GW.scale);
        glBegin(GL_LINES);
            glxVertex3v(a.Plus(adj));
            glxVertex3v(b.Plus(adj));
        glEnd();
    } else {
        Point2d ap = SS.GW.ProjectPoint(a);
        Point2d bp = SS.GW.ProjectPoint(b);

        double d = dogd.mp.DistanceToLine(ap, bp.Minus(ap), true);
        dogd.dmin = min(dogd.dmin, d);
    }
    dogd.refp = (a.Plus(b)).ScaledBy(0.5);
}

void Entity::LineDrawOrGetDistanceOrEdge(Vector a, Vector b) {
    LineDrawOrGetDistance(a, b);
    if(dogd.edges && !construction) {
        dogd.edges->AddEdge(a, b);
    }
}

void Entity::DrawAll(void) {
    // This handles points and line segments as a special case, because I
    // seem to be able to get a huge speedup that way, by consolidating
    // stuff to gl.
    int i;
    if(SS.GW.showPoints) {
        double s = 3.5/SS.GW.scale;
        Vector r = SS.GW.projRight.ScaledBy(s);
        Vector d = SS.GW.projUp.ScaledBy(s);
        glxColor3d(0, 0.8, 0);
        glPolygonOffset(-10, -10);
        glBegin(GL_QUADS);
        for(i = 0; i < SS.entity.n; i++) {
            Entity *e = &(SS.entity.elem[i]);
            if(!e->IsPoint()) continue;
            if(!(SS.GetGroup(e->group)->visible)) continue;
            if(SS.GroupsInOrder(SS.GW.activeGroup, e->group)) continue;

            Vector v = e->PointGetNum();
            glxVertex3v(v.Plus (r).Plus (d));
            glxVertex3v(v.Plus (r).Minus(d));
            glxVertex3v(v.Minus(r).Minus(d));
            glxVertex3v(v.Minus(r).Plus (d));
        }
        glEnd();
        glPolygonOffset(0, 0);
    }

    glLineWidth(1.5);
    for(i = 0; i < SS.entity.n; i++) {
        Entity *e = &(SS.entity.elem[i]);
        if(e->IsPoint())
        {
            continue; // already handled
        }
        e->Draw();
    }
    glLineWidth(1);
}

void Entity::Draw(void) {
    dogd.drawing = true;
    dogd.edges = NULL;
    DrawOrGetDistance();
}

void Entity::GenerateEdges(SEdgeList *el) {
    dogd.drawing = false;
    dogd.edges = el;
    DrawOrGetDistance();
    dogd.edges = NULL;
}

double Entity::GetDistance(Point2d mp) {
    dogd.drawing = false;
    dogd.edges = NULL;
    dogd.mp = mp;
    dogd.dmin = 1e12;

    DrawOrGetDistance();
    
    return dogd.dmin;
}

Vector Entity::GetReferencePos(void) {
    dogd.drawing = false;
    dogd.edges = NULL;

    dogd.refp = SS.GW.offset.ScaledBy(-1);
    DrawOrGetDistance();

    return dogd.refp;
}

bool Entity::IsVisible(void) {
    Group *g = SS.GetGroup(group);

    if(g->h.v == Group::HGROUP_REFERENCES.v && IsNormal()) {
        // The reference normals are always shown
        return true;
    }
    if(!g->visible) return false;
    if(SS.GroupsInOrder(SS.GW.activeGroup, group)) return false;

    if(IsPoint() && !SS.GW.showPoints) return false;
    if(IsWorkplane() && !SS.GW.showWorkplanes) return false;
    if(IsNormal() && !SS.GW.showNormals) return false;

    if(IsWorkplane() && !h.isFromRequest()) {
        // The group-associated workplanes are hidden outside their group.
        if(g->h.v != SS.GW.activeGroup.v) return false;
    }
    return true;
}

void Entity::DrawOrGetDistance(void) {  
    // If an entity is invisible, then it doesn't get shown, and it doesn't
    // contribute a distance for the selection, but it still generates edges.
    if(!dogd.edges) {
        if(!IsVisible()) return;
    }

    Group *g = SS.GetGroup(group);

    if(group.v != SS.GW.activeGroup.v) {
        glxColor3d(0.5, 0.3, 0.0);
    } else if(construction) {
        glxColor3d(0.1, 0.7, 0.1);
    } else {
        glxColor3d(1, 1, 1);
    }

    switch(type) {
        case POINT_N_COPY:
        case POINT_N_TRANS:
        case POINT_N_ROT_TRANS:
        case POINT_N_ROT_AA:
        case POINT_IN_3D:
        case POINT_IN_2D: {
            Vector v = PointGetNum();
            dogd.refp = v;

            if(dogd.drawing) {
                double s = 3.5;
                Vector r = SS.GW.projRight.ScaledBy(s/SS.GW.scale);
                Vector d = SS.GW.projUp.ScaledBy(s/SS.GW.scale);

                glxColor3d(0, 0.8, 0);
                glPolygonOffset(-10, -10);
                glBegin(GL_QUADS);
                    glxVertex3v(v.Plus (r).Plus (d));
                    glxVertex3v(v.Plus (r).Minus(d));
                    glxVertex3v(v.Minus(r).Minus(d));
                    glxVertex3v(v.Minus(r).Plus (d));
                glEnd();
                glPolygonOffset(0, 0);
            } else {
                Point2d pp = SS.GW.ProjectPoint(v);
                // Make a free point slightly easier to select, so that with
                // coincident points, we select the free one.
                dogd.dmin = pp.DistanceTo(dogd.mp) - 4;
            }
            break;
        }

        case NORMAL_N_COPY:
        case NORMAL_N_ROT:
        case NORMAL_N_ROT_AA:
        case NORMAL_IN_3D:
        case NORMAL_IN_2D: {
            int i;
            for(i = 0; i < 2; i++) {
                hRequest hr = h.request();
                double f = (i == 0 ? 0.4 : 1);
                if(hr.v == Request::HREQUEST_REFERENCE_XY.v) {
                    glxColor3d(0, 0, f);
                } else if(hr.v == Request::HREQUEST_REFERENCE_YZ.v) {
                    glxColor3d(f, 0, 0);
                } else if(hr.v == Request::HREQUEST_REFERENCE_ZX.v) {
                    glxColor3d(0, f, 0);
                } else {
                    glxColor3d(0, 0.4, 0.4);
                    if(i > 0) break;
                }

                Quaternion q = NormalGetNum();
                Vector tail;
                if(i == 0) {
                    tail = SS.GetEntity(point[0])->PointGetNum();
                    glLineWidth(1);
                } else {
                    // Draw an extra copy of the x, y, and z axes, that's
                    // always in the corner of the view and at the front.
                    // So those are always available, perhaps useful.
                    double s = SS.GW.scale;
                    double h = 60 - SS.GW.height/2;
                    double w = 60 - SS.GW.width/2;
                    Vector gn = SS.GW.projRight.Cross(SS.GW.projUp);
                    tail = SS.GW.projRight.ScaledBy(w/s).Plus(
                           SS.GW.projUp.   ScaledBy(h/s)).Plus(
                           gn.ScaledBy(-4*w/s)).Minus(SS.GW.offset);
                    glLineWidth(2);
                }

                Vector v = (q.RotationN()).WithMagnitude(50/SS.GW.scale);
                Vector tip = tail.Plus(v);
                LineDrawOrGetDistance(tail, tip);

                v = v.WithMagnitude(12/SS.GW.scale);
                Vector axis = q.RotationV();
                LineDrawOrGetDistance(tip,tip.Minus(v.RotatedAbout(axis, 0.6)));
                LineDrawOrGetDistance(tip,tip.Minus(v.RotatedAbout(axis,-0.6)));
            }
            glLineWidth(1.5);
            break;
        }

        case DISTANCE:
        case DISTANCE_N_COPY:
            // These are used only as data structures, nothing to display.
            break;

        case WORKPLANE: {
            Vector p;
            p = SS.GetEntity(point[0])->PointGetNum();

            Vector u = Normal()->NormalU();
            Vector v = Normal()->NormalV();

            double s = (min(SS.GW.width, SS.GW.height))*0.45/SS.GW.scale;

            Vector us = u.ScaledBy(s);
            Vector vs = v.ScaledBy(s);

            Vector pp = p.Plus (us).Plus (vs);
            Vector pm = p.Plus (us).Minus(vs);
            Vector mm = p.Minus(us).Minus(vs), mm2 = mm;
            Vector mp = p.Minus(us).Plus (vs);

            glLineWidth(1);
            glxColor3d(0, 0.3, 0.3);
            glEnable(GL_LINE_STIPPLE);
            glLineStipple(3, 0x1111);
            if(!h.isFromRequest()) {
                mm = mm.Plus(v.ScaledBy(60/SS.GW.scale));
                mm2 = mm2.Plus(u.ScaledBy(60/SS.GW.scale));
                LineDrawOrGetDistance(mm2, mm);
            }
            LineDrawOrGetDistance(pp, pm);
            LineDrawOrGetDistance(pm, mm2);
            LineDrawOrGetDistance(mm, mp);
            LineDrawOrGetDistance(mp, pp);
            glDisable(GL_LINE_STIPPLE);
            glLineWidth(1.5);

            char *str = DescriptionString()+5;
            if(dogd.drawing) {
                glPushMatrix();
                    glxTranslatev(mm2);
                    glxOntoWorkplane(u, v);
                    glxWriteText(str);
                glPopMatrix();
            } else {
                Vector pos = mm2.Plus(u.ScaledBy(glxStrWidth(str)/2)).Plus(
                                      v.ScaledBy(glxStrHeight()/2));
                Point2d pp = SS.GW.ProjectPoint(pos);
                dogd.dmin = min(dogd.dmin, pp.DistanceTo(dogd.mp) - 10);
                // If a line lies in a plane, then select the line, not
                // the plane.
                dogd.dmin += 3;
            }
            break;
        }

        case LINE_SEGMENT: {
            Vector a = SS.GetEntity(point[0])->PointGetNum();
            Vector b = SS.GetEntity(point[1])->PointGetNum();
            LineDrawOrGetDistanceOrEdge(a, b);
            break;
        }

        case CUBIC: {
            Vector p0 = SS.GetEntity(point[0])->PointGetNum();
            Vector p1 = SS.GetEntity(point[1])->PointGetNum();
            Vector p2 = SS.GetEntity(point[2])->PointGetNum();
            Vector p3 = SS.GetEntity(point[3])->PointGetNum();
            int i, n = 20;
            Vector prev = p0;
            for(i = 1; i <= n; i++) {
                double t = ((double)i)/n;
                Vector p =
                    (p0.ScaledBy((1 - t)*(1 - t)*(1 - t))).Plus(
                    (p1.ScaledBy(3*t*(1 - t)*(1 - t))).Plus(
                    (p2.ScaledBy(3*t*t*(1 - t))).Plus(
                    (p3.ScaledBy(t*t*t)))));
                LineDrawOrGetDistanceOrEdge(prev, p);
                prev = p;
            }
            break;
        }

#define CIRCLE_SIDES(r) (7 + (int)(sqrt(r*SS.GW.scale)))
        case ARC_OF_CIRCLE: {
            Vector c  = SS.GetEntity(point[0])->PointGetNum();
            Vector pa = SS.GetEntity(point[1])->PointGetNum();
            Vector pb = SS.GetEntity(point[2])->PointGetNum();
            Quaternion q = SS.GetEntity(normal)->NormalGetNum();
            Vector u = q.RotationU(), v = q.RotationV();

            double ra = (pa.Minus(c)).Magnitude();
            double rb = (pb.Minus(c)).Magnitude();

            double thetaa, thetab, dtheta;
            ArcGetAngles(&thetaa, &thetab, &dtheta);

            int i, n = 3 + (int)(CIRCLE_SIDES(ra)*dtheta/(2*PI));
            Vector prev = pa;
            for(i = 1; i <= n; i++) {
                double theta = thetaa + (dtheta*i)/n;
                double r = ra + ((rb - ra)*i)/n;
                Vector d = u.ScaledBy(cos(theta)).Plus(v.ScaledBy(sin(theta)));
                Vector p = c.Plus(d.ScaledBy(r));
                LineDrawOrGetDistanceOrEdge(prev, p);
                prev = p;
            }
            break;
        }

        case CIRCLE: {
            Quaternion q = SS.GetEntity(normal)->NormalGetNum();
            double r = SS.GetEntity(distance)->DistanceGetNum();
            Vector center = SS.GetEntity(point[0])->PointGetNum();
            Vector u = q.RotationU(), v = q.RotationV();

            int i, c = CIRCLE_SIDES(r); 
            Vector prev = u.ScaledBy(r).Plus(center);
            for(i = 1; i <= c; i++) {
                double phi = (2*PI*i)/c;
                Vector p = (u.ScaledBy(r*cos(phi))).Plus(
                            v.ScaledBy(r*sin(phi)));
                p = p.Plus(center);
                LineDrawOrGetDistanceOrEdge(prev, p);
                prev = p;
            }
            break;
        }

        case FACE_NORMAL_PT:
        case FACE_XPROD:
        case FACE_N_ROT_TRANS:
            // Do nothing; these are drawn with the triangle mesh
            break;

        default:
            oops();
    }
}

