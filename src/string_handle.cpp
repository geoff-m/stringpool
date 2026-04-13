#include "stringpool/stringpool.h"
#include "include/hash.h"
#include "include/pack_utils.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

using namespace stringpool;

#ifdef STRINGPOOL_TRACK_OWNERS
string_handle::string_handle(const char* data, pool* owner)
    : data(data), owner(owner) {
}
#else
string_handle::string_handle(const char* data)
    : data(data) {
}
#endif

size_t string_handle::copy(char* destination, size_t destination_size) const {
    if (destination_size == 0)
        return 0;
    tree_walker walker(data);
    size_t copiedSoFar = 0;
    const char* piece;
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
    tree_walker walker(data);
    const char* piece;
    while (size_t pieceLength = walker.get_next_bytes(&piece)) {
        h.add(piece, pieceLength);
    }
    return h.finish();
}

void string_handle::visit_chunks(void (*callback)(const char* piece, size_t pieceSize, void* state),
                                 void* state) const {
    tree_walker walker(data);
    const char* piece;
    while (size_t pieceLength = walker.get_next_bytes(&piece)) {
        callback(piece, pieceLength, state);
    }
}

void string_handle::visit_chunks(void (*callback)(std::string_view chunk, void* state), void* state) const {
    tree_walker walker(data);
    const char* piece;
    while (size_t pieceLength = walker.get_next_bytes(&piece)) {
        callback({piece, pieceLength}, state);
    }
}

// Assumes rhs is null-terminated.
int string_handle::strcmp(const char* rhs) const {
    tree_walker walker(data);
    const char* piece;
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
    if (data == rhs.data)
        return 0;
    tree_walker leftWalker(data);
    tree_walker rightWalker(rhs.data);
    const char* leftPiece;
    const char* rightPiece;
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
    if (data == rhs.data)
        return 0;
    tree_walker leftWalker(data);
    tree_walker rightWalker(rhs.data);
    const char* leftPiece;
    const char* rightPiece;
    size_t leftPieceLength = leftWalker.get_next_bytes(&leftPiece);
    size_t rightPieceLength = rightWalker.get_next_bytes(&rightPiece);
    assert(get_length(data) >= length && get_length(rhs.data) >= length);
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

[[nodiscard]] int string_handle::memcmp(const char* rhs, size_t length) const {
    tree_walker walker(data);
    const char* piece;
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
            msg += std::to_string(get_length(data));
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

bool string_handle::equals(const char* rhs, size_t length) const {
    const auto thisLength = get_length(data);
    if (thisLength != length)
        return false;
    return 0 == memcmp(rhs, length);
}

bool string_handle::equals(std::string_view rhs) const {
    return equals(rhs.data(), rhs.size());
}

bool string_handle::equals(const char* rhs) const {
    return equals(rhs, strlen(rhs));
}

bool string_handle::equals(const string_handle& rhs) const {
    if (this == &rhs)
        return true;
    tree_walker leftWalker(data);
    tree_walker rightWalker(rhs.data);
    const char* leftPiece;
    const char* rightPiece;
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

std::string string_handle::to_string() const {
    std::string ret;
    ret.resize(size());
    copy(ret.data(), size());
    return ret;
}

bool string_handle::concat_equals(string_handle single, string_handle left, string_handle right) {
    const auto comparandLength = get_length(single.data);
    const auto leftLength = get_length(left.data);
    const auto rightLength = get_length(right.data);
    if (comparandLength != leftLength + rightLength)
        return false;
    tree_walker singleWalker(single.data);
    tree_walker comparandWalker(left.data);
    bool onLeft = true;
    const char* singlePiece;
    const char* comparandPiece;
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
                comparandWalker = tree_walker(right.data);
                comparandPieceLength = comparandWalker.get_next_bytes(&comparandPiece);
            }
            comparandPieceIndex = 0;
        }
    }
}

string_handle::char_iterator_forward string_handle::begin() const {
    return char_iterator_forward(*this);
}

string_handle::char_iterator_forward string_handle::end() const {
    return {};
}

string_handle::char_iterator_backward string_handle::rbegin() const {
    return char_iterator_backward(*this);
}

string_handle::char_iterator_backward string_handle::rend() const {
    return {};
}

size_t string_handle::size() const {
    auto ret = get_length(data);
    return ret;
}

size_t string_handle::length() const {
    return size();
}
