#include "solvespace.h"

#include "harness.h"

// A cube minus a fillet-shaped tool whose curved surface is the extrusion
// of a cubic spline, tangent to a face of the cube where the spline ends on
// it. Since the spline is not an arc, the extruded surface is not a
// cylinder, so the intersection curve along the tangent line cannot come
// from a closed-form line-surface intersection; and the numerical
// intersection cannot converge on a grazing intersection. The line is
// instead recovered from the tool's trim curve at the tangency, which lies
// entirely within the cube face's plane (issue #1291; the model is
// phkahler's relocated cube_cut).
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

    // The cube is 60³ = 216000 mm³; the tool cuts away a sliver bounded by
    // the spline surface, roughly 11360 mm³ (slightly more than the true
    // value, since the mesh approximates the spline surface with inscribed
    // facets).
    CHECK_EQ_EPS(m->CalculateVolume() / 204638.6271882122, 1.0);
}
