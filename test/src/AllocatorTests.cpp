#include <gtest/gtest.h>
#include "stringpool/stringpool.h"
#include <atomic>
#include <cstddef>
#include "Utility.h"

using namespace stringpool;

class Allocator : public stringpool::allocator {
public:
    char* allocate(size_t size) override {
        auto* ret = std::malloc(size);
        printf("Thread %#lx Allocated %ld at %p\n", pthread_self(), size, ret);
        totalAllocated += size;
        return static_cast<char*>(ret);
    }

    void deallocate(char* ptr, size_t size) override {
        std::free(ptr);
        printf("Freed %ld at %p\n", size, ptr);
        totalFreed += size;
    }

    [[nodiscard]] size_t total_allocated() const {
        return totalAllocated.load(std::memory_order::acquire);
    }

    [[nodiscard]] size_t total_freed() const {
        return totalFreed.load(std::memory_order::acquire);
    }

private:
    std::atomic<size_t> totalAllocated = 0;
    std::atomic<size_t> totalFreed = 0;
};

TEST(Allocator, ShortAtom) {
    Allocator a;
    const auto string = "hello";
    size_t memoryUsed;
    {
        pool p(&a);
        const auto interned = p.intern(string);
        memoryUsed = a.total_allocated();
    }
    EXPECT_EQ(memoryUsed, a.total_allocated());
    EXPECT_EQ(a.total_freed(), a.total_allocated());
    EXPECT_GT(a.total_freed(), strlen(string));
}

TEST(Allocator, Cleanup) {
#ifndef STRINGPOOL_REFCOUNT_ENABLE
    GTEST_SKIP() << "Test requires reference counting";
#endif
    Allocator a;
    const auto string = "hello";
    size_t memoryUsed;
    pool p(&a);
    {
        const auto interned = p.intern(string);
        memoryUsed = a.total_allocated();
    } // destroy interned string due to reference counting.
    EXPECT_EQ(memoryUsed, a.total_allocated());
    EXPECT_EQ(a.total_freed(), a.total_allocated());
    EXPECT_GT(a.total_freed(), strlen(string));
}
