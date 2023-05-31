// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "ChangeGraph.h"

std::optional<ChangeHash> ChangeGraph::add_change(const Change& change, usize actor_idx) {
    auto& hash = change.hash;
    if (nodes_by_hash.count(hash)) {
        return {};
    }

    std::vector<u32> parent_indics;
    parent_indics.reserve(change.deps.size());
    for (auto& h : change.deps) {
        try {
            parent_indics.push_back(nodes_by_hash.at(h));
        }
        catch (std::out_of_range&) {
            return h;
        }
    }

    auto node_idx = add_node(actor_idx, change);
    nodes_by_hash.insert({ hash, node_idx });
    for (auto parent_idx : parent_indics) {
        add_parent(node_idx, parent_idx);
    }

    return {};
}

u32 ChangeGraph::add_node(usize actor_index, const Change& change) {
    u32 idx = (u32)nodes.size();
    auto hash_idx = add_hash(ChangeHash(change.hash));
    nodes.push_back(ChangeNode{
        hash_idx,
        actor_index,
        change.seq,
        change.max_op(),
        {}
        });

    return idx;
}

u32 ChangeGraph::add_hash(ChangeHash&& hash) {
    u32 idx = (u32)hashes.size();
    hashes.push_back(hash);

    return idx;
}

void ChangeGraph::add_parent(u32 child_idx, u32 parent_idx) {
    u32 new_edge_idx = (u32)edges.size();
    edges.push_back({ parent_idx, {} });

    auto& child = nodes[child_idx];
    auto& edge_idx = child.parents;
    if (edge_idx) {
        auto edge = &edges[*edge_idx];
        while (edge->next) {
            edge = &edges[*edge->next];
        }
        edge->next = new_edge_idx;
    }
    else {
        child.parents = new_edge_idx;
    }
}

Clock ChangeGraph::clock_for_heads(const std::vector<ChangeHash>& heads) const {
    Clock clock;

    traverse_ancestors(heads, [&clock](const ChangeNode& node, const ChangeHash& _hash) {
        clock.include(
            node.actor_index,
            ClockData{
                node.max_op,
                node.seq
            }
        );
    });

    return clock;
}

void ChangeGraph::remove_ancestors(std::set<ChangeHash>& changes, const std::vector<ChangeHash>& heads) const {
    traverse_ancestors(heads, [&changes](const ChangeNode& _node, const ChangeHash& hash) {
        changes.erase(hash);
    });
}

void ChangeGraph::traverse_ancestors(
    const std::vector<ChangeHash>& heads, 
    std::function<void(const ChangeNode&, const ChangeHash&)> f
) const {
    std::vector<u32> to_visit;
    to_visit.reserve(heads.size());
    for (auto& h : heads) {
        auto node_iter = nodes_by_hash.find(h);
        if  (node_iter != nodes_by_hash.end()) {
            to_visit.push_back(node_iter->second);
        }
    }

    std::set<u32> visited;
    while (!to_visit.empty()) {
        auto idx = to_visit.back();
        to_visit.pop_back();

        if (visited.count(idx)) {
            continue;
        }
        else {
            visited.insert(idx);
        }

        auto& node = nodes[idx];
        auto& hash = hashes[node.hash_idx];
        f(node, hash);

        // fn parents
        auto edge_idx = nodes[idx].parents;
        while (edge_idx.has_value()) {
            auto& edge = edges[*edge_idx];
            edge_idx = edge.next;
            to_visit.push_back(edge.target);
        }
    }
}