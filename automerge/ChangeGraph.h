// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <utility>
#include <algorithm>
#include <optional>
#include <variant>
#include <functional>
#include <stdexcept>

#include "type.h"
#include "Clock.h"
#include "Change.h"

// #[derive(Debug, Clone)]
struct Edge {
    // Edges are always child -> parent so we only store the target, the child is implicit
    // as you get the edge from the child
    u32 target;
    std::optional<u32> next;
};

// #[derive(Debug, Clone)]
struct ChangeNode {
    u32 hash_idx;
    usize actor_index;
    u64 seq;
    u64  max_op;
    std::optional<u32> parents;
};

// The graph of changes
//
// This is a sort of adjacency list based representation, except that instead of using linked
// lists, we keep all the edges and nodes in two vecs and reference them by index which plays nice
// with the cache
// #[derive(Debug, Clone)]
struct ChangeGraph {
    std::vector<ChangeNode> nodes;
    std::vector<Edge> edges;
    std::vector<ChangeHash> hashes;
    std::map<ChangeHash, u32> nodes_by_hash;
    
    // success: return null; fail: return missing dep
    std::optional<ChangeHash> add_change(const Change& change, usize actor_idx);

    Clock clock_for_heads(const std::vector<ChangeHash>& heads) const;

    void remove_ancestors(std::set<ChangeHash>& changes, const std::vector<ChangeHash>& heads) const;

private:
    u32 add_node(usize actor_index, const Change& change);

    u32 add_hash(ChangeHash&& hash);

    void add_parent(u32 child_idx, u32 parent_idx);

    // Call `f` for each (node, hash) in the graph, starting from the given heads
    //
    // No guarantees are made about the order of traversal but each node will only be visited
    // once.
    void traverse_ancestors(const std::vector<ChangeHash>& heads, std::function<void(const ChangeNode&, const ChangeHash&)> f) const;
};
