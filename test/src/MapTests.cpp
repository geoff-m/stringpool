#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include <map>

using namespace stringpool;

TEST(Map, Create)
{
    std::map<string_handle, int> m;
}

TEST(Map, Order) {
    std::map<string_handle, int> m;
    pool p;
    m.insert({p.intern("b"), 0});
    m.insert({p.intern("c"), 0});
    m.insert({p.intern("a"), 0});
    auto it = m.begin();
    EXPECT_EQ("a", it++->first.to_string());
    EXPECT_EQ("b", it++->first.to_string());
    EXPECT_EQ("c", it->first.to_string());
}