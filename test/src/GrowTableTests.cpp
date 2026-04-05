#include <gtest/gtest.h>
#include "stringpool/stringpool.h"

using namespace stringpool;

TEST(GrowTable, Basic) {
    const auto TOTAL_ENTRIES = 100;
    pool p(TOTAL_ENTRIES, 1000);
    for (int i = 0; i < TOTAL_ENTRIES; ++i) {
        (void)p.intern(std::to_string(i).c_str());
    }
}
