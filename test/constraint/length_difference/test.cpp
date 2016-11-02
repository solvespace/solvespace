#include "harness.h"

TEST_CASE(normal_roundtrip) {
    CHECK_LOAD("normal.slvs");
    CHECK_RENDER("normal.png");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(normal_migrate_from_v22) {
    CHECK_LOAD("normal_v22.slvs");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(reference_roundtrip) {
    CHECK_LOAD("reference.slvs");
    CHECK_RENDER("reference.png");
    CHECK_SAVE("reference.slvs");
}

TEST_CASE(reference_migrate_from_v22) {
    CHECK_LOAD("reference_v22.slvs");
    CHECK_SAVE("reference.slvs");
}
