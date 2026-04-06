#include "stringpool/stringpool.h"
#include "include/hash.h"
#include "include/pack_utils.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

using namespace stringpool;

string_handle::string_handle(pool* owner, size_t dataIndex)
    : owner(owner), dataIndex(dataIndex) {
}

string_handle::tree_walker::tree_walker(const pool& owner, size_t rootIndex) : tree_walker(owner.data, rootIndex) {
}

string_handle::tree_walker::tree_walker(char* baseAddress, size_t rootIndex)
    : baseAddress(baseAddress),
      rootIndex(rootIndex) {
    toVisit.emplace_back(rootIndex);
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
    if (owner == rhs.owner && dataIndex == rhs.dataIndex)
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
    if (owner == rhs.owner && dataIndex == rhs.dataIndex)
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
    auto ret = unpackLength(owner->data + dataIndex);
    return ret;
}

size_t string_handle::length() const {
    return size();
}
