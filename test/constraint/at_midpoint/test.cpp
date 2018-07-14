#include "harness.h"

TEST_CASE(line_pt_normal_roundtrip) {
    CHECK_LOAD("line_pt_normal.slvs");
    CHECK_RENDER("line_pt_normal.png");
    CHECK_SAVE("line_pt_normal.slvs");
}

TEST_CASE(line_pt_normal_migrate_from_v20) {
    CHECK_LOAD("line_pt_normal_v20.slvs");
    CHECK_SAVE("line_pt_normal.slvs");
}

TEST_CASE(line_pt_normal_migrate_from_v22) {
    CHECK_LOAD("line_pt_normal_v22.slvs");
    CHECK_SAVE("line_pt_normal.slvs");
}

TEST_CASE(line_pt_free_in_3d_roundtrip) {
    CHECK_LOAD("line_pt_free_in_3d.slvs");
    CHECK_RENDER("line_pt_free_in_3d.png");
    CHECK_SAVE("line_pt_free_in_3d.slvs");
}

TEST_CASE(line_pt_free_in_3d_migrate_from_v20) {
    CHECK_LOAD("line_pt_free_in_3d_v20.slvs");
    CHECK_SAVE("line_pt_free_in_3d.slvs");
}

TEST_CASE(line_pt_free_in_3d_migrate_from_v22) {
    CHECK_LOAD("line_pt_free_in_3d_v22.slvs");
    CHECK_SAVE("line_pt_free_in_3d.slvs");
}

TEST_CASE(line_plane_normal_roundtrip) {
    CHECK_LOAD("line_plane_normal.slvs");
    CHECK_RENDER("line_plane_normal.png");
    CHECK_SAVE("line_plane_normal.slvs");
}

TEST_CASE(line_plane_normal_migrate_from_v20) {
    CHECK_LOAD("line_plane_normal_v20.slvs");
    CHECK_SAVE("line_plane_normal.slvs");
}

TEST_CASE(line_plane_normal_migrate_from_v22) {
    CHECK_LOAD("line_plane_normal_v22.slvs");
    CHECK_SAVE("line_plane_normal.slvs");
}

TEST_CASE(line_plane_free_in_3d_roundtrip) {
    CHECK_LOAD("line_plane_free_in_3d.slvs");
    CHECK_RENDER("line_plane_free_in_3d.png");
    CHECK_SAVE("line_plane_free_in_3d.slvs");
}

TEST_CASE(line_plane_free_in_3d_migrate_from_v20) {
    CHECK_LOAD("line_plane_free_in_3d_v20.slvs");
    CHECK_SAVE("line_plane_free_in_3d.slvs");
}

TEST_CASE(line_plane_free_in_3d_migrate_from_v22) {
    CHECK_LOAD("line_plane_free_in_3d_v22.slvs");
    CHECK_SAVE("line_plane_free_in_3d.slvs");
}
