#include "stringpool/stringpool.h"

using namespace stringpool;

string_handle::tree_walker::tree_walker()
    : root(nullptr) {
}

string_handle::tree_walker::tree_walker(const internal::node* root)
    : root(root) {
    toVisit.emplace_back(root);
}

size_t string_handle::tree_walker::get_next_bytes(const char** bytes) {
    if (toVisit.empty())
        return 0;
    auto* current = toVisit.back();
    toVisit.pop_back();
    while (current->type == internal::NodeType::CONCAT) {
        const auto* currentConcat = reinterpret_cast<const internal::concat_node*>(current);
        const auto rightChild = currentConcat->right;
        const auto leftChild =currentConcat->left;
        toVisit.emplace_back(rightChild);
        current = leftChild;
    }
    *bytes = get_string_from_leaf(current);
    return get_length(current);
}