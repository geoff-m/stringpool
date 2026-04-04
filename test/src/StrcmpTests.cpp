#include <gtest/gtest.h>
#include "stringpool/stringpool.h"

using namespace stringpool;

void expectStrcmp(int expectation, string_handle left, string_handle right) {
    EXPECT_EQ(expectation, left.strcmp(right));
    EXPECT_EQ(-expectation, right.strcmp(left));
}

TEST(Strcmp, Empty) {
    pool p;
    auto e = p.intern("");
    expectStrcmp(0, e, e);
}

TEST(Strcmp, L1) {
    pool p;
    auto e = p.intern("a");
    expectStrcmp(0, e, e);
}

TEST(Strcmp, L10) {
    pool p;
    auto e = p.intern("0123456789");
    expectStrcmp(0, e, e);
}

TEST(Strcmp, L5) {
    pool p;
    auto a = p.intern("apples");
    auto b = p.intern("bananas");
    expectStrcmp(-1, a, b);
}

TEST(Strcmp, ConcatAtomConcatLongLong) {
    pool p;
    auto a = p.concat(
        p.intern("a"),
        p.concat(
            p.intern("leaf0123456789"),
            p.intern("leaf9876543210")));
    auto b = p.intern("b");
    expectStrcmp(-1, a, b);
}
