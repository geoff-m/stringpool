#include <cstring>
#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include "Utility.h"

using namespace stringpool;

TEST(CrossPool, Equals) {
    pool p1;
    pool p2;
    const auto i1 = p1.intern("hello");
    const auto i2 = p2.intern("hello");
    expectEqual(i1, i2);
}

TEST(CrossPool, ConcatSamePoolToDifferent) {
    pool p1;
    pool p2;
    const auto a = p1.intern("a");
    EXPECT_ANY_THROW(p2.concat(a, a));
}

TEST(CrossPool, ConactStraddleToDifferent) {
    pool p1;
    pool p2;
    const auto a = p1.intern("a");
    const auto b = p2.intern("b");
    EXPECT_ANY_THROW(p2.concat(a, b));
    EXPECT_ANY_THROW(p2.concat(b, a));
    EXPECT_ANY_THROW(p1.concat(a, b));
    EXPECT_ANY_THROW(p1.concat(b, a));
}