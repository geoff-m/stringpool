#include <gtest/gtest.h>
#include "stringpool/stringpool.h"

using namespace stringpool;

void testCharIterator(const char* expectedString, size_t expectedSize, const string_handle& sh) {
    size_t i = 0;
    for (char c : sh) {
        EXPECT_EQ(expectedString[i], c);
        ++i;
    }
    EXPECT_EQ(expectedSize, i);
}

void testCharIterator(const char* string) {
    pool p;
    testCharIterator(string, strlen(string), p.intern(string));
}

void testCharIterator(const std::string& string, const string_handle& sh) {
    testCharIterator(string.c_str(), string.size(), sh);
}

TEST(CharIterator, Empty) {
    pool p;
    auto e = p.intern("");
    auto begin = e.begin();
    auto end = e.end();
    EXPECT_EQ(end, begin);
}

TEST(CharIterator, One) {
    testCharIterator("a");
}

TEST(CharIterator, LongAtom) {
    std::string s(512, 'a');
    testCharIterator(s.c_str());
}

TEST(CharIterator, ConcatLongLong) {
    pool p;
    const std::string s1(512, 'a');
    const std::string s2(512, 'b');
    const auto ia = p.intern(s1);
    const auto ib = p.intern(s2);
    const auto iab = p.concat(ia, ib);
    const auto eab = s1 + s2;
    testCharIterator(eab, iab);
}

TEST(CharIterator, ConcatShortLong) {
    pool p;
    const std::string s1 = "a";
    const std::string s2(512, 'b');
    const auto ia = p.intern(s1);
    const auto ib = p.intern(s2);
    const auto iab = p.concat(ia, ib);
    const auto eab = s1 + s2;
    testCharIterator(eab, iab);
}

TEST(CharIterator, ConcatLongShort) {
    pool p;
    const std::string s1(512, 'a');
    const std::string s2 = "b";
    const auto ia = p.intern(s1);
    const auto ib = p.intern(s2);
    const auto iab = p.concat(ia, ib);
    const auto eab = s1 + s2;
    testCharIterator(eab, iab);
}
