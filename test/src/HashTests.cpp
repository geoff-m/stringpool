#include <gtest/gtest.h>
#include "stringpool/stringpool.h"

using namespace stringpool;

TEST(Hash, Empty) {
    pool p1;
    const auto e1 = p1.intern("");
    const auto h1 = e1.hash();
    pool p2;
    const auto e2 = p2.intern("");
    const auto h2 = e2.hash();
    EXPECT_EQ(h1, h2);
}

TEST(Hash, AtomEqualsConcat) {
    pool p1;
    const auto atom = p1.intern("Hello, World!");
    pool p2;
    const auto concat = p2.concat(p2.intern("Hello, "),
        p2.intern("World!"));
    EXPECT_EQ(atom.hash(), concat.hash());
}
