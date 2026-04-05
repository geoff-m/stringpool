#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include "Utility.h"

using namespace stringpool;

TEST(Statistics, Empty) {
    pool p;
    EXPECT_EQ(0, p.get_data_size());
    EXPECT_EQ(0, p.get_total_intern_request_count());
    EXPECT_EQ(0, p.get_total_intern_request_hits());
    EXPECT_EQ(0, p.get_total_intern_request_misses());
    EXPECT_EQ(0, p.get_total_intern_request_size());
}

TEST(Statistics, OneMiss) {
    pool p;
    const auto string = "foo";
    p.intern(string);
    EXPECT_LT(0, p.get_data_size());
    EXPECT_EQ(1, p.get_total_intern_request_count());
    EXPECT_EQ(0, p.get_total_intern_request_hits());
    EXPECT_EQ(1, p.get_total_intern_request_misses());
    EXPECT_EQ(strlen(string), p.get_total_intern_request_size());
}

TEST(Statistics, OneMissOneHit) {
    pool p;
    const auto string = "foo";
    p.intern(string); // miss
    const auto sizeAfterMiss = p.get_data_size();
    EXPECT_LT(0, sizeAfterMiss);
    p.intern(string); // hit
    const auto sizeAfterHit = p.get_data_size();
    EXPECT_EQ(sizeAfterMiss, sizeAfterHit);
    EXPECT_EQ(2, p.get_total_intern_request_count());
    EXPECT_EQ(1, p.get_total_intern_request_hits());
    EXPECT_EQ(1, p.get_total_intern_request_misses());
    EXPECT_EQ(2 * strlen(string), p.get_total_intern_request_size());
}