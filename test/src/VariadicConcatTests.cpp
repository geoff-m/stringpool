#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include "Utility.h"

using namespace stringpool;

TEST(VariadicConcat, Zero) {
    pool p;
    auto c = p.concat();
    EXPECT_EQ(0, c.size());
}

TEST(VariadicConcat, LeftZero) {
    pool p;
    auto c = p.concatLeft();
    EXPECT_EQ(0, c.size());
}

TEST(VariadicConcat, RightZero) {
    pool p;
    auto c = p.concatRight();
    EXPECT_EQ(0, c.size());
}

TEST(VariadicConcat, One) {
    pool p;
    auto a = p.intern("abc");
    auto c = p.concat(a);
    expectEqual(a, c);
}

TEST(VariadicConcat, LeftOne) {
    pool p;
    auto a = p.intern("abc");
    auto c = p.concatLeft(a);
    expectEqual(a, c);
}

TEST(VariadicConcat, RightOne) {
    pool p;
    auto a = p.intern("abc");
    auto c = p.concatRight(a);
    expectEqual(a, c);
}

string_handle makeLong(pool& pool, char c) {
    std::string s;
    s.resize(1024, c);
    return pool.intern(s);
}

void concatExpectInPool(pool& p, string_handle x, string_handle y) {
    const auto sizeBefore= p.get_data_size();
    (void)p.concat(x, y);
    const auto sizeAfter = p.get_data_size();
    EXPECT_EQ(sizeBefore, sizeAfter);
}

void concatExpectNotInPool(pool& p, string_handle x, string_handle y) {
    const auto sizeBefore= p.get_data_size();
    (void)p.concat(x, y);
    const auto sizeAfter = p.get_data_size();
    EXPECT_LE(sizeBefore, sizeAfter);
}

TEST(VariadicConcat, Three) {
    pool p;
    auto a = makeLong(p, 'a');
    auto b = makeLong(p, 'b');
    auto c = makeLong(p, 'c');
    auto concat = p.concat(a, b, c);
    concatExpectInPool(p, a, b);
    concatExpectNotInPool(p, b, c);
}

TEST(VariadicConcat, LeftThree) {
    pool p;
    auto a = makeLong(p, 'a');
    auto b = makeLong(p, 'b');
    auto c = makeLong(p, 'c');
    auto concat = p.concatLeft(a, b, c);
    concatExpectInPool(p, a, b);
    concatExpectNotInPool(p, b, c);
}
TEST(VariadicConcat, RightThree) {
    pool p;
    auto a = makeLong(p, 'a');
    auto b = makeLong(p, 'b');
    auto c = makeLong(p, 'c');
    auto concat = p.concatRight(a, b, c);
    concatExpectNotInPool(p, a, b);
    concatExpectInPool(p, b, c);
}