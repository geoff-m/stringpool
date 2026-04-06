#include "stringpool/stringpool.h"
#include "include/hash.h"
#include "include/pack_utils.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

using namespace stringpool;

pool::pool(size_t initial_table_capacity, size_t initial_data_capacity)
    : data(static_cast<char*>(std::calloc(initial_data_capacity, 1))),
      dataCapacity(initial_data_capacity) {
    table.reserve(initial_table_capacity);
}

pool::pool()
    : pool(128, 4096) {
}

pool::~pool() {
    std::free(data);
}

string_handle pool::intern(const char* string) {
    return intern(string, strlen(string));
}

pool::InternResult pool::do_intern_unsafe(size_t hash, const char* string, size_t size, bool haveWriterLock,
                                          string_handle& result) {
    auto it = table.find(hash);
    if (it != table.end()) {
        auto existingEntries = it->second;
        for (string_handle existingEntry : existingEntries) {
            if (existingEntry.equals_unsafe(string, size)) {
                result = existingEntry;
                ++internHits;
                return InternResult::Success;
            }
        }
        if (!haveWriterLock)
            return InternResult::NeedWriterLock;
        char* atom = add_atom_unsafe(string, size);
        string_handle ret(this, atom - data);
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
    string_handle ret(this, atom - data);
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

void pool::updateDataSizeUnsafe(size_t newSize) {
    if (newSize > dataCapacity) {
        const auto newDataCapacity = std::max(
            dataCapacity + (dataCapacity >> 1),
            dataCapacity + (newSize - dataCapacity) * 2);
        dataCapacity = newDataCapacity;
        data = static_cast<char*>(std::realloc(data, dataCapacity));
    }
    dataSize = newSize;
    assert(dataCapacity >= newSize);
}

char* pool::add_atom_unsafe(const char* string, size_t stringSize) {
    if (stringSize < 256) {
        const auto blockSize = 2 + stringSize;
        updateDataSizeUnsafe(dataSize + blockSize);
        char* startOfAtom = data + dataSize - blockSize;
        startOfAtom[0] = static_cast<char>(EntryType::SHORT_ATOM);
        startOfAtom[offsets::short_atom::STRING_LENGTH] = static_cast<char>(stringSize);
        std::memcpy(startOfAtom + offsets::short_atom::STRING_VALUE, string, stringSize);
        return startOfAtom;
    } else {
        const auto blockSize = 16 + stringSize;
        updateDataSizeUnsafe(dataSize + blockSize);
        char* startOfAtom = data + dataSize - blockSize;
        if (stringSize != (stringSize & ~UPPER_1_MASK))
            std::abort(); // string is too large.
        startOfAtom[0] = static_cast<char>(EntryType::ATOM);
        std::memcpy(startOfAtom + offsets::atom::STRING_LENGTH, &stringSize, 7);
        std::memcpy(startOfAtom + offsets::atom::STRING_VALUE, string, stringSize);
        return startOfAtom;
    }
}

static void addToHash(char* piece, size_t size, void* pHasher) {
    static_cast<hasher*>(pHasher)->add(piece, size);
}

string_handle pool::insertConcatUnsafe(size_t hash, string_handle left, string_handle right) {
    const auto* leftEntry = left.owner->data + left.dataIndex;
    const auto* rightEntry = right.owner->data + right.dataIndex;
    const auto leftLength = unpackLength(leftEntry);
    const auto rightLength = unpackLength(rightEntry);
    const auto totalLength = leftLength + rightLength;
    if (totalLength <= CONCAT_ENTRY_SIZE - ATOM_ENTRY_SIZE) {
        // We will store the concatenation as a single atom node.
        const auto blockSize = ATOM_ENTRY_SIZE + totalLength;
        updateDataSizeUnsafe(dataSize + blockSize);
        char* startOfAtom = data + dataSize - blockSize;
        startOfAtom[0] = static_cast<char>(EntryType::ATOM);
        std::memcpy(startOfAtom + 1, &totalLength, 7);
        left.copy_unsafe(startOfAtom + offsets::atom::STRING_VALUE, leftLength);
        right.copy_unsafe(startOfAtom + offsets::atom::STRING_VALUE + leftLength, rightLength);
        auto r = table.emplace(hash, std::list<string_handle>());
        string_handle ret(this, startOfAtom - data);
        r.first->second.push_back(ret);
        return ret;
    } else {
        // We will store the concatenation as a concat node.
        // We should never have both short in a concat,
        // since if we could, we'd just make it an atom instead of a concat.
        const auto blockSize = CONCAT_ENTRY_SIZE;
        updateDataSizeUnsafe(dataSize + blockSize);
        char* startOfConcat = data + dataSize - blockSize;
        const auto leftIsShort = leftLength <= 7;
        const auto rightIsShort = rightLength <= 7;
        startOfConcat[0] = static_cast<char>(makeConcatType(leftIsShort, rightIsShort));
        std::memcpy(startOfConcat + 1, &totalLength, 7);
        if (leftIsShort) {
            startOfConcat[offsets::concat::LEFT_PTR] = static_cast<char>(
                (leftLength << 4) | static_cast<char>(EntryType::SHORT_CONCAT_CHILD));
            left.copy_unsafe(startOfConcat + offsets::concat::LEFT_PTR + 1, 7);
        } else {
            std::memcpy(startOfConcat + offsets::concat::LEFT_PTR, &left.dataIndex, 8);
        }
        if (rightIsShort) {
            startOfConcat[offsets::concat::RIGHT_PTR] = static_cast<char>(
                (rightLength << 4) | static_cast<char>(EntryType::SHORT_CONCAT_CHILD));
            right.copy_unsafe(startOfConcat + offsets::concat::RIGHT_PTR + 1, 7);
        } else {
            std::memcpy(startOfConcat + offsets::concat::RIGHT_PTR, &right.dataIndex, 8);
        }
        auto r = table.emplace(hash, std::list<string_handle>());
        string_handle ret(this, startOfConcat - data);
        r.first->second.push_back(ret);
        return ret;
    }
}

pool::InternResult pool::concat_helper_unsafe(size_t hash, string_handle left, string_handle right, bool haveWriterLock,
                                              string_handle& result) {
    auto it = table.find(hash);
    if (it == table.end()) {
        // Nothing in table has this hash.
        if (!haveWriterLock)
            return InternResult::NeedWriterLock;
        result = insertConcatUnsafe(hash, left, right);
        return InternResult::Success;
    }
    auto existingEntries = it->second;
    for (string_handle existingEntry : existingEntries) {
        if (string_handle::concat_equals_unsafe(existingEntry, left, right)) {
            result = existingEntry;
            return InternResult::Success;
        }
    }
    if (!haveWriterLock)
        return InternResult::NeedWriterLock;
    result = insertConcatUnsafe(hash, left, right);
    return InternResult::Success;
}

string_handle pool::concat(string_handle left, string_handle right) {
    if (left.owner != this || right.owner != this)
        throw std::invalid_argument("Concatenated strings must be owned by this string_pool");

    auto readLock = lock_for_reading(*this);
    hasher h;
    left.visit_pieces(addToHash, &h);
    right.visit_pieces(addToHash, &h);
    const auto hash = h.finish();
    string_handle result{};
    auto readResult = concat_helper_unsafe(hash, left, right, false, result);
    if (readResult == InternResult::NeedWriterLock) {
        readLock.unlock();
        auto writeLock = lock_for_writing(*this);
        auto writeResult = concat_helper_unsafe(hash, left, right, true, result);
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
