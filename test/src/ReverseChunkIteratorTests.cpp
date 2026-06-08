#include <cstring>
#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include <format>

using namespace stringpool;

void expectReverseEquals(std::string_view expected, const string_handle& sh) {
    size_t checkedChars = 0;
    for (auto it = sh.rbegin_chunk();
         it != sh.rend_chunk(); ++it) {
        const auto chunk = *it;
        const auto chunkSize = chunk.size();
        const auto* expectedChunk = expected.data() + expected.size() - checkedChars - chunkSize;
        EXPECT_EQ(0, std::memcmp(expectedChunk, chunk.data(), chunkSize));
        checkedChars += chunkSize;
        ASSERT_LE(checkedChars, expected.size());
    }
    EXPECT_EQ(expected.size(), checkedChars);
}

TEST(ReverseChunkIterator, Empty) {
    pool p;
    const auto empty = p.intern("");
    auto it = empty.rbegin_chunk();
    const auto end = empty.rend_chunk();
    EXPECT_NE(end, it);
    EXPECT_EQ(0, (*it).size());
    ++it;
    EXPECT_EQ(end, it);
}

TEST(ReverseChunkIterator, Concat2) {
    pool p;
    std::stringstream ss;
    const auto concat = p.concat(p.intern("a"), p.intern("b"));
    expectReverseEquals("ab", concat);
}

TEST(ReverseChunkIterator, Concat2Long) {
    pool p;
    std::string long1;
    long1.resize(1024, 'x');
    std::string long2;
    long2.resize(1024, 'y');
    const auto concat = p.concat(p.intern(long1.c_str()), p.intern(long2.c_str()));
    const auto expected = long1 + long2;
    expectReverseEquals(expected, concat);
}
