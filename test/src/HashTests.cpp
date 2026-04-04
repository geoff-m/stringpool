#include <gtest/gtest.h>
#include "stringpool/stringpool.h"

using namespace stringpool;

TEST(Hash, Empty) {
    pool p1;
    auto e1 = p1.intern("");
    auto h1 = e1.hash();
    pool p2;
    auto e2 = p2.intern("");
    auto h2 = e2.hash();
    EXPECT_EQ(h1, h2);
}