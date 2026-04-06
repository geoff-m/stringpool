#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include "Utility.h"

using namespace stringpool;

void cb(const char* string, size_t length, void* state) {
    auto* totalLength = static_cast<size_t*>(state);
    *totalLength += length;
}

TEST(Visit, Empty) {
    pool p;
    auto s = p.intern("");
    size_t length = 0;
    s.visit_pieces(cb, &length);
}