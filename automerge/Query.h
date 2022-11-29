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

#include "type.h"
#include "Op.h"

struct ReplaceArgs {
    OpId old_id;
    OpId new_id;
    bool old_visible = false;
    bool new_visible = false;
    Key new_key;
};

struct QueryResult {
    enum {
        NEXT,
        // Skip this many elements, only allowed from the root node.
        SKIP,
        DESCEND,
        FINISH
    } tag = NEXT;
    usize skip = 0;

    bool operator==(const QueryResult& other) const {
        return (tag == other.tag) && (tag != SKIP || skip == other.skip);
    }
};

struct OpSetMetadata;
struct OpTreeNode;

class TreeQuery {
public:
    virtual ~TreeQuery() = default;

    virtual QueryResult query_node_with_metadata(const OpTreeNode& child, const OpSetMetadata& _m) {
        return query_node(child);
    }

    virtual QueryResult query_node(const OpTreeNode& _child) {
        return QueryResult{ QueryResult::DESCEND, 0 };
    }

    virtual QueryResult query_element_with_metadata(const Op& element, const OpSetMetadata& _m) {
        return query_element(element);
    }

    virtual QueryResult query_element(const Op& _element) {
        throw("invalid element query");
    }
};

struct Index {
public:
    // The map of visible keys to the number of visible operations for that key.
    std::unordered_map<Key, usize> visible;
    // Set of opids found in this node and below.
    std::unordered_set<OpId> ops;

    usize visible_len() const {
        return visible.size();
    }

    bool has_visible(const Key& seen) const {
        return visible.count(seen);
    }

    void replace(const ReplaceArgs& args);

    void insert(const Op& op);

    void remove(const Op& op);

    void merge(const Index& other);

private:
    void visible_remove(const Key& key);
};

usize binary_search_by(const OpTreeNode& node, OpCmpFunc f);
