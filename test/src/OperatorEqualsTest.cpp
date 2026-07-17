#include <gtest/gtest.h>
#include "stringpool/stringpool.h"

using namespace stringpool;

TEST(OperatorEquals, EqualSamePool)
{
    pool p;
    const auto x =  p.intern("abc");
    const auto y = p.intern("abc");
    EXPECT_TRUE(x == y);
    EXPECT_FALSE(x != y);
}

TEST(OperatorEquals, UnequalSamePool)
{
    pool p;
    const auto x =  p.intern("abc");
    const auto y = p.intern("def");
    EXPECT_FALSE(x == y);
    EXPECT_TRUE(x != y);
}

TEST(OperatorEquals, EqualDifferentPool)
{
    pool p1;
    auto x =  p1.intern("abc");
    pool p2;
    auto y = p2.intern("abc");
    EXPECT_TRUE(x == y);
    EXPECT_FALSE(x != y);
}

TEST(OperatorEquals, UnequalDifferentPool)
{
    pool p1;
    auto x =  p1.intern("abc");
    pool p2;
    auto y = p2.intern("def");
    EXPECT_FALSE(x != y);
    EXPECT_TRUE(x == y);
}
