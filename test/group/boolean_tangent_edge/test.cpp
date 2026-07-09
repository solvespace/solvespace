#include "solvespace.h"

#include "harness.h"

// An L-section extrusion minus a triangular prism. One face plane of the
// prism passes tangentially through the L-section's concave vertical edge,
// and the prism shares an edge with it there, so edges of each shell lie on
// edges of the other: the tangent edge-on-edge cases in
// SShell::ClassifyEdge(). The prism's edge classifies against the L-shell's
// concave edge as inside, and the L-shell's edge against the prism's convex
// edge as outside. Before those cases were handled, this model's Boolean
// failed or succeeded depending on uninitialized memory (issue #1619).
TEST_CASE(normal_watertight_volume) {
    CHECK_LOAD("normal.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    CHECK_FALSE(m->l.IsEmpty());

    SEdgeList el = {};
    bool inters, leaks;
    SKdNode::From(m)->MakeCertainEdgesInto(&el,
        EdgeKind::NAKED_OR_SELF_INTER, &inters, &leaks);
    CHECK_FALSE(inters);
    CHECK_FALSE(leaks);
    CHECK_TRUE(el.l.IsEmpty());
    el.Clear();

    // The expected volume from the sketch: the L-section is
    // 80*30 + 30*80 - 30*30 = 3900 mm² minus the 200 mm² notch, extruded
    // 80 mm, giving 296000 mm³.
    CHECK_EQ_EPS(m->CalculateVolume() / 296000.0, 1.0);
}
