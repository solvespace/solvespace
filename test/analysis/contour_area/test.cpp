#include "harness.h"

TEST_CASE(normal_roundtrip) {
    SS.showContourAreas = true;
    CHECK_LOAD("normal.slvs");
    CHECK_RENDER("normal.png");
}
