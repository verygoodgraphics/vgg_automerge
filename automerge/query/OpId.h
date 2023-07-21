// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <optional>
#include <algorithm>
#include <stdexcept>

#include "../type.h"
#include "../Op.h"
#include "../Query.h"

// Search for an OpId in a tree.
// Returns the index of the operation in the tree.
struct OpIdSearch : public TreeQuery {
    OpId target;
    usize pos;
    bool found;
    std::optional<Key> key;

    OpIdSearch(const OpId& t) : target(t), pos(0), found(false), key(std::nullopt) {}

    // Get the index of the operation, if found.
    auto index() {
        return found ? std::optional<usize>(pos) : std::nullopt;
    }

    QueryResult query_node(const OpTreeNode& child) override;

    QueryResult query_element(const Op& element) override;
};
