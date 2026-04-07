#include "stringpool/stringpool.h"

using namespace stringpool;

string_handle::char_iterator::char_iterator()
    : chunk(nullptr),
      chunkSize(0),
      indexInChunk(0) {
}

string_handle::char_iterator::char_iterator(const string_handle& sh)
    : walker(sh.data),
      indexInChunk(0) {
    chunkSize = walker.get_next_bytes(&chunk);
    if (chunkSize == 0) {
        // Mark this iterator as ended.
        chunk = nullptr;
    }
}

string_handle::char_iterator::value_type string_handle::char_iterator::operator*() const {
    return chunk[indexInChunk];
}

string_handle::char_iterator& string_handle::char_iterator::operator++() {
    ++indexInChunk;
    if (indexInChunk < chunkSize)
        return *this;
    chunkSize = walker.get_next_bytes(&chunk);
    if (chunkSize != 0) {
        indexInChunk = 0;
    } else [[unlikely]] {
        // Mark this iterator as ended.
        chunk = nullptr;
    }
    return *this;
}

bool string_handle::char_iterator::operator==(const char_iterator& other) const {
    if (chunk == nullptr) [[unlikely]]
        return true;
    return indexInChunk == other.indexInChunk && chunk == other.chunk && walker == other.walker;
}

bool string_handle::char_iterator::operator!=(const char_iterator& other) const {
    return !(*this == other);
}
