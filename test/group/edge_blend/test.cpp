#include "solvespace.h"

#include "harness.h"

// Fillet and chamfer groups. The models are extrusions of a 40 x 25 mm
// rectangle, 12 mm high, and of an L-shaped section (30 x 10 mm and
// 10 x 10 mm), 15 mm high, followed by a group that rounds or bevels some of
// their straight edges.

static bool IsWatertight(SMesh *m) {
    SEdgeList el = {};
    bool inters, leaks;
    SKdNode::From(m)->MakeCertainEdgesInto(&el,
        EdgeKind::NAKED_OR_SELF_INTER, /*coplanarIsInter=*/true, &inters, &leaks);
    bool noEdges = el.l.IsEmpty();
    el.Clear();
    return noEdges && !inters && !leaks;
}

// All four top edges of the box rounded with a radius of 2 mm. Where two
// rounds meet at a corner, their tools are mitered, and the cylinders meet
// in the bisecting plane, tangent to the top face at one point. Exactly,
// 130 mm of edge lose (1 - pi/4) * 4 mm^2 each, less the overlap at the four
// mitered corners, for 11891.47 mm^3; the mesh approximates the cylinders
// with inscribed facets.
TEST_CASE(round_ring) {
    CHECK_LOAD("round_ring.slvs");
    CHECK_SAVE("round_ring.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(g->type == Group::Type::FILLET);
    CHECK_TRUE(g->edgeError.empty());
    CHECK_FALSE(g->booleanFailed);
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    CHECK_TRUE(IsWatertight(m));
    CHECK_EQ_EPS(m->CalculateVolume() / 11887.405418, 1.0);
}

// A convex edge of the L rounded, which removes material, and its concave
// inner edge, which adds it. Exactly 6000 - 30 * 0.8584 + 15 * 0.8584 mm^3.
TEST_CASE(round_convex_and_concave) {
    CHECK_LOAD("round_mixed.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(g->edgeError.empty());
    CHECK_FALSE(g->booleanFailed);
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    CHECK_TRUE(IsWatertight(m));
    CHECK_EQ_EPS(m->CalculateVolume() / 5986.819831, 1.0);
}

// The six top edges and the six vertical edges of the L beveled by 2 mm,
// with three bevels meeting at each top corner, one reflex corner on the top
// face, and the concave vertical edge, which gets material added and ends
// just below the bevels of the top edges. All faces are planar, so the mesh
// volume is exact.
TEST_CASE(chamfer_corners) {
    CHECK_LOAD("chamfer.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(g->subtype == Group::Subtype::EDGE_CHAMFER);
    CHECK_TRUE(g->edgeError.empty());
    CHECK_FALSE(g->booleanFailed);
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    CHECK_TRUE(IsWatertight(m));
    CHECK_EQ_EPS(m->CalculateVolume() / 5703.253333, 1.0);
}

// A radius larger than the faces at the edge leaves the solid unchanged and
// reports the edge.
TEST_CASE(too_large) {
    CHECK_LOAD("too_large.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_FALSE(g->edgeError.empty());
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    CHECK_TRUE(IsWatertight(m));
    CHECK_EQ_EPS(m->CalculateVolume() / 6000.0, 1.0);
}

// The six top edges of the L rounded with a radius of 2 mm, around the reflex
// corner of the top face, and the concave vertical edge at that corner, which
// ends just below the rounds of the top edges instead of sticking out of them.
TEST_CASE(round_reflex_corner) {
    CHECK_LOAD("round_l.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(g->edgeError.empty());
    CHECK_FALSE(g->booleanFailed);
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    CHECK_TRUE(IsWatertight(m));
    CHECK_EQ_EPS(m->CalculateVolume() / 5926.704136, 1.0);
}
