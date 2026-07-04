#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include <format>

using namespace stringpool;

TEST(Format, Empty) {
    pool p;
    const auto empty = p.intern("");
    const auto actual = std::format("x{}y", empty);
    EXPECT_EQ("xy", actual);
}

TEST(Format, OneShort) {
    pool p;
    const auto h = p.intern("h");
    const auto actual = std::format("x{}y", h);
    EXPECT_EQ("xhy", actual);
}

TEST(Format, TwoShort) {
    pool p;
    const auto left = p.intern("foo");
    const auto right = p.intern("bar");
    const auto concat = p.concat(left, right);
    const auto actual = std::format("x{}y", concat);
    EXPECT_EQ("xfoobary", actual);
}

TEST(Format, TwoLong) {
    pool p;
    constexpr auto PIECE_LENGTH = 300;
    char leftStr[PIECE_LENGTH];
    memset(leftStr, 'a', PIECE_LENGTH);
    leftStr[PIECE_LENGTH - 1] = 0;
    char rightStr[PIECE_LENGTH];
    memset(rightStr, 'b', PIECE_LENGTH);
    rightStr[PIECE_LENGTH - 1] = 0;
    const auto left = p.intern(leftStr);
    const auto right = p.intern(rightStr);
    const auto concat = p.concat(left, right);
    const auto actual = std::format("x{}y", concat);
    EXPECT_EQ('x', actual[0]);
    for (int i = 1; i < PIECE_LENGTH; ++i) {
        EXPECT_EQ('a', actual[i]);
    }
    for (int i = PIECE_LENGTH; i < PIECE_LENGTH * 2 - 1; ++i) {
        EXPECT_EQ('b', actual[i]);
    }
    EXPECT_EQ('y', actual[PIECE_LENGTH * 2 -1]);
}
