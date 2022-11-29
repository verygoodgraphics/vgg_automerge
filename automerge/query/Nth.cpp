// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "Nth.h"
#include "../OpTree.h"
#include "../Error.h"

Key Nth::key() {
    // the query collects the ops so we can use that to get the key they all use
    if (ops.empty()) {
        throw AutomergeError{ AutomergeError::InvalidIndex, (u64)target };
    }

    auto e = (*ops.begin())->elemid();
    if (e) {
        return Key{ Key::Seq, *e };
    }
    else {
        throw AutomergeError{ AutomergeError::InvalidIndex, (u64)target };
    }
}

QueryResult Nth::query_node(const OpTreeNode& child) {
    usize num_vis = child.index.visible_len();
    if (last_seen && child.index.has_visible(*last_seen)) {
        --num_vis;
    }

    if (seen + num_vis > target) {
        return QueryResult{ QueryResult::DESCEND, 0 };
    }

    // skip this node as no useful ops in it
    pos += child.len();
    seen += num_vis;

    // We have updated seen by the number of visible elements in this index, before we skip it.
    // We also need to keep track of the last elemid that we have seen (and counted as seen).
    // We can just use the elemid of the last op in this node as either:
    // - the insert was at a previous node and this is a long run of overwrites so last_seen should already be set correctly
    // - the visible op is in this node and the elemid references it so it can be set here
    // - the visible op is in a future node and so it will be counted as seen there
    auto last_elemid = child.last().elemid_or_key();
    if (child.index.has_visible(last_elemid)) {
        last_seen = last_elemid;
    }

    return QueryResult{ QueryResult::NEXT, 0 };
}

QueryResult Nth::query_element(const Op& element) {
    if (element.insert) {
        if (seen > target) {
            return QueryResult{ QueryResult::FINISH, 0 };
        }
        last_seen.reset();
    }
    bool visible = element.visible();
    if (visible && !last_seen.has_value()) {
        ++seen;
        // we have a new visible element
        last_seen = element.elemid_or_key();
    }
    if ((seen == target + 1) && visible) {
        ops.push_back(&element);
        ops_pos.push_back(pos);
    }
    ++pos;
    return QueryResult{ QueryResult::NEXT, 0 };
}