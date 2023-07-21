// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <optional>

#include "../type.h"
#include "../Op.h"
#include "../Query.h"

struct InsertNth : public TreeQuery {
    // the index in the realised list that we want to insert at
    usize target = 0;
    // the number of visible operations seen
    usize seen = 0;
    // the number of operations (including non-visible) that we have seen
    usize n = 0;
    std::optional<usize> valid;
    // last_seen is the target elemid of the last `seen` operation.
    // It is used to avoid double counting visible elements (which arise through conflicts) that are split across nodes.
    std::optional<Key> last_seen;
    std::optional<ElemId> last_insert;
    std::optional<Key> last_valid_insert;

    InsertNth(usize t) : target(t), seen(0), n(0), valid(), last_seen(), last_insert(), last_valid_insert() {
        if (target == 0) {
            valid = 0;
            last_valid_insert = Key{ Key::Seq, HEAD };
        }
    }

    usize pos() {
        return valid ? *valid : n;
    }

    // throw AutomergeError::InvalidIndex
    Key key() const;

    QueryResult query_node(const OpTreeNode& child);

    QueryResult query_element(const Op& element);
};
