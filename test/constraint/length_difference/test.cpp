#include "harness.h"

TEST_CASE(normal_roundtrip) {
    CHECK_LOAD("normal.slvs");
    CHECK_RENDER("normal.png");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(reference_roundtrip) {
    CHECK_LOAD("reference.slvs");
    CHECK_RENDER("reference.png");
    CHECK_SAVE("reference.slvs");
}
