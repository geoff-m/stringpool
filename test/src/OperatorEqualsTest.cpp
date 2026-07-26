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

TEST(OpereatorEquals, CString) {
    pool p;
    const char* cs = "abc";
    const auto sh = p.intern(cs);
    EXPECT_TRUE(cs == sh);
    EXPECT_TRUE(sh == cs);
    EXPECT_FALSE(cs != sh);
    EXPECT_FALSE(sh != cs);
}

TEST(OpereatorEquals, StringView) {
    pool p;
    const std::string_view sv = "abc";
    const auto sh = p.intern(sv);
    EXPECT_TRUE(sv == sh);
    EXPECT_TRUE(sh == sv);
    EXPECT_FALSE(sv != sh);
    EXPECT_FALSE(sh != sv);
}

TEST(OpereatorEquals, String) {
    pool p;
    const std::string s = "abc";
    const auto sh = p.intern(s);
    EXPECT_TRUE(s == sh);
    EXPECT_TRUE(sh == s);
    EXPECT_FALSE(s != sh);
    EXPECT_FALSE(sh != s);
}