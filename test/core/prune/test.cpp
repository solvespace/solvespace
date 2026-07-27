#include "solvespace.h"

#include "harness.h"

// angle.slvs is a right triangle: a horizontal line 100 mm long, a vertical
// line, and a third line closing them, with an angle constraint (handle 7)
// between that third line and the horizontal one.
static const hConstraint HANGLE = { 7 };

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

TEST_CASE(failed_solve_keeps_constraints) {
    CHECK_LOAD("angle.slvs");

    const int constraints = SK.constraint.n;
    const int requests = SK.request.n;

    // No triangle like this one has a 95 degree angle there -- the direction
    // cosine is 100/sqrt(100^2 + h^2), which is positive for every h, and the
    // cosine of 95 degrees is not -- so the solver gives up, and marks the
    // constraints whose equations it couldn't satisfy so that they can be
    // reported. Those marks used to be mistaken for delete-me tags by the
    // prune pass that runs with the next regeneration, which silently
    // destroyed exactly the constraints that had failed. Either of the
    // didn't-converge results reaches the marking path, so don't insist on
    // which one.
    SolveResult how = SetValueAndSolve(HANGLE, 95.0);
    CHECK_TRUE(how == SolveResult::DIDNT_CONVERGE ||
               how == SolveResult::REDUNDANT_DIDNT_CONVERGE);

    // The sketch is still all there, so the value that couldn't be solved is
    // still on screen to be corrected.
    CHECK_TRUE(SK.constraint.n == constraints);
    CHECK_TRUE(SK.request.n == requests);
    Constraint *c = SK.constraint.FindByIdNoOops(HANGLE);
    CHECK_TRUE(c != NULL);
    CHECK_EQ_EPS(c->valA, 95.0);
}
