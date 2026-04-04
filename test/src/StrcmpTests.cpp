#include <cstring>
#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include "Utility.h"

using namespace stringpool;

void expectStrcmp(int expectation, string_handle left, string_handle right) {
    expectSameSign(expectation, left.strcmp(right));
    expectSameSign(-expectation, right.strcmp(left));
}

void expectStrcmp(int expectation, string_handle left, const char* right) {
    expectSameSign(expectation, left.strcmp(right));
}

void expectStrcmp(int expectation, const char* left, string_handle right) {
    expectSameSign(-expectation, right.strcmp(left));
}

TEST(Strcmp, Empty) {
    pool p;
    auto e = p.intern("");
    expectStrcmp(0, e, e);
}

TEST(Strcmp, EqualLength1) {
    pool p;
    auto e = p.intern("a");
    expectStrcmp(0, e, e);
}

TEST(Strcmp, EqualLenth10) {
    pool p;
    auto e = p.intern("0123456789");
    expectStrcmp(0, e, e);
}

void testStrcmp(const char* string1, const char* string2) {
    pool p;
    auto interned1 = p.intern(string1);
    auto interned2 = p.intern(string2);
    const auto expectedComparison = std::strcmp(string1, string2);
    expectStrcmp(expectedComparison, interned1, interned2);
    expectStrcmp(expectedComparison, interned1, string2);
    expectStrcmp(expectedComparison, string1, interned2);
}

TEST(Strcmp, Length5_6) {
    testStrcmp("apples", "bananas");
}

TEST(Strcmp, Length0_1) {
    testStrcmp("", "x");
}

TEST(Strcmp, Length10_11) {
    testStrcmp("0123456789", "01234567891");
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
