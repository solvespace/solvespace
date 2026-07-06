#include "solvespace.h"

#include "harness.h"

// Two triangular prisms extruded together, sharing only a single vertical
// edge, plus a third prism whose edge lies along that shared line: regions
// of the shell join along a knife edge there, so the third prism's edge
// falls on four coincident edges of the shell (the edge_inters == 4 case in
// SShell::ClassifyEdge(), issue #1091). Before that case was handled, the
// Boolean failed to assemble the trim polygon and left naked edges.
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
    CHECK_FALSE(inters);
    CHECK_FALSE(leaks);
    CHECK_TRUE(el.l.IsEmpty());
    el.Clear();

    // The expected volume from the sketch: the two big prisms are
    // (0.5*1.6126052390506482 + 0.5*1.5498346036820765)*1.0 and the small
    // one is 0.5*1*1*0.49834022880539446.
    CHECK_EQ_EPS(m->CalculateVolume() / 1.8303900357690596, 1.0);
}
