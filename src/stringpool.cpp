#include "stringpool/stringpool.h"
#include "include/hash.h"
#include "include/pack_utils.h"
#include <cassert>
#include <cstring>
#include <stdexcept>

using namespace stringpool;

pool::pool(size_t initial_table_capacity) {
    table.reserve(initial_table_capacity);
}

pool::pool()
    : pool(32) {
}

pool::~pool() {
    for (auto* buffer : data)
        delete[] buffer;
}

string_handle pool::intern(const char* string) {
    return intern(string, strlen(string));
}

// Attempts to intern the given string, or else return its handle if it already exists in the cache.
pool::InternResult pool::do_intern_unsafe(size_t hash, const char* string, size_t size, bool haveWriterLock,
                                          string_handle& result) {
    auto it = table.find(hash);
    if (it != table.end()) {
        auto existingEntries = it->second;
        for (string_handle existingEntry : existingEntries) {
            if (existingEntry.equals(string, size)) {
                result = existingEntry;
                ++internHits;
                return InternResult::Success;
            }
        }
        if (!haveWriterLock)
            return InternResult::NeedWriterLock;
        char* atom = add_atom_unsafe(string, size);
#ifdef STRINGPOOL_TRACK_OWNERS
        string_handle ret(atom, this);
#else
        string_handle ret(atom);
#endif
        existingEntries.push_back(ret);
        result = ret;
        ++internMisses;
        return InternResult::Success;
    }
    // Nothing in table has this hash.
    if (!haveWriterLock)
        return InternResult::NeedWriterLock;
    char* atom = add_atom_unsafe(string, size);
    auto r = table.emplace(hash, std::list<string_handle>());
#ifdef STRINGPOOL_TRACK_OWNERS
    string_handle ret(atom, this);
#else
    string_handle ret(atom);
#endif
    r.first->second.push_back(ret);
    result = ret;
    ++internMisses;
    return InternResult::Success;
}

string_handle pool::intern(const char* string, size_t size) {
    const auto hash = hasher::hash(string, size);
    auto readLock = lock_for_reading(*this);
    ++totalInternRequestCount;
    totalInternRequestSize += size;
    string_handle result{};
    auto resultWithRead = do_intern_unsafe(hash, string, size, false, result);
    if (resultWithRead == InternResult::NeedWriterLock) {
        readLock.unlock();
        auto writeLock = lock_for_writing(*this);
        auto resultWithWrite = do_intern_unsafe(hash, string, size, true, result);
        assert(resultWithWrite == InternResult::Success);
        return result;
    } else {
        assert(resultWithRead == InternResult::Success);
        return result;
    }
}

string_handle pool::intern(std::string_view string) {
    return intern(string.data(), string.size());
}

char* pool::add_buffer(size_t size) {
#ifdef STRINGPOOL_REFCOUNT_ENABLE
    // Ensure size_t alignment so that ref count is aligned.
    auto qWords = size / sizeof(size_t);
    if (qWords * sizeof(size_t) != size)
        ++qWords;
    auto* ret = reinterpret_cast<char*>(new size_t[qWords]);
#else
    auto* ret = new char[size];
#endif
    data.emplace_back(ret);
    totalDataSize += size;
    return ret;
}

char* pool::add_atom_unsafe(const char* string, size_t stringSize) {
    EntryType type;
    const auto blockSize = compute_atom_size(stringSize, &type);
    char* startOfAtom = add_buffer(blockSize);
    if (type == EntryType::SHORT_ATOM) {
        startOfAtom[offsets::short_atom::NODE_TYPE] = static_cast<char>(EntryType::SHORT_ATOM);
        startOfAtom[offsets::short_atom::STRING_LENGTH] = static_cast<char>(stringSize);
        std::memcpy(startOfAtom + offsets::short_atom::STRING_VALUE, string, stringSize);
        assert(get_length(startOfAtom) == stringSize);
        return startOfAtom;
    } else {
        if (stringSize != (stringSize & ~UPPER_1_MASK)) [[unlikely]]
                std::abort(); // string is too large.
        startOfAtom[offsets::atom::NODE_TYPE] = static_cast<char>(EntryType::ATOM);
        std::memcpy(startOfAtom + offsets::atom::STRING_LENGTH, &stringSize, 7);
        std::memcpy(startOfAtom + offsets::atom::STRING_VALUE, string, stringSize);
        assert(get_length(startOfAtom) == stringSize);
        return startOfAtom;
    }
}

static void addToHash(const char* piece, size_t size, void* pHasher) {
    static_cast<hasher*>(pHasher)->add(piece, size);
}

string_handle pool::add_concat_unsafe(size_t hash, string_handle left, string_handle right) {
    const auto leftLength = get_length(left.data);
    const auto rightLength = get_length(right.data);
    const auto totalLength = leftLength + rightLength;

    if (totalLength <= MAX_SHORT_ATOM_STRING_LENGTH) {
        // We will store the concatenation as a single atom node.

        EntryType type;
        const auto atomSize = compute_atom_size(totalLength, &type);
        assert(type == EntryType::SHORT_ATOM);
        char* startOfAtom = add_buffer(atomSize);
        *reinterpret_cast<size_t*>(startOfAtom + offsets::short_atom::REFCOUNT) = 0x1badf00d;
        startOfAtom[offsets::short_atom::NODE_TYPE] = static_cast<char>(EntryType::SHORT_ATOM);
        std::memcpy(startOfAtom + offsets::short_atom::STRING_LENGTH, &totalLength, 7);
        left.copy(startOfAtom + offsets::short_atom::STRING_VALUE, leftLength);
        right.copy(startOfAtom + offsets::short_atom::STRING_VALUE + leftLength, rightLength);
        assert(get_length(startOfAtom) == totalLength);
        auto r = table.emplace(hash, std::list<string_handle>());
#ifdef STRINGPOOL_TRACK_OWNERS
        string_handle ret(startOfAtom, this);
#else
        string_handle ret(startOfAtom);
#endif
        r.first->second.push_back(ret);
        return ret;
    } else {
        // We will store the concatenation as a concat node.
        // We should never have both short in a concat,
        // since if we could, we'd just make it an atom instead of a concat.
        const auto blockSize = sizes::CONCAT;
        char* concat = add_buffer(blockSize);
        *reinterpret_cast<size_t*>(concat + offsets::concat::REFCOUNT) = 0x2badf00d;
        concat[offsets::concat::NODE_TYPE] = static_cast<char>(EntryType::CONCAT);
        std::memcpy(concat + offsets::concat::STRING_LENGTH, &totalLength, 7);
        std::memcpy(concat + offsets::concat::LEFT_PTR, &left.data, 8);
        std::memcpy(concat + offsets::concat::RIGHT_PTR, &right.data, 8);
        auto r = table.emplace(hash, std::list<string_handle>());
#ifdef STRINGPOOL_TRACK_OWNERS
        string_handle ret(concat, this);
#else
        string_handle ret(concat);
#endif
        r.first->second.push_back(ret);
        return ret;
    }
}

// Attempts to intern the concatenation of the given string handles, or else return its handle if it already exists in the cache.
pool::InternResult pool::do_concat_unsafe(size_t hash, string_handle left, string_handle right, bool haveWriterLock,
                                          string_handle& result) {
    auto it = table.find(hash);
    if (it == table.end()) {
        // Nothing in table has this hash.
        if (!haveWriterLock)
            return InternResult::NeedWriterLock;
        result = add_concat_unsafe(hash, left, right);
        return InternResult::Success;
    }
    auto existingEntries = it->second;
    for (string_handle existingEntry : existingEntries) {
        if (string_handle::concat_equals(existingEntry, left, right)) {
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
    auto readLock = lock_for_reading(*this);
    hasher h;
    left.visit_chunks(addToHash, &h);
    right.visit_chunks(addToHash, &h);
    const auto hash = h.finish();
    string_handle result{};
    auto readResult = do_concat_unsafe(hash, left, right, false, result);
    if (readResult == InternResult::NeedWriterLock) {
        readLock.unlock();
        auto writeLock = lock_for_writing(*this);
        auto writeResult = do_concat_unsafe(hash, left, right, true, result);
        assert(writeResult == InternResult::Success);
        return result;
    } else {
        assert(readResult == InternResult::Success);
        return result;
    }
}

bool operator==(const string_handle& lhs, const string_handle& rhs) {
    return lhs.equals(rhs);
}

bool operator!=(const string_handle& lhs, const string_handle& rhs) {
    return !(lhs == rhs);
}
