#include "stringpool/stringpool.h"
#include "include/hash.h"
#include "include/pack_utils.h"
#include <cassert>
#include <cstring>
#include <stdexcept>

using namespace stringpool;

class default_allocator : public allocator {
public:
    char* allocate(size_t size) override {
        return static_cast<char*>(std::malloc(size));
    }

    void deallocate(char* ptr, size_t size) override {
        return std::free(ptr);
    }
};

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

[[nodiscard]] static size_t get_buffer_size(const char* buffer) {
    const auto type = get_node_type(buffer);
    switch (type) {
        case EntryType::ATOM:
            return sizes::ATOM + get_length(buffer);
        case EntryType::SHORT_ATOM:
            return sizes::SHORT_ATOM + get_length(buffer);
        case EntryType::CONCAT:
            return sizes::CONCAT;
        default:
            std::abort(); // unreachable.
    }
}

pool::~pool() {
    std::unique_lock lock(tableRwMutex);
    for (auto kvp : table)
    {
        auto& list = kvp.second;
        for (auto& listIt : list)
        {
            free_buffer(const_cast<char*>(listIt.data));
        }
    }
}

string_handle pool::intern(const char* string) {
    return intern(string, strlen(string));
}

// Attempts to intern the given string, or else return its handle if it already exists in the cache.
pool::InternResult pool::do_intern_unsafe(size_t hash, const char* string, size_t size, bool haveWriterLock,
                                          weak_string_handle& result) {
    auto it = table.find(hash);
    if (it != table.end()) {
        auto existingEntries = it->second;
        for (auto existingEntry : existingEntries) {
            if (string_handle::equals(existingEntry.data, string, size)) {
                result = existingEntry;
                ++internHits;
                return InternResult::Success;
            }
        }
        if (!haveWriterLock)
            return InternResult::NeedWriterLock;
        char* atom = add_atom_unsafe(string, size, hash);
#ifdef STRINGPOOL_TRACK_OWNERS
        weak_string_handle ret(atom, this);
#else
        weak_string_handle ret(atom);
#endif
        existingEntries.push_back(ret);
        result = ret;
        ++internMisses;
        return InternResult::Success;
    }
    // Nothing in table has this hash.
    if (!haveWriterLock)
        return InternResult::NeedWriterLock;
    char* atom = add_atom_unsafe(string, size, hash);
    auto r = table.emplace(hash, std::list<weak_string_handle>());
#ifdef STRINGPOOL_TRACK_OWNERS
    weak_string_handle ret(atom, this);
#else
    weak_string_handle ret(atom);
#endif
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
    auto resultWithRead = do_intern_unsafe(hash, string, size, false, result);
    if (resultWithRead == InternResult::NeedWriterLock) {
        readLock.unlock();
        std::unique_lock writeLock(tableRwMutex);
        auto resultWithWrite = do_intern_unsafe(hash, string, size, true, result);
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

weak_string_handle::weak_string_handle(const char* data, pool* owner)
    : data(data)
#ifdef STRINGPOOL_TRACK_OWNERS
      , owner(owner)
#endif
{
}

string_handle weak_string_handle::make_strong() const {
    return string_handle(data, owner);
}

char* pool::add_buffer(size_t size) {
    // todo: ensure alignment.
    // currently, we assume the allocator will give us regions that are 8-byte aligned
    // when we need it for (efficient) atomic refcount updates,
    // but nothing ensures this.
    // what we can do here is over-allocate if necessary
    // then waste space (up to 7 bytes) in order to guarantee the region we actually use is aligned.
    // when such adjustment occurs,
    // we must have a way to recover the original true size of the allocation request
    // for when we call deallocate.
    // a helper function should be able to infer this original size
    // from the size-in-use and the pointer's value.
    auto* ret = alloc->allocate(size);
    data.emplace_back(ret);
    totalDataSize += size;
    return ret;
}

void pool::free_buffer(char* node) {
    const auto size = get_buffer_size(node);
    alloc->deallocate(node, size);
    totalDataSize -= size;
}

char* pool::add_atom_unsafe(const char* string, size_t stringSize, size_t hash) {
    EntryType type;
    const auto blockSize = compute_atom_size(stringSize, &type);
    char* startOfAtom = add_buffer(blockSize);
    if (type == EntryType::SHORT_ATOM) {
        startOfAtom[offsets::short_atom::NODE_TYPE] = static_cast<char>(EntryType::SHORT_ATOM);
        *reinterpret_cast<size_t*>(startOfAtom + offsets::short_atom::HASH) = hash;
#ifdef STRINGPOOL_REFCOUNT_ENABLE
        *reinterpret_cast<size_t*>(startOfAtom + offsets::short_atom::REFCOUNT) = 0;
#endif
        startOfAtom[offsets::short_atom::STRING_LENGTH] = static_cast<char>(stringSize);
        std::memcpy(startOfAtom + offsets::short_atom::STRING_VALUE, string, stringSize);
        assert(get_length(startOfAtom) == stringSize);
        return startOfAtom;
    } else {
        if (stringSize != (stringSize & ~UPPER_1_MASK)) [[unlikely]]
                std::abort(); // string is too large.
        startOfAtom[offsets::atom::NODE_TYPE] = static_cast<char>(EntryType::ATOM);
        *reinterpret_cast<size_t*>(startOfAtom + offsets::atom::HASH) = hash;
#ifdef STRINGPOOL_REFCOUNT_ENABLE
        *reinterpret_cast<size_t*>(startOfAtom + offsets::atom::REFCOUNT) = 0;
#endif
        std::memcpy(startOfAtom + offsets::atom::STRING_LENGTH, &stringSize, 7);
        std::memcpy(startOfAtom + offsets::atom::STRING_VALUE, string, stringSize);
        assert(get_length(startOfAtom) == stringSize);
        return startOfAtom;
    }
}

static void addToHash(const char* piece, size_t size, void* pHasher) {
    static_cast<hasher*>(pHasher)->add(piece, size);
}

weak_string_handle pool::add_concat_unsafe(size_t hash, string_handle left, string_handle right) {
    const auto leftLength = get_length(left.data);
    const auto rightLength = get_length(right.data);
    const auto totalLength = leftLength + rightLength;
    if (totalLength <= MAX_SHORT_ATOM_STRING_LENGTH) {
        // We will store the concatenation as a single atom node.
        EntryType type;
        const auto atomSize = compute_atom_size(totalLength, &type);
        assert(type == EntryType::SHORT_ATOM);
        char* startOfAtom = add_buffer(atomSize);
#ifdef STRINGPOOL_REFCOUNT_ENABLE
        *reinterpret_cast<size_t*>(startOfAtom + offsets::short_atom::REFCOUNT) = 0;
#endif
        startOfAtom[offsets::short_atom::NODE_TYPE] = static_cast<char>(EntryType::SHORT_ATOM);
        *reinterpret_cast<size_t*>(startOfAtom + offsets::short_atom::HASH) = hash;
        startOfAtom[offsets::short_atom::STRING_LENGTH] = static_cast<char>(totalLength);
        left.copy(startOfAtom + offsets::short_atom::STRING_VALUE, leftLength);
        right.copy(startOfAtom + offsets::short_atom::STRING_VALUE + leftLength, rightLength);
        assert(get_length(startOfAtom) == totalLength);
        auto r = table.emplace(hash, std::list<weak_string_handle>());
#ifdef STRINGPOOL_TRACK_OWNERS
        weak_string_handle ret(startOfAtom, this);
#else
        weak_string_handle ret(startOfAtom);
#endif
        r.first->second.push_back(ret);
        return ret;
    } else {
        // We will store the concatenation as a concat node.
        // We should never have both short in a concat,
        // since if we could, we'd just make it an atom instead of a concat.
        const auto blockSize = sizes::CONCAT;
        char* concat = add_buffer(blockSize);
#ifdef STRINGPOOL_REFCOUNT_ENABLE
        *reinterpret_cast<size_t*>(concat + offsets::concat::REFCOUNT) = 0;
#endif
        concat[offsets::concat::NODE_TYPE] = static_cast<char>(EntryType::CONCAT);
        *reinterpret_cast<size_t*>(concat + offsets::concat::HASH) = hash;
        std::memcpy(concat + offsets::concat::STRING_LENGTH, &totalLength, 7);
        std::memcpy(concat + offsets::concat::LEFT_PTR, &left.data, 8);
        std::memcpy(concat + offsets::concat::RIGHT_PTR, &right.data, 8);
        auto r = table.emplace(hash, std::list<weak_string_handle>());
#ifdef STRINGPOOL_REFCOUNT_ENABLE
        left.refcount_increment();
        right.refcount_increment();
#endif
#ifdef STRINGPOOL_TRACK_OWNERS
        weak_string_handle ret(concat, this);
#else
        weak_string_handle ret(concat);
#endif
        r.first->second.push_back(ret);
        return ret;
    }
}

// Attempts to intern the concatenation of the given string handles, or else return its handle if it already exists in the cache.
pool::InternResult pool::do_concat_unsafe(size_t hash, string_handle left, string_handle right, bool haveWriterLock,
                                          weak_string_handle& result) {
    auto it = table.find(hash);
    if (it == table.end()) {
        // Nothing in table has this hash.
        if (!haveWriterLock)
            return InternResult::NeedWriterLock;
        result = add_concat_unsafe(hash, left, right);
        return InternResult::Success;
    }
    auto existingEntries = it->second;
    for (auto existingEntry : existingEntries) {
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
#ifdef STRINGPOOL_TRACK_OWNERS
    if (left.owner != this || right.owner != this)
        throw std::invalid_argument("Both strings for concatenation must belong to this pool instance");
#endif
    std::shared_lock readLock(tableRwMutex);
    hasher h;
    left.visit_chunks(addToHash, &h);
    right.visit_chunks(addToHash, &h);
    const auto hash = h.finish();
    weak_string_handle result{};
    auto readResult = do_concat_unsafe(hash, left, right, false, result);
    if (readResult == InternResult::NeedWriterLock) {
        readLock.unlock();
        std::unique_lock writeLock(tableRwMutex);
        auto writeResult = do_concat_unsafe(hash, left, right, true, result);
        assert(writeResult == InternResult::Success);
        return result.make_strong();
    } else {
        assert(readResult == InternResult::Success);
        return result.make_strong();
    }
}

bool operator==(const string_handle& lhs, const string_handle& rhs) {
    return lhs.equals(rhs);
}

bool operator!=(const string_handle& lhs, const string_handle& rhs) {
    return !(lhs == rhs);
}
