#pragma once
#include <cstddef>
#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"


namespace stringpool {
    class hasher {
    public:
        hasher();

        [[nodiscard]] static size_t hash(const char* string, size_t length);

        [[nodiscard]] static size_t hash(const char* string);

        void add(const char* data, size_t length);

        size_t finish();

        void reset();

    private:
        XXH3_state_t state;
    };
}
