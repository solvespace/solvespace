#include "solvespace.h"

#include "harness.h"

// A stack of cuboids unioned face-to-face, from issue #1452: cuboid A is
// extruded down from the sketch plane, cuboid B up from the same plane, and
// cuboid C up from the top of B, which leaves several coplanar side faces
// that the union steps merge into single surfaces. Then cuboid D is sketched
// on that shared side plane and extruded away from the stack, so it joins
// the shell only across the coincident plane, with D's face spanning the
// side faces of A, B and part of C. Exact intersection curves along the
// shared edges get trimmed to the merged surfaces' padded bounds and used
// to end just past the real vertices; splitting other curves at those
// phantom points, plus keeping intersection edges that duplicate the
// original trim boundary with different splits, made the trim polygons fail
// to assemble and left an entire side face missing.
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

    // The expected volume: A is 30*20*10 extruded down, B is 20*20*10 up,
    // C is 10*20*10 on top of B, and D is 10 wide by 5 deep, spanning from
    // the bottom of A to halfway up C, so 10*5*25 attached to the side.
    CHECK_EQ_EPS(m->CalculateVolume() / 13250.0, 1.0);
}
