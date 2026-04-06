#include <cstring>
#include <gtest/gtest.h>
#include "stringpool/stringpool.h"

using namespace stringpool;

TEST(ToString, Empty) {
    pool p;
    auto s = p.intern("");
    auto ss = s.to_string();
    EXPECT_TRUE(ss.empty());
}

TEST(ToString, ShortAtom) {
    pool p;
    auto i = p.intern("hello");
    auto ss = i.to_string();
    EXPECT_EQ("hello", ss);
}

TEST(ToString, LongAtom) {
    pool p;
    std::string expected(1024, 'a');
    auto i = p.intern(expected.c_str());
    auto ss = i.to_string();
    EXPECT_EQ(expected, ss);
}

TEST(ToString, ConcatLongChildren) {
    pool p;
    std::string a(1024, 'a');
    std::string b(1024, 'b');
    auto ia = p.intern(a.c_str());
    auto ib = p.intern(b.c_str());
    auto iab = p.concat(ia, ib);
    auto expected = a + b;
    auto actual = iab.to_string();
    EXPECT_EQ(expected, actual);
}

TEST(ToString, ConcatLeftShort) {
    pool p;
    std::string a = "a";
    std::string b(1024, 'b');
    auto ia = p.intern(a.c_str());
    auto ib = p.intern(b.c_str());
    auto iab = p.concat(ia, ib);
    auto expected = a + b;
    auto actual = iab.to_string();
    EXPECT_EQ(expected, actual);
}

TEST(ToString, ConcatRightShort) {
    pool p;
    std::string a(1024, 'a');
    std::string b = "b";
    auto ia = p.intern(a.c_str());
    auto ib = p.intern(b.c_str());
    auto iab = p.concat(ia, ib);
    auto expected = a + b;
    auto actual = iab.to_string();
    EXPECT_EQ(expected, actual);
}