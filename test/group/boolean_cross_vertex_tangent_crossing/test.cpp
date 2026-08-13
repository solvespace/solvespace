#include "solvespace.h"

#include "harness.h"

// A 2mm high prism with an isosceles right triangle with legs of 2mm as the
// base. The fillet part is a bezier spline, tool's flat cap face is outside
// the prism. On one side this reproduces #1291 and on the other #1743. The
// model is ruevs's "1291_1743_cube_cut_tangent_outside_still_fails_simplified"
// This case covers both "boolean_cross_vertex" and "boolean_tangent_crossing".
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

    // The same value that the mesh Boolean gives for this model (build it
    // with Group.forceToMesh set), to twelve significant figures.
    CHECK_EQ_EPS(m->CalculateVolume() / 3.880640, 1.0);
}

