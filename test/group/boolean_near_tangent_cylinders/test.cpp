#include "solvespace.h"

#include "harness.h"

// A cylinder minus another of the same radius at right angles to it, their
// axes intersecting, so that they touch at two points, where the ellipses on
// which they intersect cross. Each cylinder is made of four surfaces, and the
// second one is turned about its axis very slightly, so that its seams pass
// 0.008 mm from where the cylinders touch, closer than the chord tolerance,
// but much farther than LENGTH_EPS. Near there the cylinders stay within
// LENGTH_EPS of each other, so a short piece of each seam lies within
// LENGTH_EPS of the other cylinder, and the two surfaces on either side of it
// disagreed about whether to keep it.
//
// The triangles of the two cylinders can still cross a little near where
// they touch, since each lies within the chord tolerance inside its own
// surface, so check only that the mesh is closed, not that it doesn't
// intersect itself.
TEST_CASE(normal_closed_volume) {
    CHECK_LOAD("normal.slvs");

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    CHECK_FALSE(m->l.IsEmpty());

    SEdgeList el = {};
    bool inters, leaks;
    SKdNode::From(m)->MakeCertainEdgesInto(&el,
        EdgeKind::NAKED_OR_SELF_INTER, /*coplanarIsInter=*/true, &inters, &leaks);
    el.Clear();
    CHECK_FALSE(leaks);
    CHECK_FALSE(SK.GetGroup(SS.GW.activeGroup)->runningShell.booleanFailed);

    // The exact solid is about 237268 mm³: the first cylinder, less the
    // 16*r^3/3 that the two have in common. When the trims failed it was
    // about 500000 mm³, so allow for the facets approximating the cylinders,
    // and for the chord tolerance, rather than expecting this mesh exactly.
    double vol = m->CalculateVolume();
    CHECK_TRUE(fabs(vol/237268 - 1) < 0.01);
}
