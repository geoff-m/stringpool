#pragma once
#include "stringpool/stringpool.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <stdexcept>


enum class EntryType : uint8_t {
    ATOM = 0,
    SHORT_STRING = 1,
    CONCAT_LEFT_ENTRY_RIGHT_ENTRY = 2,
    CONCAT_LEFT_ENTRY_RIGHT_SHORT = 3,
    CONCAT_LEFT_SHORT_RIGHT_ENTRY = 4,
    CONCAT_LEFT_SHORT_RIGHT_SHORT = 5,
};

constexpr uint64_t UPPER_7_MASK = 0xffffffffffffff00;
constexpr uint64_t UPPER_1_MASK = 0xff00000000000000;

// Minimum size of an atom entry.
constexpr uint64_t ATOM_ENTRY_SIZE = 8;

// Size of a concat entry.
constexpr uint64_t CONCAT_ENTRY_SIZE = 8 * 3;

namespace offsets {
    constexpr int ENTRY_TYPE_STRING_LENGTH = 0;

    namespace atom {
        constexpr int STRING_VALUE = 8;
    }

    namespace concat {
        constexpr int LEFT_PTR = 8;
        constexpr int RIGHT_PTR = 16;
    }
}


[[nodiscard]] inline size_t min(const size_t x, const size_t y) {
    return x <= y ? x : y;
}

[[nodiscard]] EntryType unpack_node_type(const char* node);

[[nodiscard]] bool isConcat(const char* node);

[[nodiscard]] bool isShortConcatChild(const char* node);

[[nodiscard]] size_t unpackLength(const char* node);

[[nodiscard]] char* unpackStringFromLeaf(char* node);

// Returns true iff the child is a leaf.
[[nodiscard]] size_t unpackLeftChild(char* base, size_t concatNodeIndex);

// Returns true iff the child is a leaf.
[[nodiscard]] size_t unpackRightChild(char* base, size_t concatNodeIndex);

[[nodiscard]] EntryType makeConcatType(bool leftIsShort, bool rightIsShort);