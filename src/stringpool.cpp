#include "stringpool/stringpool.h"
#include "include/hash.h"
#include "include/default_allocator.h"
#include <cassert>
#include <cstring>
#include <stdexcept>

using namespace stringpool;
using namespace stringpool::internal;

default_allocator default_allocator;

pool::pool()
    : pool(32, &default_allocator) {
}

pool::pool(size_t initial_table_capacity)
    : pool(initial_table_capacity, &default_allocator) {
}

pool::pool(allocator* allocator)
    : pool(32, allocator) {
}

pool::pool(size_t initial_table_capacity, allocator* allocator)
    : alloc(allocator) {
    table.reserve(initial_table_capacity);
}

[[nodiscard]] static size_t get_buffer_size(const node* buffer) {
    switch (buffer->type) {
        case NodeType::ATOM:
            return sizeof(atom_node) + get_length(buffer);
        case NodeType::SHORT_ATOM:
            return sizeof(short_atom_node) + get_length(buffer);
        case NodeType::CONCAT:
            return sizeof(concat_node);
        default:
            std::abort(); // unreachable.
    }
}

pool::~pool() {
    std::lock_guard lock(tableRwMutex);
    for (const auto& kvp : table) {
        const auto& list = kvp.second;
        for (const auto& listIt : list) {
            free_buffer(listIt.data);
        }
    }
}

string_handle pool::intern(const char* string) {
    return intern(string, strlen(string));
}

// Attempts to intern the given string, or else return its handle if it already exists in the cache.
pool::InternResult pool::do_intern_unsafe(size_t hash, const char* string, size_t size, bool haveWriterLock,
                                          weak_string_handle& result) {
    const auto it = table.find(hash);
    if (it != table.end()) {
        auto existingEntries = it->second;
        for (const auto existingEntry : existingEntries) {
            if (string_handle::equals(existingEntry.data, string, size)) {
                result = existingEntry;
                ++internHits;
                return InternResult::Success;
            }
        }
        if (!haveWriterLock)
            return InternResult::NeedWriterLock;
        node* atom = add_atom_unsafe(string, size, hash);
        const weak_string_handle ret(atom);
        existingEntries.push_back(ret);
        result = ret;
        ++internMisses;
        return InternResult::Success;
    }
    // Nothing in table has this hash.
    if (!haveWriterLock)
        return InternResult::NeedWriterLock;
    node* atom = add_atom_unsafe(string, size, hash);
    const auto r = table.emplace(hash, std::list<weak_string_handle>());
    const weak_string_handle ret(atom);
    r.first->second.push_back(ret);
    result = ret;
    ++internMisses;
    return InternResult::Success;
}

string_handle pool::intern(const char* string, size_t size) {
    const auto hash = hasher::hash(string, size);
    std::shared_lock readLock(tableRwMutex);
    ++totalInternRequestCount;
    totalInternRequestSize += size;
    weak_string_handle result{};
    const auto resultWithRead = do_intern_unsafe(hash, string, size, false, result);
    if (resultWithRead == InternResult::NeedWriterLock) {
        readLock.unlock();
        std::lock_guard writeLock(tableRwMutex);
        [[maybe_unused]] const auto resultWithWrite = do_intern_unsafe(hash, string, size, true, result);
        assert(resultWithWrite == InternResult::Success);
        return result.make_strong();
    } else {
        assert(resultWithRead == InternResult::Success);
        return result.make_strong();
    }
}

string_handle pool::intern(std::string_view string) {
    return intern(string.data(), string.size());
}

weak_string_handle::weak_string_handle(node* data)
    : data(data) {
}

string_handle weak_string_handle::make_strong() const {
    return string_handle(data);
}

atom_node* pool::allocate_atom(size_t stringSize, size_t hash, pool* owner) {
    const auto nodeSize = sizeof(atom_node) + stringSize;
    auto* ret = alloc->allocate(nodeSize, alignof(size_t));
    new(ret) node(NodeType::ATOM, hash, owner);
    totalDataSize += nodeSize;
    return reinterpret_cast<atom_node*>(ret);
}

short_atom_node* pool::allocate_short_atom(size_t stringSize, size_t hash, pool* owner) {
    const auto nodeSize = sizeof(short_atom_node) + stringSize;
    auto* ret = alloc->allocate(nodeSize, alignof(size_t));
    new(ret) node(NodeType::SHORT_ATOM, hash, owner);
    totalDataSize += nodeSize;
    return reinterpret_cast<short_atom_node*>(ret);
}

concat_node* pool::allocate_concat(size_t hash, pool* owner) {
    constexpr auto nodeSize = sizeof(concat_node);
    auto* ret = alloc->allocate(nodeSize, alignof(size_t));
    new(ret) node(NodeType::CONCAT, hash, owner);
    totalDataSize += nodeSize;
    return reinterpret_cast<concat_node*>(ret);
}

void pool::free_buffer(node* node) {
    const auto size = get_buffer_size(node);
    alloc->deallocate(reinterpret_cast<char*>(node), size);
    totalDataSize -= size;
}

void abortIfStringTooLarge(size_t stringSize) {
    if (stringSize != (stringSize & 0x00ffffffffffffff)) [[unlikely]] {
        std::abort();
    }
}

node* pool::add_atom_unsafe(const char* string, size_t stringSize, size_t hash) {
    if (stringSize <= MAX_SHORT_ATOM_STRING_LENGTH) {
        auto* shortAtom = allocate_short_atom(stringSize, hash, this);
        shortAtom->length = static_cast<unsigned char>(stringSize);
        std::memcpy(reinterpret_cast<char*>(shortAtom) + sizeof(*shortAtom), string, stringSize);
        assert(internal::get_length(shortAtom) == stringSize);
        assert(shortAtom->owner == this);
        return shortAtom;
    } else {
        abortIfStringTooLarge(stringSize);
        auto* atom = allocate_atom(stringSize, hash, this);
        atom->length = stringSize & 0x00ffffffffffffff;
        std::memcpy(reinterpret_cast<char*>(atom) + sizeof(atom_node), string, stringSize);
        assert(internal::get_length(atom) == stringSize);
        return atom;
    }
}

static void addToHash(const char* piece, size_t size, void* pHasher) {
    static_cast<hasher*>(pHasher)->add(piece, size);
}

weak_string_handle pool::add_concat_unsafe(size_t hash, string_handle left, string_handle right) {
    const auto leftLength = get_length(left.data);
    const auto rightLength = get_length(right.data);
    const auto totalLength = leftLength + rightLength;
    if (sizeof(short_atom_node) + totalLength <= sizeof(concat_node)) {
        // We will store the concatenation as a single atom node.
        short_atom_node* shortAtom = allocate_short_atom(totalLength, hash, this);
        shortAtom->length = static_cast<unsigned char>(totalLength);
        left.copy(reinterpret_cast<char*>(shortAtom) + sizeof(short_atom_node), leftLength);
        right.copy(reinterpret_cast<char*>(shortAtom) + sizeof(short_atom_node) + leftLength, rightLength);
        assert(get_length(shortAtom) == totalLength);
        const auto r = table.emplace(hash, std::list<weak_string_handle>());
        const weak_string_handle ret(shortAtom);
        r.first->second.push_back(ret);
        return ret;
    } else {
        // We will store the concatenation as a concat node.
        // We should never have both short in a concat,
        // since if we could, we'd just make it an atom instead of a concat.
        abortIfStringTooLarge(totalLength);
        concat_node* concat = allocate_concat(hash, this);
        concat->length = totalLength & 0x00ffffffffffffff;
        concat->left = left.data;
        concat->right = right.data;
        const auto r = table.emplace(hash, std::list<weak_string_handle>());
#ifdef STRINGPOOL_REFCOUNT_ENABLE
        left.refcount_increment();
        right.refcount_increment();
#endif
        const weak_string_handle ret(concat);
        r.first->second.push_back(ret);
        return ret;
    }
}

// Attempts to intern the concatenation of the given string handles, or else return its handle if it already exists in the cache.
pool::InternResult pool::do_concat_unsafe(size_t hash, const string_handle& left, const string_handle& right,
                                          bool haveWriterLock,
                                          weak_string_handle& result) {
    const auto it = table.find(hash);
    if (it == table.end()) {
        // Nothing in table has this hash.
        if (!haveWriterLock)
            return InternResult::NeedWriterLock;
        result = add_concat_unsafe(hash, left, right);
        return InternResult::Success;
    }
    const auto existingEntries = it->second;
    for (const auto existingEntry : existingEntries) {
        if (string_handle::concat_equals(existingEntry.data, left.data, right.data)) {
            result = existingEntry;
            return InternResult::Success;
        }
    }
    if (!haveWriterLock)
        return InternResult::NeedWriterLock;
    result = add_concat_unsafe(hash, left, right);
    return InternResult::Success;
}

string_handle pool::concat(string_handle left, string_handle right) {
    // Special cases to allow left or right to be default string handles.
    // Such handles are not owned by any pool, but we allow them to be concated
    // to a string that is, or to each other.
    if (left.is_default()) {
        if (right.is_default())
            return intern("");
        if (right.data->owner == this)
            return right;
    } else {
        if (right.is_default())
            if (left.data->owner == this)
                return left;
    }

    if (left.data->owner != this || right.data->owner != this)
        throw std::invalid_argument("Both strings for concatenation must belong to this pool instance");
    hasher h;
    left.visit_chunks(addToHash, &h);
    right.visit_chunks(addToHash, &h);
    const auto hash = h.finish();
    weak_string_handle result{};
    std::shared_lock readLock(tableRwMutex);
    const auto readResult = do_concat_unsafe(hash, left, right, false, result);
    if (readResult == InternResult::NeedWriterLock) {
        readLock.unlock();
        std::lock_guard writeLock(tableRwMutex);
        [[maybe_unused]] const auto writeResult = do_concat_unsafe(hash, left, right, true, result);
        assert(writeResult == InternResult::Success);
        return result.make_strong();
    } else {
        assert(readResult == InternResult::Success);
        return result.make_strong();
    }
}

node::node(NodeType type, size_t hash, pool* owner)
    : hash(hash), owner(owner), type(type) {
}

node node::make_empty() {
    hasher h;
    const auto emptyStringHash = h.hash("", 0);
    return node(NodeType::SHORT_ATOM, emptyStringHash, 0);
}

bool operator==(const std::string_view& left, const string_handle& right) {
    return right.equals(left);
}

bool operator==(const string_handle& left, const std::string_view& right) {
    return left.equals(right);
}

bool operator!=(const std::string_view& left, const string_handle& right) {
    return !right.equals(left);
}

bool operator!=(const string_handle& left, const std::string_view& right) {
    return !left.equals(right);
}
