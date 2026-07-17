#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include "Utility.h"

using namespace stringpool;

TEST(DefaultString, Create) {
    const string_handle d;
    pool p;
    const string_handle empty = p.intern("");
    expectEqual(empty, d);
}

TEST(DefaultString, ConcatSelf) {
    const string_handle d;
    pool p;
    const auto e = p.concat(d, d);
    expectEqual(d, e);
}

TEST(DefaultString, ConcatTwo) {
    const string_handle d1;
    const string_handle d2;
    pool p;
    const auto c = p.concat(d1, d2);
    expectEqual(d1, c);
    expectEqual(d2, c);
}

TEST(DefaultString, ConcatLeftSamePool) {
    const string_handle d;
    pool p;
    const auto a = p.intern("a");
    const auto b = p.concat(d, a);
    expectEqual(a, b);
}

TEST(DefaultString, ConcatRightSamePool) {
    const string_handle d;
    pool p;
    const auto a = p.intern("a");
    const auto b = p.concat(a, d);
    expectEqual(a, b);
}

TEST(DefaultString, ConcatLeftOtherPool) {
    const string_handle d;
    pool p1;
    const auto a = p1.intern("a");
    pool p2;
    EXPECT_ANY_THROW((void)p2.concat(d, a));
}

TEST(DefaultString, ConcatRightOtherPool) {
    const string_handle d;
    pool p1;
    const auto a = p1.intern("a");
    pool p2;
    EXPECT_ANY_THROW((void)p2.concat(a, d));
}