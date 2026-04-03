#include <gtest/gtest.h>
#include "stringpool/stringpool.h"

using namespace stringpool;

TEST(Basic, Create) {
    pool p;
}

TEST(Basic, Add) {
    pool p;
    const auto string = "hello";
    const auto interned = p.intern(string);
    EXPECT_TRUE(interned.equals(string, strlen(string)));
}

TEST(Basic, Dedup) {
    pool p;
    const auto string = "hello";
    const auto interned1 = p.intern(string);
    const auto interned2 = p.intern(string);
    EXPECT_TRUE(interned1.equals(string, strlen(string)));
    EXPECT_TRUE(interned2.equals(string, strlen(string)));
    EXPECT_TRUE(interned1.equals(interned2));
    EXPECT_TRUE(interned2.equals(interned1));
}