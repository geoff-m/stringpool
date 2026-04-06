#include "include/pack_utils.h"

using namespace stringpool;

[[nodiscard]] EntryType unpack_node_type(const char* node) {
    return static_cast<EntryType>(node[offsets::ENTRY_TYPE_STRING_LENGTH] & 0b111);
}

[[nodiscard]] bool isConcat(const char* node) {
    const auto type = unpack_node_type(node);
    return type > EntryType::SHORT_STRING;
}

[[nodiscard]] bool isShortConcatChild(const char* node) {
    return unpack_node_type(node) == EntryType::SHORT_STRING;
}

[[nodiscard]] size_t unpackLength(const char* node) {
    if (isShortConcatChild(node))
        return node[offsets::ENTRY_TYPE_STRING_LENGTH] >> 4;
    const auto ret = reinterpret_cast<const uint64_t*>(node)[offsets::ENTRY_TYPE_STRING_LENGTH] >> 8;
    return ret;
}

[[nodiscard]] char* unpackStringFromLeaf(char* node) {
    if (isShortConcatChild(node))
        return node + 1;
    assert(unpack_node_type(node) == EntryType::ATOM);
    return node + offsets::atom::STRING_VALUE;
}

[[nodiscard]] size_t unpackLeftChild(char* base, size_t concatNodeIndex) {
    auto* child = base + concatNodeIndex + offsets::concat::LEFT_PTR;
    const auto type = unpack_node_type(base + concatNodeIndex);
    if (type == EntryType::CONCAT_LEFT_ENTRY_RIGHT_SHORT || type == EntryType::CONCAT_LEFT_ENTRY_RIGHT_ENTRY) {
        return *reinterpret_cast<size_t*>(child);
    }
    return concatNodeIndex + offsets::concat::LEFT_PTR;
}

[[nodiscard]] size_t unpackRightChild(char* base, size_t concatNodeIndex) {
    auto* child = base + concatNodeIndex + offsets::concat::RIGHT_PTR;
    const auto type = unpack_node_type(base + concatNodeIndex);
    if (type == EntryType::CONCAT_LEFT_ENTRY_RIGHT_ENTRY || type == EntryType::CONCAT_LEFT_SHORT_RIGHT_ENTRY) {
        return *reinterpret_cast<size_t*>(child);
    }
    return concatNodeIndex + offsets::concat::RIGHT_PTR;
}

[[nodiscard]] EntryType makeConcatType(bool leftIsShort, bool rightIsShort) {
    if (!leftIsShort && !rightIsShort)
        return EntryType::CONCAT_LEFT_ENTRY_RIGHT_ENTRY;
    if (leftIsShort && !rightIsShort)
        return EntryType::CONCAT_LEFT_SHORT_RIGHT_ENTRY;
    if (!leftIsShort && rightIsShort)
        return EntryType::CONCAT_LEFT_ENTRY_RIGHT_SHORT;
    return EntryType::CONCAT_LEFT_SHORT_RIGHT_SHORT;
}