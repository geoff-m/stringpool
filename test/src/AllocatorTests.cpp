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

    char* allocate(size_t size, size_t alignment) override {
        // std::aligned_alloc requires size to be a multiple of alignment.
        // Therefore we round it up here.
        const auto alignmentMask = alignment - 1;
        assert((alignment & alignmentMask) == 0); // Alignment must be a power of 2.
        const size_t alignedSize = (size + alignmentMask) & -alignment;
        auto* ret = std::aligned_alloc(alignment, alignedSize);
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

TEST(Allocator, LongAtom) {
    Allocator a;
    const auto length = 1234;
    auto string = std::make_unique<char[]>(length);
    memset(string.get(), 'a', length);
    string.get()[length - 1] = 0;
    size_t memoryUsed;
    {
        pool p(&a);
        const auto interned = p.intern(string.get());
        memoryUsed = a.total_allocated();
    }
    EXPECT_EQ(memoryUsed, a.total_allocated());
    EXPECT_EQ(a.total_freed(), a.total_allocated());
    EXPECT_GT(a.total_freed(), length);
}

TEST(Allocator, Concat) {
    Allocator a;
    const auto length = 1234;
    auto a1 = std::make_unique<char[]>(length);
    memset(a1.get(), 'a', length);
    a1.get()[length - 1] = 0;
    auto a2 = std::make_unique<char[]>(length);
    memset(a2.get(), 'b', length);
    a2.get()[length - 1] = 0;
    size_t memoryUsed;
    {
        pool p(&a);
        const auto a1i = p.intern(a1.get());
        const auto a2i = p.intern(a2.get());
        const auto concat = p.concat(a1i, a2i);
        memoryUsed = a.total_allocated();
    }
    EXPECT_EQ(memoryUsed, a.total_allocated());
    EXPECT_EQ(a.total_freed(), a.total_allocated());
    EXPECT_GT(a.total_freed(), length * 2);
}
