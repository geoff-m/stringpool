#include "stringpool/stringpool.h"
#include <cassert>
#include <cstdlib>

namespace stringpool::internal
{
    [[nodiscard]] size_t get_length(const node* node)
    {
        switch (node->type)
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
        switch (node->type)
        {
        case EntryType::ATOM:
            return reinterpret_cast<const char*>(node) + sizeof(atom_node);
        case EntryType::SHORT_ATOM:
            return reinterpret_cast<const char*>(node) + sizeof(short_atom_node);
        default:
            std::abort(); // should be unreachable
        }
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
}