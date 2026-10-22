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
