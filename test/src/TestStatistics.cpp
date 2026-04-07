#include <gtest/gtest.h>
#include "stringpool/stringpool.h"

using namespace stringpool;

// Not thread-safe.
void expectInternHit(pool& p, const char* string, size_t length) {
    const auto sizeBeforeHit = p.get_data_size();
    const auto reqCountBeforeHit = p.get_total_intern_request_count();
    const auto hitCountBeforeHit = p.get_total_intern_request_hits();
    const auto missCountBeforeHit = p.get_total_intern_request_misses();
    const auto reqSizeBeforeHit = p.get_total_intern_request_size();
    EXPECT_LT(0, sizeBeforeHit);
    p.intern(string, length); // hit.
    const auto sizeAfterHit = p.get_data_size();
    EXPECT_EQ(sizeBeforeHit, sizeAfterHit);
    EXPECT_EQ(1, p.get_total_intern_request_count() - reqCountBeforeHit);
    EXPECT_EQ(1, p.get_total_intern_request_hits() - hitCountBeforeHit);
    EXPECT_EQ(missCountBeforeHit, p.get_total_intern_request_misses());
    EXPECT_EQ(reqSizeBeforeHit + length, p.get_total_intern_request_size());
}

// Not thread-safe.
void expectInternMiss(pool& p, const char* string, size_t length) {
    const auto sizeBeforeMiss = p.get_data_size();
    const auto reqCountBeforeMiss = p.get_total_intern_request_count();
    const auto hitCountBeforeMiss = p.get_total_intern_request_hits();
    const auto missCountBeforeMiss = p.get_total_intern_request_misses();
    const auto reqSizeBeforeMiss = p.get_total_intern_request_size();
    p.intern(string, length); // miss.
    const auto sizeAfterMiss = p.get_data_size();
    EXPECT_LT(sizeBeforeMiss, sizeAfterMiss);
    EXPECT_EQ(1, p.get_total_intern_request_count() - reqCountBeforeMiss);
    EXPECT_EQ(hitCountBeforeMiss, p.get_total_intern_request_hits());
    EXPECT_EQ(1, p.get_total_intern_request_misses() - missCountBeforeMiss);
    EXPECT_EQ(reqSizeBeforeMiss + length, p.get_total_intern_request_size());
}

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
    expectInternMiss(p, "foo", 3);
}

TEST(Statistics, OneMissOneHit) {
    pool p;
    expectInternMiss(p, "foo", 3);
    expectInternHit(p, "foo", 3);
}

TEST(Statistics, OneMissEmpty) {
    pool p;
    expectInternMiss(p, "", 0);
    expectInternHit(p, "", 0);
}
