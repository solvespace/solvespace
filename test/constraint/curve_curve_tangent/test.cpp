#include "harness.h"

TEST_CASE(arc_arc_roundtrip) {
    CHECK_LOAD("arc_arc.slvs");
    CHECK_RENDER("arc_arc.png");
    CHECK_SAVE("arc_arc.slvs");
}

TEST_CASE(arc_arc_migrate_from_v20) {
    CHECK_LOAD("arc_arc_v20.slvs");
    CHECK_SAVE("arc_arc.slvs");
}

TEST_CASE(arc_arc_migrate_from_v22) {
    CHECK_LOAD("arc_arc_v22.slvs");
    CHECK_SAVE("arc_arc.slvs");
}

TEST_CASE(arc_cubic_roundtrip) {
    CHECK_LOAD("arc_cubic.slvs");
    CHECK_RENDER("arc_cubic.png");
    CHECK_SAVE("arc_cubic.slvs");
}

TEST_CASE(arc_cubic_migrate_from_v20) {
    CHECK_LOAD("arc_cubic_v20.slvs");
    CHECK_SAVE("arc_cubic.slvs");
}

TEST_CASE(arc_cubic_migrate_from_v22) {
    CHECK_LOAD("arc_cubic_v22.slvs");
    CHECK_SAVE("arc_cubic.slvs");
}
