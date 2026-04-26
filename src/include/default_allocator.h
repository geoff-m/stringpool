#pragma once
#include "stringpool/stringpool.h"

class default_allocator : public stringpool::allocator {
public:
    char* allocate(size_t size) override;

    char* allocate(size_t size, size_t alignment) override;

    void deallocate(char* ptr, size_t size) override;
};