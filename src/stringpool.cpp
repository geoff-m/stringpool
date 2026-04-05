#include "stringpool/stringpool.h"
#include "include/hash.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

using namespace stringpool;

enum class EntryType : uint8_t {
    ATOM = 0,
    SHORT_STRING = 1,
    CONCAT_LEFT_ENTRY_RIGHT_ENTRY = 2,
    CONCAT_LEFT_ENTRY_RIGHT_SHORT = 3,
    CONCAT_LEFT_SHORT_RIGHT_ENTRY = 4,
    CONCAT_LEFT_SHORT_RIGHT_SHORT = 5,
};

constexpr uint64_t UPPER_7_MASK = 0xffffffffffffff00;
constexpr uint64_t UPPER_1_MASK = 0xff00000000000000;

// Minimum size of an atom entry.
constexpr uint64_t ATOM_ENTRY_SIZE = 8;

// Size of a concat entry.
constexpr uint64_t CONCAT_ENTRY_SIZE = 8 * 3;

namespace offsets {
    constexpr int ENTRY_TYPE_STRING_LENGTH = 0;

    namespace atom {
        constexpr int STRING_VALUE = 8;
    }

    namespace concat {
        constexpr int LEFT_PTR = 8;
        constexpr int RIGHT_PTR = 16;
    }
}

string_handle::string_handle(pool* owner, size_t dataIndex)
    : owner(owner), dataIndex(dataIndex) {
}

[[nodiscard]] size_t min(const size_t x, const size_t y) {
    return x <= y ? x : y;
}

string_handle::tree_walker::tree_walker(const pool& owner, size_t rootIndex) : tree_walker(owner.data, rootIndex) {
}

string_handle::tree_walker::tree_walker(char* baseAddress, size_t rootIndex)
    : baseAddress(baseAddress),
      rootIndex(rootIndex) {
    toVisit.emplace_back(rootIndex);
}

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

// Returns true iff the child is a leaf.
[[nodiscard]] size_t unpackLeftChild(char* base, size_t concatNodeIndex) {
    auto* child = base + concatNodeIndex + offsets::concat::LEFT_PTR;
    const auto type = unpack_node_type(base + concatNodeIndex);
    if (type == EntryType::CONCAT_LEFT_ENTRY_RIGHT_SHORT || type == EntryType::CONCAT_LEFT_ENTRY_RIGHT_ENTRY) {
        return *reinterpret_cast<size_t*>(child);
    }
    return concatNodeIndex + offsets::concat::LEFT_PTR;
}

// Returns true iff the child is a leaf.
[[nodiscard]] size_t unpackRightChild(char* base, size_t concatNodeIndex) {
    auto* child = base + concatNodeIndex + offsets::concat::RIGHT_PTR;
    const auto type = unpack_node_type(base + concatNodeIndex);
    if (type == EntryType::CONCAT_LEFT_ENTRY_RIGHT_ENTRY || type == EntryType::CONCAT_LEFT_SHORT_RIGHT_ENTRY) {
        return *reinterpret_cast<size_t*>(child);
    }
    return concatNodeIndex + offsets::concat::RIGHT_PTR;
}

size_t string_handle::tree_walker::get_next_bytes(char** bytes) {
    if (toVisit.empty())
        return 0;
    size_t current = toVisit.back();
    toVisit.pop_back();
    while (isConcat(baseAddress + current)) {
        const auto rightChildIndex = unpackRightChild(baseAddress, current);
        const auto leftChildIndex = unpackLeftChild(baseAddress, current);
        toVisit.emplace_back(rightChildIndex);
        current = leftChildIndex;
    }
    *bytes = unpackStringFromLeaf(baseAddress + current);
    return unpackLength(baseAddress + current);
}

size_t string_handle::copy_unsafe(char* destination, size_t destination_size) const {
    if (destination_size == 0)
        return 0;
    tree_walker walker(*owner, dataIndex);
    size_t copiedSoFar = 0;
    char* piece;
    size_t pieceLength;
    while (copiedSoFar < destination_size && 0 != (pieceLength = walker.get_next_bytes(&piece))) {
        const auto copyNow = std::min(pieceLength, destination_size - copiedSoFar);
        std::memcpy(destination + copiedSoFar, piece, copyNow);
        copiedSoFar += pieceLength;
    }
    return copiedSoFar;
}

size_t string_handle::copy(char* destination, size_t destination_size) const {
    if (destination_size == 0)
        return 0;
    std::shared_lock lock(owner->tableRwMutex);
    return copy_unsafe(destination, destination_size);
}

size_t string_handle::hash() const {
    hasher h;
    std::shared_lock lock(owner->tableRwMutex);
    tree_walker walker(*owner, dataIndex);
    char* piece;
    while (size_t pieceLength = walker.get_next_bytes(&piece)) {
        h.add(piece, pieceLength);
    }
    return h.finish();
}

void string_handle::visit_pieces(void (*callback)(char* piece, size_t pieceSize, void* state), void* state) const {
    std::shared_lock lock(owner->tableRwMutex);
    tree_walker walker(*owner, dataIndex);
    char* piece;
    while (size_t pieceLength = walker.get_next_bytes(&piece)) {
        callback(piece, pieceLength, state);
    }
}

// Assumes rhs is null-terminated.
int string_handle::strcmp(const char* rhs) const {
    std::shared_lock lock(owner->tableRwMutex);
    tree_walker walker(*owner, dataIndex);
    char* piece;
    size_t comparedChars = 0;
    while (size_t pieceLength = walker.get_next_bytes(&piece)) {
        const auto thisResult = std::strncmp(piece, rhs + comparedChars, pieceLength);
        if (thisResult != 0)
            return thisResult;
        comparedChars += pieceLength;
    }
    if (rhs[comparedChars] == 0)
        return 0; // Left and right finished at same time.
    return -1; // Left finished first.
}

int string_handle::strcmp(const string_handle& rhs) const {
    auto lock = pool::lock_for_reading(*owner, *rhs.owner);
    if (&owner == &rhs.owner && dataIndex == rhs.dataIndex)
        return 0;
    tree_walker leftWalker(*owner, dataIndex);
    tree_walker rightWalker(*rhs.owner, rhs.dataIndex);
    char* leftPiece;
    char* rightPiece;
    size_t leftPieceLength = leftWalker.get_next_bytes(&leftPiece);
    size_t rightPieceLength = rightWalker.get_next_bytes(&rightPiece);
    size_t leftPieceIndex = 0;
    size_t rightPieceIndex = 0;
    while (true) {
        if (leftPieceLength == 0 || rightPieceLength == 0) {
            if (leftPieceLength == 0 && rightPieceLength == 0)
                return 0;
            if (leftPieceLength == 0)
                return -1; // Left string is shorter.
            return 1; // Right string is shorter.
        }
        const auto thisLength = min(leftPieceLength, rightPieceLength);
        const auto thisResult = std::strncmp(
            leftPiece + leftPieceIndex,
            rightPiece + rightPieceIndex,
            thisLength);
        if (thisResult != 0)
            return thisResult;
        leftPieceIndex += thisLength;
        rightPieceIndex += thisLength;
        if (leftPieceIndex == leftPieceLength) {
            leftPieceLength = leftWalker.get_next_bytes(&leftPiece);
            leftPieceIndex = 0;
        }
        if (rightPieceIndex == rightPieceLength) {
            rightPieceLength = rightWalker.get_next_bytes(&rightPiece);
            rightPieceIndex = 0;
        }
    }
}

int string_handle::memcmp(const string_handle& rhs, size_t length) const {
    auto lock = pool::lock_for_reading(*owner, *rhs.owner);
    if (&owner == &rhs.owner && dataIndex == rhs.dataIndex)
        return 0;
    auto* leftEntry = owner->data + dataIndex;
    auto* rightEntry = rhs.owner->data + rhs.dataIndex;
    tree_walker leftWalker(*owner, dataIndex);
    tree_walker rightWalker(*rhs.owner, rhs.dataIndex);
    char* leftPiece;
    char* rightPiece;
    size_t leftPieceLength = leftWalker.get_next_bytes(&leftPiece);
    size_t rightPieceLength = rightWalker.get_next_bytes(&rightPiece);
    assert(unpackLength(leftEntry) >= length && unpackLength(rightEntry) >= length);
    size_t leftPieceIndex = 0;
    size_t rightPieceIndex = 0;
    size_t comparedChars = 0;
    while (comparedChars < length) {
        const auto thisLength = min(leftPieceLength, rightPieceLength);
        if (thisLength == 0)
            std::abort(); // at least one string is shorter than the given length!
        const auto thisResult = std::memcmp(
            leftPiece + leftPieceIndex,
            rightPiece + rightPieceIndex,
            thisLength);
        if (thisResult != 0)
            return thisResult;
        comparedChars += thisLength;
        leftPieceIndex += thisLength;
        rightPieceIndex += thisLength;
        if (leftPieceIndex == leftPieceLength) {
            leftPieceLength = leftWalker.get_next_bytes(&leftPiece);
            leftPieceIndex = 0;
        }
        if (rightPieceIndex == rightPieceLength) {
            rightPieceLength = rightWalker.get_next_bytes(&rightPiece);
            rightPieceIndex = 0;
        }
    }
    return 0;
}

[[nodiscard]] int string_handle::memcmp_unsafe(const char* rhs, size_t length) const {
    tree_walker walker(*owner, dataIndex);
    char* piece;
    size_t charsCompared = 0;
    size_t pieceLength = walker.get_next_bytes(&piece);
    size_t pieceIndex = 0;
    size_t iterations = 0;
    while (charsCompared < length) {
        ++iterations;
        assert(iterations < 10);
        if (pieceLength == 0) {
            std::string msg = "Length argument of ";
            msg += std::to_string(length);
            msg += " exceeds this string's length of ";
            msg += std::to_string(unpackLength(owner->data + dataIndex));
            throw std::invalid_argument(msg);
        }
        const auto thisLength = min(pieceLength, length);
        const auto thisResult = std::memcmp(piece, rhs + charsCompared, thisLength);
        if (thisResult != 0)
            return thisResult;
        charsCompared += thisLength;
        pieceIndex += thisLength;
        if (pieceIndex == pieceLength) {
            pieceLength = walker.get_next_bytes(&piece);
            pieceIndex = 0;
        }
    }
    return 0;
}

int string_handle::memcmp(const char* rhs, size_t length) const {
    auto lock = pool::lock_for_reading(*owner);
    return memcmp_unsafe(rhs, length);
}

bool string_handle::equals_unsafe(const char* rhs, size_t length) const {
    if (unpackLength(owner->data + dataIndex) != length)
        return false;
    return 0 == memcmp_unsafe(rhs, length);
}

bool string_handle::equals(const char* rhs, size_t length) const {
    auto lock = pool::lock_for_reading(*owner);
    return equals_unsafe(rhs, length);
}

bool string_handle::equals(const char* rhs) const {
    return equals(rhs, strlen(rhs));
}

bool string_handle::equals(const string_handle& rhs) const {
    if (this == &rhs)
        return true;
    auto lock = pool::lock_for_reading(*owner, *rhs.owner);
    tree_walker leftWalker(*owner, dataIndex);
    tree_walker rightWalker(*rhs.owner, rhs.dataIndex);
    char* leftPiece;
    char* rightPiece;
    size_t leftPieceLength = leftWalker.get_next_bytes(&leftPiece);
    size_t rightPieceLength = rightWalker.get_next_bytes(&rightPiece);
    if (leftPieceLength != rightPieceLength)
        return false;
    size_t leftPieceIndex = 0;
    size_t rightPieceIndex = 0;
    while (true) {
        if (leftPieceLength == 0 || rightPieceLength == 0)
            return leftPieceLength == 0 && rightPieceLength == 0;
        const auto thisLength = min(leftPieceLength, rightPieceLength);
        const auto thisResult = std::memcmp(
            leftPiece + leftPieceIndex,
            rightPiece + rightPieceIndex,
            thisLength);
        if (thisResult != 0)
            return thisResult;
        leftPieceIndex += thisLength;
        rightPieceIndex += thisLength;
        if (leftPieceIndex == leftPieceLength) {
            leftPieceLength = leftWalker.get_next_bytes(&leftPiece);
            leftPieceIndex = 0;
        }
        if (rightPieceIndex == rightPieceLength) {
            rightPieceLength = rightWalker.get_next_bytes(&rightPiece);
            rightPieceIndex = 0;
        }
    }
}


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

char* pool::add_atom_unsafe(const char* string, size_t size) {
    const auto blockSize = 16 + size;
    updateDataSizeUnsafe(dataSize + blockSize);
    char* startOfAtom = data + dataSize - blockSize;
    if (size != (size & ~UPPER_1_MASK))
        std::abort(); // string is too large.
    startOfAtom[0] = static_cast<char>(EntryType::ATOM);
    std::memcpy(startOfAtom + 1, &size, 7);
    std::memcpy(startOfAtom + offsets::atom::STRING_VALUE, string, size);
    return startOfAtom;
}

bool string_handle::concat_equals_unsafe(string_handle single, string_handle left, string_handle right) {
    auto* singleEntry = single.owner->data + single.dataIndex;
    auto* leftEntry = left.owner->data + left.dataIndex;
    auto* rightEntry = right.owner->data + right.dataIndex;
    const auto comparandLength = unpackLength(singleEntry);
    const auto leftLength = unpackLength(leftEntry);
    const auto rightLength = unpackLength(rightEntry);
    if (comparandLength != leftLength + rightLength)
        return false;
    tree_walker singleWalker(*single.owner, single.dataIndex);
    tree_walker comparandWalker(*left.owner, left.dataIndex);
    bool onLeft = true;
    char* singlePiece;
    char* comparandPiece;
    size_t comparedLength = 0;
    size_t singlePieceLength = singleWalker.get_next_bytes(&singlePiece);
    size_t comparandPieceLength = comparandWalker.get_next_bytes(&comparandPiece);
    size_t entryPieceIndex = 0;
    size_t comparandPieceIndex = 0;
    while (true) {
        if (singlePieceLength == 0 || comparandPieceLength == 0)
            return comparedLength == comparandLength;
        const auto thisLength = min(singlePieceLength, comparandPieceLength);
        const auto thisResult = std::memcmp(
            singlePiece + entryPieceIndex,
            comparandPiece + comparandPieceIndex,
            thisLength);
        if (thisResult != 0)
            return thisResult;
        comparedLength += thisLength;
        entryPieceIndex += thisLength;
        comparandPieceIndex += thisLength;
        if (entryPieceIndex == singlePieceLength) {
            singlePieceLength = singleWalker.get_next_bytes(&singlePiece);
            entryPieceIndex = 0;
        }
        if (comparandPieceIndex == comparandPieceLength) {
            comparandPieceLength = comparandWalker.get_next_bytes(&comparandPiece);
            if (comparandPieceLength == 0 && onLeft) {
                onLeft = false;
                comparandWalker = tree_walker(*right.owner, right.dataIndex);
                comparandPieceLength = comparandWalker.get_next_bytes(&comparandPiece);
            }
            comparandPieceIndex = 0;
        }
    }
}

size_t string_handle::size() const {
    auto lock = pool::lock_for_reading(*owner);
    return unpackLength(owner->data + dataIndex);
}

size_t string_handle::length() const {
    return size();
}

static void addToHash(char* piece, size_t size, void* pHasher) {
    static_cast<hasher*>(pHasher)->add(piece, size);
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
                (leftLength << 4) | static_cast<char>(EntryType::SHORT_STRING));
            left.copy_unsafe(startOfConcat + offsets::concat::LEFT_PTR + 1, 7);
        } else {
            std::memcpy(startOfConcat + offsets::concat::LEFT_PTR, &left.dataIndex, 8);
        }
        if (rightIsShort) {
            startOfConcat[offsets::concat::RIGHT_PTR] = static_cast<char>(
                (rightLength << 4) | static_cast<char>(EntryType::SHORT_STRING));
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
