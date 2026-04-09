#include "include/pack_utils.h"
#include "stringpool/stringpool.h"
#include <cassert>
#include <cstdlib>

using namespace stringpool;

[[nodiscard]] EntryType get_node_type(const char* node) {
    return static_cast<EntryType>(node[offsets::ENTRY_TYPE] & 0b111);
}

[[nodiscard]] bool is_concat(const char* node) {
    const auto type = get_node_type(node);
    return type > EntryType::SHORT_CONCAT_CHILD;
}

[[nodiscard]] size_t get_length(const char* node) {
    const auto nodeType = get_node_type(node);
    switch (nodeType) {
        case EntryType::SHORT_CONCAT_CHILD:
            return node[offsets::ENTRY_TYPE] >> 4;
        case EntryType::SHORT_ATOM:
            return static_cast<unsigned char>(node[offsets::short_atom::STRING_LENGTH]);
        default:
            return reinterpret_cast<const uint64_t*>(node)[offsets::ENTRY_TYPE] >> 8;
    }
}

[[nodiscard]] const char* get_string_from_leaf(const char* node) {
    const auto nodeType = get_node_type(node);
    switch (nodeType) {
        case EntryType::SHORT_CONCAT_CHILD:
            return node + 1;
        case EntryType::ATOM:
            return node + offsets::atom::STRING_VALUE;
        case EntryType::SHORT_ATOM:
            return node + offsets::short_atom::STRING_VALUE;
        default:
            std::abort(); // should be unreachable
    }
}

[[nodiscard]] const char* get_left_child(const char* concatNode) {
    auto* child = concatNode + offsets::concat::LEFT_PTR;
    const auto type = get_node_type( concatNode);
    if (type == EntryType::CONCAT_LEFT_ENTRY_RIGHT_SHORT || type == EntryType::CONCAT_LEFT_ENTRY_RIGHT_ENTRY) {
        return *reinterpret_cast<const char* const*>(child);
    }
    return concatNode + offsets::concat::LEFT_PTR;
}

[[nodiscard]] const char* get_right_child(const char* concatNode) {
    auto* child = concatNode + offsets::concat::RIGHT_PTR;
    const auto type = get_node_type(concatNode);
    if (type == EntryType::CONCAT_LEFT_ENTRY_RIGHT_ENTRY || type == EntryType::CONCAT_LEFT_SHORT_RIGHT_ENTRY) {
        return *reinterpret_cast<const char* const*>(child);
    }
    return concatNode + offsets::concat::RIGHT_PTR;
}

[[nodiscard]] EntryType make_concat_type(bool leftIsShort, bool rightIsShort) {
    if (!leftIsShort && !rightIsShort)
        return EntryType::CONCAT_LEFT_ENTRY_RIGHT_ENTRY;
    if (leftIsShort && !rightIsShort)
        return EntryType::CONCAT_LEFT_SHORT_RIGHT_ENTRY;
    if (!leftIsShort)
        return EntryType::CONCAT_LEFT_ENTRY_RIGHT_SHORT;
    return EntryType::CONCAT_LEFT_SHORT_RIGHT_SHORT;
}