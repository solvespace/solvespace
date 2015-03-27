//-----------------------------------------------------------------------------
// Implementation of the Constraint menu, to create new constraints in
// the sketch.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

char *Constraint::DescriptionString(void) {
    static char ret[1024];

    const char *s;
    switch(type) {
        case POINTS_COINCIDENT:     s = "pts-coincident"; break;
        case PT_PT_DISTANCE:        s = "pt-pt-distance"; break;
        case PT_LINE_DISTANCE:      s = "pt-line-distance"; break;
        case PT_PLANE_DISTANCE:     s = "pt-plane-distance"; break;
        case PT_FACE_DISTANCE:      s = "pt-face-distance"; break;
        case PROJ_PT_DISTANCE:      s = "proj-pt-pt-distance"; break;
        case PT_IN_PLANE:           s = "pt-in-plane"; break;
        case PT_ON_LINE:            s = "pt-on-line"; break;
        case PT_ON_FACE:            s = "pt-on-face"; break;
        case EQUAL_LENGTH_LINES:    s = "eq-length"; break;
        case EQ_LEN_PT_LINE_D:      s = "eq-length-and-pt-ln-dist"; break;
        case EQ_PT_LN_DISTANCES:    s = "eq-pt-line-distances"; break;
        case LENGTH_RATIO:          s = "length-ratio"; break;
        case LENGTH_DIFFERENCE:     s = "length-difference"; break;
        case SYMMETRIC:             s = "symmetric"; break;
        case SYMMETRIC_HORIZ:       s = "symmetric-h"; break;
        case SYMMETRIC_VERT:        s = "symmetric-v"; break;
        case SYMMETRIC_LINE:        s = "symmetric-line"; break;
        case AT_MIDPOINT:           s = "at-midpoint"; break;
        case HORIZONTAL:            s = "horizontal"; break;
        case VERTICAL:              s = "vertical"; break;
        case DIAMETER:              s = "diameter"; break;
        case PT_ON_CIRCLE:          s = "pt-on-circle"; break;
        case SAME_ORIENTATION:      s = "same-orientation"; break;
        case ANGLE:                 s = "angle"; break;
        case PARALLEL:              s = "parallel"; break;
        case ARC_LINE_TANGENT:      s = "arc-line-tangent"; break;
        case CUBIC_LINE_TANGENT:    s = "cubic-line-tangent"; break;
        case CURVE_CURVE_TANGENT:   s = "curve-curve-tangent"; break;
        case PERPENDICULAR:         s = "perpendicular"; break;
        case EQUAL_RADIUS:          s = "eq-radius"; break;
        case EQUAL_ANGLE:           s = "eq-angle"; break;
        case EQUAL_LINE_ARC_LEN:    s = "eq-line-len-arc-len"; break;
        case WHERE_DRAGGED:         s = "lock-where-dragged"; break;
        case COMMENT:               s = "comment"; break;
        default:                    s = "???"; break;
    }

    sprintf(ret, "c%03x-%s", h.v, s);
    return ret;
}

#ifndef LIBRARY

//-----------------------------------------------------------------------------
// Delete all constraints with the specified type, entityA, ptA. We use this
// when auto-removing constraints that would become redundant.
//-----------------------------------------------------------------------------
void Constraint::DeleteAllConstraintsFor(int type, hEntity entityA, hEntity ptA)
{
    SK.constraint.ClearTags();
    for(int i = 0; i < SK.constraint.n; i++) {
        ConstraintBase *ct = &(SK.constraint.elem[i]);
        if(ct->type != type) continue;

        if(ct->entityA.v != entityA.v) continue;
        if(ct->ptA.v != ptA.v) continue;
        ct->tag = 1;
    }
    SK.constraint.RemoveTagged();
    // And no need to do anything special, since nothing
    // ever depends on a constraint. But do clear the
    // hover, in case the just-deleted constraint was
    // hovered.
    SS.GW.hover.Clear();
}

void Constraint::AddConstraint(Constraint *c) {
    AddConstraint(c, true);
}
void Constraint::AddConstraint(Constraint *c, bool rememberForUndo) {
    if(rememberForUndo) SS.UndoRemember();

    SK.constraint.AddAndAssignId(c);

    SS.MarkGroupDirty(c->group);
    SS.ScheduleGenerateAll();
}

void Constraint::Constrain(int type, hEntity ptA, hEntity ptB,
                                     hEntity entityA, hEntity entityB,
                                     bool other, bool other2)
{
    Constraint c = {};
    c.group = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();
    c.type = type;
    c.ptA = ptA;
    c.ptB = ptB;
    c.entityA = entityA;
    c.entityB = entityB;
    c.other = other;
    c.other2 = other2;
    AddConstraint(&c, false);
}
void Constraint::Constrain(int type, hEntity ptA, hEntity ptB, hEntity entityA){
    Constrain(type, ptA, ptB, entityA, Entity::NO_ENTITY, false, false);
}
void Constraint::ConstrainCoincident(hEntity ptA, hEntity ptB) {
    Constrain(POINTS_COINCIDENT, ptA, ptB,
        Entity::NO_ENTITY, Entity::NO_ENTITY, false, false);
}

void Constraint::MenuConstrain(int id) {
    Constraint c = {};
    c.group = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();

    SS.GW.GroupSelection();
#define gs (SS.GW.gs)

    switch(id) {
        case GraphicsWindow::MNU_DISTANCE_DIA:
        case GraphicsWindow::MNU_REF_DISTANCE: {
            if(gs.points == 2 && gs.n == 2) {
                c.type = PT_PT_DISTANCE;
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
            } else if(gs.lineSegments == 1 && gs.n == 1) {
                c.type = PT_PT_DISTANCE;
                Entity *e = SK.GetEntity(gs.entity[0]);
                c.ptA = e->point[0];
                c.ptB = e->point[1];
            } else if(gs.vectors == 1 && gs.points == 2 && gs.n == 3) {
                c.type = PROJ_PT_DISTANCE;
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
                c.entityA = gs.vector[0];
            } else if(gs.workplanes == 1 && gs.points == 1 && gs.n == 2) {
                c.type = PT_PLANE_DISTANCE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.lineSegments == 1 && gs.points == 1 && gs.n == 2) {
                c.type = PT_LINE_DISTANCE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.faces == 1 && gs.points == 1 && gs.n == 2) {
                c.type = PT_FACE_DISTANCE;
                c.ptA = gs.point[0];
                c.entityA = gs.face[0];
            } else if(gs.circlesOrArcs == 1 && gs.n == 1) {
                c.type = DIAMETER;
                c.entityA = gs.entity[0];
            } else {
                Error(
"Bad selection for distance / diameter constraint. This "
"constraint can apply to:\n\n"
"    * two points (distance between points)\n"
"    * a line segment (length)\n"
"    * two points and a line segment or normal (projected distance)\n"
"    * a workplane and a point (minimum distance)\n"
"    * a line segment and a point (minimum distance)\n"
"    * a plane face and a point (minimum distance)\n"
"    * a circle or an arc (diameter)\n");
                return;
            }
            if(c.type == PT_PT_DISTANCE || c.type == PROJ_PT_DISTANCE) {
                Vector n = SS.GW.projRight.Cross(SS.GW.projUp);
                Vector a = SK.GetEntity(c.ptA)->PointGetNum();
                Vector b = SK.GetEntity(c.ptB)->PointGetNum();
                c.disp.offset = n.Cross(a.Minus(b));
                c.disp.offset = (c.disp.offset).WithMagnitude(50/SS.GW.scale);
            } else {
                c.disp.offset = Vector::From(0, 0, 0);
            }

            if(id == GraphicsWindow::MNU_REF_DISTANCE) {
                c.reference = true;
            }

            c.valA = 0;
            c.ModifyToSatisfy();
            AddConstraint(&c);
            break;
        }

        case GraphicsWindow::MNU_ON_ENTITY:
            if(gs.points == 2 && gs.n == 2) {
                c.type = POINTS_COINCIDENT;
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
            } else if(gs.points == 1 && gs.workplanes == 1 && gs.n == 2) {
                c.type = PT_IN_PLANE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.points == 1 && gs.lineSegments == 1 && gs.n == 2) {
                c.type = PT_ON_LINE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.points == 1 && gs.circlesOrArcs == 1 && gs.n == 2) {
                c.type = PT_ON_CIRCLE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.points == 1 && gs.faces == 1 && gs.n == 2) {
                c.type = PT_ON_FACE;
                c.ptA = gs.point[0];
                c.entityA = gs.face[0];
            } else {
                Error("Bad selection for on point / curve / plane constraint. "
                      "This constraint can apply to:\n\n"
                      "    * two points (points coincident)\n"
                      "    * a point and a workplane (point in plane)\n"
                      "    * a point and a line segment (point on line)\n"
                      "    * a point and a circle or arc (point on curve)\n"
                      "    * a point and a plane face (point on face)\n");
                return;
            }
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_EQUAL:
            if(gs.lineSegments == 2 && gs.n == 2) {
                c.type = EQUAL_LENGTH_LINES;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];
            } else if(gs.lineSegments == 2 && gs.points == 2 && gs.n == 4) {
                c.type = EQ_PT_LN_DISTANCES;
                c.entityA = gs.entity[0];
                c.ptA = gs.point[0];
                c.entityB = gs.entity[1];
                c.ptB = gs.point[1];
            } else if(gs.lineSegments == 1 && gs.points == 2 && gs.n == 3) {
                // The same line segment for the distances, but different
                // points.
                c.type = EQ_PT_LN_DISTANCES;
                c.entityA = gs.entity[0];
                c.ptA = gs.point[0];
                c.entityB = gs.entity[0];
                c.ptB = gs.point[1];
            } else if(gs.lineSegments == 2 && gs.points == 1 && gs.n == 3) {
                c.type = EQ_LEN_PT_LINE_D;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];
                c.ptA = gs.point[0];
            } else if(gs.vectors == 4 && gs.n == 4) {
                c.type = EQUAL_ANGLE;
                c.entityA = gs.vector[0];
                c.entityB = gs.vector[1];
                c.entityC = gs.vector[2];
                c.entityD = gs.vector[3];
            } else if(gs.vectors == 3 && gs.n == 3) {
                c.type = EQUAL_ANGLE;
                c.entityA = gs.vector[0];
                c.entityB = gs.vector[1];
                c.entityC = gs.vector[1];
                c.entityD = gs.vector[2];
            } else if(gs.circlesOrArcs == 2 && gs.n == 2) {
                c.type = EQUAL_RADIUS;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];
            } else if(gs.arcs == 1 && gs.lineSegments == 1 && gs.n == 2) {
                c.type = EQUAL_LINE_ARC_LEN;
                if(SK.GetEntity(gs.entity[0])->type == Entity::ARC_OF_CIRCLE) {
                    c.entityA = gs.entity[1];
                    c.entityB = gs.entity[0];
                } else {
                    c.entityA = gs.entity[0];
                    c.entityB = gs.entity[1];
                }
            } else {
                Error("Bad selection for equal length / radius constraint. "
                      "This constraint can apply to:\n\n"
                      "    * two line segments (equal length)\n"
                      "    * two line segments and two points "
                              "(equal point-line distances)\n"
                      "    * a line segment and two points "
                              "(equal point-line distances)\n"
                      "    * a line segment, and a point and line segment "
                              "(point-line distance equals length)\n"
                      "    * four line segments or normals "
                              "(equal angle between A,B and C,D)\n"
                      "    * three line segments or normals "
                              "(equal angle between A,B and B,C)\n"
                      "    * two circles or arcs (equal radius)\n"
                      "    * a line segment and an arc "
                              "(line segment length equals arc length)\n");
                return;
            }
            if(c.type == EQUAL_ANGLE) {
                // Infer the nearest supplementary angle from the sketch.
                Vector a1 = SK.GetEntity(c.entityA)->VectorGetNum(),
                       b1 = SK.GetEntity(c.entityB)->VectorGetNum(),
                       a2 = SK.GetEntity(c.entityC)->VectorGetNum(),
                       b2 = SK.GetEntity(c.entityD)->VectorGetNum();
                double d1 = a1.Dot(b1), d2 = a2.Dot(b2);

                if(d1*d2 < 0) {
                    c.other = true;
                }
            }
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_RATIO:
            if(gs.lineSegments == 2 && gs.n == 2) {
                c.type = LENGTH_RATIO;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];
            } else {
                Error("Bad selection for length ratio constraint. This "
                      "constraint can apply to:\n\n"
                      "    * two line segments\n");
                return;
            }

            c.valA = 0;
            c.ModifyToSatisfy();
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_DIFFERENCE:
            if(gs.lineSegments == 2 && gs.n == 2) {
                c.type = LENGTH_DIFFERENCE;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];
            } else {
                Error("Bad selection for length difference constraint. This "
                      "constraint can apply to:\n\n"
                      "    * two line segments\n");
                return;
            }

            c.valA = 0;
            c.ModifyToSatisfy();
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_AT_MIDPOINT:
            if(gs.lineSegments == 1 && gs.points == 1 && gs.n == 2) {
                c.type = AT_MIDPOINT;
                c.entityA = gs.entity[0];
                c.ptA = gs.point[0];

                // If a point is at-midpoint, then no reason to also constrain
                // it on-line; so auto-remove that.
                DeleteAllConstraintsFor(PT_ON_LINE, c.entityA, c.ptA);
            } else if(gs.lineSegments == 1 && gs.workplanes == 1 && gs.n == 2) {
                c.type = AT_MIDPOINT;
                int i = SK.GetEntity(gs.entity[0])->IsWorkplane() ? 1 : 0;
                c.entityA = gs.entity[i];
                c.entityB = gs.entity[1-i];
            } else {
                Error("Bad selection for at midpoint constraint. This "
                      "constraint can apply to:\n\n"
                      "    * a line segment and a point "
                            "(point at midpoint)\n"
                      "    * a line segment and a workplane "
                            "(line's midpoint on plane)\n");
                return;
            }
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_SYMMETRIC:
            if(gs.points == 2 &&
                                ((gs.workplanes == 1 && gs.n == 3) ||
                                 (gs.n == 2)))
            {
                c.entityA = gs.entity[0];
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
            } else if(gs.lineSegments == 1 &&
                                ((gs.workplanes == 1 && gs.n == 2) ||
                                 (gs.n == 1)))
            {
                int i = SK.GetEntity(gs.entity[0])->IsWorkplane() ? 1 : 0;
                Entity *line = SK.GetEntity(gs.entity[i]);
                c.entityA = gs.entity[1-i];
                c.ptA = line->point[0];
                c.ptB = line->point[1];
            } else if(SS.GW.LockedInWorkplane()
                        && gs.lineSegments == 2 && gs.n == 2)
            {
                Entity *l0 = SK.GetEntity(gs.entity[0]),
                       *l1 = SK.GetEntity(gs.entity[1]);

                if((l1->group.v != SS.GW.activeGroup.v) ||
                   (l1->construction && !(l0->construction)))
                {
                    swap(l0, l1);
                }
                c.ptA = l1->point[0];
                c.ptB = l1->point[1];
                c.entityA = l0->h;
                c.type = SYMMETRIC_LINE;
            } else if(SS.GW.LockedInWorkplane()
                        && gs.lineSegments == 1 && gs.points == 2 && gs.n == 3)
            {
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
                c.entityA = gs.entity[0];
                c.type = SYMMETRIC_LINE;
            } else {
                Error("Bad selection for symmetric constraint. This constraint "
                      "can apply to:\n\n"
                      "    * two points or a line segment "
                          "(symmetric about workplane's coordinate axis)\n"
                      "    * line segment, and two points or a line segment "
                          "(symmetric about line segment)\n"
                      "    * workplane, and two points or a line segment "
                          "(symmetric about workplane)\n");
                return;
            }
            if(c.type != 0) {
                // Already done, symmetry about a line segment in a workplane
            } else if(c.entityA.v == Entity::NO_ENTITY.v) {
                // Horizontal / vertical symmetry, implicit symmetry plane
                // normal to the workplane
                if(c.workplane.v == Entity::FREE_IN_3D.v) {
                    Error("Must be locked in to workplane when constraining "
                          "symmetric without an explicit symmetry plane.");
                    return;
                }
                Vector pa = SK.GetEntity(c.ptA)->PointGetNum();
                Vector pb = SK.GetEntity(c.ptB)->PointGetNum();
                Vector dp = pa.Minus(pb);
                EntityBase *norm = SK.GetEntity(c.workplane)->Normal();;
                Vector u = norm->NormalU(), v = norm->NormalV();
                if(fabs(dp.Dot(u)) > fabs(dp.Dot(v))) {
                    c.type = SYMMETRIC_HORIZ;
                } else {
                    c.type = SYMMETRIC_VERT;
                }
                if(gs.lineSegments == 1) {
                    // If this line segment is already constrained horiz or
                    // vert, then auto-remove that redundant constraint.
                    DeleteAllConstraintsFor(HORIZONTAL, (gs.entity[0]),
                        Entity::NO_ENTITY);
                    DeleteAllConstraintsFor(VERTICAL, (gs.entity[0]),
                        Entity::NO_ENTITY);

                }
            } else {
                // Symmetry with a symmetry plane specified explicitly.
                c.type = SYMMETRIC;
            }
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_VERTICAL:
        case GraphicsWindow::MNU_HORIZONTAL: {
            hEntity ha, hb;
            if(c.workplane.v == Entity::FREE_IN_3D.v) {
                Error("Select workplane before constraining horiz/vert.");
                return;
            }
            if(gs.lineSegments == 1 && gs.n == 1) {
                c.entityA = gs.entity[0];
                Entity *e = SK.GetEntity(c.entityA);
                ha = e->point[0];
                hb = e->point[1];
            } else if(gs.points == 2 && gs.n == 2) {
                ha = c.ptA = gs.point[0];
                hb = c.ptB = gs.point[1];
            } else {
                Error("Bad selection for horizontal / vertical constraint. "
                      "This constraint can apply to:\n\n"
                      "    * two points\n"
                      "    * a line segment\n");
                return;
            }
            if(id == GraphicsWindow::MNU_HORIZONTAL) {
                c.type = HORIZONTAL;
            } else {
                c.type = VERTICAL;
            }
            AddConstraint(&c);
            break;
        }

        case GraphicsWindow::MNU_ORIENTED_SAME: {
            if(gs.anyNormals == 2 && gs.n == 2) {
                c.type = SAME_ORIENTATION;
                c.entityA = gs.anyNormal[0];
                c.entityB = gs.anyNormal[1];
            } else {
                Error("Bad selection for same orientation constraint. This "
                      "constraint can apply to:\n\n"
                      "    * two normals\n");
                return;
            }
            SS.UndoRemember();

            Entity *nfree = SK.GetEntity(c.entityA);
            Entity *nref  = SK.GetEntity(c.entityB);
            if(nref->group.v == SS.GW.activeGroup.v) {
                swap(nref, nfree);
            }
            if(nfree->group.v == SS.GW.activeGroup.v &&
               nref ->group.v != SS.GW.activeGroup.v)
            {
                // nfree is free, and nref is locked (since it came from a
                // previous group); so let's force nfree aligned to nref,
                // and make convergence easy
                Vector ru = nref ->NormalU(), rv = nref ->NormalV();
                Vector fu = nfree->NormalU(), fv = nfree->NormalV();

                if(fabs(fu.Dot(ru)) < fabs(fu.Dot(rv))) {
                    // There might be an odd*90 degree rotation about the
                    // normal vector; allow that, since the numerical
                    // constraint does
                    swap(ru, rv);
                }
                fu = fu.Dot(ru) > 0 ? ru : ru.ScaledBy(-1);
                fv = fv.Dot(rv) > 0 ? rv : rv.ScaledBy(-1);

                nfree->NormalForceTo(Quaternion::From(fu, fv));
            }
            AddConstraint(&c, false);
            break;
        }

        case GraphicsWindow::MNU_OTHER_ANGLE:
            if(gs.constraints == 1 && gs.n == 0) {
                Constraint *c = SK.GetConstraint(gs.constraint[0]);
                if(c->type == ANGLE) {
                    SS.UndoRemember();
                    c->other = !(c->other);
                    c->ModifyToSatisfy();
                    break;
                }
                if(c->type == EQUAL_ANGLE) {
                    SS.UndoRemember();
                    c->other = !(c->other);
                    SS.MarkGroupDirty(c->group);
                    SS.ScheduleGenerateAll();
                    break;
                }
            }
            Error("Must select an angle constraint.");
            return;

        case GraphicsWindow::MNU_REFERENCE:
            if(gs.constraints == 1 && gs.n == 0) {
                Constraint *c = SK.GetConstraint(gs.constraint[0]);
                if(c->HasLabel() && c->type != COMMENT) {
                    (c->reference) = !(c->reference);
                    SK.GetGroup(c->group)->clean = false;
                    SS.GenerateAll();
                    break;
                }
            }
            Error("Must select a constraint with associated label.");
            return;

        case GraphicsWindow::MNU_ANGLE:
        case GraphicsWindow::MNU_REF_ANGLE: {
            if(gs.vectors == 2 && gs.n == 2) {
                c.type = ANGLE;
                c.entityA = gs.vector[0];
                c.entityB = gs.vector[1];
                c.valA = 0;
            } else {
                Error("Bad selection for angle constraint. This constraint "
                      "can apply to:\n\n"
                      "    * two line segments\n"
                      "    * a line segment and a normal\n"
                      "    * two normals\n");
                return;
            }

            Entity *ea = SK.GetEntity(c.entityA),
                   *eb = SK.GetEntity(c.entityB);
            if(ea->type == Entity::LINE_SEGMENT &&
               eb->type == Entity::LINE_SEGMENT)
            {
                Vector a0 = SK.GetEntity(ea->point[0])->PointGetNum(),
                       a1 = SK.GetEntity(ea->point[1])->PointGetNum(),
                       b0 = SK.GetEntity(eb->point[0])->PointGetNum(),
                       b1 = SK.GetEntity(eb->point[1])->PointGetNum();
                if(a0.Equals(b0) || a1.Equals(b1)) {
                    // okay, vectors should be drawn in same sense
                } else if(a0.Equals(b1) || a1.Equals(b0)) {
                    // vectors are in opposite sense
                    c.other = true;
                } else {
                    // no shared point; not clear which intersection to draw
                }
            }

            if(id == GraphicsWindow::MNU_REF_ANGLE) {
                c.reference = true;
            }

            c.ModifyToSatisfy();
            AddConstraint(&c);
            break;
        }

        case GraphicsWindow::MNU_PARALLEL:
            if(gs.vectors == 2 && gs.n == 2) {
                c.type = PARALLEL;
                c.entityA = gs.vector[0];
                c.entityB = gs.vector[1];
            } else if(gs.lineSegments == 1 && gs.arcs == 1 && gs.n == 2) {
                Entity *line = SK.GetEntity(gs.entity[0]);
                Entity *arc  = SK.GetEntity(gs.entity[1]);
                if(line->type == Entity::ARC_OF_CIRCLE) {
                    swap(line, arc);
                }
                Vector l0 = SK.GetEntity(line->point[0])->PointGetNum(),
                       l1 = SK.GetEntity(line->point[1])->PointGetNum();
                Vector a1 = SK.GetEntity(arc->point[1])->PointGetNum(),
                       a2 = SK.GetEntity(arc->point[2])->PointGetNum();

                if(l0.Equals(a1) || l1.Equals(a1)) {
                    c.other = false;
                } else if(l0.Equals(a2) || l1.Equals(a2)) {
                    c.other = true;
                } else {
                    Error("The tangent arc and line segment must share an "
                          "endpoint. Constrain them with Constrain -> "
                          "On Point before constraining tangent.");
                    return;
                }
                c.type = ARC_LINE_TANGENT;
                c.entityA = arc->h;
                c.entityB = line->h;
            } else if(gs.lineSegments == 1 && gs.cubics == 1 && gs.n == 2) {
                Entity *line  = SK.GetEntity(gs.entity[0]);
                Entity *cubic = SK.GetEntity(gs.entity[1]);
                if(line->type == Entity::CUBIC) {
                    swap(line, cubic);
                }
                Vector l0 = SK.GetEntity(line->point[0])->PointGetNum(),
                       l1 = SK.GetEntity(line->point[1])->PointGetNum();
                Vector as = cubic->CubicGetStartNum(),
                       af = cubic->CubicGetFinishNum();

                if(l0.Equals(as) || l1.Equals(as)) {
                    c.other = false;
                } else if(l0.Equals(af) || l1.Equals(af)) {
                    c.other = true;
                } else {
                    Error("The tangent cubic and line segment must share an "
                          "endpoint. Constrain them with Constrain -> "
                          "On Point before constraining tangent.");
                    return;
                }
                c.type = CUBIC_LINE_TANGENT;
                c.entityA = cubic->h;
                c.entityB = line->h;
            } else if(gs.cubics + gs.arcs == 2 && gs.n == 2) {
                if(!SS.GW.LockedInWorkplane()) {
                    Error("Curve-curve tangency must apply in workplane.");
                    return;
                }
                Entity *eA = SK.GetEntity(gs.entity[0]),
                       *eB = SK.GetEntity(gs.entity[1]);
                Vector as = eA->EndpointStart(),
                       af = eA->EndpointFinish(),
                       bs = eB->EndpointStart(),
                       bf = eB->EndpointFinish();
                if(as.Equals(bs)) {
                    c.other = false; c.other2 = false;
                } else if(as.Equals(bf)) {
                    c.other = false; c.other2 = true;
                } else if(af.Equals(bs)) {
                    c.other = true; c.other2 = false;
                } else if(af.Equals(bf)) {
                    c.other = true; c.other2 = true;
                } else {
                    Error("The curves must share an endpoint. Constrain them "
                          "with Constrain -> On Point before constraining "
                          "tangent.");
                    return;
                }
                c.type = CURVE_CURVE_TANGENT;
                c.entityA = eA->h;
                c.entityB = eB->h;
            } else {
                Error("Bad selection for parallel / tangent constraint. This "
                      "constraint can apply to:\n\n"
                      "    * two line segments (parallel)\n"
                      "    * a line segment and a normal (parallel)\n"
                      "    * two normals (parallel)\n"
                      "    * two line segments, arcs, or beziers, that share "
                            "an endpoint (tangent)\n");
                return;
            }
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_PERPENDICULAR:
            if(gs.vectors == 2 && gs.n == 2) {
                c.type = PERPENDICULAR;
                c.entityA = gs.vector[0];
                c.entityB = gs.vector[1];
            } else {
                Error("Bad selection for perpendicular constraint. This "
                      "constraint can apply to:\n\n"
                      "    * two line segments\n"
                      "    * a line segment and a normal\n"
                      "    * two normals\n");
                return;
            }
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_WHERE_DRAGGED:
            if(gs.points == 1 && gs.n == 1) {
                c.type = WHERE_DRAGGED;
                c.ptA = gs.point[0];
            } else {
                Error("Bad selection for lock point where dragged constraint. "
                      "This constraint can apply to:\n\n"
                      "    * a point\n");
                return;
            }
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_COMMENT:
            SS.GW.pending.operation = GraphicsWindow::MNU_COMMENT;
            SS.GW.pending.description = "click center of comment text";
            SS.ScheduleShowTW();
            break;

        default: oops();
    }

    SS.GW.ClearSelection();
    InvalidateGraphics();
}

#endif /* ! LIBRARY */
