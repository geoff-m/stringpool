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
    EXPECT_EQ(0, (*it).size());
    ++it;
    EXPECT_EQ(end, it);
}

TEST(ChunkIterator, Concat2) {
    pool p;
    const auto concat = p.concat(p.intern("a"), p.intern("b"));
    std::stringstream ss;
    for (auto it = concat.begin_chunk(); it != concat.end_chunk(); ++it)
        ss << *it;
    EXPECT_STREQ("ab", ss.str().c_str());
}

TEST(ChunkIterator, Concat2Long) {
    pool p;
    std::string long1;
    long1.resize(1024, 'x');
    std::string long2;
    long2.resize(1024, 'y');
    const auto concat = p.concat(p.intern(long1.c_str()), p.intern(long2.c_str()));
    const auto expected = long1 + long2;
    std::stringstream ss;
    for (auto it = concat.begin_chunk(); it != concat.end_chunk(); ++it)
        ss << *it;
    EXPECT_EQ(expected, ss.str());
}
