#include <cstring>
#include <gtest/gtest.h>
#include "stringpool/stringpool.h"

using namespace stringpool;

TEST(ToString, Empty) {
    pool p;
    const auto s = p.intern("");
    const auto ss = s.to_string();
    EXPECT_TRUE(ss.empty());
}

TEST(ToString, ShortAtom) {
    pool p;
    const auto i = p.intern("hello");
    const auto ss = i.to_string();
    EXPECT_EQ("hello", ss);
}

TEST(ToString, LongAtom) {
    pool p;
    const std::string expected(1024, 'a');
    const auto i = p.intern(expected.c_str());
    const auto ss = i.to_string();
    EXPECT_EQ(expected, ss);
}

TEST(ToString, ConcatLongChildren) {
    pool p;
    const std::string a(1024, 'a');
    const std::string b(1024, 'b');
    const auto ia = p.intern(a.c_str());
    const auto ib = p.intern(b.c_str());
    const auto iab = p.concat(ia, ib);
    const auto expected = a + b;
    const auto actual = iab.to_string();
    EXPECT_EQ(expected, actual);
}

TEST(ToString, ConcatLeftShort) {
    pool p;
    const std::string a = "a";
    const std::string b(1024, 'b');
    const auto ia = p.intern(a.c_str());
    const auto ib = p.intern(b.c_str());
    const auto iab = p.concat(ia, ib);
    const auto expected = a + b;
    const auto actual = iab.to_string();
    EXPECT_EQ(expected, actual);
}

TEST(ToString, ConcatRightShort) {
    pool p;
    const std::string a(1024, 'a');
    const std::string b = "b";
    const auto ia = p.intern(a.c_str());
    const auto ib = p.intern(b.c_str());
    const auto iab = p.concat(ia, ib);
    const auto expected = a + b;
    const auto actual = iab.to_string();
    EXPECT_EQ(expected, actual);
}