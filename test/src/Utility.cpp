#include "Utility.h"
#include <gtest/gtest.h>

using namespace stringpool;

bool sameSign(int x, int y) {
    if (x == 0)
        return y == 0;
    if (x < 0)
        return y < 0;
    return y > 0;
}

void failSameSign(int x, int y) {
    ADD_FAILURE() << "Expected same sign: " << x << " and " << y;
}

void expectSameSign(int x, int y) {
    if (x == 0) {
        if (y != 0)
            failSameSign(x, y);
        else return;
    }
    if (x < 0) {
        if (y >= 0)
            failSameSign(x, y);
        else return;
    }
    // x > 0.
    if (y <= 0)
        failSameSign(x, y);
}

void expectEqual(const string_handle interned, const char* str) {
    const auto len = strlen(str);
    bool eq =  interned.equals(str, len);
    EXPECT_TRUE(eq);
    EXPECT_TRUE(interned.equals(str, len));
    EXPECT_TRUE(interned.equals(str));
    EXPECT_EQ(0, interned.strcmp(str));
    EXPECT_EQ(0, interned.memcmp(str, len));
}

void expectEqual(const string_handle x, const string_handle y) {
    EXPECT_TRUE(x.equals(y));
    EXPECT_TRUE(y.equals(x));
    EXPECT_TRUE(x == y);
    EXPECT_TRUE(y == x);
    EXPECT_FALSE(x != y);
    EXPECT_FALSE(y != x);
    EXPECT_EQ(0, x.strcmp(y));
    EXPECT_EQ(0, y.strcmp(x));
    EXPECT_EQ(0, x.memcmp(y, x.size()));
    EXPECT_EQ(0, y.memcmp(x, y.size()));
}

void expectLength(size_t length, const string_handle interned) {
    EXPECT_EQ(length, interned.size());
    EXPECT_EQ(length, interned.length());
}