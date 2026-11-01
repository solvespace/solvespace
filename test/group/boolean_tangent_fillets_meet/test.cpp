#include "solvespace.h"

#include "harness.h"

// An extruded profile with an arc fillet of radius 7.5 where a flank meets a
// flat face, minus a tool extruded at right angles to it, with an arc fillet
// of the same radius blending into the same flat face, and a flat face that
// blends into that fillet tangent to it.

// Check that the active group's mesh is watertight, and return the volume
// that its Boolean removed from the solid before it. The CHECK_* macros
// return from this function on failure, so the caller tells that it failed by
// the count of failed checks.
static void CheckWatertight(Test::Helper *helper, double *removed) {
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    g->GenerateDisplayItems();
    SMesh *m = &g->displayMesh;
    CHECK_FALSE(m->l.IsEmpty());

    SEdgeList el = {};
    bool inters, leaks;
    SKdNode::From(m)->MakeCertainEdgesInto(&el,
        EdgeKind::NAKED_OR_SELF_INTER, /*coplanarIsInter=*/true, &inters, &leaks);
    // Free the edge list before checking. CHECK_* returns from the test case on
    // failure, so checking first leaks the list whenever the check that matters
    // fails, and the leak report then buries the actual failure.
    bool noEdges = el.l.IsEmpty();
    el.Clear();
    CHECK_FALSE(inters);
    CHECK_FALSE(leaks);
    CHECK_TRUE(noEdges);

    Group *prev = g->PreviousGroup();
    prev->GenerateDisplayItems();
    *removed = prev->displayMesh.CalculateVolume() - m->CalculateVolume();
}

// The two fillets meet on the flat face, where they're both tangent to it, so
// one end of the curve where they intersect is a point of tangency. The
// intersection is found by marching between points where the trim edges of
// one surface cross the other, and the tangent point wasn't found for the
// arcs, so there was nowhere for the march to end; neither fillet was trimmed
// by that curve, and the tool's fillet was lost.
TEST_CASE(normal_watertight_volume) {
    CHECK_LOAD("normal.slvs");
    double removed = 0;
    size_t failCount = helper->failCount;
    CheckWatertight(helper, &removed);
    if(helper->failCount != failCount) return;

    // The tool cuts away a small piece of the profile, about 23.7 mm³ exactly;
    // without the tool's fillet it cut away about 160 mm³. The piece is small
    // enough that its facets make a difference of a few percent, so allow for
    // that and for the chord tolerance, rather than expecting this mesh exactly.
    CHECK_TRUE(fabs(removed/23.68 - 1) < 0.25);
}

// With the tool extruded farther, its flat face also cuts the profile's
// fillet, and the curve where they intersect ends on the edge where the
// tool's fillet blends into that flat face. Splitting that curve against the
// tool's fillet, which is tangent to the curve there, put the end of it 2e-4
// mm off; the trim curves of the profile's fillet didn't join up and it was
// lost, and the tool's flat face was left untrimmed, outside the profile.
TEST_CASE(longer_watertight_volume) {
    CHECK_LOAD("longer.slvs");
    double removed = 0;
    size_t failCount = helper->failCount;
    CheckWatertight(helper, &removed);
    if(helper->failCount != failCount) return;

    // The tool cuts away about 1444.4 mm³ exactly; with its flat face left
    // untrimmed, the volume grew instead. Allow for the facets, and for the
    // chord tolerance, rather than expecting this mesh exactly.
    CHECK_TRUE(fabs(removed/1444.4 - 1) < 0.1);
}
