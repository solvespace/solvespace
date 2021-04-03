#include "harness.h"

TEST_CASE(normal_roundtrip) {
    CHECK_LOAD("normal.slvs");
    CHECK_RENDER("normal.png");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(linked_roundtrip) {
    CHECK_LOAD("linked.slvs");
    CHECK_RENDER("linked.png");
    CHECK_SAVE("linked.slvs");
}
