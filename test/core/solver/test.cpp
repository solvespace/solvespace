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
// does after the user edits one. The solution we start from is the previous
// solve's, so these cases are about a sequence of edits, and cannot be
// reproduced by loading a file with one of the values already in it.
static SolveResult SetValueAndSolve(hConstraint hc, double value) {
    Constraint *c = SK.GetConstraint(hc);
    hGroup hg = c->group;
    c->valA = value;
    SS.MarkGroupDirty(hg);
    SS.GenerateAll();
    return SK.GetGroup(hg)->solved.how;
}

// Signed, so that it also tells us which of the two mirrored solutions
// (+angle and -angle both satisfy the constraint) we landed on.
static double VerticalLength() {
    return SK.GetEntity(VERTICAL_B)->PointGetNum().y -
           SK.GetEntity(VERTICAL_A)->PointGetNum().y;
}

// Note the relative form of the checks below. NewtonSolve() stops as soon as
// every equation's residual is under CONVERGE_TOLERANCE, which is
// dimensionless; the angle equation's residual is a direction cosine, so
// satisfying it to 1e-8 only pins this triangle's height to a few times 1e-6
// mm. Comparing the height itself against LENGTH_EPS therefore tests the
// solver's luck rather than its answer.

TEST_CASE(angle_step_over_critical_point) {
    CHECK_LOAD("angle.slvs");

    // Both of these angles are perfectly solvable, but the Newton step from
    // the 60 degree solution towards the 30 degree one used to overshoot the
    // root and land right on top of a critical point of the angle equation,
    // from where the next step threw the geometry off to nowhere.
    CHECK_TRUE(SetValueAndSolve(HANGLE, 60.0) == SolveResult::OKAY);
    CHECK_EQ_EPS(VerticalLength() / (100*tan(60*PI/180)), 1.0);

    // Signed, since -30 degrees satisfies the angle constraint just as well as
    // +30 does, and mirroring the triangle about the horizontal line is not
    // something the user asked for.
    CHECK_TRUE(SetValueAndSolve(HANGLE, 30.0) == SolveResult::OKAY);
    CHECK_EQ_EPS(VerticalLength() / (100*tan(30*PI/180)), 1.0);
}

TEST_CASE(angle_step_from_steep_solution) {
    CHECK_LOAD("angle.slvs");

    // The same failure, with much more room to spare: from a steep angle the
    // step towards a shallow one used to overshoot far past the critical
    // point, and every target below 75 degrees was unsolvable from here.
    CHECK_TRUE(SetValueAndSolve(HANGLE, 85.0) == SolveResult::OKAY);
    CHECK_EQ_EPS(VerticalLength() / (100*tan(85*PI/180)), 1.0);

    // Magnitude only: -30 degrees satisfies the constraint exactly as well as
    // +30, and from this far up the hill the step that first reduces the
    // residual below its value here is one that crosses the horizontal line,
    // so the triangle ends up mirrored. That is a solution, where before
    // there was none, but it is not the nearer of the two.
    CHECK_TRUE(SetValueAndSolve(HANGLE, 30.0) == SolveResult::OKAY);
    CHECK_EQ_EPS(fabs(VerticalLength()) / (100*tan(30*PI/180)), 1.0);
}
