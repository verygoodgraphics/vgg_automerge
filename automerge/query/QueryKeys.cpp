// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "QueryKeys.h"
#include "../OpTree.h"

QueryKeys::QueryKeys(const OpTreeNode* r) :
    index(0), last_key(std::nullopt), index_back(r->len()),
    last_key_back(std::nullopt), root_child(r) {}

std::optional<Key> QueryKeys::next() {
    for (usize i = index; i < index_back; ++i) {
        auto op = root_child->get(i);
        if (!op) {
            return {};
        }

        ++index;
        auto op_key = std::optional<Key>((*op)->elemid_or_key());
        if (!(op_key == last_key) && (*op)->visible()) {
            last_key = op_key;
            return op_key;
        }
    }

    return {};
}

std::optional<Key> QueryKeys::next_back() {
    for (usize i = 0; i < index_back - index; ++i) {
        usize i_back = index_back - i - 1;
        auto op = root_child->get(i_back);
        if (!op) {
            return {};
        }

        --index_back;
        auto op_key = std::optional<Key>((*op)->elemid_or_key());
        if (!(op_key == last_key_back) && (*op)->visible()) {
            last_key_back = op_key;
            return op_key;
        }
    }

    return {};
}