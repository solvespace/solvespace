#include "solvespace.h"

#include "harness.h"

// A cube minus a tool swept along a spline profile. The profile is tangent to
// one face of the cube, and the tool's flat cap face is coincident with that
// same face of the cube, so the tool's tangent edge runs within the cube's
// face and crosses the cube's own edge there. That crossing point is a vertex
// of neither input shell, and neither curve passes through a surface of the
// other shell transversally at it, so SCurve::MakeCopySplitAgainst() splits
// only the tool's edge (which does cross the cube's face) and leaves the
// cube's edge whole. The cube's face is then trimmed by an edge whose two
// halves classify differently, its trim polygon fails to assemble, and two
// faces of the result go missing (issue #1743; the model is phkahler's
// cube_cut_2).
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
    CHECK_EQ_EPS(m->CalculateVolume() / 203485.2242132, 1.0);
}

