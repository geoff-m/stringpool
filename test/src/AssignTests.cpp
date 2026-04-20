#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include "Utility.h"

using namespace stringpool;

TEST(Assign, Simple)
{
    pool p;
    auto a = p.intern("a");
    auto b = p.intern("bb");
    b = a;
    EXPECT_EQ(1, b.size());
}

TEST(Assign, SimpleConcat)
{
    pool p;
    const auto length = 1024;
    auto s1 = std::make_unique<char[]>(length);
    auto s2 = std::make_unique<char[]>(length);
    memset(s1.get(), 'a', length);
    s1.get()[length - 1] = 0;
    memset(s2.get(), 'b', length);
    s2.get()[length - 1] = 0;
    auto a = p.intern(s1.get());
    auto b = p.intern(s2.get());
    auto c = p.concat(a, b);
    a = c;
    EXPECT_EQ((length - 1) * 2, a.size());
}

TEST(Assign, SelfAssign)
{
    pool p;
    auto a = p.intern("a");
    auto* p1 = &a;
    auto* p2 = &a;
    *p1 = *p2;
    EXPECT_EQ(1, a.size());
}
