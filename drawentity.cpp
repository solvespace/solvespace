#include "solvespace.h"

void Entity::LineDrawOrGetDistance(Vector a, Vector b) {
    if(dogd.drawing) {
        // Draw lines from active group in front of those from previous
        glxDepthRangeOffset((group.v == SS.GW.activeGroup.v) ? 4 : 3);
        glBegin(GL_LINES);
            glxVertex3v(a);
            glxVertex3v(b);
        glEnd();
        glxDepthRangeOffset(0);
    } else {
        Point2d ap = SS.GW.ProjectPoint(a);
        Point2d bp = SS.GW.ProjectPoint(b);

        double d = dogd.mp.DistanceToLine(ap, bp.Minus(ap), true);
        // A little bit easier to select in the active group
        if(group.v == SS.GW.activeGroup.v) d -= 1;
        dogd.dmin = min(dogd.dmin, d);
    }
    dogd.refp = (a.Plus(b)).ScaledBy(0.5);
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
        glxDepthRangeOffset(6);
        glBegin(GL_QUADS);
        for(i = 0; i < SS.entity.n; i++) {
            Entity *e = &(SS.entity.elem[i]);
            if(!e->IsPoint()) continue;
            if(!(SS.GetGroup(e->group)->visible)) continue;
            if(SS.GroupsInOrder(SS.GW.activeGroup, e->group)) continue;
            if(e->forceHidden) continue;

            Vector v = e->PointGetNum();

            bool free = false;
            if(e->type == POINT_IN_3D) {
                Param *px = SS.GetParam(e->param[0]),
                      *py = SS.GetParam(e->param[1]),
                      *pz = SS.GetParam(e->param[2]);

                free = (px->free) || (py->free) || (pz->free);
            } else if(e->type == POINT_IN_2D) {
                Param *pu = SS.GetParam(e->param[0]),
                      *pv = SS.GetParam(e->param[1]);

                free = (pu->free) || (pv->free);
            }
            if(free) {
                Vector re = r.ScaledBy(2.5), de = d.ScaledBy(2.5);

                glxColor3d(0, 1.0, 1.0);
                glxVertex3v(v.Plus (re).Plus (de));
                glxVertex3v(v.Plus (re).Minus(de));
                glxVertex3v(v.Minus(re).Minus(de));
                glxVertex3v(v.Minus(re).Plus (de));
                glxColor3d(0, 0.8, 0);
            }

            glxVertex3v(v.Plus (r).Plus (d));
            glxVertex3v(v.Plus (r).Minus(d));
            glxVertex3v(v.Minus(r).Minus(d));
            glxVertex3v(v.Minus(r).Plus (d));
        }
        glEnd();
        glxDepthRangeOffset(0);
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
    DrawOrGetDistance();
}

void Entity::GenerateEdges(SEdgeList *el, bool includingConstruction) {
    if(construction && !includingConstruction) return;

    SPolyCurveList spcl;
    ZERO(&spcl);
    GeneratePolyCurves(&spcl);

    int i, j;
    for(i = 0; i < spcl.l.n; i++) {
        SPolyCurve *spc = &(spcl.l.elem[i]);

        List<Vector> lv;
        ZERO(&lv);
        spc->MakePwlInto(&lv);
        for(j = 1; j < lv.n; j++) {
            el->AddEdge(lv.elem[j-1], lv.elem[j]);
        }
        lv.Clear();
    }

    spcl.Clear();
}

double Entity::GetDistance(Point2d mp) {
    dogd.drawing = false;
    dogd.mp = mp;
    dogd.dmin = 1e12;

    DrawOrGetDistance();
    
    return dogd.dmin;
}

Vector Entity::GetReferencePos(void) {
    dogd.drawing = false;

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

    // Don't check if points are hidden; this gets called only for
    // selected or hovered points, and those should always be shown.
    if(IsNormal() && !SS.GW.showNormals) return false;

    if(!SS.GW.showWorkplanes) {
        if(IsWorkplane() && !h.isFromRequest()) {
            if(g->h.v != SS.GW.activeGroup.v) {
                // The group-associated workplanes are hidden outside
                // their group.
                return false;
            }
        }
    }

    if(forceHidden) return false;

    return true;
}

void Entity::GeneratePolyCurves(SPolyCurveList *spcl) {
    SPolyCurve spc;

    switch(type) {
        case LINE_SEGMENT: {
            Vector a = SS.GetEntity(point[0])->PointGetNum();
            Vector b = SS.GetEntity(point[1])->PointGetNum();
            spc = SPolyCurve::From(a, b);
            spcl->l.Add(&spc);
            break;
        }
        case CUBIC: {
            Vector p0 = SS.GetEntity(point[0])->PointGetNum();
            Vector p1 = SS.GetEntity(point[1])->PointGetNum();
            Vector p2 = SS.GetEntity(point[2])->PointGetNum();
            Vector p3 = SS.GetEntity(point[3])->PointGetNum();
            spc = SPolyCurve::From(p0, p1, p2, p3);
            spcl->l.Add(&spc);
            break;
        }

        case CIRCLE:
        case ARC_OF_CIRCLE: {
            Vector center = SS.GetEntity(point[0])->PointGetNum();
            Quaternion q = SS.GetEntity(normal)->NormalGetNum();
            Vector u = q.RotationU(), v = q.RotationV();
            double r = CircleGetRadiusNum();
            double thetaa, thetab, dtheta;

            if(type == CIRCLE) {
                thetaa = 0;
                thetab = 2*PI;
                dtheta = 2*PI;
            } else {
                ArcGetAngles(&thetaa, &thetab, &dtheta);
            }
            int i, n;
            if(dtheta > 3*PI/2) {
                n = 4;
            } else if(dtheta > PI) {
                n = 3;
            } else if(dtheta > PI/2) {
                n = 2;
            } else {
                n = 1;
            }
            dtheta /= n;

            for(i = 0; i < n; i++) {
                double s, c;

                c = cos(thetaa);
                s = sin(thetaa);
                // The start point of the curve, and the tangent vector at
                // that start point.
                Vector p0 = center.Plus(u.ScaledBy( r*c)).Plus(v.ScaledBy(r*s)),
                       t0 =             u.ScaledBy(-r*s). Plus(v.ScaledBy(r*c));

                thetaa += dtheta;

                c = cos(thetaa);
                s = sin(thetaa);
                Vector p2 = center.Plus(u.ScaledBy( r*c)).Plus(v.ScaledBy(r*s)),
                       t2 =             u.ScaledBy(-r*s). Plus(v.ScaledBy(r*c));

                // The control point must lie on both tangents.
                Vector p1 = Vector::AtIntersectionOfLines(p0, p0.Plus(t0),
                                                          p2, p2.Plus(t2),
                                                          NULL);

                SPolyCurve spc = SPolyCurve::From(p0, p1, p2);
                spc.weight[1] = cos(dtheta/2);
                spcl->l.Add(&spc);
            }
            break;
        }
            
        case TTF_TEXT: {
            Vector topLeft = SS.GetEntity(point[0])->PointGetNum();
            Vector botLeft = SS.GetEntity(point[1])->PointGetNum();
            Vector n = Normal()->NormalN();
            Vector v = topLeft.Minus(botLeft);
            Vector u = (v.Cross(n)).WithMagnitude(v.Magnitude());

            SS.fonts.PlotString(font.str, str.str, 0, spcl, botLeft, u, v);
            break;
        }

        default:
            // Not a problem, points and normals and such don't generate curves
            break;
    }
}

void Entity::DrawOrGetDistance(void) {
    if(!IsVisible()) return;

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
                glxDepthRangeOffset(6);
                glBegin(GL_QUADS);
                    glxVertex3v(v.Plus (r).Plus (d));
                    glxVertex3v(v.Plus (r).Minus(d));
                    glxVertex3v(v.Minus(r).Minus(d));
                    glxVertex3v(v.Minus(r).Plus (d));
                glEnd();
                glxDepthRangeOffset(0);
            } else {
                Point2d pp = SS.GW.ProjectPoint(v);
                dogd.dmin = pp.DistanceTo(dogd.mp) - 6;
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
                    tail = SS.GW.projRight.ScaledBy(w/s).Plus(
                           SS.GW.projUp.   ScaledBy(h/s)).Minus(SS.GW.offset);
                    glxDepthRangeLockToFront(true);
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
            glxDepthRangeLockToFront(false);
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

        case LINE_SEGMENT:
        case CIRCLE:
        case ARC_OF_CIRCLE:
        case CUBIC:
        case TTF_TEXT:
            // Nothing but the curve(s).
            break;

        case FACE_NORMAL_PT:
        case FACE_XPROD:
        case FACE_N_ROT_TRANS:
        case FACE_N_TRANS:
        case FACE_N_ROT_AA:
            // Do nothing; these are drawn with the triangle mesh
            break;

        default:
            oops();
    }

    // And draw the curves; generate the rational polynomial curves for
    // everything, then piecewise linearize them, and display those.
    SEdgeList sel;
    ZERO(&sel);
    GenerateEdges(&sel, true);
    int i;
    for(i = 0; i < sel.l.n; i++) {
        SEdge *se = &(sel.l.elem[i]);
        LineDrawOrGetDistance(se->a, se->b);
    }
    sel.Clear();
}

