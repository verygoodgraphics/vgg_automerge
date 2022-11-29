// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "QueryProp.h"
#include "../OpSet.h"

QueryResult QueryProp::query_node_with_metadata(const OpTreeNode& child, const OpSetMetadata& m) {
    if (start) {
        if (pos + child.len() >= start->idx) {
            // skip empty nodes
            if (child.index.visible_len() == 0) {
                if (pos + child.len() >= start->optree_len) {
                    pos = start->optree_len;
                    return QueryResult{ QueryResult::FINISH, 0 };
                }
                else {
                    pos += child.len();
                    return QueryResult{ QueryResult::NEXT, 0 };
                }
            }
            else {
                return QueryResult{ QueryResult::DESCEND, 0 };
            }
        }
        else {
            pos += child.len();
            return QueryResult{ QueryResult::NEXT, 0 };
        }
    }
    else {
        // in the root node find the first op position for the key
        usize new_start = binary_search_by(child, [&](const Op* op) {
            return m.key_cmp(op->key, this->key);
            });
        start = Start{ new_start, child.len() };
        pos = new_start;
        return QueryResult{ QueryResult::SKIP, new_start };
    }
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