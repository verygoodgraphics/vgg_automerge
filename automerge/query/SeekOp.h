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

struct SeekOp : public TreeQuery {
    // the op we are looking for
    const Op& op;
    // The position to insert at
    usize pos;
    // The indices of ops that this op overwrites
    std::vector<usize> succ;
    // whether a position has been found
    bool found;

    SeekOp(const Op& _op) : op(_op), pos(0), succ(), found(false) {}

    bool lesser_insert(const Op& op, const OpSetMetadata& m);

    bool greater_opid(const Op& op, const OpSetMetadata& m);

    bool is_target_insert(const Op& op);

    QueryResult query_node_with_metadata(const OpTreeNode& child, const OpSetMetadata& m) override;

    QueryResult query_element_with_metadata(const Op& e, const OpSetMetadata& m) override;
};
