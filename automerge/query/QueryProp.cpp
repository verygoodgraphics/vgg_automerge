// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "QueryProp.h"
#include "../OpSet.h"

QueryResult QueryProp::query_node_with_metadata(const OpTreeNode& child, const OpSetMetadata& m) {
    auto cmp = m.key_cmp(child.last().key, this->key);
    if (cmp < 0 ||
        (cmp == 0 && !child.index.has_visible(this->key))) {
        pos += child.len();
        return QueryResult{ QueryResult::NEXT, 0 };
    }

    return QueryResult{ QueryResult::DESCEND, 0 };
}

QueryResult QueryProp::query_element_with_metadata(const Op& element, const OpSetMetadata& m) {
    auto cmp = m.key_cmp(element.key, this->key);
    
    if (cmp > 0) {
        return QueryResult{ QueryResult::FINISH, 0 };
    }
    else if (cmp == 0) {
        if (element.visible()) {
            ops.push_back(&element);
            ops_pos.push_back(pos);
        }
    }

    ++pos;
    return QueryResult{ QueryResult::NEXT, 0 };
}
