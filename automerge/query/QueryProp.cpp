// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "QueryProp.h"
#include "../OpSet.h"

QueryResult QueryProp::query_node_with_metadata(const OpTreeNode& child, const OpSetMetadata& m) {
    auto start = binary_search_by(child, [&](const Op* op) {
        return m.key_cmp(op->key, this->key);
    });
    pos = start;
    return QueryResult{ QueryResult::SKIP, start };
}

QueryResult QueryProp::query_element(const Op& op) {
    // don't bother looking at things past our key
    if (!(op.key == key)) {
        return QueryResult{ QueryResult::FINISH, 0 };
    }
    if (op.visible()) {
        ops.push_back(&op);
        ops_pos.push_back(pos);
    }
    ++pos;
    return QueryResult{ QueryResult::NEXT, 0 };
}