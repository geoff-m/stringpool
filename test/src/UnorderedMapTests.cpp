#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include <unordered_map>

using namespace stringpool;

TEST(UnorderedMap, Create) {
    std::unordered_map<string_handle, int> m;
}

TEST(UnorderedMap, Insert) {
    pool p;
    std::unordered_map<string_handle, int> m;
    m.insert({p.intern("a"), 0});
    auto it = m.begin();
    EXPECT_EQ("a", it->first.to_string());
}

TEST(UnorderedMap, Find) {
    pool p;
    std::unordered_map<string_handle, int> m;
    auto s1 = p.intern("ab");
    auto s2 = p.concat(p.intern("a"), p.intern("b"));
    m.insert({s1, 0});
    auto it = m.find(s2);
    EXPECT_NE(it, m.end());
}
