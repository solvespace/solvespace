#include "solvespace.h"

#include "harness.h"

// A triangle whose edge lies in another triangle's interior — for example
// where a surface of a Boolean result grazes the interior of a face without
// cutting it — touches the mesh, but does not intersect it.
TEST_CASE(grazing_edge_is_not_self_intersecting) {
    SMesh m = {};
    STriMeta meta = {};
    // A square floor in the xy plane, and a tent rising from it whose ridge
    // base lies across the floor's interior.
    m.AddTriangle(meta, Vector::From(0, 0, 0), Vector::From(10, 0, 0),
                        Vector::From(10, 10, 0));
    m.AddTriangle(meta, Vector::From(0, 0, 0), Vector::From(10, 10, 0),
                        Vector::From(0, 10, 0));
    m.AddTriangle(meta, Vector::From(2, 5, 0), Vector::From(8, 5, 0),
                        Vector::From(5, 5, 3));

    SEdgeList el = {};
    bool inters, leaks;
    SKdNode::From(&m)->MakeCertainEdgesInto(&el,
        EdgeKind::SELF_INTER, &inters, &leaks);
    CHECK_FALSE(inters);
    el.Clear();
    m.Clear();
}

// An edge that passes through another triangle's interior is a real
// self-intersection, and must still be reported.
TEST_CASE(crossing_edge_is_self_intersecting) {
    SMesh m = {};
    STriMeta meta = {};
    m.AddTriangle(meta, Vector::From(0, 0, 0), Vector::From(10, 0, 0),
                        Vector::From(10, 10, 0));
    m.AddTriangle(meta, Vector::From(0, 0, 0), Vector::From(10, 10, 0),
                        Vector::From(0, 10, 0));
    // A triangle poking through the floor's interior.
    m.AddTriangle(meta, Vector::From(3, 3, 1), Vector::From(7, 3, 1),
                        Vector::From(5, 3, -1));

    SEdgeList el = {};
    bool inters, leaks;
    SKdNode::From(&m)->MakeCertainEdgesInto(&el,
        EdgeKind::SELF_INTER, &inters, &leaks);
    CHECK_TRUE(inters);
    CHECK_FALSE(el.l.IsEmpty());
    el.Clear();
    m.Clear();
}
