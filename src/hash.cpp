#include "include/hash.h"
#include <cstdlib>

using namespace stringpool;
constexpr auto SEED = 0x7448652047614D65;
size_t hasher::hash(const char* string, size_t length) {
    return XXH3_64bits_withSeed(string, length, SEED);
}

void abortOnError(XXH_errorcode ec) {
    if (ec == XXH_ERROR)
        std::abort();
}

hasher::hasher() {
    reset();
}

void hasher::add(const char* data, size_t length) {
    abortOnError(XXH3_64bits_update(&state, data, length));
}

size_t hasher::finish() {
    return XXH3_64bits_digest(&state);
}

void hasher::reset() {
    abortOnError(XXH3_64bits_reset_withSeed(&state, SEED));
}