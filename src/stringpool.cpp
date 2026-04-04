// todo: can't have parent pointers,
// because we want to allow things to have multiple parents.
// therefore we have to use temporary stacks for walking trees.
// on the bright side, we don't have to store parent pointers.
#include "stringpool/stringpool.h"
#include "include/hash.h"
#include <atomic>
#include <cassert>
#include <cstring>
#include <cstdlib>

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

string_handle::string_handle(pool& owner, char* data)
    : owner(owner), entry(data) {
}

[[nodiscard]] size_t min(const size_t x, const size_t y) {
    return x <= y ? x : y;
}

string_handle::tree_walker::tree_walker(char* root)
    : root(root) {
    toVisit.emplace_back(root);
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
[[nodiscard]] char* unpackLeftChild(char* concatNode) {
    auto* child = concatNode + offsets::concat::LEFT_PTR;
    const auto type = unpack_node_type(concatNode);
    if (type == EntryType::CONCAT_LEFT_ENTRY_RIGHT_SHORT || type == EntryType::CONCAT_LEFT_ENTRY_RIGHT_ENTRY) {
        return *reinterpret_cast<char**>(child);
    }
    return concatNode + offsets::concat::LEFT_PTR;
}

// Returns true iff the child is a leaf.
[[nodiscard]] char* unpackRightChild(char* concatNode) {
    auto* child = concatNode + offsets::concat::RIGHT_PTR;
    const auto type = unpack_node_type(concatNode);
    if (type == EntryType::CONCAT_LEFT_ENTRY_RIGHT_ENTRY || type == EntryType::CONCAT_LEFT_SHORT_RIGHT_ENTRY) {
        return *reinterpret_cast<char**>(child);
    }
    return concatNode + offsets::concat::RIGHT_PTR;
}

size_t string_handle::tree_walker::get_next_bytes(char** bytes) {
    if (toVisit.empty())
        return 0;
    auto* current = toVisit.back();
    toVisit.pop_back();
    while (isConcat(current)) {
        toVisit.emplace_back(unpackRightChild(current));
        current = unpackLeftChild(current);
    }
    *bytes = unpackStringFromLeaf(current);
    return unpackLength(current);
}

size_t string_handle::copy(char* destination, size_t destination_size) const {
    if (destination_size == 0)
        return 0;
    tree_walker walker(entry);
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

size_t string_handle::hash() const {
    hasher h;
    tree_walker walker(entry);
    char* piece;
    while (size_t pieceLength = walker.get_next_bytes(&piece)) {
        h.add(piece, pieceLength);
    }
    return h.finish();
}

void string_handle::visit_pieces(void (*callback)(char* piece, size_t pieceSize, void* state), void* state) const {
    tree_walker w(entry);
    char* piece;
    while (size_t pieceLength = w.get_next_bytes(&piece)) {
        callback(piece, pieceLength, state);
    }
}

// Assumes rhs is null-terminated.
int string_handle::strcmp(const char* rhs) const {
    tree_walker walker(entry);
    char* piece;
    while (size_t pieceLength = walker.get_next_bytes(&piece)) {
        const auto thisResult = std::strncmp(piece, rhs, pieceLength);
        if (thisResult != 0)
            return thisResult;
    }
    return 0;
}

int string_handle::strcmp(const string_handle& rhs) const {
    if (entry == rhs.entry)
        return 0;
    tree_walker leftWalker(entry);
    tree_walker rightWalker(rhs.entry);
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
                return 1; // Right string is shorter.
            return -1; // Left string is shorter.
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
        if (leftPieceLength == leftPieceLength - 1)
            leftPieceLength = leftWalker.get_next_bytes(&leftPiece);
        if (rightPieceIndex == rightPieceLength - 1)
            rightPieceLength = rightWalker.get_next_bytes(&rightPiece);
    }
}

bool string_handle::equal_entry(char* rhsEntry, size_t length) const {
    if (entry == rhsEntry)
        return true;
    if (length == 0)
        return true;
    tree_walker leftWalker(entry);
    tree_walker rightWalker(rhsEntry);
    char* leftPiece;
    char* rightPiece;
    size_t leftPieceLength = leftWalker.get_next_bytes(&leftPiece);
    size_t rightPieceLength = rightWalker.get_next_bytes(&rightPiece);
    if (leftPieceLength <= length) {
        if (rightPieceLength != leftPieceLength)
            return false;
    }
    if (rightPieceLength <= length) {
        if (rightPieceLength != leftPieceLength)
            return false;
    }
    size_t leftPieceIndex = 0;
    size_t rightPieceIndex = 0;
    size_t comparedChars = 0;
    while (true) {
        if (leftPieceLength == 0 || rightPieceLength == 0)
            return false;
        const auto thisLength = min(leftPieceLength, rightPieceLength);
        const auto thisResult = std::memcmp(
            leftPiece + leftPieceIndex,
            rightPiece + rightPieceIndex,
            thisLength);
        if (thisResult != 0)
            return false;
        if (comparedChars >= length)
            return true;
        comparedChars += thisLength;
        leftPieceIndex += thisLength;
        rightPieceIndex += thisLength;
        if (leftPieceLength == leftPieceLength - 1)
            leftPieceLength = leftWalker.get_next_bytes(&leftPiece);
        if (rightPieceIndex == rightPieceLength - 1)
            rightPieceLength = rightWalker.get_next_bytes(&rightPiece);
    }
}

int string_handle::memcmp(const string_handle& rhs) const {
    return equal_entry(rhs.entry, unpackLength(entry));
}

int string_handle::memcmp(const char* rhs, size_t rhsLength) const {
    tree_walker walker(entry);
    char* piece;
    size_t i = 0;
    while (size_t pieceLength = walker.get_next_bytes(&piece)) {
        const auto thisLength = min(pieceLength, rhsLength);
        const auto thisResult = std::memcmp(rhs + i, piece, thisLength);
        if (thisResult != 0)
            return thisResult;
        i += thisLength;
    }
    return 0;
}

bool string_handle::equals(const char* rhs, size_t length) const {
    return 0 == memcmp(rhs, length);
}

bool string_handle::equals(const char* rhs) const {
    return equals(rhs, strlen(rhs));
}

bool string_handle::equals(const string_handle& rhs) const {
    if (this == &rhs)
        return true;
    tree_walker leftWalker(entry);
    tree_walker rightWalker(rhs.entry);
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
        if (leftPieceIndex == leftPieceLength)
            leftPieceLength = leftWalker.get_next_bytes(&leftPiece);
        if (rightPieceIndex == rightPieceLength)
            rightPieceLength = rightWalker.get_next_bytes(&rightPiece);
    }
}

pool::table_lock::table_lock(pool& owner)
    : owner(owner) {
    std::atomic_ref(owner.tableState).wait(TableState::Busy, std::memory_order::relaxed);
}

pool::table_lock::~table_lock() {
    owner.tableState = TableState::Ready;
}

void pool::updateTableSize(size_t newSize) {
    tableSize = newSize;
    if (tableSize > tableSizeBeforeReallocate) {
        const auto newCapacity = tableCapacity * 2;
        table = static_cast<char**>(std::realloc(table, newCapacity));
        tableCapacity = newCapacity;
        tableSizeBeforeReallocate = static_cast<size_t>(newCapacity / 4 * 3);
    }
}

void pool::updateDataSize(size_t newSize) {
    if (newSize > dataCapacity) {
        data = static_cast<char*>(std::realloc(data, dataCapacity + (dataCapacity >> 1)));
    }
    dataSize = newSize;
}


pool::pool(size_t initial_table_capacity, size_t initial_data_capacity)
    : data(static_cast<char*>(std::calloc(initial_data_capacity, 1))),
      dataCapacity(initial_data_capacity),
      table(static_cast<char**>(std::calloc(initial_table_capacity, sizeof(char*)))),
      tableCapacity(initial_table_capacity),
      tableSizeBeforeReallocate(static_cast<size_t>(initial_table_capacity / 4 * 3)) {
}

pool::pool()
    : pool(128, 4096) {
}

pool::~pool() {
    std::free(data);
    std::free(table);
}

string_handle pool::intern(const char* string) {
    return intern(string, strlen(string));
}

string_handle pool::intern(const char* string, size_t size) {
    const auto h = hasher::hash(string, size);
    const auto indexMask = tableCapacity - 1;
    auto index = static_cast<size_t>(h & indexMask);
    table_lock lock(*this);
    char* entry;
    for (entry = table[index]; entry != 0; index = (index + 1) & indexMask) {
        // Possible collision. Check for data equality.
        if (equals(string, size, entry)) {
            // Found identical string already in table.
            return string_handle(*this, entry);
        }

        // Load from next index.
        entry = table[index];
    }

    // entry == 0, so no existing entry has this hash.
    // Make copy and insert.
    char* atom = addAtom(string, size);
    table[index] = atom;
    return string_handle(*this, atom);
}


char* pool::addAtom(const char* string, size_t size) {
    updateTableSize(tableSize + 1);
    const auto blockSize = 16 + size;
    updateDataSize(dataSize + blockSize);
    assert(size == (size & ~UPPER_1_MASK)); // todo: replace this assert with some nicer failure mechanism
    char* startOfAtom = data + dataSize - blockSize;
    startOfAtom[0] = static_cast<char>(EntryType::ATOM);
    std::memcpy(startOfAtom + 1, &size, 7);
    std::memcpy(startOfAtom + offsets::atom::STRING_VALUE, string, size);
    return startOfAtom;
}

bool string_handle::concat_equals(char* entry, string_handle left, string_handle right) {
    const auto entryLength = unpackLength(entry);
    const auto leftLength = unpackLength(left.entry);
    const auto rightLength = unpackLength(right.entry);
    if (entryLength != leftLength + rightLength)
        return false;

    tree_walker entryWalker(entry);
    tree_walker comparandWalker(left.entry);
    bool onLeft = true;
    char* entryPiece;
    char* comparandPiece;
    size_t comparedLength = 0;
    size_t entryPieceLength = entryWalker.get_next_bytes(&entryPiece);
    size_t comparandPieceLength = comparandWalker.get_next_bytes(&comparandPiece);
    if (entryPieceLength != comparandPieceLength)
        return false; // Root pieces must be of equal lengths.
    size_t entryPieceIndex = 0;
    size_t comparandPieceIndex = 0;
    while (true) {
        if (entryPieceLength == 0 || comparandPieceLength == 0)
            return comparedLength == entryPieceIndex;
        const auto thisLength = min(entryPieceLength, comparandPieceLength);
        const auto thisResult = std::memcmp(
            entryPiece + entryPieceIndex,
            comparandPiece + comparandPieceIndex,
            thisLength);
        if (thisResult != 0)
            return thisResult;
        comparedLength += thisLength;
        entryPieceIndex += thisLength;
        comparandPieceIndex += thisLength;
        if (entryPieceLength == entryPieceLength - 1) {
            entryPieceLength = entryWalker.get_next_bytes(&entryPiece);
            entryPieceIndex = 0;
        }
        if (comparandPieceIndex == comparandPieceLength - 1) {
            comparandPieceLength = comparandWalker.get_next_bytes(&comparandPiece);
            if (comparandPieceLength == 0 && onLeft) {
                onLeft = false;
                comparandWalker = tree_walker(right.entry);
                comparandPieceLength = comparandWalker.get_next_bytes(&comparandPiece);
            }
            comparandPieceIndex = 0;
        }
    }
}

void addToHash(char* piece, size_t size, void* pHasher) {
    static_cast<hasher*>(pHasher)->add(piece, size);
}

[[nodiscard]] EntryType makeConcatType(bool leftIsShort, bool rightIsShort) {
    if (!leftIsShort && !rightIsShort)
        return EntryType::CONCAT_LEFT_ENTRY_RIGHT_ENTRY;
    if (leftIsShort && !rightIsShort)
        return EntryType::CONCAT_LEFT_SHORT_RIGHT_ENTRY;
    if (!leftIsShort && rightIsShort)
        return EntryType::CONCAT_LEFT_ENTRY_RIGHT_SHORT;
    if (leftIsShort && rightIsShort)
        return EntryType::CONCAT_LEFT_SHORT_RIGHT_SHORT;
}

string_handle pool::concat(string_handle left, string_handle right) {
    hasher h;
    left.visit_pieces(addToHash, &h);
    right.visit_pieces(addToHash, &h);
    const auto hash = h.finish();
    const auto indexMask = tableCapacity - 1;
    auto index = static_cast<size_t>(hash & indexMask);
    table_lock lock(*this);
    char* entry;
    for (entry = table[index]; entry != 0; index = (index + 1) & indexMask) {
        // Possible collision. Check for data equality.
        if (string_handle::concat_equals(entry, left, right)) {
            // Found identical string already in table.
            return string_handle(*this, entry);
        }

        // Load from next index.
        entry = table[index];
    }

    // entry == 0, so no existing entry has this hash.
    const auto leftLength = unpackLength(left.entry);
    const auto rightLength = unpackLength(right.entry);
    const auto totalLength = leftLength + rightLength;
    if (totalLength <= CONCAT_ENTRY_SIZE - ATOM_ENTRY_SIZE) {
        // We will store the concatenation as a single atom node.
        updateTableSize(tableSize + 1);
        const auto blockSize = ATOM_ENTRY_SIZE + totalLength;
        updateDataSize(dataSize + blockSize);
        char* startOfAtom = data + dataSize - blockSize;
        startOfAtom[0] = static_cast<char>(EntryType::ATOM);
        std::memcpy(startOfAtom + 1, &totalLength, 7);
        left.copy(startOfAtom + offsets::atom::STRING_VALUE, leftLength);
        right.copy(startOfAtom + offsets::atom::STRING_VALUE + leftLength, rightLength);
        table[index] = startOfAtom;
        return string_handle(*this, startOfAtom);
    } else {
        // We will store the concatenation as a concat node.
        // We should never have both short in a concat,
        // since if we could, we'd just make it an atom instead of a concat.
        updateTableSize(tableSize + 1);
        const auto blockSize = CONCAT_ENTRY_SIZE;
        updateDataSize(dataSize + blockSize);
        char* startOfConcat = data + dataSize - blockSize;
        const auto leftIsShort = leftLength <= 7;
        const auto rightIsShort = rightLength <= 7;
        startOfConcat[0] = static_cast<char>(makeConcatType(leftIsShort, rightIsShort));
        std::memcpy(startOfConcat + 1, &totalLength, 7);
        if (leftIsShort) {
            startOfConcat[offsets::concat::LEFT_PTR] = static_cast<char>(
                (leftLength << 4) | static_cast<char>(EntryType::SHORT_STRING));
            left.copy(startOfConcat + offsets::concat::LEFT_PTR + 1, 7);
        } else {
            std::memcpy(startOfConcat + offsets::concat::LEFT_PTR, &left.entry, 8);
        }
        if (rightIsShort) {
            startOfConcat[offsets::concat::RIGHT_PTR] = static_cast<char>(
                (rightLength << 4) | static_cast<char>(EntryType::SHORT_STRING));
            right.copy(startOfConcat + offsets::concat::RIGHT_PTR + 1, 7);
        } else {
            std::memcpy(startOfConcat + offsets::concat::RIGHT_PTR, &right.entry, 8);
        }
        table[index] = startOfConcat;
        return string_handle(*this, startOfConcat);
    }
}

bool pool::equals(const char* string, size_t size, const char* entry) {
    const auto entryType = static_cast<EntryType>(entry[0]);
    if (entryType == EntryType::ATOM) {
        return 0 == std::memcmp(string, entry + 8, size);
    } else {
        // todo: handle other entry types.
        return false;
    }
}
