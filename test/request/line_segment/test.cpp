#include "harness.h"

TEST_CASE(load_v20) {
    CHECK_LOAD("line_segment_v20.slvs");
    CHECK_RENDER("line_segment.png");
}

TEST_CASE(roundtrip_v21) {
    CHECK_LOAD("line_segment_v21.slvs");
    CHECK_RENDER("line_segment.png");
    CHECK_SAVE("line_segment_v21.slvs");
}

TEST_CASE(migrate_v20_to_v21) {
    CHECK_LOAD("line_segment_v20.slvs");
    CHECK_SAVE("line_segment_v21.slvs");
}
