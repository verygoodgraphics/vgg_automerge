// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "SeekOp.h"
#include "../OpSet.h"

bool SeekOp::lesser_insert(const Op& op, const OpSetMetadata& m) {
    return op.insert && (m.lamport_cmp(op.id, this->op.id) < 0);
}

bool SeekOp::greater_opid(const Op& op, const OpSetMetadata& m) {
    return (m.lamport_cmp(op.id, this->op.id) > 0);
}

bool SeekOp::is_target_insert(const Op& op) {
    return op.insert && (op.elemid() == this->op.key.elemid());
}

QueryResult SeekOp::query_node_with_metadata(const OpTreeNode& child, const OpSetMetadata& m) {
    if (found) {
        return QueryResult{ QueryResult::DESCEND, 0 };
    }

    if (op.key.tag == Key::Map) {
        if (start) {
            if (pos + child.len() >= *start) {
                // skip empty nodes
                if (child.index.visible_len() == 0) {
                    pos += child.len();
                    return QueryResult{ QueryResult::NEXT, 0 };
                }
                else {
                    return QueryResult{ QueryResult::DESCEND, 0 };
                }
            }
            else {
                pos += child.len();
                return QueryResult{ QueryResult::NEXT, 0 };
            }
        }
        else {
            // in the root node find the first op position for the key
            usize new_start = binary_search_by(child, [&](const Op* op) {
                return m.key_cmp(op->key, this->op.key);
                });
            start = new_start;
            pos = new_start;
            return QueryResult{ QueryResult::SKIP, new_start };
        }
    }
    else if (std::get<ElemId>(op.key.data) == HEAD) {
        while (pos < child.len()) {
            auto op = *(child.get(pos));
            if (op->insert && (m.lamport_cmp(op->id, this->op.id) < 0)) {
                break;
            }
            ++pos;
        }
        return QueryResult{ QueryResult::FINISH, 0 };
    }
    else {
        if (child.index.ops.count(std::get<ElemId>(op.key.data))) {
            return QueryResult{ QueryResult::DESCEND, 0 };
        }
        else {
            pos += child.len();
            return QueryResult{ QueryResult::NEXT, 0 };
        }
    }
}

QueryResult SeekOp::query_element_with_metadata(const Op& e, const OpSetMetadata& m) {
    if (op.key.tag == Key::Map) {
        // don't bother looking at things past our key
        if (!(e.key == op.key)) {
            return QueryResult{ QueryResult::FINISH, 0 };
        }

        if (op.overwrites(e)) {
            succ.push_back(pos);
        }

        if (m.lamport_cmp(e.id, op.id) > 0) {
            return QueryResult{ QueryResult::FINISH, 0 };
        }

        ++pos;
        return QueryResult{ QueryResult::NEXT, 0 };
    }
    else {
        if (!found) {
            if (is_target_insert(e)) {
                found = true;
                if (op.overwrites(e)) {
                    succ.push_back(pos);
                }
            }
            ++pos;
            return QueryResult{ QueryResult::NEXT, 0 };
        }
        else {
            // we have already found the target
            if (op.overwrites(e)) {
                succ.push_back(pos);
            }
            if (op.insert) {
                if (lesser_insert(e, m)) {
                    return QueryResult{ QueryResult::FINISH, 0 };
                }
                else {
                    ++pos;
                    return QueryResult{ QueryResult::NEXT, 0 };
                }
            }
            else if (e.insert || greater_opid(e, m)) {
                return QueryResult{ QueryResult::FINISH, 0 };
            }
            else {
                ++pos;
                return QueryResult{ QueryResult::NEXT, 0 };
            }
        }
    }
}