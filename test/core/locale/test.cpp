#include "harness.h"

TEST_CASE(parseable) {
    for(auto locale : Locales()) {
        SetLocale(locale.Culture());
    }
    CHECK_TRUE(true); // didn't crash
}
