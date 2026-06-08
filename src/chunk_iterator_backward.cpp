#include "stringpool/stringpool.h"

using namespace stringpool;

string_handle::chunk_iterator_backward::chunk_iterator_backward()
    : chunkData(nullptr),
      chunkSize(0) {
}

string_handle::chunk_iterator_backward::chunk_iterator_backward(const string_handle& sh)
    : walker(sh.data) {
    chunkSize = walker.get_next_bytes(&chunkData);
    if (chunkSize == 0) {
        // Mark this iterator as ended.
        chunkData = nullptr;
    }
}

string_handle::chunk_iterator_backward::value_type string_handle::chunk_iterator_backward::operator*() const {
    return std::string_view(chunkData, chunkSize);
}

string_handle::chunk_iterator_backward& string_handle::chunk_iterator_backward::operator++() {
    chunkSize = walker.get_next_bytes(&chunkData);
    if (chunkSize == 0) [[unlikely]] {
        // Mark this iterator as ended.
        walker = reverse_tree_walker();
        chunkData = nullptr;
    }
    return *this;
}

string_handle::chunk_iterator_backward string_handle::chunk_iterator_backward::operator++(int) {
    auto old = *this;
    ++*this;
    return old;
}

bool string_handle::chunk_iterator_backward::operator==(const chunk_iterator_backward& other) const {
    return walker == other.walker;
}

bool string_handle::chunk_iterator_backward::operator!=(const chunk_iterator_backward& other) const {
    return walker != other.walker;
}
