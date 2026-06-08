#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include <format>

using namespace stringpool;

TEST(Format, Empty) {
    pool p;
    const auto empty = p.intern("");
    const auto actual = std::format("{}", empty);
    EXPECT_EQ("", actual);
}

TEST(Format, OneShort) {
    pool p;
    const auto expected = "h";
    const auto h = p.intern(expected);
    const auto actual = std::format("{}", h);
    EXPECT_EQ(expected, actual);
}

TEST(Format, TwoShort) {
    pool p;
    const auto left = p.intern("foo");
    const auto right = p.intern("bar");
    const auto concat = p.concat(left, right);
    const auto actual = std::format("{}", concat);
    EXPECT_EQ("foobar", actual);
}
