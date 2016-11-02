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

TEST_CASE(pi) {
    CHECK_LOAD("pi.slvs");
    CHECK_RENDER("pi.png");
}

TEST_CASE(tau) {
    CHECK_LOAD("tau.slvs");
    CHECK_RENDER("tau.png");
}
