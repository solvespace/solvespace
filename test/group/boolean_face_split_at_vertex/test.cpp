#include "solvespace.h"

#include "harness.h"

// A triangular prism minus another at right angles to it, whose bottom edges
// cross. The tool cuts a notch into each of two faces of the prism, reaching
// the opposite edge of the face exactly where the bottom edges cross, so each
// face is left in two pieces that touch only at that point. Assembling the
// trims of such a face could go around both pieces in one contour, through
// that point twice, which the triangulation couldn't handle.
TEST_CASE(normal_watertight_volume) {
    CHECK_LOAD("normal.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    CHECK_FALSE(m->l.IsEmpty());

    SEdgeList el = {};
    bool inters, leaks;
    SKdNode::From(m)->MakeCertainEdgesInto(&el,
        EdgeKind::NAKED_OR_SELF_INTER, /*coplanarIsInter=*/true, &inters, &leaks);
    // Free the edge list before checking. CHECK_* returns from the test case on
    // failure, so checking first leaks the list whenever the check that matters
    // fails, and the leak report then buries the actual failure.
    bool noEdges = el.l.IsEmpty();
    el.Clear();
    CHECK_FALSE(inters);
    CHECK_FALSE(leaks);
    CHECK_TRUE(noEdges);

    // The prism is 23328.8 mm³, and the tool overlaps 5469.35 mm³ of it. The
    // holes left by the failed triangulation didn't change the volume, so
    // it's the checks above that catch that; this one catches a wrong cut.
    double vol = m->CalculateVolume();
    CHECK_TRUE(fabs(vol/17859.45 - 1) < 0.01);
}
