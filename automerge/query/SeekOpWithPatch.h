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

struct SeekOpWithPatch : public TreeQuery {
    Op op;
    usize pos;
    std::vector<usize> succ;
    bool found;
    usize seen;
    std::optional<Key> last_seen;
    std::vector<const Op*> values;
    bool had_value_before;

    SeekOpWithPatch(const Op& _op) : op(_op), pos(0), succ(), found(false), seen(0),
        last_seen(), values(), had_value_before(false) {}

    bool lesser_insert(const Op& op, const OpSetMetadata& m);

    bool greater_opid(const Op& op, const OpSetMetadata& m);

    bool is_target_insert(const Op& op);

    /// Keeps track of the number of visible list elements we have seen. Increments `self.seen` if
    /// operation `e` associates a visible value with a list element, and if we have not already
    /// counted that list element (this ensures that if a list element has several values, i.e.
    /// a conflict, then it is still only counted once).
    void cout_visible(const Op& e);

    QueryResult query_node_with_metadata(const OpTreeNode& child, const OpSetMetadata& m) override;

    QueryResult query_element_with_metadata(const Op& e, const OpSetMetadata& m) override;
};
