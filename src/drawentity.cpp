//-----------------------------------------------------------------------------
// Draw a representation of an entity on-screen, in the case of curves up
// to our chord tolerance, or return the distance from the user's mouse pointer
// to the entity for selection.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

std::string Entity::DescriptionString(void) {
    if(h.isFromRequest()) {
        Request *r = SK.GetRequest(h.request());
        return r->DescriptionString();
    } else {
        Group *g = SK.GetGroup(h.group());
        return g->DescriptionString();
    }
}

void Entity::LineDrawOrGetDistance(Vector a, Vector b, bool maybeFat) {
    if(dogd.drawing) {
        // Draw lines from active group in front of those from previous
        ssglDepthRangeOffset((group.v == SS.GW.activeGroup.v) ? 4 : 3);
        // Narrow lines are drawn as lines, but fat lines must be drawn as
        // filled polygons, to get the line join style right.
        if(!maybeFat || dogd.lineWidth < 3) {
            glBegin(GL_LINES);
                ssglVertex3v(a);
                ssglVertex3v(b);
            glEnd();
        } else {
            ssglFatLine(a, b, dogd.lineWidth/SS.GW.scale);
        }

        ssglDepthRangeOffset(0);
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
        ssglColorRGB(Style::Color(Style::DATUM));
        ssglDepthRangeOffset(6);
        glBegin(GL_QUADS);
        for(i = 0; i < SK.entity.n; i++) {
            Entity *e = &(SK.entity.elem[i]);
            if(!e->IsPoint()) continue;
            if(!(SK.GetGroup(e->group)->IsVisible())) continue;
            if(e->forceHidden) continue;

            Vector v = e->PointGetNum();

            // If we're analyzing the sketch to show the degrees of freedom,
            // then we draw big colored squares over the points that are
            // free to move.
            bool free = false;
            if(e->type == POINT_IN_3D) {
                Param *px = SK.GetParam(e->param[0]),
                      *py = SK.GetParam(e->param[1]),
                      *pz = SK.GetParam(e->param[2]);

                free = (px->free) || (py->free) || (pz->free);
            } else if(e->type == POINT_IN_2D) {
                Param *pu = SK.GetParam(e->param[0]),
                      *pv = SK.GetParam(e->param[1]);

                free = (pu->free) || (pv->free);
            }
            if(free) {
                Vector re = r.ScaledBy(2.5), de = d.ScaledBy(2.5);

                ssglColorRGB(Style::Color(Style::ANALYZE));
                ssglVertex3v(v.Plus (re).Plus (de));
                ssglVertex3v(v.Plus (re).Minus(de));
                ssglVertex3v(v.Minus(re).Minus(de));
                ssglVertex3v(v.Minus(re).Plus (de));
                ssglColorRGB(Style::Color(Style::DATUM));
            }

            ssglVertex3v(v.Plus (r).Plus (d));
            ssglVertex3v(v.Plus (r).Minus(d));
            ssglVertex3v(v.Minus(r).Minus(d));
            ssglVertex3v(v.Minus(r).Plus (d));
        }
        glEnd();
        ssglDepthRangeOffset(0);
    }

    for(i = 0; i < SK.entity.n; i++) {
        Entity *e = &(SK.entity.elem[i]);
        if(e->IsPoint())
        {
            continue; // already handled
        }
        e->Draw();
    }
}

void Entity::Draw(void) {
    hStyle hs = Style::ForEntity(h);
    dogd.lineWidth = Style::Width(hs);
    ssglLineWidth((float)dogd.lineWidth);
    ssglColorRGB(Style::Color(hs));

    dogd.drawing = true;
    DrawOrGetDistance();
}

void Entity::GenerateEdges(SEdgeList *el, bool includingConstruction) {
    if(construction && !includingConstruction) return;

    SBezierList sbl = {};
    GenerateBezierCurves(&sbl);

    int i, j;
    for(i = 0; i < sbl.l.n; i++) {
        SBezier *sb = &(sbl.l.elem[i]);

        List<Vector> lv = {};
        sb->MakePwlInto(&lv);
        for(j = 1; j < lv.n; j++) {
            el->AddEdge(lv.elem[j-1], lv.elem[j], style.v);
        }
        lv.Clear();
    }

    sbl.Clear();
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
    Group *g = SK.GetGroup(group);

    if(g->h.v == Group::HGROUP_REFERENCES.v && IsNormal()) {
        // The reference normals are always shown
        return true;
    }
    if(!(g->IsVisible())) return false;

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

    if(style.v) {
        Style *s = Style::Get(style);
        if(!s->visible) return false;
    }

    if(forceHidden) return false;

    return true;
}

void Entity::CalculateNumerical(bool forExport) {
    if(IsPoint()) actPoint = PointGetNum();
    if(IsNormal()) actNormal = NormalGetNum();
    if(type == DISTANCE || type == DISTANCE_N_COPY) {
        actDistance = DistanceGetNum();
    }
    if(IsFace()) {
        actPoint  = FaceGetPointNum();
        Vector n = FaceGetNormalNum();
        actNormal = Quaternion::From(0, n.x, n.y, n.z);
    }
    if(forExport) {
        // Visibility in copied import entities follows source file
        actVisible = IsVisible();
    } else {
        // Copied entities within a file are always visible
        actVisible = true;
    }
}

bool Entity::PointIsFromReferences(void) {
    return h.request().IsFromReferences();
}

//-----------------------------------------------------------------------------
// Compute a cubic, second derivative continuous, interpolating spline. Same
// routine for periodic splines (in a loop) or open splines (with specified
// end tangents).
//-----------------------------------------------------------------------------
void Entity::ComputeInterpolatingSpline(SBezierList *sbl, bool periodic) {
    static const int MAX_N = BandedMatrix::MAX_UNKNOWNS;
    int ep = extraPoints;

    // The number of unknowns to solve for.
    int n   = periodic ? 3 + ep : ep;
    if(n >= MAX_N) oops();
    // The number of on-curve points, one more than the number of segments.
    int pts = periodic ? 4 + ep : 2 + ep;

    int i, j, a;

    // The starting and finishing control points that define our end tangents
    // (if the spline isn't periodic), and the on-curve points.
    Vector ctrl_s = Vector::From(0, 0, 0);
    Vector ctrl_f = Vector::From(0, 0, 0);
    Vector pt[MAX_N+4];
    if(periodic) {
        for(i = 0; i < ep + 3; i++) {
            pt[i] = SK.GetEntity(point[i])->PointGetNum();
        }
        pt[i++] = SK.GetEntity(point[0])->PointGetNum();
    } else {
        ctrl_s = SK.GetEntity(point[1])->PointGetNum();
        ctrl_f = SK.GetEntity(point[ep+2])->PointGetNum();
        j = 0;
        pt[j++] = SK.GetEntity(point[0])->PointGetNum();
        for(i = 2; i <= ep + 1; i++) {
            pt[j++] = SK.GetEntity(point[i])->PointGetNum();
        }
        pt[j++] = SK.GetEntity(point[ep+3])->PointGetNum();
    }

    // The unknowns that we will be solving for, a set for each coordinate.
    double Xx[MAX_N], Xy[MAX_N], Xz[MAX_N];
    // For a cubic Bezier section f(t) as t goes from 0 to 1,
    //    f' (0) = 3*(P1 - P0)
    //    f' (1) = 3*(P3 - P2)
    //    f''(0) = 6*(P0 - 2*P1 + P2)
    //    f''(1) = 6*(P3 - 2*P2 + P1)
    for(a = 0; a < 3; a++) {
        BandedMatrix bm = {};
        bm.n = n;

        for(i = 0; i < n; i++) {
            int im, it, ip;
            if(periodic) {
                im = WRAP(i - 1, n);
                it = i;
                ip = WRAP(i + 1, n);
            } else {
                im = i;
                it = i + 1;
                ip = i + 2;
            }
            // All of these are expressed in terms of a constant part, and
            // of X[i-1], X[i], and X[i+1]; so let these be the four
            // components of that vector;
            Vector4 A, B, C, D, E;
            // The on-curve interpolated point
            C = Vector4::From((pt[it]).Element(a), 0, 0, 0);
            // control point one back, C - X[i]
            B = C.Plus(Vector4::From(0, 0, -1, 0));
            // control point one forward, C + X[i]
            D = C.Plus(Vector4::From(0, 0, 1, 0));
            // control point two back
            if(i == 0 && !periodic) {
                A = Vector4::From(ctrl_s.Element(a), 0, 0, 0);
            } else {
                // pt[im] + X[i-1]
                A = Vector4::From(pt[im].Element(a), 1, 0, 0);
            }
            // control point two forward
            if(i == (n - 1) && !periodic) {
                E = Vector4::From(ctrl_f.Element(a), 0, 0, 0);
            } else {
                // pt[ip] - X[i+1]
                E = Vector4::From((pt[ip]).Element(a), 0, 0, -1);
            }
            // Write the second derivatives of each segment, dropping constant
            Vector4 fprev_pp = (C.Minus(B.ScaledBy(2))).Plus(A),
                    fnext_pp = (C.Minus(D.ScaledBy(2))).Plus(E),
                    eq       = fprev_pp.Minus(fnext_pp);

            bm.B[i] = -eq.w;
            if(periodic) {
                bm.A[i][WRAP(i-2, n)] = eq.x;
                bm.A[i][WRAP(i-1, n)] = eq.y;
                bm.A[i][i]            = eq.z;
            } else {
                // The wrapping would work, except when n = 1 and everything
                // wraps to zero...
                if(i > 0)     bm.A[i][i - 1] = eq.x;
                              bm.A[i][i]     = eq.y;
                if(i < (n-1)) bm.A[i][i + 1] = eq.z;
            }
        }
        bm.Solve();
        double *X = (a == 0) ? Xx :
                    (a == 1) ? Xy :
                               Xz;
        memcpy(X, bm.X, n*sizeof(double));
    }

    for(i = 0; i < pts - 1; i++) {
        Vector p0, p1, p2, p3;
        if(periodic) {
            p0 = pt[i];
            int iw = WRAP(i - 1, n);
            p1 = p0.Plus(Vector::From(Xx[iw], Xy[iw], Xz[iw]));
        } else if(i == 0) {
            p0 = pt[0];
            p1 = ctrl_s;
        } else {
            p0 = pt[i];
            p1 = p0.Plus(Vector::From(Xx[i-1], Xy[i-1], Xz[i-1]));
        }
        if(periodic) {
            p3 = pt[i+1];
            int iw = WRAP(i, n);
            p2 = p3.Minus(Vector::From(Xx[iw], Xy[iw], Xz[iw]));
        } else if(i == (pts - 2)) {
            p3 = pt[pts-1];
            p2 = ctrl_f;
        } else {
            p3 = pt[i+1];
            p2 = p3.Minus(Vector::From(Xx[i], Xy[i], Xz[i]));
        }
        SBezier sb = SBezier::From(p0, p1, p2, p3);
        sbl->l.Add(&sb);
    }
}

void Entity::GenerateBezierCurves(SBezierList *sbl) {
    SBezier sb;

    int i = sbl->l.n;

    switch(type) {
        case LINE_SEGMENT: {
            Vector a = SK.GetEntity(point[0])->PointGetNum();
            Vector b = SK.GetEntity(point[1])->PointGetNum();
            sb = SBezier::From(a, b);
            sb.entity = h.v;
            sbl->l.Add(&sb);
            break;
        }
        case CUBIC:
            ComputeInterpolatingSpline(sbl, false);
            break;

        case CUBIC_PERIODIC:
            ComputeInterpolatingSpline(sbl, true);
            break;

        case CIRCLE:
        case ARC_OF_CIRCLE: {
            Vector center = SK.GetEntity(point[0])->PointGetNum();
            Quaternion q = SK.GetEntity(normal)->NormalGetNum();
            Vector u = q.RotationU(), v = q.RotationV();
            double r = CircleGetRadiusNum();
            double thetaa, thetab, dtheta;

            if(r < LENGTH_EPS) {
                // If a circle or an arc gets dragged through zero radius,
                // then we just don't generate anything.
                break;
            }

            if(type == CIRCLE) {
                thetaa = 0;
                thetab = 2*PI;
                dtheta = 2*PI;
            } else {
                ArcGetAngles(&thetaa, &thetab, &dtheta);
            }
            int i, n;
            if(dtheta > (3*PI/2 + 0.01)) {
                n = 4;
            } else if(dtheta > (PI + 0.01)) {
                n = 3;
            } else if(dtheta > (PI/2 + 0.01)) {
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

                SBezier sb = SBezier::From(p0, p1, p2);
                sb.weight[1] = cos(dtheta/2);
                sbl->l.Add(&sb);
            }
            break;
        }

        case TTF_TEXT: {
            Vector topLeft = SK.GetEntity(point[0])->PointGetNum();
            Vector botLeft = SK.GetEntity(point[1])->PointGetNum();
            Vector n = Normal()->NormalN();
            Vector v = topLeft.Minus(botLeft);
            Vector u = (v.Cross(n)).WithMagnitude(v.Magnitude());

            SS.fonts.PlotString(font.c_str(), str.c_str(), 0, sbl, botLeft, u, v);
            break;
        }

        default:
            // Not a problem, points and normals and such don't generate curves
            break;
    }

    // Record our style for all of the Beziers that we just created.
    for(; i < sbl->l.n; i++) {
        sbl->l.elem[i].auxA = style.v;
    }
}

void Entity::DrawOrGetDistance(void) {
    if(!IsVisible()) return;

    Group *g = SK.GetGroup(group);

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

                ssglColorRGB(Style::Color(Style::DATUM));
                ssglDepthRangeOffset(6);
                glBegin(GL_QUADS);
                    ssglVertex3v(v.Plus (r).Plus (d));
                    ssglVertex3v(v.Plus (r).Minus(d));
                    ssglVertex3v(v.Minus(r).Minus(d));
                    ssglVertex3v(v.Minus(r).Plus (d));
                glEnd();
                ssglDepthRangeOffset(0);
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
                if(i == 0 && !SS.GW.showNormals) {
                    // When the normals are hidden, we will continue to show
                    // the coordinate axes at the bottom left corner, but
                    // not at the origin.
                    continue;
                }

                hRequest hr = h.request();
                // Always draw the x, y, and z axes in red, green, and blue;
                // brighter for the ones at the bottom left of the screen,
                // dimmer for the ones at the model origin.
                int f = (i == 0 ? 100 : 255);
                if(hr.v == Request::HREQUEST_REFERENCE_XY.v) {
                    if(dogd.drawing)
                        ssglColorRGB(RGBi(0, 0, f));
                } else if(hr.v == Request::HREQUEST_REFERENCE_YZ.v) {
                    if(dogd.drawing)
                        ssglColorRGB(RGBi(f, 0, 0));
                } else if(hr.v == Request::HREQUEST_REFERENCE_ZX.v) {
                    if(dogd.drawing)
                        ssglColorRGB(RGBi(0, f, 0));
                } else {
                    if(dogd.drawing)
                        ssglColorRGB(Style::Color(Style::NORMALS));
                    if(i > 0) break;
                }

                Quaternion q = NormalGetNum();
                Vector tail;
                if(i == 0) {
                    tail = SK.GetEntity(point[0])->PointGetNum();
                    if(dogd.drawing)
                        ssglLineWidth(1);
                } else {
                    // Draw an extra copy of the x, y, and z axes, that's
                    // always in the corner of the view and at the front.
                    // So those are always available, perhaps useful.
                    double s = SS.GW.scale;
                    double h = 60 - SS.GW.height/2;
                    double w = 60 - SS.GW.width/2;
                    tail = SS.GW.projRight.ScaledBy(w/s).Plus(
                           SS.GW.projUp.   ScaledBy(h/s)).Minus(SS.GW.offset);
                    if(dogd.drawing) {
                        ssglDepthRangeLockToFront(true);
                        ssglLineWidth(2);
                    }
                }

                Vector v = (q.RotationN()).WithMagnitude(50/SS.GW.scale);
                Vector tip = tail.Plus(v);
                LineDrawOrGetDistance(tail, tip);

                v = v.WithMagnitude(12/SS.GW.scale);
                Vector axis = q.RotationV();
                LineDrawOrGetDistance(tip,tip.Minus(v.RotatedAbout(axis, 0.6)));
                LineDrawOrGetDistance(tip,tip.Minus(v.RotatedAbout(axis,-0.6)));
            }
            if(dogd.drawing)
                ssglDepthRangeLockToFront(false);
            break;
        }

        case DISTANCE:
        case DISTANCE_N_COPY:
            // These are used only as data structures, nothing to display.
            break;

        case WORKPLANE: {
            Vector p;
            p = SK.GetEntity(point[0])->PointGetNum();

            Vector u = Normal()->NormalU();
            Vector v = Normal()->NormalV();

            double s = (min(SS.GW.width, SS.GW.height))*0.45/SS.GW.scale;

            Vector us = u.ScaledBy(s);
            Vector vs = v.ScaledBy(s);

            Vector pp = p.Plus (us).Plus (vs);
            Vector pm = p.Plus (us).Minus(vs);
            Vector mm = p.Minus(us).Minus(vs), mm2 = mm;
            Vector mp = p.Minus(us).Plus (vs);

            if(dogd.drawing) {
                ssglLineWidth(1);
                ssglColorRGB(Style::Color(Style::NORMALS));
                glEnable(GL_LINE_STIPPLE);
                glLineStipple(3, 0x1111);
            }

            if(!h.isFromRequest()) {
                mm = mm.Plus(v.ScaledBy(60/SS.GW.scale));
                mm2 = mm2.Plus(u.ScaledBy(60/SS.GW.scale));
                LineDrawOrGetDistance(mm2, mm);
            }
            LineDrawOrGetDistance(pp, pm);
            LineDrawOrGetDistance(pm, mm2);
            LineDrawOrGetDistance(mm, mp);
            LineDrawOrGetDistance(mp, pp);

            if(dogd.drawing)
                glDisable(GL_LINE_STIPPLE);

            std::string str = DescriptionString().substr(5);
            double th = DEFAULT_TEXT_HEIGHT;
            if(dogd.drawing) {
                ssglWriteText(str.c_str(), th, mm2, u, v, NULL, NULL);
            } else {
                Vector pos = mm2.Plus(u.ScaledBy(ssglStrWidth(str.c_str(), th)/2)).Plus(
                                      v.ScaledBy(ssglStrHeight(th)/2));
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
        case CUBIC_PERIODIC:
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
    SEdgeList sel = {};
    GenerateEdges(&sel, true);
    int i;
    for(i = 0; i < sel.l.n; i++) {
        SEdge *se = &(sel.l.elem[i]);
        LineDrawOrGetDistance(se->a, se->b, true);
    }
    sel.Clear();
}

