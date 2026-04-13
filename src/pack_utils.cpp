#include "include/pack_utils.h"
#include "stringpool/stringpool.h"
#include <cassert>
#include <cstdlib>

using namespace stringpool;

size_t compute_atom_size(size_t stringLength, EntryType* typeOutput) {
    if (stringLength <= MAX_SHORT_ATOM_STRING_LENGTH) {
        *typeOutput = EntryType::SHORT_ATOM;
        return stringLength + sizes::SHORT_ATOM;
    }
    *typeOutput = EntryType::ATOM;
    return stringLength + sizes::ATOM;
}

[[nodiscard]] EntryType get_node_type(const char* node) {
    return static_cast<EntryType>(node[offsets::NODE_TYPE] & 0b111);
}

[[nodiscard]] bool is_concat(const char* node) {
    const auto type = get_node_type(node);
    return type == EntryType::CONCAT;
}

[[nodiscard]] size_t get_length(const char* node) {
    // todo: problem:
    // short concat children have a different structure from the rest.
    // their entry type is their first byte, and they have no ref count.
    // something about this needs to change.
    // give them a ref count,
    // and/or somehow make that node type correctly detectable.
    // they should not have a ref count, because they cannot be referred to.
    // they cannot be referred to because they are not nodes.
    const auto nodeType = get_node_type(node);
    switch (nodeType) {
        case EntryType::SHORT_ATOM: {
            const auto len = static_cast<unsigned char>(node[offsets::short_atom::STRING_LENGTH]);
            return len;
        }
        case EntryType::ATOM: {
            // -1 and >> 8 because we are the upper 7 bytes of the qword.
            auto len = *reinterpret_cast<const uint64_t*>(node + offsets::atom::STRING_LENGTH - 1) >> 8;
            return len;
        }
        case EntryType::CONCAT: {
            // -1 and >> 8 because we are the upper 7 bytes of the qword.
            auto len = *reinterpret_cast<const uint64_t*>(node + offsets::atom::STRING_LENGTH - 1) >> 8;
            return len;
        }
        default:
            std::abort(); // unreachable?
            return reinterpret_cast<const uint64_t*>(node)[offsets::NODE_TYPE] >> 8;
    }
}

[[nodiscard]] const char* get_string_from_leaf(const char* node) {
    const auto nodeType = get_node_type(node);
    switch (nodeType) {
        case EntryType::ATOM:
            return node + offsets::atom::STRING_VALUE;
        case EntryType::SHORT_ATOM:
            return node + offsets::short_atom::STRING_VALUE;
        default:
            std::abort(); // should be unreachable
    }
}

[[nodiscard]] const char* get_left_child(const char* concatNode) {
    return *reinterpret_cast<const char*const*>(concatNode + offsets::concat::LEFT_PTR);
}

[[nodiscard]] const char* get_right_child(const char* concatNode) {
    return *reinterpret_cast<const char*const*>(concatNode + offsets::concat::RIGHT_PTR);
}
