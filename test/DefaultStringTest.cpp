#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include "Utility.h"

using namespace stringpool;

TEST(DefaultString, Create) {
    string_handle d;
    pool p;
    string_handle empty = p.intern("");
    expectEqual(empty, d);
}

TEST(DefaultString, ConcatSelf) {
    string_handle d;
    pool p;
    auto e = p.concat(d, d);
    expectEqual(d, e);
}

TEST(DefaultString, ConcatTwo) {
    string_handle d1;
    string_handle d2;
    pool p;
    auto c = p.concat(d1, d2);
    expectEqual(d1, c);
    expectEqual(d2, c);
}

TEST(DefaultString, ConcatLeftSamePool) {
    string_handle d;
    pool p;
    auto a = p.intern("a");
    auto b = p.concat(d, a);
    expectEqual(a, b);
}

TEST(DefaultString, ConcatRightSamePool) {
    string_handle d;
    pool p;
    auto a = p.intern("a");
    auto b = p.concat(a, d);
    expectEqual(a, b);
}

TEST(DefaultString, ConcatLeftOtherPool) {
    string_handle d;
    pool p1;
    auto a = p1.intern("a");
    pool p2;
    EXPECT_ANY_THROW((void)p2.concat(d, a));
}

TEST(DefaultString, ConcatRightOtherPool) {
    string_handle d;
    pool p1;
    auto a = p1.intern("a");
    pool p2;
    EXPECT_ANY_THROW((void)p2.concat(a, d));
}