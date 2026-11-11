#include "solvespace.h"

#include "harness.h"

// angle.slvs is a right triangle: a horizontal line 100 mm long, a vertical
// line, and a third line closing them, with an angle constraint (handle 7)
// between that third line and the horizontal one.
static const hConstraint HANGLE = { 7 };
// The endpoints of the vertical line, whose length is 100*tan(angle).
static const hEntity VERTICAL_A = { 0x00050001 };
static const hEntity VERTICAL_B = { 0x00050002 };

// Change the value of a dimension and re-solve, the same way that the GUI
// does after the user edits one.
static SolveResult SetValueAndSolve(hConstraint hc, double value) {
    Constraint *c = SK.GetConstraint(hc);
    hGroup hg = c->group;
    c->valA = value;
    SS.MarkGroupDirty(hg);
    SS.GenerateAll();
    return SK.GetGroup(hg)->solved.how;
}

static double VerticalLength() {
    return SK.GetEntity(VERTICAL_B)->PointGetNum().y -
           SK.GetEntity(VERTICAL_A)->PointGetNum().y;
}

TEST_CASE(angle_step_over_critical_point) {
    CHECK_LOAD("angle.slvs");

    // Both of these angles are perfectly solvable, but the undamped Newton
    // step from the 60 degree solution towards the 30 degree one used to
    // land right on top of a critical point of the angle equation, from
    // where the next step threw the geometry off to infinity.
    CHECK_TRUE(SetValueAndSolve(HANGLE, 60.0) == SolveResult::OKAY);
    CHECK_EQ_EPS(VerticalLength(), 100*tan(60*PI/180));

    CHECK_TRUE(SetValueAndSolve(HANGLE, 30.0) == SolveResult::OKAY);
    CHECK_EQ_EPS(VerticalLength(), 100*tan(30*PI/180));
}

TEST_CASE(redundant_parallel_on_first_solve) {
    for(auto pair : { std::make_pair(0x000b0000u, 0x000e0000u),
                     std::make_pair(0x000c0000u, 0x00100000u) }) {
        CHECK_LOAD("redundant.slvs");
        hGroup hg = { 2 };
        CHECK_TRUE(SK.GetGroup(hg)->solved.how == SolveResult::OKAY);
        Constraint c = {};
        c.type = Constraint::Type::PARALLEL;
        c.group = hg;
        c.workplane.v = 0x80020000;
        c.entityA.v = pair.first;
        c.entityB.v = pair.second;
        Constraint::AddConstraint(&c, false);
        for(int i = 0; i < 10; i++) {
            SS.MarkGroupDirty(hg);
            SS.GenerateAll();
            Group *g = SK.GetGroup(hg);
            CHECK_TRUE(g->solved.how == SolveResult::REDUNDANT_OKAY);
            CHECK_TRUE(g->solved.dof == 3);
        }
    }
}

TEST_CASE(redundant_parallel_transformed) {
    hGroup hg = { 2 };
    for(double angle : { 0.0, 0.3, 1.2, 2.7 }) {
        for(double scale : { 0.1, 1.0, 10.0 }) {
            for(auto pair : { std::make_pair(0x000b0000u, 0x000e0000u),
                             std::make_pair(0x000c0000u, 0x00100000u) }) {
                CHECK_LOAD("redundant.slvs");
                for(Entity &e : SK.entity) {
                    if(e.group != hg || !e.IsPoint()) continue;
                    Vector p = e.PointGetNum().RotatedAbout(Vector::From(0, 0, 1), angle);
                    e.PointForceTo(p.ScaledBy(scale).Plus(Vector::From(10, -20, 0)));
                }
                for(Param &p : SK.param) {
                    if((p.h.v >> 16) >= 4 && (p.h.v & 0xffff) == 0x40) p.val *= scale;
                }
                for(Constraint &c : SK.constraint) {
                    if(c.type == Constraint::Type::DIAMETER) c.valA *= scale;
                }
                Constraint c = {};
                c.type = Constraint::Type::PARALLEL;
                c.group = hg;
                c.workplane.v = 0x80020000;
                c.entityA.v = pair.first;
                c.entityB.v = pair.second;
                Constraint::AddConstraint(&c, false);
                for(int i = 0; i < 3; i++) {
                    SS.MarkGroupDirty(hg);
                    SS.GenerateAll();
                    Group *g = SK.GetGroup(hg);
                    CHECK_TRUE(g->solved.how == SolveResult::REDUNDANT_OKAY);
                    CHECK_TRUE(g->solved.dof == 3);
                }
            }
        }
    }
}

TEST_CASE(rank_preserves_small_independent_columns) {
    System sys = {};
    sys.mat.m = 2;
    sys.mat.n = 2;
    sys.mat.A.num.resize(2, 2);
    sys.mat.A.num.insert(0, 0) = 1;
    sys.mat.A.num.insert(1, 1) = 1e-8;
    sys.mat.A.num.makeCompressed();
    CHECK_TRUE(sys.CalculateRank() == 2);
    sys.mat.A.num.coeffRef(0, 1) = 1;
    sys.mat.A.num.makeCompressed();
    CHECK_TRUE(sys.CalculateRank() == 2);
    sys.mat.A.num.coeffRef(1, 1) = 0;
    CHECK_TRUE(sys.CalculateRank() == 1);
}
