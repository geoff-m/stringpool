#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include <sstream>

using namespace stringpool;

TEST(OstreamOperator, Atom)
{
    std::stringstream ss;
    pool p;
    const auto* str = "Hello";
    const auto sh = p.intern(str);
    ss << sh;
    EXPECT_EQ(str, ss.str());
}

TEST(OstreamOperator, Concat3)
{
    std::stringstream ss;
    pool p;
    const auto sh = p.concat(p.intern("a"), p.intern("b"), p.intern("c"));
    ss << sh;
    EXPECT_EQ("abc", ss.str());
}

