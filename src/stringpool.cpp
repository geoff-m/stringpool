#include "stringpool/stringpool.h"
#include <atomic>
#include <cassert>
#include <cstring>
#include "xxhash.h"
#include <cstdlib>

using namespace stringpool;

enum class EntryType : uint8_t {
    LEFT_SHORT_STRING = 0, // embedded in concat
    RIGHT_SHORT_STRING = 1, // embedded in concat
    ATOM = 2,
    CONCAT_LEFT_ENTRY_RIGHT_SHORT = 3, // todo: i think we can consolidate these down to just 1 concat node type.
    CONCAT_LEFT_SHORT_RIGHT_ENTRY = 4,
    CONCAT_LEFT_SHORT_RIGHT_SHORT = 5,
    CONCAT_LEFT_ENTRY_RIGHT_ENTRY = 6,
};

constexpr uint64_t LOWER_7_MASK = 0x00ffffffffffffff;

namespace offsets {
    constexpr int ENTRY_TYPE_STRING_LENGTH = 0;
    constexpr int PARENT_PTR = 8;

    namespace atom {
        constexpr int STRING_VALUE = 16;
    }

    namespace concat {
        constexpr int LEFT_PTR = 16;
        constexpr int RIGHT_PTR = 24;
    }
}

string_handle::string_handle(pool& owner, char* data)
    : owner(owner), data(data) {
}

[[nodiscard]] size_t min(const size_t x, const size_t y) {
    return x <= y ? x : y;
}

string_handle::tree_walker::tree_walker(char* root)
    : root(root),
      lastLeaf(nullptr) {
}

[[nodiscard]] EntryType unpack_node_type(const char* node) {
    return static_cast<EntryType>(node[offsets::ENTRY_TYPE_STRING_LENGTH] & 0b111);
}

[[nodiscard]] bool isInternalNode(const char* node) {
    return unpack_node_type(node) >= EntryType::CONCAT_LEFT_ENTRY_RIGHT_SHORT;
}

[[nodiscard]] bool isShortString(const char* node) {
    return unpack_node_type(node) < EntryType::ATOM;
}

[[nodiscard]] size_t unpackLength(const char* node) {
    if (isShortString(node))
        return node[offsets::ENTRY_TYPE_STRING_LENGTH] >> 3;
    return reinterpret_cast<const uint64_t*>(node)[offsets::ENTRY_TYPE_STRING_LENGTH] & LOWER_7_MASK;
}

[[nodiscard]] char* unpackStringFromLeaf(char* node) {
    if (isShortString(node))
        return node + 1;
    assert(unpack_node_type(node) == EntryType::ATOM);
    return reinterpret_cast<char*>(node[offsets::atom::STRING_VALUE]);
}

[[nodiscard]] char* unpackLeftChild(const char* concatNode) {
    return reinterpret_cast<char*>(concatNode[offsets::concat::LEFT_PTR]);
}

[[nodiscard]] char* unpackRightChild(const char* concatNode) {
    return reinterpret_cast<char*>(concatNode[offsets::concat::RIGHT_PTR]);
}

[[nodiscard]] char* unpackParent(char* node, bool& isLeftChild) {
    const auto nodeType = unpack_node_type(node);
    switch (nodeType) {
        case EntryType::LEFT_SHORT_STRING:
            isLeftChild = true;
            return node - offsets::concat::LEFT_PTR;
        case EntryType::RIGHT_SHORT_STRING:
            isLeftChild = false;
            return node - offsets::concat::RIGHT_PTR;
        default:
            auto* parent = reinterpret_cast<char*>(node[offsets::PARENT_PTR]);
            if (parent != nullptr) {
                isLeftChild = unpackLeftChild(parent) == node;
                assert(isLeftChild != (unpackRightChild(parent) == node));
            }
            return parent;
    }
}

char* find_next_unvisited_leaf(char* lastVisitedLeaf) {
    bool wasLeftChild;
    auto* parent = unpackParent(lastVisitedLeaf, wasLeftChild);
    if (parent == nullptr)
        return nullptr;
    if (wasLeftChild) {
        // Last visited leaf was left child.
        // Visit leftmost right child of parent.
        auto* next = unpackRightChild(parent);
        while (isInternalNode(next)) {
            next = unpackLeftChild(next);
        }
        return next;
    } else {
        // Last visited leaf was right child.
        // Visit leftmost child to the right of grandparent.
        auto* next = unpackParent(parent, wasLeftChild);
        while (next && !wasLeftChild) {
            next = unpackParent(parent, wasLeftChild);
        }
        return next;
    }
}

size_t string_handle::tree_walker::get_next_bytes(char** bytes) {
    // Find next unvisited leaf starting from 'last'.
    if (lastLeaf == nullptr) {
        if (isInternalNode(root)) {
            auto* leftmost = unpackLeftChild(root);
            while (isInternalNode(leftmost))
                leftmost = unpackLeftChild(root);
            lastLeaf = leftmost;
            *bytes = lastLeaf + offsets::atom::STRING_VALUE;
        } else {
            // Root is an atom.
            lastLeaf = root;
            *bytes = lastLeaf + offsets::atom::STRING_VALUE;
            return lastLeaf[offsets::ENTRY_TYPE_STRING_LENGTH] & LOWER_7_MASK;
        }
    }
    auto* next = find_next_unvisited_leaf(lastLeaf);
    if (next == nullptr) {
        return 0;
    }
    lastLeaf = next;
    *bytes = unpackStringFromLeaf(lastLeaf);
    return lastLeaf[offsets::ENTRY_TYPE_STRING_LENGTH] & LOWER_7_MASK;
}

size_t string_handle::copy(char* destination, size_t destination_size) const {
    tree_walker walker(data);
    size_t totalLength = 0;
    char* piece;
    while (size_t pieceLength = walker.get_next_bytes(&piece)) {
        std::memcpy(destination, piece, destination_size - totalLength);
        totalLength += pieceLength;
    }
    return totalLength;
}

// Assumes rhs is null-terminated.
int string_handle::strcmp(const char* rhs) const {
    tree_walker walker(data);
    char* piece;
    while (size_t pieceLength = walker.get_next_bytes(&piece)) {
        const auto thisResult = std::strncmp(piece, rhs, pieceLength);
        if (thisResult != 0)
            return thisResult;
    }
    return 0;
}

int string_handle::memcmp(const char* rhs, size_t rhsLength) const {
    tree_walker walker(data);
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

bool string_handle::equals(const string_handle& rhs) const {
    if (this == &rhs)
        return true;
    tree_walker leftWalker(data);
    tree_walker rightWalker(rhs.data);
    char* leftPiece;
    char* rightPiece;
    size_t i = 0;
    size_t leftPieceLength = leftWalker.get_next_bytes(&leftPiece);
    size_t rightPieceLength = rightWalker.get_next_bytes(&rightPiece);
    if (leftPieceLength != rightPieceLength)
        return false;
    size_t leftPieceIndex = 0;
    size_t rightPieceIndex = 0;
    while (true) {
        if (leftPieceLength == 0 || rightPieceLength == 0)
            return false;
        const auto thisLength = min(leftPieceLength, rightPieceLength);
        const auto thisResult = std::memcmp(
            leftPiece + leftPieceIndex,
            rightPiece + rightPieceIndex,
            thisLength);
        if (thisResult != 0)
            return thisResult;
        i += thisLength;
        leftPieceIndex += thisLength;
        rightPieceIndex += thisLength;
        if (leftPieceLength == leftPieceLength - 1)
            leftPieceLength = leftWalker.get_next_bytes(&leftPiece);
        if (rightPieceIndex == rightPieceLength - 1)
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

[[nodiscard]] size_t hash(const char* string, size_t length) {
    return XXH64(string, length, 0x7448652047614D65);
}

[[nodiscard]] size_t hash(const char* string) {
    return hash(string, strlen(string));
}

string_handle pool::intern(const char* string) {
    return intern(string, strlen(string));
}

string_handle pool::intern(const char* string, size_t size) {
    const auto h = hash(string, size);
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
    char* atom = addAtom(string, size, nullptr);
    table[index] = atom;
    return string_handle(*this, atom);
}


char* pool::addAtom(const char* string, size_t size, const char* parent) {
    updateTableSize(tableSize + 1);
    const auto blockSize = 16 + size;
    updateDataSize(dataSize + blockSize);
    assert(size == (size & LOWER_7_MASK)); // todo: replace this assert with some nicer failure mechanism
    char* startOfAtom = data + dataSize - blockSize;
    startOfAtom[0] = static_cast<char>(EntryType::ATOM);
    std::memcpy(startOfAtom + 1, &size, 7);
    std::memcpy(startOfAtom + 8, &parent, sizeof(parent));
    std::memcpy(startOfAtom + 16, string, size);
    return startOfAtom;
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
