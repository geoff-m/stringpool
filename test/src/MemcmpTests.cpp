#include <gtest/gtest.h>
#include "stringpool/stringpool.h"

using namespace stringpool;

bool sameSign(int x, int y) {
    if (x == 0)
        return y == 0;
    if (x < 0)
        return y < 0;
    return y > 0;
}

void expectMemcmp(int expectation, string_handle left, string_handle right, size_t length) {
    EXPECT_TRUE(sameSign(expectation, left.memcmp(right, length)));
    EXPECT_TRUE(sameSign(-expectation, right.memcmp(left, length)));
}

TEST(Memcmp, ShortLong) {
    pool p;
    auto a = p.intern("a");
    auto b = p.intern("leaf0123456789");
    expectMemcmp(-1, a, b, 1);
}

TEST(Memcmp, LongLong) {
    pool p;
    auto a = p.intern("leaf9876543210");
    auto b = p.intern("leaf0123456789");
    expectMemcmp(1, a, b, 1);
}

TEST(Memcmp, ConcatAtomConcatLongLong) {
    pool p;
    auto a = p.concat(
        p.intern("a"),
        p.concat(
            p.intern("leaf0123456789"),
            p.intern("leaf9876543210"))); // ends with 0
    auto b = p.concat(
        p.intern("a"),
        p.concat(
            p.intern("leaf0123456789"),
            p.intern("leaf9876543211"))); // ends with 1
    expectMemcmp(-1, a, b, 29);
}


TEST(Memcmp, ConcatLongLong) {
    pool p;
    auto a = p.concat(
        p.intern("leaf0123456789"),
        p.intern("leaf9876543210")); // ends with 0
    auto b = p.concat(
        p.intern("leaf0123456789"),
        p.intern("leaf9876543211")); // ends with 1
    expectMemcmp(-1, a, b, 28);
}
