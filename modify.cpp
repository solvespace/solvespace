#include "solvespace.h"

//-----------------------------------------------------------------------------
// Replace a point-coincident constraint on oldpt with that same constraint
// on newpt. Useful when splitting or tangent arcing.
//-----------------------------------------------------------------------------
void GraphicsWindow::ReplacePointInConstraints(hEntity oldpt, hEntity newpt) {
    int i;
    for(i = 0; i < SK.constraint.n; i++) {
        Constraint *c = &(SK.constraint.elem[i]);

        if(c->type == Constraint::POINTS_COINCIDENT) {
            if(c->ptA.v == oldpt.v) c->ptA = newpt;
            if(c->ptB.v == oldpt.v) c->ptB = newpt;
        }
    }
}

//-----------------------------------------------------------------------------
// A single point must be selected when this function is called. We find two
// non-construction line segments that join at this point, and create a
// tangent arc joining them.
//-----------------------------------------------------------------------------
void GraphicsWindow::MakeTangentArc(void) {
    if(!LockedInWorkplane()) {
        Error("Must be sketching in workplane to create tangent "
              "arc.");
        return;
    }

    // Find two line segments that join at the specified point,
    // and blend them with a tangent arc. First, find the
    // requests that generate the line segments.
    Vector pshared = SK.GetEntity(gs.point[0])->PointGetNum();
    ClearSelection();

    int i, c = 0;
    Entity *line[2];
    Request *lineReq[2];
    bool point1[2];
    for(i = 0; i < SK.request.n; i++) {
        Request *r = &(SK.request.elem[i]);
        if(r->group.v != activeGroup.v) continue;
        if(r->type != Request::LINE_SEGMENT) continue;
        if(r->construction) continue;

        Entity *e = SK.GetEntity(r->h.entity(0));
        Vector p0 = SK.GetEntity(e->point[0])->PointGetNum(),
               p1 = SK.GetEntity(e->point[1])->PointGetNum();
        
        if(p0.Equals(pshared) || p1.Equals(pshared)) {
            if(c < 2) {
                line[c] = e;
                lineReq[c] = r;
                point1[c] = (p1.Equals(pshared));
            }
            c++;
        }
    }
    if(c != 2) {
        Error("To create a tangent arc, select a point where "
              "two non-construction line segments join.");
        return;
    }

    SS.UndoRemember();

    Entity *wrkpl = SK.GetEntity(ActiveWorkplane());
    Vector wn = wrkpl->Normal()->NormalN();

    hEntity hshared = (line[0])->point[point1[0] ? 1 : 0],
            hother0 = (line[0])->point[point1[0] ? 0 : 1],
            hother1 = (line[1])->point[point1[1] ? 0 : 1];

    Vector pother0 = SK.GetEntity(hother0)->PointGetNum(),
           pother1 = SK.GetEntity(hother1)->PointGetNum();

    Vector v0shared = pshared.Minus(pother0),
           v1shared = pshared.Minus(pother1);

    hEntity srcline0 = (line[0])->h,
            srcline1 = (line[1])->h;

    (lineReq[0])->construction = true;
    (lineReq[1])->construction = true;

    // And thereafter we mustn't touch the entity or req ptrs,
    // because the new requests/entities we add might force a
    // realloc.
    memset(line, 0, sizeof(line));
    memset(lineReq, 0, sizeof(lineReq));

    // The sign of vv determines whether shortest distance is
    // clockwise or anti-clockwise.
    Vector v = (wn.Cross(v0shared)).WithMagnitude(1);
    double vv = v1shared.Dot(v);

    double dot = (v0shared.WithMagnitude(1)).Dot(
                  v1shared.WithMagnitude(1));
    double theta = acos(dot);
    double r = 200/scale;
    // Set the radius so that no more than one third of the 
    // line segment disappears.
    r = min(r, v0shared.Magnitude()*tan(theta/2)/3);
    r = min(r, v1shared.Magnitude()*tan(theta/2)/3);
    double el = r/tan(theta/2);

    hRequest rln0 = AddRequest(Request::LINE_SEGMENT, false),
             rln1 = AddRequest(Request::LINE_SEGMENT, false);
    hRequest rarc = AddRequest(Request::ARC_OF_CIRCLE, false);

    Entity *ln0 = SK.GetEntity(rln0.entity(0)),
           *ln1 = SK.GetEntity(rln1.entity(0));
    Entity *arc = SK.GetEntity(rarc.entity(0));

    SK.GetEntity(ln0->point[0])->PointForceTo(pother0);
    Constraint::ConstrainCoincident(ln0->point[0], hother0);

    SK.GetEntity(ln1->point[0])->PointForceTo(pother1);
    Constraint::ConstrainCoincident(ln1->point[0], hother1);

    Vector arc0 = pshared.Minus(v0shared.WithMagnitude(el));
    Vector arc1 = pshared.Minus(v1shared.WithMagnitude(el));

    SK.GetEntity(ln0->point[1])->PointForceTo(arc0);
    SK.GetEntity(ln1->point[1])->PointForceTo(arc1);

    Constraint::Constrain(Constraint::PT_ON_LINE,
        ln0->point[1], Entity::NO_ENTITY, srcline0);
    Constraint::Constrain(Constraint::PT_ON_LINE,
        ln1->point[1], Entity::NO_ENTITY, srcline1);

    Vector center = arc0;
    int a, b;
    if(vv < 0) {
        a = 1; b = 2;
        center = center.Minus(v0shared.Cross(wn).WithMagnitude(r));
    } else {
        a = 2; b = 1;
        center = center.Plus(v0shared.Cross(wn).WithMagnitude(r));
    }

    SK.GetEntity(arc->point[0])->PointForceTo(center);
    SK.GetEntity(arc->point[a])->PointForceTo(arc0);
    SK.GetEntity(arc->point[b])->PointForceTo(arc1);

    Constraint::ConstrainCoincident(arc->point[a], ln0->point[1]);
    Constraint::ConstrainCoincident(arc->point[b], ln1->point[1]);

    Constraint::Constrain(Constraint::ARC_LINE_TANGENT,
        Entity::NO_ENTITY, Entity::NO_ENTITY,
        arc->h, ln0->h, (a==2));
    Constraint::Constrain(Constraint::ARC_LINE_TANGENT,
        Entity::NO_ENTITY, Entity::NO_ENTITY,
        arc->h, ln1->h, (b==2));

    SS.later.generateAll = true;
}

void GraphicsWindow::SplitLine(hEntity he, Vector pinter) {
    // Save the original endpoints, since we're about to delete this entity.
    Entity *e01 = SK.GetEntity(he);
    hEntity hep0 = e01->point[0], hep1 = e01->point[1];
    Vector p0 = SK.GetEntity(hep0)->PointGetNum(),
           p1 = SK.GetEntity(hep1)->PointGetNum();

    SS.UndoRemember();

    // Add the two line segments this one gets split into.
    hRequest r0i = AddRequest(Request::LINE_SEGMENT, false),
             ri1 = AddRequest(Request::LINE_SEGMENT, false);
    // Don't get entities till after adding, realloc issues

    Entity *e0i = SK.GetEntity(r0i.entity(0)),
           *ei1 = SK.GetEntity(ri1.entity(0));

    SK.GetEntity(e0i->point[0])->PointForceTo(p0);
    SK.GetEntity(e0i->point[1])->PointForceTo(pinter);
    SK.GetEntity(ei1->point[0])->PointForceTo(pinter);
    SK.GetEntity(ei1->point[1])->PointForceTo(p1);

    ReplacePointInConstraints(hep0, e0i->point[0]);
    ReplacePointInConstraints(hep1, ei1->point[1]);

    // Finally, delete the original line
    int i;
    SK.request.ClearTags();
    for(i = 0; i < SK.request.n; i++) {
        Request *r = &(SK.request.elem[i]);
        if(r->group.v != activeGroup.v) continue;
        if(r->type != Request::LINE_SEGMENT) continue;
    
        // If the user wants to keep the old lines around, they can just
        // mark it construction first.
        if(he.v == r->h.entity(0).v && !r->construction) {
            r->tag = 1;
            break;
        }
    }
    DeleteTaggedRequests();
}

void GraphicsWindow::SplitCircle(hEntity he, Vector pinter) {
    SS.UndoRemember();

    Entity *circle = SK.GetEntity(he);
    if(circle->type == Entity::CIRCLE) {
        // Start with an unbroken circle, split it into a 360 degree arc.
        Vector center = SK.GetEntity(circle->point[0])->PointGetNum();

        circle = NULL; // shortly invalid!
        hRequest hr = AddRequest(Request::ARC_OF_CIRCLE, false);

        Entity *arc = SK.GetEntity(hr.entity(0));

        SK.GetEntity(arc->point[0])->PointForceTo(center);
        SK.GetEntity(arc->point[1])->PointForceTo(pinter);
        SK.GetEntity(arc->point[2])->PointForceTo(pinter);
    } else {
        // Start with an arc, break it in to two arcs
        Vector center = SK.GetEntity(circle->point[0])->PointGetNum(),
               start  = SK.GetEntity(circle->point[1])->PointGetNum(),
               finish = SK.GetEntity(circle->point[2])->PointGetNum();

        circle = NULL; // shortly invalid!
        hRequest hr0 = AddRequest(Request::ARC_OF_CIRCLE, false),
                 hr1 = AddRequest(Request::ARC_OF_CIRCLE, false);

        Entity *arc0 = SK.GetEntity(hr0.entity(0)),
               *arc1 = SK.GetEntity(hr1.entity(0));

        SK.GetEntity(arc0->point[0])->PointForceTo(center);
        SK.GetEntity(arc0->point[1])->PointForceTo(start);
        SK.GetEntity(arc0->point[2])->PointForceTo(pinter);

        SK.GetEntity(arc1->point[0])->PointForceTo(center);
        SK.GetEntity(arc1->point[1])->PointForceTo(pinter);
        SK.GetEntity(arc1->point[2])->PointForceTo(finish);
    }

    // Finally, delete the original circle or arc
    int i;
    SK.request.ClearTags();
    for(i = 0; i < SK.request.n; i++) {
        Request *r = &(SK.request.elem[i]);
        if(r->group.v != activeGroup.v) continue;
        if(r->type != Request::CIRCLE && r->type != Request::ARC_OF_CIRCLE) {
            continue;
        }
    
        // If the user wants to keep the old lines around, they can just
        // mark it construction first.
        if(he.v == r->h.entity(0).v && !r->construction) {
            r->tag = 1;
            break;
        }
    }
    DeleteTaggedRequests();
}

void GraphicsWindow::SplitLinesOrCurves(void) {
    if(!LockedInWorkplane()) {
        Error("Must be sketching in workplane to split.");
        return;
    }

    GroupSelection();
    if(gs.n == 2 && gs.lineSegments == 2) {
        Entity *la = SK.GetEntity(gs.entity[0]),
               *lb = SK.GetEntity(gs.entity[1]);
        Vector a0 = SK.GetEntity(la->point[0])->PointGetNum(),
               a1 = SK.GetEntity(la->point[1])->PointGetNum(),
               b0 = SK.GetEntity(lb->point[0])->PointGetNum(),
               b1 = SK.GetEntity(lb->point[1])->PointGetNum();
        Vector da = a1.Minus(a0), db = b1.Minus(b0);
        
        // First, check if the lines intersect.
        bool skew;
        Vector pinter = Vector::AtIntersectionOfLines(a0, a1, b0, b1, &skew);
        if(skew) {
            Error("Lines are parallel or skew; no intersection to split.");
            goto done;
        }
            
        double ta = (pinter.Minus(a0)).DivPivoting(da),
               tb = (pinter.Minus(b0)).DivPivoting(db);

        double tola = LENGTH_EPS/da.Magnitude(),
               tolb = LENGTH_EPS/db.Magnitude();

        hEntity ha = la->h, hb = lb->h;
        la = NULL; lb = NULL;
        // Following adds will cause a realloc, break the pointers

        bool didSomething = false;
        if(ta > tola && ta < (1 - tola)) {
            SplitLine(ha, pinter);
            didSomething = true;
        }
        if(tb > tolb && tb < (1 - tolb)) {
            SplitLine(hb, pinter);
            didSomething = true;
        }
        if(!didSomething) {
            Error(
                "Nothing to split; intersection does not lie on either line.");
        }
    } else if(gs.n == 2 && gs.lineSegments == 1 && gs.circlesOrArcs == 1) {
        Entity *line   = SK.GetEntity(gs.entity[0]),
               *circle = SK.GetEntity(gs.entity[1]);
        if(line->type != Entity::LINE_SEGMENT) {
            SWAP(Entity *, line, circle);
        }
        hEntity hline = line->h, hcircle = circle->h;

        Vector l0 = SK.GetEntity(line->point[0])->PointGetNum(),
               l1 = SK.GetEntity(line->point[1])->PointGetNum();
        Vector dl = l1.Minus(l0);

        Quaternion q = SK.GetEntity(circle->normal)->NormalGetNum();
        Vector cn = q.RotationN();
        Vector cc = SK.GetEntity(circle->point[0])->PointGetNum();
        double cd = cc.Dot(cn);
        double cr = circle->CircleGetRadiusNum();

        if(fabs(l0.Dot(cn) - cd) > LENGTH_EPS ||
           fabs(l1.Dot(cn) - cd) > LENGTH_EPS)
        {
            Error("Lines does not lie in same plane as circle.");
            goto done;
        }
        // Now let's see if they intersect; transform everything into a csys
        // with origin at the center of the circle, and where the line is
        // horizontal.
        Vector n = cn.WithMagnitude(1);
        Vector u = dl.WithMagnitude(1);
        Vector v = n.Cross(u);
        
        Vector nl0 = (l0.Minus(cc)).DotInToCsys(u, v, n),
               nl1 = (l1.Minus(cc)).DotInToCsys(u, v, n);

        double yint = nl0.y;
        if(fabs(yint) > (cr - LENGTH_EPS)) {
            Error("Line does not intersect (or is tangent to) circle.");
            goto done;
        }
        double xint = sqrt(cr*cr - yint*yint);

        Vector inter0 = Vector::From( xint, yint, 0),
               inter1 = Vector::From(-xint, yint, 0);

        // While we're here, let's calculate the angles at which the
        // intersections (and the endpoints of the arc) occur.
        double theta0 = atan2(yint, xint), theta1 = atan2(yint, -xint);
        double thetamin, thetamax;
        if(circle->type == Entity::CIRCLE) {
            thetamin = 0;
            thetamax = 2.1*PI; // fudge, make sure it's a good complete circle
        } else {
            Vector start  = SK.GetEntity(circle->point[1])->PointGetNum(),
                   finish = SK.GetEntity(circle->point[2])->PointGetNum();
            start  = (start .Minus(cc)).DotInToCsys(u, v, n);
            finish = (finish.Minus(cc)).DotInToCsys(u, v, n);
            thetamin = atan2(start.y, start.x);
            thetamax = atan2(finish.y, finish.x);

            // Normalize; arc is drawn with increasing theta from start,
            // so subtract that off and make all angles in (0, 2*pi]
            theta0   = WRAP_NOT_0(theta0   - thetamin, 2*PI);
            theta1   = WRAP_NOT_0(theta1   - thetamin, 2*PI);
            thetamax = WRAP_NOT_0(thetamax - thetamin, 2*PI);
        }

        // And move our intersections back out to the base frame.
        inter0 = inter0.ScaleOutOfCsys(u, v, n).Plus(cc);
        inter1 = inter1.ScaleOutOfCsys(u, v, n).Plus(cc);

        // So now we have our intersection points. Let's see where they are
        // on the line.
        double t0 = (inter0.Minus(l0)).DivPivoting(dl),
               t1 = (inter1.Minus(l0)).DivPivoting(dl);
        double tol = LENGTH_EPS/dl.Magnitude();
   
        bool didSomething = false;
        // Split only once, even if it crosses multiple times; just pick
        // arbitrarily which.
        if(t0 > tol && t0 < (1 - tol) && theta0 < thetamax) {
            SplitLine(hline, inter0);
            SplitCircle(hcircle, inter0);
            didSomething = true;
        } else if(t1 > tol && t1 < (1 - tol) && theta1 < thetamax) {
            SplitLine(hline, inter1);
            SplitCircle(hcircle, inter1);
            didSomething = true;
        }
        if(!didSomething) {
            Error("Nothing to split; neither intersection lies on both the "
                  "line and the circle.");
        }
    } else {
        Error("Can't split these entities; select two lines, a line and "
              "a circle, or a line and an arc.");
    }

done:
    ClearSelection();
    SS.later.generateAll = true;
}

