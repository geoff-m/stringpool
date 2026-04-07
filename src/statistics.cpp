#include "stringpool/stringpool.h"

using namespace stringpool;

size_t pool::get_data_size() const {
    return totalDataSize;
}

size_t pool::get_total_intern_request_size() const {
    return totalInternRequestSize;
}

size_t pool::get_total_intern_request_count() const {
    return totalInternRequestCount;
}

size_t pool::get_total_intern_request_hits() const {
    return internHits;
}

size_t pool::get_total_intern_request_misses() const {
    return internMisses;
}
