// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "SeekOpWithPatch.h"
#include "../OpSet.h"

bool SeekOpWithPatch::lesser_insert(const Op& op, const OpSetMetadata& m) {
    return op.insert && (m.lamport_cmp(op.id, this->op.id) < 0);
}

bool SeekOpWithPatch::greater_opid(const Op& op, const OpSetMetadata& m) {
    return (m.lamport_cmp(op.id, this->op.id) > 0);
}

bool SeekOpWithPatch::is_target_insert(const Op& op) {
    return op.insert && (op.elemid() == this->op.key.elemid());
}

void SeekOpWithPatch::cout_visible(const Op& e) {
    if (e.elemid() == op.elemid()) {
        return;
    }
    if (e.insert) {
        last_seen.reset();
    }
    if (e.visible() && !last_seen.has_value()) {
        ++seen;
        last_seen = e.elemid_or_key();
    }
}

QueryResult SeekOpWithPatch::query_node_with_metadata(const OpTreeNode& child, const OpSetMetadata& m) {
    if (found) {
        return QueryResult{ QueryResult::DESCEND, 0 };
    }

    // Updating a map: operations appear in sorted order by key
    if (op.key.tag == Key::Map) {
        auto start = binary_search_by(child, [&](const Op* op) {
            return m.key_cmp(op->key, this->op.key);
        });
        pos = start;
        return QueryResult{ QueryResult::SKIP, start };
    }
    // Special case for insertion at the head of the list (`e == HEAD` is only possible for
    // an insertion operation). Skip over any list elements whose elemId is greater than
    // the opId of the operation being inserted.
    else if (std::get<ElemId>(op.key.data) == HEAD) {
        while (pos < child.len()) {
            auto op = *(child.get(pos));
            if (op->insert && (m.lamport_cmp(op->id, this->op.id) < 0)) {
                break;
            }
            cout_visible(*op);
            ++pos;
        }
        return QueryResult{ QueryResult::FINISH, 0 };
    }
    // Updating a list: search for the tree node that contains the new operation's
    // reference element (i.e. the element we're updating or inserting after)
    else {
        if (found || child.index.ops.count(std::get<ElemId>(op.key.data))) {
            return QueryResult{ QueryResult::DESCEND, 0 };
        }
        else {
            pos += child.len();

            // When we skip over a subtree, we need to count the number of visible list
            // elements we're skipping over. Each node stores the number of visible
            // elements it contains. However, it could happen that a visible element is
            // split across two tree nodes. To avoid double-counting in this situation, we
            // subtract one if the last visible element also appears in this tree node.
            usize num_vis = child.index.visible_len();
            if (last_seen.has_value() && child.index.has_visible(*last_seen)) {
                --num_vis;
            }
            seen += num_vis;

            // We have updated seen by the number of visible elements in this index, before we skip it.
            // We also need to keep track of the last elemid that we have seen (and counted as seen).
            // We can just use the elemid of the last op in this node as either:
            // - the insert was at a previous node and this is a long run of overwrites so last_seen should already be set correctly
            // - the visible op is in this node and the elemid references it so it can be set here
            // - the visible op is in a future node and so it will be counted as seen there
            // Note: We also need to reset last_seen if it is set to something else than the last item
            //   in the child. This means that the child contains an `insert` (so last_seen should
            //   be reset to None), but no visible op (so last_seen should not be set to a new value)
            //   The visible op also cannot be in a previous node, because then `last_seen` would
            //   already be set to the same elemid as the last element in the child.
            auto last_elemid = child.last().elemid_or_key();
            if (child.index.has_visible(last_elemid)) {
                last_seen = last_elemid;
            }
            else if (last_seen.has_value() && !(last_elemid == *last_seen)) {
                last_seen.reset();
            }

            return QueryResult{ QueryResult::NEXT, 0 };
        }
    }
}

QueryResult SeekOpWithPatch::query_element_with_metadata(const Op& e, const OpSetMetadata& m) {
    if (op.key.tag == Key::Map) {
        if (!found) {
            // Iterate over any existing operations for the same key; stop when we reach an
            // operation with a different key
            if (!(e.key == op.key)) {
                return QueryResult{ QueryResult::FINISH, 0 };
            }

            // Keep track of any ops we're overwriting and any conflicts on this key
            if (op.overwrites(e)) {
                // when we encounter an increment op we also want to find the counter for
                // it.
                if (op.is_inc() && e.is_counter() && e.visible()) {
                    values.push_back(&e);
                }
                succ.push_back(pos);

                if (e.visible()) {
                    had_value_before = true;
                }
            }
            else if (e.visible()) {
                values.push_back(&e);
            }

            // Ops for the same key should be in ascending order of opId, so we break when
            // we reach an op with an opId greater than that of the new operation
            if (m.lamport_cmp(e.id, op.id) > 0) {
                found = true;
                return QueryResult{ QueryResult::NEXT, 0 };
            }

            ++pos;
        }
        else {
            // For the purpose of reporting conflicts, we also need to take into account any
            // ops for the same key that appear after the new operation
            if (!(e.key == op.key)) {
                return QueryResult{ QueryResult::FINISH, 0 };
            }
            // No need to check if `self.op.overwrites(op)` because an operation's `preds`
            // must always have lower Lamport timestamps than that op itself, and the ops
            // here all have greater opIds than the new op
            if (e.visible()) {
                values.push_back(&e);
            }
        }
        return QueryResult{ QueryResult::NEXT, 0 };
    }
    else {
        QueryResult result;

        if (!found) {
            // First search for the referenced list element (i.e. the element we're updating, or
            // after which we're inserting)
            if (is_target_insert(e)) {
                found = true;
                if (op.overwrites(e)) {
                    // when we encounter an increment op we also want to find the counter for
                    // it.
                    if (op.is_inc() && e.is_counter() && e.visible()) {
                        values.push_back(&e);
                    }
                    succ.push_back(pos);
                }
                if (e.visible()) {
                    had_value_before = true;
                }
            }
            ++pos;
            result = QueryResult{ QueryResult::NEXT, 0 };
        }
        else {
            // Once we've found the reference element, keep track of any ops that we're overwriting
            bool overwritten = op.overwrites(e);
            if (overwritten) {
                // when we encounter an increment op we also want to find the counter for
                // it.
                if (op.is_inc() && e.is_counter() && e.visible()) {
                    values.push_back(&e);
                }
                succ.push_back(pos);
            }

            // If the new op is an insertion, skip over any existing list elements whose elemId is
            // greater than the ID of the new insertion
            if (op.insert) {
                if (lesser_insert(e, m)) {
                    // Insert before the first existing list element whose elemId is less than that
                    // of the new insertion
                    result = QueryResult{ QueryResult::FINISH, 0 };
                }
                else {
                    ++pos;
                    result = QueryResult{ QueryResult::NEXT, 0 };
                }
            }
            else if (e.insert) {
                // If the new op is an update of an existing list element, the first insertion op
                // we encounter after the reference element indicates the end of the reference elem
                result = QueryResult{ QueryResult::FINISH, 0 };
            }
            else {
                // When updating an existing list element, keep track of any conflicts on this list
                // element. We also need to remember if the list element had any visible elements
                // prior to applying the new operation: if not, the new operation is resurrecting
                // a deleted list element, so it looks like an insertion in the patch.
                if (e.visible()) {
                    had_value_before = true;
                    if (!overwritten) {
                        values.push_back(&e);
                    }
                }

                // We now need to put the ops for the same list element into ascending order, so we
                // skip over any ops whose ID is less than that of the new operation.
                if (greater_opid(e, m)) {
                    ++pos;
                }
                result = QueryResult{ QueryResult::NEXT, 0 };
            }
        }

        // The patch needs to know the list index of each operation, so we count the number of
        // visible list elements up to the insertion position of the new operation
        if (result.tag == QueryResult::NEXT) {
            cout_visible(e);
        }
        return result;
    }
}