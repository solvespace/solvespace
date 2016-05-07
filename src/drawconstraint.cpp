//-----------------------------------------------------------------------------
// Given a constraint, draw a graphical and user-selectable representation
// of that constraint on-screen. We can either draw with gl, or compute the
// distance from a point (the location of the mouse pointer) to the lines
// that we would have drawn, for selection.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

void Constraint::LineDrawOrGetDistance(Vector a, Vector b) {
    if(dogd.drawing) {
        hStyle hs = GetStyle();

        if(dogd.sel) {
            dogd.sel->AddEdge(a, b, hs.v);
        } else {
            if(Style::Width(hs) >= 3.0) {
                ssglFatLine(a, b, Style::Width(hs) / SS.GW.scale);
            } else {
                glBegin(GL_LINE_STRIP);
                    ssglVertex3v(a);
                    ssglVertex3v(b);
                glEnd();
            }
        }
    } else {
        Point2d ap = SS.GW.ProjectPoint(a);
        Point2d bp = SS.GW.ProjectPoint(b);

        double d = dogd.mp.DistanceToLine(ap, bp.Minus(ap), true);
        dogd.dmin = min(dogd.dmin, d);
    }
    dogd.refp = (a.Plus(b)).ScaledBy(0.5);
}

static void LineCallback(void *fndata, Vector a, Vector b)
{
    Constraint *c = (Constraint *)fndata;
    c->LineDrawOrGetDistance(a, b);
}

std::string Constraint::Label(void) {
    std::string result;
    if(type == ANGLE) {
        if(valA == floor(valA)) {
            result = ssprintf("%.0f°", valA);
        } else {
            result = ssprintf("%.2f°", valA);
        }
    } else if(type == LENGTH_RATIO) {
        result = ssprintf("%.3f:1", valA);
    } else if(type == COMMENT) {
        result = comment;
    } else if(type == DIAMETER) {
        if(!other) {
            result = "⌀" + SS.MmToString(valA);
        } else {
            result = "R" + SS.MmToString(valA / 2);
        }
    } else {
        // valA has units of distance
        result = SS.MmToString(fabs(valA));
    }
    if(reference) {
        result += " REF";
    }
    return result;
}

void Constraint::DoLabel(Vector ref, Vector *labelPos, Vector gr, Vector gu) {
    hStyle hs = GetStyle();
    double th = Style::TextHeight(hs);

    std::string s = Label();
    double swidth  = ssglStrWidth(s, th),
           sheight = ssglStrCapHeight(th);

    // By default, the reference is from the center; but the style could
    // specify otherwise if one is present, and it could also specify a
    // rotation.
    if(type == COMMENT && disp.style.v) {
        Style *st = Style::Get(disp.style);
        // rotation first
        double rads = st->textAngle*PI/180;
        double c = cos(rads), s = sin(rads);
        Vector pr = gr, pu = gu;
        gr = pr.ScaledBy( c).Plus(pu.ScaledBy(s));
        gu = pr.ScaledBy(-s).Plus(pu.ScaledBy(c));
        // then origin
        int o = st->textOrigin;
        if(o & Style::ORIGIN_LEFT) ref = ref.Plus(gr.WithMagnitude(swidth/2));
        if(o & Style::ORIGIN_RIGHT) ref = ref.Minus(gr.WithMagnitude(swidth/2));
        if(o & Style::ORIGIN_BOT) ref = ref.Plus(gu.WithMagnitude(sheight/2));
        if(o & Style::ORIGIN_TOP) ref = ref.Minus(gu.WithMagnitude(sheight/2));
    }

    if(labelPos) {
        // labelPos is from the top left corner (for the text box used to
        // edit things), but ref is from the center.
        *labelPos = ref.Minus(gr.WithMagnitude(swidth/2)).Minus(
                              gu.WithMagnitude(sheight/2));
    }


    if(dogd.drawing) {
        ssglWriteTextRefCenter(s, th, ref, gr, gu, LineCallback, this);
    } else {
        double l = swidth/2 - sheight/2;
        l = max(l, 5/SS.GW.scale);
        Point2d a = SS.GW.ProjectPoint(ref.Minus(gr.WithMagnitude(l)));
        Point2d b = SS.GW.ProjectPoint(ref.Plus (gr.WithMagnitude(l)));
        double d = dogd.mp.DistanceToLine(a, b.Minus(a), true);

        dogd.dmin = min(dogd.dmin, d - (th / 2));
        dogd.refp = ref;
    }
}

void Constraint::StippledLine(Vector a, Vector b) {
    glLineStipple(4, 0x5555);
    glEnable(GL_LINE_STIPPLE);
    LineDrawOrGetDistance(a, b);
    glDisable(GL_LINE_STIPPLE);
}

void Constraint::DoProjectedPoint(Vector *r) {
    Vector p = r->ProjectInto(workplane);
    StippledLine(p, *r);
    *r = p;
}

//-----------------------------------------------------------------------------
// There is a rectangular box, aligned to our display axes (projRight, projUp)
// centered at ref. This is where a dimension label will be drawn. We want to
// draw a line from A to B. If that line would intersect the label box, then
// trim the line to leave a gap for it, and return zero. If not, then extend
// the line to almost meet the box, and return either positive or negative,
// depending whether that extension was from A or from B.
//-----------------------------------------------------------------------------
int Constraint::DoLineTrimmedAgainstBox(Vector ref, Vector a, Vector b, bool extend) {
    hStyle hs = GetStyle();
    double th = Style::TextHeight(hs);
    Vector gu = SS.GW.projUp.WithMagnitude(1),
           gr = SS.GW.projRight.WithMagnitude(1);

    double pixels = 1.0 / SS.GW.scale;
    std::string s = Label();
    double swidth  = ssglStrWidth(s, th) + 4*pixels,
           sheight = ssglStrCapHeight(th) + 8*pixels;

    return DoLineTrimmedAgainstBox(ref, a, b, extend, gr, gu, swidth, sheight);
}

int Constraint::DoLineTrimmedAgainstBox(Vector ref, Vector a, Vector b, bool extend,
                                        Vector gr, Vector gu, double swidth, double sheight) {
    struct {
        Vector n;
        double d;
    } planes[4];
    // reference pos is the center of box occupied by text; build a rectangle
    // around that, aligned to axes gr and gu, from four planes will all four
    // normals pointing inward
    planes[0].n = gu.ScaledBy(-1); planes[0].d = -(gu.Dot(ref) + sheight/2);
    planes[1].n = gu;              planes[1].d =   gu.Dot(ref) - sheight/2;
    planes[2].n = gr;              planes[2].d =   gr.Dot(ref) - swidth/2;
    planes[3].n = gr.ScaledBy(-1); planes[3].d = -(gr.Dot(ref) + swidth/2);

    double tmin = VERY_POSITIVE, tmax = VERY_NEGATIVE;
    Vector dl = b.Minus(a);

    for(int i = 0; i < 4; i++) {
        bool parallel;
        Vector p = Vector::AtIntersectionOfPlaneAndLine(
                                planes[i].n, planes[i].d,
                                a, b, &parallel);
        if(parallel) continue;

        int j;
        for(j = 0; j < 4; j++) {
            double d = (planes[j].n).Dot(p) - planes[j].d;
            if(d < -LENGTH_EPS) break;
        }
        if(j < 4) continue;

        double t = (p.Minus(a)).DivPivoting(dl);
        tmin = min(t, tmin);
        tmax = max(t, tmax);
    }

    // Both in range; so there's pieces of the line on both sides of the label box.
    if(tmin >= 0.0 && tmin <= 1.0 && tmax >= 0.0 && tmax <= 1.0) {
        LineDrawOrGetDistance(a, a.Plus(dl.ScaledBy(tmin)));
        LineDrawOrGetDistance(a.Plus(dl.ScaledBy(tmax)), b);
        return 0;
    }

    // Only one intersection in range; so the box is right on top of the endpoint
    if(tmin >= 0.0 && tmin <= 1.0) {
        LineDrawOrGetDistance(a, a.Plus(dl.ScaledBy(tmin)));
        return 0;
    }

    // Likewise.
    if(tmax >= 0.0 && tmax <= 1.0) {
        LineDrawOrGetDistance(a.Plus(dl.ScaledBy(tmax)), b);
        return 0;
    }

    // The line does not intersect the label; so the line should get
    // extended to just barely meet the label.
    // 0 means the label lies within the line, negative means it's outside
    // and closer to b, positive means outside and closer to a.
    if(tmax < 0.0) {
        if(extend) a = a.Plus(dl.ScaledBy(tmax));
        LineDrawOrGetDistance(a, b);
        return 1;
    }

    if(tmin > 1.0) {
        if(extend) b = a.Plus(dl.ScaledBy(tmin));
        LineDrawOrGetDistance(a, b);
        return -1;
    }

    // This will happen if the entire line lies within the box.
    return 0;
}

void Constraint::DoArrow(Vector p, Vector dir, Vector n, double width, double angle, double da) {
    dir = dir.WithMagnitude(width / cos(angle));
    dir = dir.RotatedAbout(n, da);
    LineDrawOrGetDistance(p, p.Plus(dir.RotatedAbout(n,  angle)));
    LineDrawOrGetDistance(p, p.Plus(dir.RotatedAbout(n, -angle)));
}

//-----------------------------------------------------------------------------
// Draw a line with arrows on both ends, and possibly a gap in the middle for
// the dimension. We will use these for most length dimensions. The length
// being dimensioned is from A to B; but those points get extended perpendicular
// to the line AB, until the line between the extensions crosses ref (the
// center of the label).
//-----------------------------------------------------------------------------
void Constraint::DoLineWithArrows(Vector ref, Vector a, Vector b,
                                  bool onlyOneExt)
{
    double pixels = 1.0 / SS.GW.scale;

    Vector ab   = a.Minus(b);
    Vector ar   = a.Minus(ref);
    // Normal to a plane containing the line and the label origin.
    Vector n    = ab.Cross(ar);
    // Within that plane, and normal to the line AB; so that's our extension
    // line.
    Vector out  = ab.Cross(n).WithMagnitude(1);
    out = out.ScaledBy(-out.Dot(ar));

    Vector ae = a.Plus(out), be = b.Plus(out);

    // Extension lines extend 10 pixels beyond where the arrows get
    // drawn (which is at the same offset perpendicular from AB as the
    // label).
    LineDrawOrGetDistance(a, ae.Plus(out.WithMagnitude(10*pixels)));
    if(!onlyOneExt) {
        LineDrawOrGetDistance(b, be.Plus(out.WithMagnitude(10*pixels)));
    }

    int within = DoLineTrimmedAgainstBox(ref, ae, be);

    // Arrow heads are 13 pixels long, with an 18 degree half-angle.
    double theta = 18*PI/180;
    Vector arrow = (be.Minus(ae)).WithMagnitude(13*pixels);

    if(within != 0) {
        arrow = arrow.ScaledBy(-1);
        Vector seg = (be.Minus(ae)).WithMagnitude(18*pixels);
        if(within < 0) LineDrawOrGetDistance(ae, ae.Minus(seg));
        if(within > 0) LineDrawOrGetDistance(be, be.Plus(seg));
    }

    DoArrow(ae, arrow, n, 13.0 * pixels, theta, 0.0);
    DoArrow(be, arrow.Negated(), n, 13.0 * pixels, theta, 0.0);
}

void Constraint::DoEqualLenTicks(Vector a, Vector b, Vector gn) {
    Vector m = (a.ScaledBy(1.0/3)).Plus(b.ScaledBy(2.0/3));
    Vector ab = a.Minus(b);
    Vector n = (gn.Cross(ab)).WithMagnitude(10/SS.GW.scale);

    LineDrawOrGetDistance(m.Minus(n), m.Plus(n));
}

void Constraint::DoEqualRadiusTicks(hEntity he) {
    Entity *circ = SK.GetEntity(he);

    Vector center = SK.GetEntity(circ->point[0])->PointGetNum();
    double r = circ->CircleGetRadiusNum();
    Quaternion q = circ->Normal()->NormalGetNum();
    Vector u = q.RotationU(), v = q.RotationV();

    double theta;
    if(circ->type == Entity::CIRCLE) {
        theta = PI/2;
    } else if(circ->type == Entity::ARC_OF_CIRCLE) {
        double thetaa, thetab, dtheta;
        circ->ArcGetAngles(&thetaa, &thetab, &dtheta);
        theta = thetaa + dtheta/2;
    } else oops();

    Vector d = u.ScaledBy(cos(theta)).Plus(v.ScaledBy(sin(theta)));
    d = d.ScaledBy(r);
    Vector p = center.Plus(d);
    Vector tick = d.WithMagnitude(10/SS.GW.scale);
    LineDrawOrGetDistance(p.Plus(tick), p.Minus(tick));
}

void Constraint::DoArcForAngle(Vector a0, Vector da, Vector b0, Vector db,
                                   Vector offset, Vector *ref, bool trim)
{
    Vector gr = SS.GW.projRight.ScaledBy(1.0);
    Vector gu = SS.GW.projUp.ScaledBy(1.0);

    if(workplane.v != Entity::FREE_IN_3D.v) {
        a0 = a0.ProjectInto(workplane);
        b0 = b0.ProjectInto(workplane);
        da = da.ProjectVectorInto(workplane);
        db = db.ProjectVectorInto(workplane);
    }

    double px = 1.0 / SS.GW.scale;
    Vector a1 = a0.Plus(da);
    Vector b1 = b0.Plus(db);

    bool skew;
    Vector pi = Vector::AtIntersectionOfLines(a0, a0.Plus(da),
                                              b0, b0.Plus(db), &skew);

    if(!skew) {
        *ref = pi.Plus(offset);
        // We draw in a coordinate system centered at the intersection point.
        // One basis vector is da, and the other is normal to da and in
        // the plane that contains our lines (so normal to its normal).
        da = da.WithMagnitude(1);
        db = db.WithMagnitude(1);

        Vector norm = da.Cross(db);

        Vector dna = norm.Cross(da).WithMagnitude(1.0);
        Vector dnb = norm.Cross(db).WithMagnitude(1.0);

        // da and db magnitudes are 1.0
        double thetaf = acos(da.Dot(db));

        // Calculate median
        Vector m = da.ScaledBy(cos(thetaf/2)).Plus(
                   dna.ScaledBy(sin(thetaf/2)));
        Vector rm = (*ref).Minus(pi);

        // Test which side we have to place an arc
        if(m.Dot(rm) < 0) {
            da = da.ScaledBy(-1); dna = dna.ScaledBy(-1);
            db = db.ScaledBy(-1); dnb = dnb.ScaledBy(-1);
        }

        double rda = rm.Dot(da), rdna = rm.Dot(dna);

        // Introduce minimal arc radius in pixels
        double r = max(sqrt(rda*rda + rdna*rdna), 15.0 * px);

        hStyle hs = disp.style;
        if(hs.v == 0) hs.v = Style::CONSTRAINT;
        double th = Style::TextHeight(hs);
        double swidth  = ssglStrWidth(Label(), th) + 4.0 * px;
        double sheight = ssglStrCapHeight(th) + 8.0 * px;
        double textR = sqrt(swidth * swidth + sheight * sheight) / 2.0;
        *ref = pi.Plus(rm.WithMagnitude(std::max(rm.Magnitude(), 15 * px + textR)));

        // Arrow points
        Vector apa = da. ScaledBy(r).Plus(pi);
        Vector apb = da. ScaledBy(r*cos(thetaf)).Plus(
                     dna.ScaledBy(r*sin(thetaf))).Plus(pi);

        double arrowW = 13 * px;
        double arrowA = 18.0 * PI / 180.0;
        bool arrowVisible = apb.Minus(apa).Magnitude() > 2.5 * arrowW;
        // Arrow reversing indicator
        bool arrowRev = false;

        // The minimal extension length in angular representation
        double extAngle = 18 * px / r;

        // Arc additional angle
        double addAngle = 0.0;
        // Arc start angle
        double startAngle = 0.0;

        // Arc extension to db.
        // We have just enlarge angle value.
        if(HasLabel() && rm.Dot(dnb) > 0.0) {
            // rm direction projected to plane with u = da, v = dna
            Vector rmp = da.ScaledBy(rda).Plus(dna.ScaledBy(rdna)).WithMagnitude(1.0);
            // rmp and db magnitudes are 1.0
            addAngle = std::max(acos(rmp.Dot(db)), extAngle);

            if(arrowVisible) {
                startAngle = -extAngle;
                addAngle += extAngle;
                arrowRev = true;
            }
        }

        // Arc extension to da.
        // We are enlarge angle value and rewrite basis to align along rm projection.
        if(HasLabel() && rm.Dot(dna) < 0.0) {
            // rm direction projected to plane with u = da, v = dna
            Vector rmp = da.ScaledBy(rda).Plus(dna.ScaledBy(rdna)).WithMagnitude(1.0);
            // rmp and da magnitudes are 1.0
            startAngle = -std::max(acos(rmp.Dot(da)), extAngle);
            addAngle = -startAngle;

            if(arrowVisible) {
                addAngle += extAngle;
                arrowRev = true;
            }
        }

        Vector prev;
        int n = 30;
        for(int i = 0; i <= n; i++) {
            double theta = startAngle + (i*(thetaf + addAngle))/n;
            Vector p =  da.ScaledBy(r*cos(theta)).Plus(
                       dna.ScaledBy(r*sin(theta))).Plus(pi);
            if(i > 0) {
                if(trim) {
                    DoLineTrimmedAgainstBox(*ref, prev, p, false, gr, gu, swidth, sheight);
                } else {
                    LineDrawOrGetDistance(prev, p);
                }
            }
            prev = p;
        }

        DoLineExtend(a0, a1, apa, 5.0 * px);
        DoLineExtend(b0, b1, apb, 5.0 * px);

        // Draw arrows only when we have enough space.
        if(arrowVisible) {
            double angleCorr = arrowW / (2.0 * r);
            if(arrowRev) {
                dna = dna.ScaledBy(-1.0);
                angleCorr = -angleCorr;
            }
            DoArrow(apa, dna, norm, arrowW, arrowA, angleCorr);
            DoArrow(apb, dna, norm, arrowW, arrowA, thetaf + PI - angleCorr);
        }
    } else {
        // The lines are skew; no wonderful way to illustrate that.
        *ref = a0.Plus(b0);
        *ref = (*ref).ScaledBy(0.5).Plus(disp.offset);
        gu = gu.WithMagnitude(1);
        Vector trans =
            (*ref).Plus(gu.ScaledBy(-1.5*ssglStrCapHeight(Style::DefaultTextHeight())));
        ssglWriteTextRefCenter("angle between skew lines", Style::DefaultTextHeight(),
            trans, gr.WithMagnitude(px), gu.WithMagnitude(px), LineCallback, this);
    }
}

bool Constraint::IsVisible() const {
    if(!SS.GW.showConstraints) return false;
    Group *g = SK.GetGroup(group);
    // If the group is hidden, then the constraints are hidden and not
    // able to be selected.
    if(!(g->visible)) return false;
    // And likewise if the group is not the active group; except for comments
    // with an assigned style.
    if(g->h.v != SS.GW.activeGroup.v && !(type == COMMENT && disp.style.v)) {
        return false;
    }
    if(disp.style.v) {
        Style *s = Style::Get(disp.style);
        if(!s->visible) return false;
    }
    return true;
}

bool Constraint::DoLineExtend(Vector p0, Vector p1, Vector pt, double salient) {
    Vector dir = p1.Minus(p0);
    double k = dir.Dot(pt.Minus(p0)) / dir.Dot(dir);
    Vector ptOnLine = p0.Plus(dir.ScaledBy(k));

    // Draw projection line.
    LineDrawOrGetDistance(pt, ptOnLine);

    // Calculate salient direction.
    Vector sd = dir.WithMagnitude(1.0).ScaledBy(salient);

    Vector from;
    Vector to;

    if(k < 0.0) {
        from = p0;
        to = ptOnLine.Minus(sd);
    } else if(k > 1.0) {
        from = p1;
        to = ptOnLine.Plus(sd);
    } else {
        return false;
    }

    // Draw extension line.
    LineDrawOrGetDistance(from, to);
    return true;
}

void Constraint::DrawOrGetDistance(Vector *labelPos) {

    if(!IsVisible()) return;

    // Unit vectors that describe our current view of the scene. One pixel
    // long, not one actual unit.
    Vector gr = SS.GW.projRight.ScaledBy(1/SS.GW.scale);
    Vector gu = SS.GW.projUp.ScaledBy(1/SS.GW.scale);
    Vector gn = (gr.Cross(gu)).WithMagnitude(1/SS.GW.scale);

    switch(type) {
        case PT_PT_DISTANCE: {
            Vector ap = SK.GetEntity(ptA)->PointGetNum();
            Vector bp = SK.GetEntity(ptB)->PointGetNum();

            if(workplane.v != Entity::FREE_IN_3D.v) {
                DoProjectedPoint(&ap);
                DoProjectedPoint(&bp);
            }

            Vector ref = ((ap.Plus(bp)).ScaledBy(0.5)).Plus(disp.offset);

            DoLineWithArrows(ref, ap, bp, false);
            DoLabel(ref, labelPos, gr, gu);
            break;
        }

        case PROJ_PT_DISTANCE: {
            Vector ap = SK.GetEntity(ptA)->PointGetNum(),
                   bp = SK.GetEntity(ptB)->PointGetNum(),
                   dp = (bp.Minus(ap)),
                   pp = SK.GetEntity(entityA)->VectorGetNum();

            Vector ref = ((ap.Plus(bp)).ScaledBy(0.5)).Plus(disp.offset);

            pp = pp.WithMagnitude(1);
            double d = dp.Dot(pp);
            Vector bpp = ap.Plus(pp.ScaledBy(d));
            StippledLine(ap, bpp);
            StippledLine(bp, bpp);

            DoLineWithArrows(ref, ap, bpp, false);
            DoLabel(ref, labelPos, gr, gu);
            break;
        }

        case PT_FACE_DISTANCE:
        case PT_PLANE_DISTANCE: {
            Vector pt = SK.GetEntity(ptA)->PointGetNum();
            Entity *enta = SK.GetEntity(entityA);
            Vector n, p;
            if(type == PT_PLANE_DISTANCE) {
                n = enta->Normal()->NormalN();
                p = enta->WorkplaneGetOffset();
            } else {
                n = enta->FaceGetNormalNum();
                p = enta->FaceGetPointNum();
            }

            double d = (p.Minus(pt)).Dot(n);
            Vector closest = pt.Plus(n.WithMagnitude(d));

            Vector ref = ((closest.Plus(pt)).ScaledBy(0.5)).Plus(disp.offset);

            if(!pt.Equals(closest)) {
                DoLineWithArrows(ref, pt, closest, true);
            }

            DoLabel(ref, labelPos, gr, gu);
            break;
        }

        case PT_LINE_DISTANCE: {
            Vector pt = SK.GetEntity(ptA)->PointGetNum();
            Entity *line = SK.GetEntity(entityA);
            Vector lA = SK.GetEntity(line->point[0])->PointGetNum();
            Vector lB = SK.GetEntity(line->point[1])->PointGetNum();
            Vector dl = lB.Minus(lA);

            if(workplane.v != Entity::FREE_IN_3D.v) {
                lA = lA.ProjectInto(workplane);
                lB = lB.ProjectInto(workplane);
                DoProjectedPoint(&pt);
            }

            // Find the closest point on the line
            Vector closest = pt.ClosestPointOnLine(lA, dl);

            Vector ref = ((closest.Plus(pt)).ScaledBy(0.5)).Plus(disp.offset);
            DoLabel(ref, labelPos, gr, gu);

            if(!pt.Equals(closest)) {
                DoLineWithArrows(ref, pt, closest, true);

                // Extensions to line
                double pixels = 1.0 / SS.GW.scale;
                Vector refClosest = ref.ClosestPointOnLine(lA, dl);
                double ddl = dl.Dot(dl);
                if(fabs(ddl) > LENGTH_EPS * LENGTH_EPS) {
                    double t = refClosest.Minus(lA).Dot(dl) / ddl;
                    if(t < 0.0) {
                        LineDrawOrGetDistance(refClosest.Minus(dl.WithMagnitude(10.0 * pixels)), lA);
                    } else if(t > 1.0) {
                        LineDrawOrGetDistance(refClosest.Plus(dl.WithMagnitude(10.0 * pixels)), lB);
                    }
                }
            }

            if(workplane.v != Entity::FREE_IN_3D.v) {
                // Draw the projection marker from the closest point on the
                // projected line to the projected point on the real line.
                Vector lAB = (lA.Minus(lB));
                double t = (lA.Minus(closest)).DivPivoting(lAB);

                Vector lA = SK.GetEntity(line->point[0])->PointGetNum();
                Vector lB = SK.GetEntity(line->point[1])->PointGetNum();

                Vector c2 = (lA.ScaledBy(1-t)).Plus(lB.ScaledBy(t));
                DoProjectedPoint(&c2);
            }
            break;
        }

        case DIAMETER: {
            Entity *circle = SK.GetEntity(entityA);
            Vector center = SK.GetEntity(circle->point[0])->PointGetNum();
            Quaternion q = SK.GetEntity(circle->normal)->NormalGetNum();
            Vector n = q.RotationN().WithMagnitude(1);
            double r = circle->CircleGetRadiusNum();

            Vector ref = center.Plus(disp.offset);
            // Force the label into the same plane as the circle.
            ref = ref.Minus(n.ScaledBy(n.Dot(ref) - n.Dot(center)));

            Vector mark = ref.Minus(center);
            mark = mark.WithMagnitude(mark.Magnitude()-r);
            DoLineTrimmedAgainstBox(ref, ref, ref.Minus(mark));

            Vector topLeft;
            DoLabel(ref, &topLeft, gr, gu);
            if(labelPos) *labelPos = topLeft;
            break;
        }

        case POINTS_COINCIDENT: {
            if(!dogd.drawing) {
                for(int i = 0; i < 2; i++) {
                    Vector p = SK.GetEntity(i == 0 ? ptA : ptB)-> PointGetNum();
                    Point2d pp = SS.GW.ProjectPoint(p);
                    // The point is selected within a radius of 7, from the
                    // same center; so if the point is visible, then this
                    // constraint cannot be selected. But that's okay.
                    dogd.dmin = min(dogd.dmin, pp.DistanceTo(dogd.mp) - 3);
                    dogd.refp = p;
                }
                break;
            }

            if(dogd.drawing) {
                // Let's adjust the color of this constraint to have the same
                // rough luma as the point color, so that the constraint does not
                // stand out in an ugly way.
                RgbaColor cd = Style::Color(Style::DATUM),
                         cc = Style::Color(Style::CONSTRAINT);
                // convert from 8-bit color to a vector
                Vector vd = Vector::From(cd.redF(), cd.greenF(), cd.blueF()),
                       vc = Vector::From(cc.redF(), cc.greenF(), cc.blueF());
                // and scale the constraint color to have the same magnitude as
                // the datum color, maybe a bit dimmer
                vc = vc.WithMagnitude(vd.Magnitude()*0.9);
                // and set the color to that.
                ssglColorRGB(RGBf(vc.x, vc.y, vc.z));

                for(int a = 0; a < 2; a++) {
                    Vector r = SS.GW.projRight.ScaledBy((a+1)/SS.GW.scale);
                    Vector d = SS.GW.projUp.ScaledBy((2-a)/SS.GW.scale);
                    for(int i = 0; i < 2; i++) {
                        Vector p = SK.GetEntity(i == 0 ? ptA : ptB)-> PointGetNum();
                        glBegin(GL_QUADS);
                            ssglVertex3v(p.Plus (r).Plus (d));
                            ssglVertex3v(p.Plus (r).Minus(d));
                            ssglVertex3v(p.Minus(r).Minus(d));
                            ssglVertex3v(p.Minus(r).Plus (d));
                        glEnd();
                    }

                }
            }
            break;
        }

        case PT_ON_CIRCLE:
        case PT_ON_LINE:
        case PT_ON_FACE:
        case PT_IN_PLANE: {
            double s = 8/SS.GW.scale;
            Vector p = SK.GetEntity(ptA)->PointGetNum();
            Vector r, d;
            if(type == PT_ON_FACE) {
                Vector n = SK.GetEntity(entityA)->FaceGetNormalNum();
                r = n.Normal(0);
                d = n.Normal(1);
            } else if(type == PT_IN_PLANE) {
                EntityBase *n = SK.GetEntity(entityA)->Normal();
                r = n->NormalU();
                d = n->NormalV();
            } else {
                r = gr;
                d = gu;
                s *= (6.0/8); // draw these a little smaller
            }
            r = r.WithMagnitude(s); d = d.WithMagnitude(s);
            LineDrawOrGetDistance(p.Plus (r).Plus (d), p.Plus (r).Minus(d));
            LineDrawOrGetDistance(p.Plus (r).Minus(d), p.Minus(r).Minus(d));
            LineDrawOrGetDistance(p.Minus(r).Minus(d), p.Minus(r).Plus (d));
            LineDrawOrGetDistance(p.Minus(r).Plus (d), p.Plus (r).Plus (d));
            break;
        }

        case WHERE_DRAGGED: {
            Vector p = SK.GetEntity(ptA)->PointGetNum(),
                   u = p.Plus(gu.WithMagnitude(8/SS.GW.scale)).Plus(
                              gr.WithMagnitude(8/SS.GW.scale)),
                   uu = u.Minus(gu.WithMagnitude(5/SS.GW.scale)),
                   ur = u.Minus(gr.WithMagnitude(5/SS.GW.scale));
            // Draw four little crop marks, uniformly spaced (by ninety
            // degree rotations) around the point.
            int i;
            for(i = 0; i < 4; i++) {
                LineDrawOrGetDistance(u, uu);
                LineDrawOrGetDistance(u, ur);
                u = u.RotatedAbout(p, gn, PI/2);
                ur = ur.RotatedAbout(p, gn, PI/2);
                uu = uu.RotatedAbout(p, gn, PI/2);
            }
            break;
        }

        case SAME_ORIENTATION: {
            for(int i = 0; i < 2; i++) {
                Entity *e = SK.GetEntity(i == 0 ? entityA : entityB);
                Quaternion q = e->NormalGetNum();
                Vector n = q.RotationN().WithMagnitude(25/SS.GW.scale);
                Vector u = q.RotationU().WithMagnitude(6/SS.GW.scale);
                Vector p = SK.GetEntity(e->point[0])->PointGetNum();
                p = p.Plus(n.WithMagnitude(10/SS.GW.scale));

                LineDrawOrGetDistance(p.Plus(u), p.Minus(u).Plus(n));
                LineDrawOrGetDistance(p.Minus(u), p.Plus(u).Plus(n));
            }
            break;
        }

        case EQUAL_ANGLE: {
            Vector ref;
            Entity *a = SK.GetEntity(entityA);
            Entity *b = SK.GetEntity(entityB);
            Entity *c = SK.GetEntity(entityC);
            Entity *d = SK.GetEntity(entityD);

            Vector a0 = a->VectorGetStartPoint();
            Vector b0 = b->VectorGetStartPoint();
            Vector c0 = c->VectorGetStartPoint();
            Vector d0 = d->VectorGetStartPoint();
            Vector da = a->VectorGetNum();
            Vector db = b->VectorGetNum();
            Vector dc = c->VectorGetNum();
            Vector dd = d->VectorGetNum();

            if(other) {
                a0 = a0.Plus(da);
                da = da.ScaledBy(-1);
            }

            DoArcForAngle(a0, da, b0, db,
                da.WithMagnitude(40/SS.GW.scale), &ref, /*trim=*/false);
            DoArcForAngle(c0, dc, d0, dd,
                dc.WithMagnitude(40/SS.GW.scale), &ref, /*trim=*/false);

            break;
        }

        case ANGLE: {
            Entity *a = SK.GetEntity(entityA);
            Entity *b = SK.GetEntity(entityB);

            Vector a0 = a->VectorGetStartPoint();
            Vector b0 = b->VectorGetStartPoint();
            Vector da = a->VectorGetNum();
            Vector db = b->VectorGetNum();
            if(other) {
                a0 = a0.Plus(da);
                da = da.ScaledBy(-1);
            }

            Vector ref;
            DoArcForAngle(a0, da, b0, db, disp.offset, &ref, /*trim=*/true);
            DoLabel(ref, labelPos, gr, gu);
            break;
        }

        case PERPENDICULAR: {
            Vector u = Vector::From(0, 0, 0), v = Vector::From(0, 0, 0);
            Vector rn, ru;
            if(workplane.v == Entity::FREE_IN_3D.v) {
                rn = gn;
                ru = gu;
            } else {
                EntityBase *normal = SK.GetEntity(workplane)->Normal();
                rn = normal->NormalN();
                ru = normal->NormalV(); // ru meaning r_up, not u/v
            }

            for(int i = 0; i < 2; i++) {
                Entity *e = SK.GetEntity(i == 0 ? entityA : entityB);

                if(i == 0) {
                    // Calculate orientation of perpendicular sign only
                    // once, so that it's the same both times it's drawn
                    u = e->VectorGetNum();
                    u = u.WithMagnitude(16/SS.GW.scale);
                    v = (rn.Cross(u)).WithMagnitude(16/SS.GW.scale);
                    // a bit of bias to stop it from flickering between the
                    // two possibilities
                    if(fabs(u.Dot(ru)) < fabs(v.Dot(ru)) + LENGTH_EPS) {
                        swap(u, v);
                    }
                    if(u.Dot(ru) < 0) u = u.ScaledBy(-1);
                }

                Vector p = e->VectorGetRefPoint();
                Vector s = p.Plus(u).Plus(v);
                LineDrawOrGetDistance(s, s.Plus(v));

                Vector m = s.Plus(v.ScaledBy(0.5));
                LineDrawOrGetDistance(m, m.Plus(u));
            }
            break;
        }

        case CURVE_CURVE_TANGENT:
        case CUBIC_LINE_TANGENT:
        case ARC_LINE_TANGENT: {
            Vector textAt, u, v;

            if(type == ARC_LINE_TANGENT) {
                Entity *arc = SK.GetEntity(entityA);
                Entity *norm = SK.GetEntity(arc->normal);
                Vector c = SK.GetEntity(arc->point[0])->PointGetNum();
                Vector p =
                    SK.GetEntity(arc->point[other ? 2 : 1])->PointGetNum();
                Vector r = p.Minus(c);
                textAt = p.Plus(r.WithMagnitude(14/SS.GW.scale));
                u = norm->NormalU();
                v = norm->NormalV();
            } else if(type == CUBIC_LINE_TANGENT) {
                Vector n;
                if(workplane.v == Entity::FREE_IN_3D.v) {
                    u = gr;
                    v = gu;
                    n = gn;
                } else {
                    EntityBase *wn = SK.GetEntity(workplane)->Normal();
                    u = wn->NormalU();
                    v = wn->NormalV();
                    n = wn->NormalN();
                }

                Entity *cubic = SK.GetEntity(entityA);
                Vector p = other ? cubic->CubicGetFinishNum() :
                                   cubic->CubicGetStartNum();
                Vector dir = SK.GetEntity(entityB)->VectorGetNum();
                Vector out = n.Cross(dir);
                textAt = p.Plus(out.WithMagnitude(14/SS.GW.scale));
            } else {
                Vector n, dir;
                EntityBase *wn = SK.GetEntity(workplane)->Normal();
                u = wn->NormalU();
                v = wn->NormalV();
                n = wn->NormalN();
                EntityBase *eA = SK.GetEntity(entityA);
                // Big pain; we have to get a vector tangent to the curve
                // at the shared point, which could be from either a cubic
                // or an arc.
                if(other) {
                    textAt = eA->EndpointFinish();
                    if(eA->type == Entity::CUBIC) {
                        dir = eA->CubicGetFinishTangentNum();
                    } else {
                        dir = SK.GetEntity(eA->point[0])->PointGetNum().Minus(
                              SK.GetEntity(eA->point[2])->PointGetNum());
                        dir = n.Cross(dir);
                    }
                } else {
                    textAt = eA->EndpointStart();
                    if(eA->type == Entity::CUBIC) {
                        dir = eA->CubicGetStartTangentNum();
                    } else {
                        dir = SK.GetEntity(eA->point[0])->PointGetNum().Minus(
                              SK.GetEntity(eA->point[1])->PointGetNum());
                        dir = n.Cross(dir);
                    }
                }
                dir = n.Cross(dir);
                textAt = textAt.Plus(dir.WithMagnitude(14/SS.GW.scale));
            }

            if(dogd.drawing) {
                ssglWriteTextRefCenter("T", Style::DefaultTextHeight(),
                    textAt, u, v, LineCallback, this);
            } else {
                dogd.refp = textAt;
                Point2d ref = SS.GW.ProjectPoint(dogd.refp);
                dogd.dmin = min(dogd.dmin, ref.DistanceTo(dogd.mp)-10);
            }
            break;
        }

        case PARALLEL: {
            for(int i = 0; i < 2; i++) {
                Entity *e = SK.GetEntity(i == 0 ? entityA : entityB);
                Vector n = e->VectorGetNum();
                n = n.WithMagnitude(25/SS.GW.scale);
                Vector u = (gn.Cross(n)).WithMagnitude(4/SS.GW.scale);
                Vector p = e->VectorGetRefPoint();

                LineDrawOrGetDistance(p.Plus(u), p.Plus(u).Plus(n));
                LineDrawOrGetDistance(p.Minus(u), p.Minus(u).Plus(n));
            }
            break;
        }

        case EQUAL_RADIUS: {
            for(int i = 0; i < 2; i++) {
                DoEqualRadiusTicks(i == 0 ? entityA : entityB);
            }
            break;
        }

        case EQUAL_LINE_ARC_LEN: {
            Entity *line = SK.GetEntity(entityA);
            DoEqualLenTicks(
                SK.GetEntity(line->point[0])->PointGetNum(),
                SK.GetEntity(line->point[1])->PointGetNum(),
                gn);

            DoEqualRadiusTicks(entityB);
            break;
        }

        case LENGTH_RATIO:
        case LENGTH_DIFFERENCE:
        case EQUAL_LENGTH_LINES: {
            Vector a, b = Vector::From(0, 0, 0);
            for(int i = 0; i < 2; i++) {
                Entity *e = SK.GetEntity(i == 0 ? entityA : entityB);
                a = SK.GetEntity(e->point[0])->PointGetNum();
                b = SK.GetEntity(e->point[1])->PointGetNum();

                if(workplane.v != Entity::FREE_IN_3D.v) {
                    DoProjectedPoint(&a);
                    DoProjectedPoint(&b);
                }

                DoEqualLenTicks(a, b, gn);
            }
            if((type == LENGTH_RATIO) || (type == LENGTH_DIFFERENCE)) {
                Vector ref = ((a.Plus(b)).ScaledBy(0.5)).Plus(disp.offset);
                DoLabel(ref, labelPos, gr, gu);
            }
            break;
        }

        case EQ_LEN_PT_LINE_D: {
            Entity *forLen = SK.GetEntity(entityA);
            Vector a = SK.GetEntity(forLen->point[0])->PointGetNum(),
                   b = SK.GetEntity(forLen->point[1])->PointGetNum();
            if(workplane.v != Entity::FREE_IN_3D.v) {
                DoProjectedPoint(&a);
                DoProjectedPoint(&b);
            }
            DoEqualLenTicks(a, b, gn);

            Entity *ln = SK.GetEntity(entityB);
            Vector la = SK.GetEntity(ln->point[0])->PointGetNum(),
                   lb = SK.GetEntity(ln->point[1])->PointGetNum();
            Vector pt = SK.GetEntity(ptA)->PointGetNum();
            if(workplane.v != Entity::FREE_IN_3D.v) {
                DoProjectedPoint(&pt);
                la = la.ProjectInto(workplane);
                lb = lb.ProjectInto(workplane);
            }

            Vector closest = pt.ClosestPointOnLine(la, lb.Minus(la));
            LineDrawOrGetDistance(pt, closest);
            DoEqualLenTicks(pt, closest, gn);
            break;
        }

        case EQ_PT_LN_DISTANCES: {
            for(int i = 0; i < 2; i++) {
                Entity *ln = SK.GetEntity(i == 0 ? entityA : entityB);
                Vector la = SK.GetEntity(ln->point[0])->PointGetNum(),
                       lb = SK.GetEntity(ln->point[1])->PointGetNum();
                Entity *pte = SK.GetEntity(i == 0 ? ptA : ptB);
                Vector pt = pte->PointGetNum();

                if(workplane.v != Entity::FREE_IN_3D.v) {
                    DoProjectedPoint(&pt);
                    la = la.ProjectInto(workplane);
                    lb = lb.ProjectInto(workplane);
                }

                Vector closest = pt.ClosestPointOnLine(la, lb.Minus(la));

                LineDrawOrGetDistance(pt, closest);
                DoEqualLenTicks(pt, closest, gn);
            }
            break;
        }

        {
        case SYMMETRIC:
            Vector n;
            n = SK.GetEntity(entityA)->Normal()->NormalN(); goto s;
        case SYMMETRIC_HORIZ:
            n = SK.GetEntity(workplane)->Normal()->NormalU(); goto s;
        case SYMMETRIC_VERT:
            n = SK.GetEntity(workplane)->Normal()->NormalV(); goto s;
        case SYMMETRIC_LINE: {
            Entity *ln = SK.GetEntity(entityA);
            Vector la = SK.GetEntity(ln->point[0])->PointGetNum(),
                   lb = SK.GetEntity(ln->point[1])->PointGetNum();
            la = la.ProjectInto(workplane);
            lb = lb.ProjectInto(workplane);
            n = lb.Minus(la);
            Vector nw = SK.GetEntity(workplane)->Normal()->NormalN();
            n = n.RotatedAbout(nw, PI/2);
            goto s;
        }
s:
            Vector a = SK.GetEntity(ptA)->PointGetNum();
            Vector b = SK.GetEntity(ptB)->PointGetNum();

            for(int i = 0; i < 2; i++) {
                Vector tail = (i == 0) ? a : b;
                Vector d = (i == 0) ? b : a;
                d = d.Minus(tail);
                // Project the direction in which the arrow is drawn normal
                // to the symmetry plane; for projected symmetry constraints,
                // they might not be in the same direction, even when the
                // constraint is fully solved.
                d = n.ScaledBy(d.Dot(n));
                d = d.WithMagnitude(20/SS.GW.scale);
                Vector tip = tail.Plus(d);

                LineDrawOrGetDistance(tail, tip);
                d = d.WithMagnitude(9/SS.GW.scale);
                LineDrawOrGetDistance(tip, tip.Minus(d.RotatedAbout(gn,  0.6)));
                LineDrawOrGetDistance(tip, tip.Minus(d.RotatedAbout(gn, -0.6)));
            }
            break;
        }

        case AT_MIDPOINT:
        case HORIZONTAL:
        case VERTICAL:
            if(entityA.v) {
                Vector r, u, n;
                if(workplane.v == Entity::FREE_IN_3D.v) {
                    r = gr; u = gu; n = gn;
                } else {
                    r = SK.GetEntity(workplane)->Normal()->NormalU();
                    u = SK.GetEntity(workplane)->Normal()->NormalV();
                    n = r.Cross(u);
                }
                // For "at midpoint", this branch is always taken.
                Entity *e = SK.GetEntity(entityA);
                Vector a = SK.GetEntity(e->point[0])->PointGetNum();
                Vector b = SK.GetEntity(e->point[1])->PointGetNum();
                Vector m = (a.ScaledBy(0.5)).Plus(b.ScaledBy(0.5));
                Vector offset = (a.Minus(b)).Cross(n);
                offset = offset.WithMagnitude(13/SS.GW.scale);
                // Draw midpoint constraint on other side of line, so that
                // a line can be midpoint and horizontal at same time.
                if(type == AT_MIDPOINT) offset = offset.ScaledBy(-1);

                if(dogd.drawing) {
                    const char *s = (type == HORIZONTAL)  ? "H" : (
                                    (type == VERTICAL)    ? "V" : (
                                    (type == AT_MIDPOINT) ? "M" : NULL));

                    ssglWriteTextRefCenter(s, Style::DefaultTextHeight(),
                        m.Plus(offset), r, u, LineCallback, this);
                } else {
                    dogd.refp = m.Plus(offset);
                    Point2d ref = SS.GW.ProjectPoint(dogd.refp);
                    dogd.dmin = min(dogd.dmin, ref.DistanceTo(dogd.mp)-10);
                }
            } else {
                Vector a = SK.GetEntity(ptA)->PointGetNum();
                Vector b = SK.GetEntity(ptB)->PointGetNum();

                Entity *w = SK.GetEntity(workplane);
                Vector cu = w->Normal()->NormalU();
                Vector cv = w->Normal()->NormalV();
                Vector cn = w->Normal()->NormalN();

                int i;
                for(i = 0; i < 2; i++) {
                    Vector o = (i == 0) ? a : b;
                    Vector oo = (i == 0) ? a.Minus(b) : b.Minus(a);
                    Vector d = (type == HORIZONTAL) ? cu : cv;
                    if(oo.Dot(d) < 0) d = d.ScaledBy(-1);

                    Vector dp = cn.Cross(d);
                    d = d.WithMagnitude(14/SS.GW.scale);
                    Vector c = o.Minus(d);
                    LineDrawOrGetDistance(o, c);
                    d = d.WithMagnitude(3/SS.GW.scale);
                    dp = dp.WithMagnitude(2/SS.GW.scale);
                    if(dogd.drawing) {
                        glBegin(GL_QUADS);
                            ssglVertex3v((c.Plus(d)).Plus(dp));
                            ssglVertex3v((c.Minus(d)).Plus(dp));
                            ssglVertex3v((c.Minus(d)).Minus(dp));
                            ssglVertex3v((c.Plus(d)).Minus(dp));
                        glEnd();
                    } else {
                        Point2d ref = SS.GW.ProjectPoint(c);
                        dogd.dmin = min(dogd.dmin, ref.DistanceTo(dogd.mp)-6);
                    }
                }
            }
            break;

        case COMMENT: {
            if(dogd.drawing && disp.style.v) {
                ssglLineWidth(Style::Width(disp.style));
                ssglColorRGB(Style::Color(disp.style));
            }
            Vector u, v;
            if(workplane.v == Entity::FREE_IN_3D.v) {
                u = gr;
                v = gu;
            } else {
                EntityBase *norm = SK.GetEntity(workplane)->Normal();
                u = norm->NormalU();
                v = norm->NormalV();
            }
            DoLabel(disp.offset, labelPos, u, v);
            break;
        }

        default: oops();
    }
}

void Constraint::Draw(void) {
    dogd.drawing = true;
    dogd.sel = NULL;
    hStyle hs = GetStyle();

    ssglLineWidth(Style::Width(hs));
    ssglColorRGB(Style::Color(hs));

    DrawOrGetDistance(NULL);
}

double Constraint::GetDistance(Point2d mp) {
    dogd.drawing = false;
    dogd.sel = NULL;
    dogd.mp = mp;
    dogd.dmin = 1e12;

    DrawOrGetDistance(NULL);

    return dogd.dmin;
}

Vector Constraint::GetLabelPos(void) {
    dogd.drawing = false;
    dogd.sel = NULL;
    dogd.mp.x = 0; dogd.mp.y = 0;
    dogd.dmin = 1e12;

    Vector p;
    DrawOrGetDistance(&p);
    return p;
}

Vector Constraint::GetReferencePos(void) {
    dogd.drawing = false;
    dogd.sel = NULL;

    dogd.refp = SS.GW.offset.ScaledBy(-1);
    DrawOrGetDistance(NULL);

    return dogd.refp;
}

void Constraint::GetEdges(SEdgeList *sel) {
    dogd.drawing = true;
    dogd.sel = sel;
    DrawOrGetDistance(NULL);
    dogd.sel = NULL;
}

bool Constraint::IsStylable() {
    if(type == COMMENT) return true;
    return false;
}

hStyle Constraint::GetStyle() const {
    if(disp.style.v != 0) return disp.style;
    return { Style::CONSTRAINT };
}

bool Constraint::HasLabel() {
    switch(type) {
        case COMMENT:
        case PT_PT_DISTANCE:
        case PT_PLANE_DISTANCE:
        case PT_LINE_DISTANCE:
        case PT_FACE_DISTANCE:
        case PROJ_PT_DISTANCE:
        case LENGTH_RATIO:
        case LENGTH_DIFFERENCE:
        case DIAMETER:
        case ANGLE:
            return true;

        default:
            return false;
    }
}
