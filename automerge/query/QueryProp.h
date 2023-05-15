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

struct QueryProp : public TreeQuery {
    Key key;
    std::vector<const Op*> ops;
    std::vector<usize> ops_pos;
    usize pos;

    QueryProp(usize prop) : key{ Key::Map, prop }, ops(), ops_pos(), pos(0) {}

    QueryResult query_node_with_metadata(const OpTreeNode& child, const OpSetMetadata& m);

    QueryResult query_element(const Op& op);
};