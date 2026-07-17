#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include "Utility.h"

using namespace stringpool;

TEST(VariadicConcat, Zero) {
    pool p;
    const auto c = p.concat();
    EXPECT_EQ(0, c.size());
}

TEST(VariadicConcat, LeftZero) {
    pool p;
    const auto c = p.concatLeft();
    EXPECT_EQ(0, c.size());
}

TEST(VariadicConcat, RightZero) {
    pool p;
    const auto c = p.concatRight();
    EXPECT_EQ(0, c.size());
}

TEST(VariadicConcat, One) {
    pool p;
    const auto a = p.intern("abc");
    const auto c = p.concat(a);
    expectEqual(a, c);
}

TEST(VariadicConcat, LeftOne) {
    pool p;
    const auto a = p.intern("abc");
    const auto c = p.concatLeft(a);
    expectEqual(a, c);
}

TEST(VariadicConcat, RightOne) {
    pool p;
    const auto a = p.intern("abc");
    const auto c = p.concatRight(a);
    expectEqual(a, c);
}

string_handle makeLong(pool& pool, char c) {
    std::string s;
    s.resize(1024, c);
    return pool.intern(s);
}

void concatExpectInPool(pool& p, const string_handle& x, const string_handle& y) {
    const auto sizeBefore= p.get_data_size();
    (void)p.concat(x, y);
    const auto sizeAfter = p.get_data_size();
    EXPECT_EQ(sizeBefore, sizeAfter);
}

void concatExpectNotInPool(pool& p, const string_handle& x, const string_handle& y) {
    const auto sizeBefore= p.get_data_size();
    (void)p.concat(x, y);
    const auto sizeAfter = p.get_data_size();
    EXPECT_LE(sizeBefore, sizeAfter);
}

TEST(VariadicConcat, Three) {
    pool p;
    const auto a = makeLong(p, 'a');
    const auto b = makeLong(p, 'b');
    const auto c = makeLong(p, 'c');
    auto concat = p.concat(a, b, c);
    concatExpectInPool(p, a, b);
    concatExpectNotInPool(p, b, c);
}

TEST(VariadicConcat, LeftThree) {
    pool p;
    const auto a = makeLong(p, 'a');
    const auto b = makeLong(p, 'b');
    const auto c = makeLong(p, 'c');
    auto concat = p.concatLeft(a, b, c);
    concatExpectInPool(p, a, b);
    concatExpectNotInPool(p, b, c);
}
TEST(VariadicConcat, RightThree) {
    pool p;
    const auto a = makeLong(p, 'a');
    const auto b = makeLong(p, 'b');
    const auto c = makeLong(p, 'c');
    auto concat = p.concatRight(a, b, c);
    concatExpectNotInPool(p, a, b);
    concatExpectInPool(p, b, c);
}