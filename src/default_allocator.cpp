#include "include/default_allocator.h"
#include <cstdlib>
#include <cassert>

char* default_allocator::allocate(size_t size) {
    return static_cast<char*>(std::malloc(size));
}

char* default_allocator::allocate(size_t size, size_t alignment) {
    // std::aligned_alloc requires size to be a multiple of alignment.
    // Therefore we round it up here.
    const auto alignmentMask = alignment - 1;
    assert((alignment & alignmentMask) == 0); // Alignment must be a power of 2.
    size = (size + alignmentMask) & -alignment;
    return static_cast<char*>(std::aligned_alloc(alignment, size));
}

void default_allocator::deallocate(char* ptr, size_t size) {
    std::free(ptr);
}
