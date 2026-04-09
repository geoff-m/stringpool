#include <gtest/gtest.h>
#include "stringpool/stringpool.h"

using namespace stringpool;

void testReverseCharIterator(const char* expectedString, size_t expectedSize, const string_handle& sh) {
    size_t i = expectedSize - 1;
    for (auto it = sh.rbegin(); it != sh.rend(); ++it) {
        const auto c = *it;
        EXPECT_EQ(expectedString[i], c);
        --i;
    }
    EXPECT_EQ(0, i + 1);
}

void testReverseCharIterator(const char* string) {
    pool p;
    testReverseCharIterator(string, strlen(string), p.intern(string));
}

void testReverseCharIterator(const std::string& string, const string_handle& sh) {
    testReverseCharIterator(string.c_str(), string.size(), sh);
}

TEST(ReverseCharIterator, Empty) {
    pool p;
    auto e = p.intern("");
    auto begin = e.rbegin();
    auto end = e.rend();
    EXPECT_EQ(end, begin);
}

TEST(ReverseCharIterator, One) {
    testReverseCharIterator("a");
}

TEST(ReverseCharIterator, PostfixIncrement) {
    pool p;
    auto e = p.intern("ab");
    auto it = e.rbegin();
    EXPECT_EQ('b', *it);
    EXPECT_EQ('b', *it++);
    EXPECT_EQ('a', *it++);
    EXPECT_EQ(e.rend(), it);
}

TEST(ReverseCharIterator, LongAtom) {
    std::string s(512, 'a');
    testReverseCharIterator(s.c_str());
}

TEST(ReverseCharIterator, ConcatLongLong) {
    pool p;
    const std::string s1(512, 'a');
    const std::string s2(512, 'b');
    const auto ia = p.intern(s1);
    const auto ib = p.intern(s2);
    const auto iab = p.concat(ia, ib);
    const auto eab = s1 + s2;
    testReverseCharIterator(eab, iab);
}

TEST(ReverseCharIterator, ConcatShortLong) {
    pool p;
    const std::string s1 = "a";
    const std::string s2(512, 'b');
    const auto ia = p.intern(s1);
    const auto ib = p.intern(s2);
    const auto iab = p.concat(ia, ib);
    const auto eab = s1 + s2;
    testReverseCharIterator(eab, iab);
}

TEST(ReverseCharIterator, ConcatLongShort) {
    pool p;
    const std::string s1(512, 'a');
    const std::string s2 = "b";
    const auto ia = p.intern(s1);
    const auto ib = p.intern(s2);
    const auto iab = p.concat(ia, ib);
    const auto eab = s1 + s2;
    testReverseCharIterator(eab, iab);
}
