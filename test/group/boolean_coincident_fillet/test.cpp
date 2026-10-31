#include "solvespace.h"

#include "harness.h"

// A 20 mm cube with one vertical edge rounded by a cubic fillet, minus a tool
// that rounds one of the top edges the same way. The tool is extruded exactly
// the width of the cube, so its ends lie on the cube's faces, and the end of
// its fillet curve lands on the vertex where the cube's fillet meets the top
// face. That vertex is one end of the curve where the two fillet surfaces
// intersect; it's a vertex of the trim curves of both surfaces, and the
// intersection was not found there, so neither fillet surface was trimmed
// and both were lost (issue #1755).
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

    // The exact solid is about 7162 mm³. Without both fillets the volume was
    // about 5761 mm³, so allow for the facets approximating the fillets, and
    // for the chord tolerance, rather than expecting this mesh exactly.
    double vol = m->CalculateVolume();
    CHECK_TRUE(fabs(vol/7162.3 - 1) < 0.01);
}
