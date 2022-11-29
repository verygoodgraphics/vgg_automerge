// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "OpId.h"
#include "../OpTree.h"

QueryResult OpIdSearch::query_node(const OpTreeNode& child) {
    if (child.index.ops.count(target)) {
        return QueryResult{ QueryResult::DESCEND };
    }
    else {
        pos += child.len();
        return QueryResult{ QueryResult::NEXT };
    }
}

QueryResult OpIdSearch::query_element(const Op& element) {
    if (element.id == target) {
        found = true;
        if (element.insert) {
            key = std::optional<Key>(Key{ Key::Seq, element.id });
        }
        else {
            key = std::optional<Key>(element.key);
        }
        return QueryResult{ QueryResult::FINISH };
    }

    ++pos;
    return QueryResult{ QueryResult::NEXT };
}