#include "solvespace.h"

#include "harness.h"

// A triangular prism minus a fillet-shaped tool: a quarter-cylinder sliver
// that rounds off the prism's right-angle corner. The tool's cylinder is
// tangent to both faces of the prism that form the corner, and its flat cap
// faces are coplanar with them, so along each tangent line a flat and a
// curved surface of the tool meet edge-on-edge, both lying in the tangent
// plane of the prism's face: the case in SShell::ClassifyEdge() where the
// first-order data (the normals at the edge) cannot classify the edge, and
// the faces' actual geometry away from the edge decides. The intersection
// curves along the tangent lines also get no orientation from the surface
// normals, which are parallel there (issue #1291; the model is ruevs'
// PrismFilletMinimalTestCase).
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

    // The prism is 1/2 * 1 * 2 * 1 = 1 mm³; the fillet cuts away a sliver
    // just under (1 - π/4) * 0.5² * 0.5 ≈ 0.0268 mm³ of it (just under,
    // since the mesh approximates the cylinder with inscribed facets).
    CHECK_EQ_EPS(m->CalculateVolume() / 0.9730161212, 1.0);
}
