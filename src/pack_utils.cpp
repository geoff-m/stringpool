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
        return stringLength + sizes::SHORT_ATOM;
    }
    *typeOutput = EntryType::ATOM;
    return stringLength + sizes::ATOM;
}

[[nodiscard]] EntryType get_node_type(const char* node)
{
    const auto type = static_cast<EntryType>(node[offsets::NODE_TYPE] & 0b111);
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

[[nodiscard]] bool is_concat(const char* node)
{
    const auto type = get_node_type(node);
    return type == EntryType::CONCAT;
}

[[nodiscard]] size_t get_length(const char* node)
{
    const auto nodeType = get_node_type(node);
    switch (nodeType)
    {
    case EntryType::SHORT_ATOM:
        {
            const auto len = static_cast<unsigned char>(node[offsets::short_atom::STRING_LENGTH]);
            return len;
        }
    case EntryType::ATOM:
        {
            // -1 and >> 8 because we are the upper 7 bytes of the qword.
            auto len = *reinterpret_cast<const uint64_t*>(node + offsets::atom::STRING_LENGTH - 1) >> 8;
            return len;
        }
    case EntryType::CONCAT:
        {
            // -1 and >> 8 because we are the upper 7 bytes of the qword.
            auto len = *reinterpret_cast<const uint64_t*>(node + offsets::concat::STRING_LENGTH - 1) >> 8;
            return len;
        }
    default:
        std::abort();
    }
}

[[nodiscard]] const char* get_string_from_leaf(const char* node)
{
    const auto nodeType = get_node_type(node);
    switch (nodeType)
    {
    case EntryType::ATOM:
        return node + offsets::atom::STRING_VALUE;
    case EntryType::SHORT_ATOM:
        return node + offsets::short_atom::STRING_VALUE;
    default:
        std::abort(); // should be unreachable
    }
}

[[nodiscard]] const char* get_left_child(const char* concatNode)
{
    return *reinterpret_cast<const char*const*>(concatNode + offsets::concat::LEFT_PTR);
}

[[nodiscard]] const char* get_right_child(const char* concatNode)
{
    return *reinterpret_cast<const char*const*>(concatNode + offsets::concat::RIGHT_PTR);
}

#ifdef STRINGPOOL_REFCOUNT_ENABLE
std::atomic_ref<size_t> get_refcount(char* node)
{
    switch (get_node_type(node))
    {
    case EntryType::ATOM:
        return std::atomic_ref(*reinterpret_cast<size_t*>(node + offsets::atom::REFCOUNT));
    case EntryType::SHORT_ATOM:
        return std::atomic_ref(*reinterpret_cast<size_t*>(node + offsets::short_atom::REFCOUNT));
    case EntryType::CONCAT:
        return std::atomic_ref(*reinterpret_cast<size_t*>(node + offsets::concat::REFCOUNT));
    default:
        std::abort(); // unreachable
    }
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

size_t get_hash(const char* node)
{
    const auto nodeType = get_node_type(node);
    switch (nodeType)
    {
    case EntryType::ATOM:
        return *reinterpret_cast<const size_t*>(node + offsets::atom::HASH);
    case EntryType::SHORT_ATOM:
        return *reinterpret_cast<const size_t*>(node + offsets::short_atom::HASH);
    case EntryType::CONCAT:
        return *reinterpret_cast<const size_t*>(node + offsets::concat::HASH);
    default:
        std::abort();
    }
}
