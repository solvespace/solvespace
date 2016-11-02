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

TEST_CASE(other_roundtrip) {
    CHECK_LOAD("other.slvs");
    CHECK_RENDER("other.png");
    CHECK_SAVE("other.slvs");
}

TEST_CASE(other_migrate_from_v20) {
    CHECK_LOAD("other_v20.slvs");
    CHECK_SAVE("other.slvs");
}

TEST_CASE(other_migrate_from_v22) {
    CHECK_LOAD("other_v22.slvs");
    CHECK_SAVE("other.slvs");
}
