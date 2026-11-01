#include "solvespace.h"

#include "harness.h"

// A block with one edge rounded by a quarter cylinder that blends into its top
// face, minus a cylinder at right angles to it whose end lies on that top
// face. The circle at the end of the cylinder crosses the line where the
// rounded surface meets the top face. Splitting the circle against the rounded
// surface, which is tangent to the top face along that line, put the split
// points off, so the trim curves didn't join up there.
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

    // The exact solid is about 40196 mm³; it was about 44645 mm³ before. Allow
    // for the facets approximating the curved surfaces, and for the chord
    // tolerance, rather than expecting this mesh exactly.
    double vol = m->CalculateVolume();
    CHECK_TRUE(fabs(vol/40196 - 1) < 0.01);
}
