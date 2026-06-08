#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include <format>

using namespace stringpool;

TEST(Readme, ChunkLoop) {
    pool p;
    const auto s = p.intern("a");
    std::string result;
    for (auto it = s.begin_chunk();
         it != s.end_chunk(); ++it) {
        const auto chunk = *it;
        result += std::string(chunk.data(), chunk.size());
    }
    EXPECT_EQ("a", result);
}
