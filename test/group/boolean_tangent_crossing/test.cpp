#include "solvespace.h"

#include "harness.h"

// A cube minus a tool whose profile is a cubic spline closed by two straight
// lines. The spline ends on a face of the cube, tangent to it, but the line
// that continues the profile from that endpoint runs outside the cube, and so
// crosses that face. Along the tangent line, then, one of the tool's faces
// lies in the cube face's tangent plane while the other cuts through it, and
// the tool's material fills the region behind the tangent face. Classifying
// the region on that side as coincident with the tool--as if the tangent face
// were flat there--drops the tangent line from the trim, and the cube face is
// then lost (issue #1291; the model is ruevs' cube_cut with the spline made
// tangent on the other side, its corner left outside the cube).
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

    // The cube is 60³ = 216000 mm³; the tool cuts away a wedge bounded by the
    // spline surface, roughly 10500 mm³ (a little less than the true value,
    // since the mesh approximates the spline surface with inscribed facets).
    CHECK_EQ_EPS(m->CalculateVolume() / 205477.058604742, 1.0);
}
