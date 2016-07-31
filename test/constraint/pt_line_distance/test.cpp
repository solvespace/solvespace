#include "harness.h"

TEST_CASE(roundtrip) {
    CHECK_LOAD("normal.slvs");
    CHECK_RENDER("normal.png");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(migrate_from_v20) {
    CHECK_LOAD("normal_v20.slvs");
    CHECK_RENDER("normal.png");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(render_extended) {
    CHECK_LOAD("extended.slvs");
    CHECK_RENDER("extended.png");
}
