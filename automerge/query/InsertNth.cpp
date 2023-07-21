// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "InsertNth.h"
#include "../OpTree.h"
#include "../Error.h"

Key InsertNth::key() const {
    if (last_valid_insert) {
        return *last_valid_insert;
    }

    throw AutomergeError{ AutomergeError::InvalidIndex, (u64)target };
}

QueryResult InsertNth::query_node(const OpTreeNode& child) {
    // if this node has some visible elements then we may find our target within
    usize num_vis = child.index.visible_len();
    if (last_seen.has_value() && child.index.has_visible(*last_seen)) {
        --num_vis;
    }

    if (seen + num_vis >= target) {
        // our target is within this node
        return QueryResult{ QueryResult::DESCEND, 0 };
    }

    // our target is not in this node so try the next one
    n += child.len();
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
    else if (last_seen.has_value() && !(last_elemid == *last_seen)) {
        last_seen.reset();
    }

    return QueryResult{ QueryResult::NEXT, 0 };
}

QueryResult InsertNth::query_element(const Op& element) {
    if (element.insert) {
        if (!valid.has_value() && (seen >= target)) {
            valid = n;
        }
        last_seen.reset();
        last_insert = element.elemid();
    }

    if (!last_seen.has_value() && element.visible()) {
        if (seen >= target) {
            return QueryResult{ QueryResult::FINISH, 0 };
        }
        ++seen;
        last_seen = element.elemid_or_key();
        last_valid_insert = last_seen;
    }
    ++n;
    return QueryResult{ QueryResult::NEXT, 0 };
}
