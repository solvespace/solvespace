#include "harness.h"

TEST_CASE(line_roundtrip) {
    CHECK_LOAD("line.slvs");
    CHECK_RENDER("line.png");
    CHECK_SAVE("line.slvs");
}

TEST_CASE(line_migrate_from_v20) {
    CHECK_LOAD("line_v20.slvs");
    CHECK_SAVE("line.slvs");
}

TEST_CASE(line_migrate_from_v22) {
    CHECK_LOAD("line_v22.slvs");
    CHECK_SAVE("line.slvs");
}

TEST_CASE(pt_pt_roundtrip) {
    CHECK_LOAD("pt_pt.slvs");
    CHECK_RENDER("pt_pt.png");
    CHECK_SAVE("pt_pt.slvs");
}

TEST_CASE(pt_pt_migrate_from_v20) {
    CHECK_LOAD("pt_pt_v20.slvs");
    CHECK_SAVE("pt_pt.slvs");
}

TEST_CASE(pt_pt_migrate_from_v22) {
    CHECK_LOAD("pt_pt_v22.slvs");
    CHECK_SAVE("pt_pt.slvs");
}
