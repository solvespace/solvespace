#include "solvespace.h"

#include "harness.h"

// angle.slvs is a right triangle: a horizontal line 100 mm long, a vertical
// line, and a third line closing them, with an angle constraint (handle 7)
// between that third line and the horizontal one.
static const hConstraint HANGLE = { 7 };
// The endpoints of the vertical line, whose length is 100*tan(angle).
static const hEntity VERTICAL_A = { 0x00050001 };
static const hEntity VERTICAL_B = { 0x00050002 };

// Change the value of a dimension and re-solve, the same way that the GUI
// does after the user edits one.
static SolveResult SetValueAndSolve(hConstraint hc, double value) {
    Constraint *c = SK.GetConstraint(hc);
    hGroup hg = c->group;
    c->valA = value;
    SS.MarkGroupDirty(hg);
    SS.GenerateAll();
    return SK.GetGroup(hg)->solved.how;
}

static double VerticalLength() {
    return SK.GetEntity(VERTICAL_B)->PointGetNum().y -
           SK.GetEntity(VERTICAL_A)->PointGetNum().y;
}

TEST_CASE(angle_step_over_critical_point) {
    CHECK_LOAD("angle.slvs");

    // Both of these angles are perfectly solvable, but the undamped Newton
    // step from the 60 degree solution towards the 30 degree one used to
    // land right on top of a critical point of the angle equation, from
    // where the next step threw the geometry off to infinity.
    CHECK_TRUE(SetValueAndSolve(HANGLE, 60.0) == SolveResult::OKAY);
    CHECK_EQ_EPS(VerticalLength(), 100*tan(60*PI/180));

    CHECK_TRUE(SetValueAndSolve(HANGLE, 30.0) == SolveResult::OKAY);
    CHECK_EQ_EPS(VerticalLength(), 100*tan(30*PI/180));
}

// empty_sketch.slvs is angle.slvs with all of the sketch geometry removed:
// just the reference planes and an empty sketch-in-plane group on the XY
// plane, whose workplane origin is the reference origin point.
static const hEntity ORIGIN = {0x00010001};

// Add a line segment in the active group between the given points (in
// workplane coordinates), and return the handle of its line entity.
static hEntity AddLine(double ax, double ay, double bx, double by) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT,
                                   /*rememberForUndo=*/false);
    SK.GetEntity(hr.entity(1))->PointForceTo(Vector::From(ax, ay, 0));
    SK.GetEntity(hr.entity(2))->PointForceTo(Vector::From(bx, by, 0));
    return hr.entity(0);
}

static hEntity PointA(hEntity line) {
    return SK.GetEntity(line)->point[0];
}
static hEntity PointB(hEntity line) {
    return SK.GetEntity(line)->point[1];
}

// Solve the way Analyze -> Show Underconstrained Points does.
static SolveResult SolveAndFindFree() {
    // The first solve after a change does a full DOF check without
    // substitution; solve once more so that the substitution path is
    // taken, as it is when the user later runs Show Underconstrained Points.
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SS.MarkGroupDirty(SS.GW.activeGroup);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL, /*andFindFree=*/true);
    return SK.GetGroup(SS.GW.activeGroup)->solved.how;
}

static bool UFree(hEntity pt) {
    return SK.GetParam(SK.GetEntity(pt)->param[0])->free;
}
static bool VFree(hEntity pt) {
    return SK.GetParam(SK.GetEntity(pt)->param[1])->free;
}

TEST_CASE(free_params_unconstrained) {
    CHECK_LOAD("empty_sketch.slvs");
    hEntity l = AddLine(10, 10, 30, 20);
    CHECK_TRUE(SolveAndFindFree() == SolveResult::OKAY);
    CHECK_TRUE(UFree(PointA(l)) && VFree(PointA(l)));
    CHECK_TRUE(UFree(PointB(l)) && VFree(PointB(l)));
}

TEST_CASE(free_params_horizontal) {
    CHECK_LOAD("empty_sketch.slvs");
    hEntity l = AddLine(10, 10, 30, 20);
    // The horizontal constraint is solved by substituting one v for the other,
    // but both endpoints can still move vertically.
    Constraint::Constrain(Constraint::Type::HORIZONTAL, Entity::NO_ENTITY, Entity::NO_ENTITY, l);
    CHECK_TRUE(SolveAndFindFree() == SolveResult::OKAY);
    CHECK_TRUE(UFree(PointA(l)) && VFree(PointA(l)));
    CHECK_TRUE(UFree(PointB(l)) && VFree(PointB(l)));
}

TEST_CASE(free_params_vertical) {
    CHECK_LOAD("empty_sketch.slvs");
    hEntity l = AddLine(10, 10, 30, 20);
    Constraint::Constrain(Constraint::Type::VERTICAL, Entity::NO_ENTITY, Entity::NO_ENTITY, l);
    CHECK_TRUE(SolveAndFindFree() == SolveResult::OKAY);
    CHECK_TRUE(UFree(PointA(l)) && VFree(PointA(l)));
    CHECK_TRUE(UFree(PointB(l)) && VFree(PointB(l)));
}

TEST_CASE(free_params_coincident) {
    CHECK_LOAD("empty_sketch.slvs");
    hEntity l1 = AddLine(10, 10, 30, 20), l2 = AddLine(31, 21, 40, 50);
    Constraint::ConstrainCoincident(PointB(l1), PointA(l2));
    CHECK_TRUE(SolveAndFindFree() == SolveResult::OKAY);
    CHECK_TRUE(UFree(PointB(l1)) && VFree(PointB(l1)));
    CHECK_TRUE(UFree(PointA(l2)) && VFree(PointA(l2)));
}

TEST_CASE(free_params_coincident_chain) {
    CHECK_LOAD("empty_sketch.slvs");
    hEntity l1 = AddLine(10, 10, 30, 20), l2 = AddLine(31, 21, 40, 50), l3 = AddLine(29, 19, 0, 50);
    // Three points made coincident pairwise, so that the substitutions chain.
    Constraint::ConstrainCoincident(PointB(l1), PointA(l2));
    Constraint::ConstrainCoincident(PointA(l2), PointA(l3));
    CHECK_TRUE(SolveAndFindFree() == SolveResult::OKAY);
    CHECK_TRUE(UFree(PointB(l1)) && VFree(PointB(l1)));
    CHECK_TRUE(UFree(PointA(l2)) && VFree(PointA(l2)));
    CHECK_TRUE(UFree(PointA(l3)) && VFree(PointA(l3)));
}

TEST_CASE(free_params_horizontal_from_fixed_point) {
    CHECK_LOAD("empty_sketch.slvs");
    hEntity l = AddLine(1, 1, 30, 20);
    Constraint::ConstrainCoincident(PointA(l), ORIGIN);
    Constraint::Constrain(Constraint::Type::HORIZONTAL, Entity::NO_ENTITY, Entity::NO_ENTITY, l);
    CHECK_TRUE(SolveAndFindFree() == SolveResult::OKAY);
    // The v of the far endpoint is substituted, but it's pinned through the
    // fixed point, so it must not be reported as free.
    CHECK_FALSE(UFree(PointA(l)) || VFree(PointA(l)));
    CHECK_TRUE(UFree(PointB(l)));
    CHECK_FALSE(VFree(PointB(l)));
}

TEST_CASE(free_params_fully_constrained) {
    CHECK_LOAD("empty_sketch.slvs");
    hEntity l = AddLine(1, 1, 30, 20);
    Constraint::ConstrainCoincident(PointA(l), ORIGIN);
    Constraint::Constrain(Constraint::Type::HORIZONTAL, Entity::NO_ENTITY, Entity::NO_ENTITY, l);
    hConstraint hc = Constraint::Constrain(Constraint::Type::PT_PT_DISTANCE, PointA(l), PointB(l),
                                           Entity::NO_ENTITY);
    SK.GetConstraint(hc)->valA = 25.0;
    CHECK_TRUE(SolveAndFindFree() == SolveResult::OKAY);
    CHECK_TRUE(SK.GetGroup(SS.GW.activeGroup)->solved.dof == 0);
    CHECK_FALSE(UFree(PointA(l)) || VFree(PointA(l)));
    CHECK_FALSE(UFree(PointB(l)) || VFree(PointB(l)));
}

TEST_CASE(free_params_distance_from_fixed_point) {
    CHECK_LOAD("empty_sketch.slvs");
    hEntity l = AddLine(1, 1, 30, 20);
    Constraint::ConstrainCoincident(PointA(l), ORIGIN);
    hConstraint hc = Constraint::Constrain(Constraint::Type::PT_PT_DISTANCE, PointA(l), PointB(l),
                                           Entity::NO_ENTITY);
    SK.GetConstraint(hc)->valA = 25.0;
    CHECK_TRUE(SolveAndFindFree() == SolveResult::OKAY);
    // No substitution involved: the far endpoint can still swing around.
    CHECK_FALSE(UFree(PointA(l)) || VFree(PointA(l)));
    CHECK_TRUE(UFree(PointB(l)) && VFree(PointB(l)));
}
