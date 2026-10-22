#include "solvespace.h"

#include "harness.h"

// The two line segments of the sketch, each dimensioned to 4 m, constrained
// perpendicular to each other with the start of the second one on the first.
static const hEntity LINE_A_A = { 0x00040001 }, LINE_A_B = { 0x00040002 };
static const hEntity LINE_B_A = { 0x00050001 }, LINE_B_B = { 0x00050002 };

static Vector Dir(hEntity a, hEntity b) {
    return SK.GetEntity(b)->PointGetNum().Minus(SK.GetEntity(a)->PointGetNum());
}

// A sketch whose dimensions are large enough that the entries of the Jacobian
// span several orders of magnitude. Solving the Newton step through the normal
// equations squared that spread, which pushed a perfectly good pivot under the
// threshold below which a column counts as linearly dependent; the step then
// came back with that component zeroed, and the solver reported these entirely
// compatible constraints as incompatible. See #1354.
TEST_CASE(perpendicular_4m) {
    // Unlike most fixtures in this suite, perpendicular_4m.slvs is the
    // reporter's file exactly as it was uploaded, and it must stay that way.
    // Re-saving it from SolveSpace writes out the *solved* parameter values,
    // so the sketch loads already at its solution, the first Newton step is
    // the zero step, and the bug this test is here for cannot happen. Do not
    // canonicalize this file.
    CHECK_LOAD("perpendicular_4m.slvs");

    for(Group &g : SK.group) {
        CHECK_TRUE(g.IsSolvedOkay());
    }

    // And it solved it correctly, not just to something.
    Vector da = Dir(LINE_A_A, LINE_A_B), db = Dir(LINE_B_A, LINE_B_B);
    CHECK_EQ_EPS(da.Magnitude(), 4000.0);
    CHECK_EQ_EPS(db.Magnitude(), 4000.0);
    CHECK_EQ_EPS(da.WithMagnitude(1).Dot(db.WithMagnitude(1)), 0.0);

    // The start of the second line lies on the first one.
    Vector onLine = Dir(LINE_A_A, LINE_B_A);
    CHECK_EQ_EPS(onLine.Cross(da).Magnitude() / da.Magnitude(), 0.0);
}
