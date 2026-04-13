#pragma once
#include <cstddef>
#include <cstdint>

enum class EntryType : uint8_t {
    ATOM = 0,
    SHORT_ATOM = 1,
    CONCAT = 2
};

constexpr uint64_t UPPER_7_MASK = 0xffffffffffffff00;
constexpr uint64_t UPPER_1_MASK = 0xff00000000000000;

namespace offsets {
    constexpr int NODE_TYPE = 8;

    namespace atom {
#ifdef STRINGPOOL_REFCOUNT_ENABLE
        constexpr int REFCOUNT = 0;
        constexpr int NODE_TYPE = 8;
        constexpr int STRING_LENGTH = 9;
        constexpr int STRING_VALUE = 16;
#else
        constexpr int STRING_LENGTH = 1;
        constexpr int STRING_VALUE = 8;
#endif
    }

    namespace short_atom {
#ifdef STRINGPOOL_REFCOUNT_ENABLE
        constexpr int REFCOUNT = 0;
        constexpr int NODE_TYPE = 8;
        constexpr int STRING_LENGTH = 9;
        constexpr int STRING_VALUE = 10;
#else
        constexpr int STRING_LENGTH = 1;
        constexpr int STRING_VALUE = 2;
#endif
    }

    namespace concat {
#ifdef STRINGPOOL_REFCOUNT_ENABLE
        constexpr int REFCOUNT = 0;
        constexpr int NODE_TYPE = 8;
        constexpr int STRING_LENGTH = 9;
        constexpr int LEFT_PTR = 16;
        constexpr int RIGHT_PTR = 24;
#else
        constexpr int STRING_LENGTH = 1;
        constexpr int LEFT_PTR = 8;
        constexpr int RIGHT_PTR = 16;
#endif
    }
}

namespace sizes {
#ifdef STRINGPOOL_REFCOUNT_ENABLE
    constexpr int ATOM = 24;
    constexpr int SHORT_ATOM = 10;
    constexpr int CONCAT = 32;
#else
    constexpr int ATOM = 16;
    constexpr int SHORT_ATOM = 2;
    constexpr int CONCAT = 24;
#endif
}

constexpr int MAX_SHORT_ATOM_STRING_LENGTH = 255;

[[nodiscard]] inline size_t min(const size_t x, const size_t y) {
    return x <= y ? x : y;
}

// Gets the total size the atom would have that contained a string of the given length.
[[nodiscard]] size_t compute_atom_size(size_t stringLength, EntryType* outputType);

[[nodiscard]] EntryType get_node_type(const char* node);

[[nodiscard]] bool is_concat(const char* node);

[[nodiscard]] size_t get_length(const char* node);

[[nodiscard]] const char* get_string_from_leaf(const char* node);

// Returns true iff the child is a leaf.
[[nodiscard]] const char* get_left_child(const char* concatNode);

// Returns true iff the child is a leaf.
[[nodiscard]] const char* get_right_child(const char* concatNode);
