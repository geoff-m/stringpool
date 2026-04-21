#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace stringpool
{
    struct concat_node;
    struct node;
}

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

constexpr int MAX_SHORT_ATOM_STRING_LENGTH = 255;

[[nodiscard]] inline size_t min(const size_t x, const size_t y) {
    return x <= y ? x : y;
}

// Gets the total size the atom would have that contained a string of the given length.
[[nodiscard]] size_t compute_atom_size(size_t stringLength, EntryType* outputType);

[[nodiscard]] EntryType get_node_type(const stringpool::node* node);

[[nodiscard]] bool is_concat(const stringpool::node* node);

[[nodiscard]] size_t get_length(const stringpool::node* node);

[[nodiscard]] const char* get_string_from_leaf(const stringpool::node* node);

[[nodiscard]] stringpool::node* get_left_child(const stringpool::concat_node* concatNode);

[[nodiscard]] stringpool::node* get_right_child(const stringpool::concat_node* concatNode);

[[nodiscard]] const char* node_type_to_string(EntryType type);

[[nodiscard]] size_t get_hash(const stringpool::node* node);

#ifdef STRINGPOOL_REFCOUNT_ENABLE
[[nodiscard]] std::atomic_ref<size_t> get_refcount(stringpool::node* node);
#endif