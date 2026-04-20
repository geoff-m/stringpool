#pragma once
#include <atomic>
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
#ifdef STRINGPOOL_REFCOUNT_ENABLE
    constexpr int NODE_TYPE = 16; // Must be the same for all node types.
#else
    constexpr int NODE_TYPE = 8; // Must be the same for all node types.
#endif

    namespace atom {
#ifdef STRINGPOOL_REFCOUNT_ENABLE
        constexpr int REFCOUNT = 0;
        constexpr int HASH = 8;
        constexpr int NODE_TYPE = 16;
        constexpr int STRING_LENGTH = 17;
        constexpr int STRING_VALUE = 24;
#else
        constexpr int HASH = 0;
        constexpr int NODE_TYPE = 8;
        constexpr int STRING_LENGTH = 9;
        constexpr int STRING_VALUE = 17;
#endif
    }

    namespace short_atom {
#ifdef STRINGPOOL_REFCOUNT_ENABLE
        constexpr int REFCOUNT = 0;
        constexpr int HASH = 8;
        constexpr int NODE_TYPE = 16;
        constexpr int STRING_LENGTH = 17;
        constexpr int STRING_VALUE = 18;
#else
        constexpr int HASH = 0;
        constexpr int NODE_TYPE = 8;
        constexpr int STRING_LENGTH = 9;
        constexpr int STRING_VALUE = 10;
#endif
    }

    namespace concat {
#ifdef STRINGPOOL_REFCOUNT_ENABLE
        constexpr int REFCOUNT = 0;
        constexpr int HASH = 8;
        constexpr int NODE_TYPE = 16;
        constexpr int STRING_LENGTH = 17;
        constexpr int LEFT_PTR = 24;
        constexpr int RIGHT_PTR = 32;
#else
        constexpr int HASH = 0;
        constexpr int NODE_TYPE = 8;
        constexpr int STRING_LENGTH = 9;
        constexpr int LEFT_PTR = 16;
        constexpr int RIGHT_PTR = 24;
#endif
    }
}

namespace sizes {
#ifdef STRINGPOOL_REFCOUNT_ENABLE
    constexpr int ATOM = 32;
    constexpr int SHORT_ATOM = 18;
    constexpr int CONCAT = 40;
#else
    constexpr int ATOM = 24;
    constexpr int SHORT_ATOM = 10;
    constexpr int CONCAT = 32;
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

[[nodiscard]] const char* node_type_to_string(EntryType type);

[[nodiscard]] size_t get_hash(const char* node);

#ifdef STRINGPOOL_REFCOUNT_ENABLE
[[nodiscard]] std::atomic_ref<size_t> get_refcount(char* node);
#endif