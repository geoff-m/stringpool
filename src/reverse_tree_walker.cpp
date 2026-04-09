#include "include/pack_utils.h"
#include "stringpool/stringpool.h"

using namespace stringpool;

string_handle::reverse_tree_walker::reverse_tree_walker()
    : root(nullptr) {
}

string_handle::reverse_tree_walker::reverse_tree_walker(const char* root)
    : root(root) {
    toVisit.emplace_back(root);
}

size_t string_handle::reverse_tree_walker::get_next_bytes(const char** bytes) {
    if (toVisit.empty())
        return 0;
    auto* current = toVisit.back();
    toVisit.pop_back();
    while (is_concat(current)) {
        const auto rightChild = get_right_child(current);
        const auto leftChild = get_left_child(current);
        toVisit.emplace_back(leftChild);
        current = rightChild;
    }
    *bytes = get_string_from_leaf(current);
    return get_length(current);
}