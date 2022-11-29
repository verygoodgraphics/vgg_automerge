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

struct Nth : public TreeQuery {
    usize target;
    usize seen;
    // last_seen is the target elemid of the last `seen` operation.
    // It is used to avoid double counting visible elements (which arise through conflicts) that are split across nodes.
    std::optional<Key> last_seen;
    std::vector<const Op*> ops;
    std::vector<usize> ops_pos;
    usize pos;

    Nth(usize t) : target(t), seen(0), last_seen(), ops(), ops_pos(), pos(0) {}

    // throw AutomergeError::InvalidIndex
    Key key();

    QueryResult query_node(const OpTreeNode& child);

    QueryResult query_element(const Op& element);
};