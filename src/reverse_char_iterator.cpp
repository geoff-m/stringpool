#include "stringpool/stringpool.h"

using namespace stringpool;

string_handle::char_iterator_backward::char_iterator_backward()
    : chunk(nullptr),
      chunkSize(0),
      indexInChunk(0) {
}

string_handle::char_iterator_backward::char_iterator_backward(const string_handle& sh)
    : walker(sh.data){
    chunkSize = walker.get_next_bytes(&chunk);
    if (chunkSize == 0) {
        // Mark this iterator as ended.
        chunk = nullptr;
    }
    indexInChunk = chunkSize - 1;
}

string_handle::char_iterator_backward::value_type string_handle::char_iterator_backward::operator*() const {
    return chunk[indexInChunk];
}

string_handle::char_iterator_backward& string_handle::char_iterator_backward::operator++() {
    --indexInChunk;
    if (indexInChunk < chunkSize)
        return *this;
    chunkSize = walker.get_next_bytes(&chunk);
    if (chunkSize != 0) {
        indexInChunk = chunkSize - 1;
    } else [[unlikely]] {
        // Mark this iterator as ended.
        chunk = nullptr;
    }
    return *this;
}

string_handle::char_iterator_backward string_handle::char_iterator_backward::operator++(int)
{
    auto old = *this;
    ++*this;
    return old;
}

bool string_handle::char_iterator_backward::operator==(const char_iterator_backward& other) const {
    if (chunk == nullptr) [[unlikely]]
        return true;
    return indexInChunk == other.indexInChunk && chunk == other.chunk && walker == other.walker;
}

bool string_handle::char_iterator_backward::operator!=(const char_iterator_backward& other) const {
    return !(*this == other);
}

