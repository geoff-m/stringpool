#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include <format>

using namespace stringpool;

TEST(ChunkIterator, Empty) {
    pool p;
    const auto empty = p.intern("");
    auto it = empty.begin_chunk();
    const auto end = empty.end_chunk();
    EXPECT_NE(end, it);
    EXPECT_STREQ("", (*it).data());
    ++it;
    EXPECT_EQ(end, it);
}

TEST(ChunkIterator, Concat2) {
    pool p;
    const auto concat = p.concat(p.intern("a"), p.intern("b"));
    auto it = concat.begin_chunk();
    const auto end = concat.end_chunk();
    EXPECT_NE(end, it);
    EXPECT_STREQ("a", (*it).data());
    ++it;
    EXPECT_STREQ("b", (*it).data());
    ++it;
    EXPECT_EQ(end, it);
}