#include <cstring>
#include <gtest/gtest.h>
#include "stringpool/stringpool.h"

using namespace stringpool;

class EqualityVisitorState {
    const char* expectedString;
    size_t expectedStringLength;
    size_t comparedChars = 0;
    bool isEqual = true;

public:
    EqualityVisitorState(const char* expectedString, size_t expectedStringLength)
        : expectedString(expectedString), expectedStringLength(expectedStringLength) {
    }

    [[nodiscard]] bool is_equal() const {
        return isEqual;
    }

    // Returns true if all data seen by this instance has matched the expected string.
    bool compare_next(const char* chunk, size_t chunkSize) {
        if (!isEqual)
            return false;
        const auto remainingExpectedLength = expectedStringLength - comparedChars;
        if (remainingExpectedLength < chunkSize) {
            // Saw more data than expected.
            isEqual = false;
            return false;
        }
        const auto compareLength = std::min(remainingExpectedLength, chunkSize);
        if (0 == std::memcmp(expectedString + comparedChars, chunk, chunkSize)) {
            comparedChars += compareLength;
            return isEqual;
        }
        isEqual = false;
        return false;
    }
};

void cStringCallback(const char* chunk, size_t length, void* state) {
    auto* evs = static_cast<EqualityVisitorState*>(state);
    evs->compare_next(chunk, length);
}

void stringViewCallback(std::string_view chunk, void* state) {
    auto* evs = static_cast<EqualityVisitorState*>(state);
    evs->compare_next(chunk.data(), chunk.size());
}

void testVisitEquals(const std::string& expected, const string_handle& actual) {
    {
        EqualityVisitorState evs(expected.c_str(), expected.size());
        actual.visit_chunks(cStringCallback, &evs);
        EXPECT_TRUE(evs.is_equal());
    }
    {
        EqualityVisitorState evs(expected.c_str(), expected.size());
        actual.visit_chunks(stringViewCallback, &evs);
        EXPECT_TRUE(evs.is_equal());
    }
}

void testVisitEquals(const std::string& expected) {
    pool p;
    const auto actual = p.intern(expected.c_str(), expected.size());
    testVisitEquals(expected, actual);
}


TEST(Visit, Empty) {
    testVisitEquals("");
}

TEST(Visit, ShortAtom) {
    testVisitEquals("hello");
}

TEST(Visit, LongAtom) {
    const std::string expected(1024, 'a');
    testVisitEquals(expected);
}

TEST(Visit, ConcatLongChildren) {
    pool p;
    const std::string a(1024, 'a');
    const std::string b(1024, 'b');
    const auto ia = p.intern(a.c_str());
    const auto ib = p.intern(b.c_str());
    const auto iab = p.concat(ia, ib);
    const auto expected = a + b;
    testVisitEquals(expected, iab);
}

TEST(Visit, ConcatLeftShort) {
    {
        pool p;
        const std::string a = "a";
        const std::string b(1024, 'b');
        const auto ia = p.intern(a.c_str());
        const auto ib = p.intern(b.c_str());
        const auto iab = p.concat(ia, ib);
        const auto expected = a + b;
        testVisitEquals(expected, iab);
    }
}

TEST(Visit, ConcatRightShort) {
    pool p;
    const std::string a(1024, 'a');
    const std::string b = "b";
    const auto ia = p.intern(a.c_str());
    const auto ib = p.intern(b.c_str());
    const auto iab = p.concat(ia, ib);
    const auto expected = a + b;
    testVisitEquals(expected, iab);
}
