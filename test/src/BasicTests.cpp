#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include "Utility.h"

using namespace stringpool;

TEST(Basic, Create) {
    pool p;
}

TEST(Basic, Add) {
    pool p;
    const auto string = "hello";
    const auto interned = p.intern(string);
    expectEqual(interned, string);
    expectLength(strlen(string), interned);
}

TEST(Basic, Dedup) {
    pool p;
    const auto string = "hello";
    const auto interned1 = p.intern(string);
    const auto interned2 = p.intern(string);
    expectEqual(interned1, interned2);
    expectEqual(interned1, string);
}

TEST(Basic, Empty) {
    pool p;
    const auto string = "";
    const auto interned = p.intern(string);
    expectEqual(interned, string);
    expectLength(0, interned);
}

TEST(Basic, DedupEmpty) {
    pool p;
    const auto string = "";
    const auto interned = p.intern(string);
    expectEqual(interned, string);
}

TEST(Basic, Concat1Plus1) {
    pool p;
    auto ia = p.intern("a");
    auto ib = p.intern("b");
    auto iab = p.concat(ia, ib);
    expectEqual(iab, "ab");
    expectLength(2, iab);
}

TEST(Basic, ConcatDedup) {
    pool p;
    auto ia = p.intern("a");
    auto ib = p.intern("b");
    auto iab = p.concat(ia, ib);
    expectEqual(iab, "ab");
    auto atomAgain = p.intern("ab");
    expectEqual(iab, atomAgain);
    auto concatAgain = p.concat(ia, ib);
    expectEqual(iab, concatAgain);
}

TEST(Basic, Concat8Plus8) {
    pool p;
    auto ia = p.intern("aaaaaaaa");
    auto ib = p.intern("bbbbbbbb");
    auto iab = p.concat(ia, ib);
    expectEqual(iab, "aaaaaaaabbbbbbbb");
}

TEST(Basic, Concat9Plus9) {
    pool p;
    auto ia = p.intern("aaaaaaaaa");
    auto ib = p.intern("bbbbbbbbb");
    auto iab = p.concat(ia, ib);
    expectEqual(iab, "aaaaaaaaabbbbbbbbb");
}

TEST(Basic, Concat0Plus9) {
    pool p;
    auto ia = p.intern("");
    auto ib = p.intern("bbbbbbbbb");
    auto iab = p.concat(ia, ib);
    expectEqual(iab, "bbbbbbbbb");
}

TEST(Basic, Concat9Plus0) {
    pool p;
    auto ia = p.intern("aaaaaaaaa");
    auto ib = p.intern("");
    auto iab = p.concat(ia, ib);
    expectEqual(iab, "aaaaaaaaa");
}

TEST(Basic, Concat0Plus0) {
    pool p;
    auto ia = p.intern("");
    auto ib = p.intern("");
    auto iab = p.concat(ia, ib);
    expectEqual(iab, "");
    expectEqual(iab, ia);
    expectEqual(iab, ib);
}

TEST(Basic, CopyConcatShortLong) {
    pool p;
    auto a = p.concat(
        p.intern("a"),
        p.intern("leaf0123456789"));
    constexpr auto len = 64;
    char buf[len] = {};
    a.copy(buf, len);
    EXPECT_STREQ("aleaf0123456789", buf);
}

TEST(Basic, CopyConcatLongLong) {
    pool p;
    auto a = p.concat(
        p.intern("leaf0123456789"),
        p.intern("leaf9876543210"));
    constexpr auto len = 64;
    char buf[len] = {};
    a.copy(buf, len);
    EXPECT_STREQ("leaf0123456789leaf9876543210", buf);
}

TEST(Basic, CopyConcatAtomConcatLongLong) {
    pool p;
    auto a = p.concat(
    p.intern("a"),
    p.concat(
        p.intern("leaf0123456789"),
        p.intern("leaf9876543210")));
    constexpr auto len = 64;
    char buf[len] = {};
    a.copy(buf, len);
    EXPECT_STREQ("aleaf0123456789leaf9876543210", buf);
}

TEST(Basic, CopyConcatConcatLongLongAtom) {
    pool p;
    auto a = p.concat(
        p.concat(
        p.intern("leaf0123456789"),
        p.intern("leaf9876543210")),
        p.intern("a"));
    constexpr auto len = 64;
    char buf[len] = {};
    a.copy(buf, len);
    EXPECT_STREQ("leaf0123456789leaf9876543210a", buf);
}