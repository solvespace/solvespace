#include "solvespace.h"

#include "harness.h"

TEST_CASE(normal_roundtrip) {
    CHECK_LOAD("normal.slvs");
    CHECK_RENDER("normal.png");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(normal_migrate_from_v20) {
    CHECK_LOAD("normal_v20.slvs");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(normal_migrate_from_v22) {
    CHECK_LOAD("normal_v22.slvs");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(free_in_3d_roundtrip) {
    CHECK_LOAD("free_in_3d.slvs");
    CHECK_RENDER("free_in_3d.png");
    CHECK_SAVE("free_in_3d.slvs");
}

TEST_CASE(free_in_3d_migrate_from_v20) {
    CHECK_LOAD("free_in_3d_v20.slvs");
    CHECK_SAVE("free_in_3d.slvs");
}

TEST_CASE(free_in_3d_migrate_from_v22) {
    CHECK_LOAD("free_in_3d_v22.slvs");
    CHECK_SAVE("free_in_3d.slvs");
}

TEST_CASE(normal_dof) {
    CHECK_LOAD("normal.slvs");
    SS.GenerateAll(SolveSpaceUI::Generate::ALL, /*andFindFree=*/true);
    CHECK_RENDER("normal_dof.png");
}

TEST_CASE(free_in_3d_dof) {
    CHECK_LOAD("free_in_3d.slvs");
    SS.GenerateAll(SolveSpaceUI::Generate::ALL, /*andFindFree=*/true);
    CHECK_RENDER_ISO("free_in_3d_dof.png");
}
