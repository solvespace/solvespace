// Demo: SolveSpace Core C++ API — create a constrained rectangle and solve.
#include <cstdio>
#include "solvespace/core.h"

using namespace SolveSpace::Api;

int main() {
    Engine engine;

    printf("SolveSpace Core API Demo\n");
    printf("========================\n\n");

    // Query initial state
    printf("Entities after init: %d\n", engine.EntityCount());
    hGroup ag = engine.GetActiveGroup();
    printf("Active group: %u\n", ag.v);

    // Get reference workplane (XY plane)
    hEntity xyPlane = engine.GetXYWorkplane();
    hEntity xyOrigin = engine.GetWorkplaneOrigin(xyPlane);
    printf("XY workplane: 0x%08x, origin: 0x%08x\n", xyPlane.v, xyOrigin.v);

    // Create 4 points for a rectangle on the XY plane
    hEntity p0 = engine.AddPoint2d(ag, xyPlane, 0.0, 0.0);
    hEntity p1 = engine.AddPoint2d(ag, xyPlane, 40.0, 0.0);
    hEntity p2 = engine.AddPoint2d(ag, xyPlane, 40.0, 30.0);
    hEntity p3 = engine.AddPoint2d(ag, xyPlane, 0.0, 30.0);

    // Create 4 lines forming a rectangle
    hEntity l0 = engine.AddLineSegment(ag, xyPlane, p0, p1);
    hEntity l1 = engine.AddLineSegment(ag, xyPlane, p1, p2);
    hEntity l2 = engine.AddLineSegment(ag, xyPlane, p2, p3);
    hEntity l3 = engine.AddLineSegment(ag, xyPlane, p3, p0);

    printf("\nCreated rectangle: 4 points + 4 lines\n");
    printf("Entity count: %d\n", engine.EntityCount());

    // Get line sub-entities (endpoints)
    EntityInfo li = engine.GetEntityInfo(l0);
    printf("Line L0 has %d sub-points\n", li.numPoints);

    // Add constraints: make lines horizontal/vertical
    engine.AddConstraint(ConstraintType::HORIZONTAL, ag, xyPlane, 0, {}, {}, l0, {});
    engine.AddConstraint(ConstraintType::VERTICAL, ag, xyPlane, 0, {}, {}, l1, {});
    engine.AddConstraint(ConstraintType::HORIZONTAL, ag, xyPlane, 0, {}, {}, l2, {});
    engine.AddConstraint(ConstraintType::VERTICAL, ag, xyPlane, 0, {}, {}, l3, {});

    // Connect corners with coincident constraints
    EntityInfo l0i = engine.GetEntityInfo(l0);
    EntityInfo l1i = engine.GetEntityInfo(l1);
    EntityInfo l2i = engine.GetEntityInfo(l2);
    EntityInfo l3i = engine.GetEntityInfo(l3);
    engine.AddConstraint(ConstraintType::POINTS_COINCIDENT, ag, xyPlane, 0,
                         l0i.point[1], l1i.point[0], {}, {});
    engine.AddConstraint(ConstraintType::POINTS_COINCIDENT, ag, xyPlane, 0,
                         l1i.point[1], l2i.point[0], {}, {});
    engine.AddConstraint(ConstraintType::POINTS_COINCIDENT, ag, xyPlane, 0,
                         l2i.point[1], l3i.point[0], {}, {});
    engine.AddConstraint(ConstraintType::POINTS_COINCIDENT, ag, xyPlane, 0,
                         l3i.point[1], l0i.point[0], {}, {});

    // Fix corner at origin
    engine.AddConstraint(ConstraintType::POINTS_COINCIDENT, ag, xyPlane, 0,
                         l0i.point[0], engine.GetWorkplaneOrigin(xyPlane), {}, {});

    // Set dimensions: width=40, height=30
    engine.AddConstraint(ConstraintType::PT_PT_DISTANCE, ag, xyPlane, 40.0,
                         l0i.point[0], l0i.point[1], {}, {});
    engine.AddConstraint(ConstraintType::PT_PT_DISTANCE, ag, xyPlane, 30.0,
                         l1i.point[0], l1i.point[1], {}, {});

    printf("Constraints: %d\n", engine.ConstraintCount());

    // Solve
    SolveStatus status = engine.Solve();
    printf("\nSolve result: %d (0=OK)\n", (int)status.result);
    printf("DOF: %d\n", status.dof);

    // Query solved positions
    printf("\nSolved corner positions:\n");
    Vec3 pp0 = engine.GetPointPosition(l0i.point[0]);
    Vec3 pp1 = engine.GetPointPosition(l0i.point[1]);
    Vec3 pp2 = engine.GetPointPosition(l2i.point[0]);
    Vec3 pp3 = engine.GetPointPosition(l2i.point[1]);
    printf("  P0: (%.1f, %.1f, %.1f)\n", pp0.x, pp0.y, pp0.z);
    printf("  P1: (%.1f, %.1f, %.1f)\n", pp1.x, pp1.y, pp1.z);
    printf("  P2: (%.1f, %.1f, %.1f)\n", pp2.x, pp2.y, pp2.z);
    printf("  P3: (%.1f, %.1f, %.1f)\n", pp3.x, pp3.y, pp3.z);

    // Get edges for rendering
    auto edges = engine.GetEdges();
    printf("\nEdges for rendering: %zu\n", edges.size());

    // Measure entity length
    double len = engine.GetEntityLength(l0);
    printf("L0 length: %.1f\n", len);

    // Group management
    auto groups = engine.GetGroupOrder();
    printf("\nGroups (%zu):\n", groups.size());
    for(auto &gh : groups) {
        GroupInfo gi = engine.GetGroupInfo(gh);
        printf("  [%u] %s (order=%d)\n", gh.v, gi.name.c_str(), gi.order);
    }

    // Undo test
    engine.SaveUndo();
    engine.SetConstraintValue(engine.GetAllConstraints().back(), 50.0);
    engine.Solve();
    Vec3 afterChange = engine.GetPointPosition(l1i.point[1]);
    printf("\nAfter changing height to 50: P2.y = %.1f\n", afterChange.y);

    engine.Undo();
    engine.Solve();
    Vec3 afterUndo = engine.GetPointPosition(l1i.point[1]);
    printf("After undo: P2.y = %.1f\n", afterUndo.y);

    // Workplane projection test
    double u, v;
    engine.ProjectOntoWorkplane(xyPlane, Vec3(10, 20, 0), &u, &v);
    printf("\nProject (10,20,0) onto XY plane: u=%.1f, v=%.1f\n", u, v);

    printf("\nDone!\n");
    return 0;
}
