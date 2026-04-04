#include <cstring>
#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include "Utility.h"

using namespace stringpool;

void expectMemcmp(int expectation, string_handle left, string_handle right, size_t length) {

    expectSameSign(expectation, left.memcmp(right, length));
    expectSameSign(-expectation, right.memcmp(left, length));
}

void expectMemcmp(int expectation, string_handle left, const char* right, size_t length) {
    expectSameSign(expectation, left.memcmp(right, length));
}

void expectMemcmp(int expectation, const char* left, string_handle right, size_t length) {
    expectSameSign(-expectation, right.memcmp(left, length));
}

void testMemcmp(const char* string1, const char* string2, size_t length) {
    pool p;
    auto interned1 = p.intern(string1);
    auto interned2 = p.intern(string2);
    const auto expectedComparison = std::memcmp(string1, string2, length);
    expectMemcmp(expectedComparison, interned1, interned2, length);
    expectMemcmp(expectedComparison, interned1, string2, length);
    expectMemcmp(expectedComparison, string1, interned2, length);
}

TEST(Memcmp, Length1_14) {
    testMemcmp("a", "leaf0123456789", 1);
}

TEST(Memcmp, Length14_14) {
    testMemcmp("leaf0123456789", "leaf0123456788", 14);
}

TEST(Memcmp, Length10_11) {
    testMemcmp("0123456789", "01234567891", 10);
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
