#include "include/pack_utils.h"
#include "stringpool/stringpool.h"
#include <cassert>
#include <cstdlib>

using namespace stringpool;

size_t compute_atom_size(size_t stringLength, EntryType* typeOutput)
{
    if (stringLength <= MAX_SHORT_ATOM_STRING_LENGTH)
    {
        *typeOutput = EntryType::SHORT_ATOM;
        return stringLength + sizeof(short_atom_node);
    }
    *typeOutput = EntryType::ATOM;
    return stringLength + sizeof(atom_node);
}

[[nodiscard]] EntryType get_node_type(const node* node)
{
    const auto type = node->type;
    switch (type)
    {
    case EntryType::ATOM:
    case EntryType::SHORT_ATOM:
    case EntryType::CONCAT:
        return type;
    default:
        std::abort();
    }
}

[[nodiscard]] bool is_concat(const node* node)
{
    const auto type = get_node_type(node);
    return type == EntryType::CONCAT;
}

[[nodiscard]] size_t get_length(const node* node)
{
    const auto nodeType = get_node_type(node);
    switch (nodeType)
    {
    case EntryType::SHORT_ATOM:
        {
            const auto len = reinterpret_cast<const short_atom_node*>(node)->length;
            return len;
        }
    case EntryType::ATOM:
        {
            auto len = reinterpret_cast<const atom_node*>(node)->length;
            return len;
        }
    case EntryType::CONCAT:
        {
            auto len = reinterpret_cast<const concat_node*>(node)->length;
            return len;
        }
    default:
        std::abort();
    }
}

[[nodiscard]] const char* get_string_from_leaf(const node* node)
{
    const auto nodeType = get_node_type(node);
    switch (nodeType)
    {
    case EntryType::ATOM:
        return reinterpret_cast<const char*>(node) + sizeof(atom_node);
    case EntryType::SHORT_ATOM:
        return reinterpret_cast<const char*>(node) + sizeof(short_atom_node);
    default:
        std::abort(); // should be unreachable
    }
}

[[nodiscard]] node* get_left_child(const concat_node* concatNode)
{
    return concatNode->left;
}

[[nodiscard]] node* get_right_child(const concat_node* concatNode)
{
    return concatNode->right;
}

#ifdef STRINGPOOL_REFCOUNT_ENABLE
std::atomic_ref<size_t> get_refcount(node* node)
{
    return std::atomic_ref(node->refCount);
}
#endif

const char* node_type_to_string(EntryType type)
{
    switch (type)
    {
    case EntryType::ATOM:
        return "atom";
    case EntryType::SHORT_ATOM:
        return "short_atom";
    case EntryType::CONCAT:
        return "concat";
    default:
        return "unknown";
    }
}

size_t get_hash(const node* node)
{
    return node->hash;
}
