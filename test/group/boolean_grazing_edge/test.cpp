#include "solvespace.h"

#include "harness.h"

// A cube minus a fillet-shaped tool that lies entirely inside the cube,
// except that the edge between the tool's two side surfaces lies exactly in
// a face of the cube, since its profile's corner is constrained onto the
// cube's edge. The tool grazes that face without cutting it, so the
// resulting surfaces touch the face's interior along the grazing edge; that
// tangential touch must not be reported as a self-intersection (issue #1291;
// the model is ruevs' relocated cube_cut).
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

    // The cube is 60³ = 216000 mm³; the tool cuts away a groove bounded by
    // the spline surface, roughly 20000 mm³.
    CHECK_EQ_EPS(m->CalculateVolume() / 196108.6117657832, 1.0);
}
