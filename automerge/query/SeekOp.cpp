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
        auto cmp = m.key_cmp(child.last().key, this->op.key);
        if (cmp < 0 ||
            (cmp == 0 && !child.index.has_visible(this->op.key))) {
            pos += child.len();
            return QueryResult{ QueryResult::NEXT, 0 };
        }

        return QueryResult{ QueryResult::DESCEND, 0 };
    }
    else if (std::get<ElemId>(op.key.data) == HEAD) {
        while (pos < child.len()) {
            auto op_optional = child.get(pos);
            auto op = *op_optional;
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
        auto cmp = m.key_cmp(e.key, this->op.key);

        if (cmp > 0) {
            return QueryResult{ QueryResult::FINISH, 0 };
        }

        if (cmp == 0) {
            if (op.overwrites(e)) {
                succ.push_back(pos);
            }

            if (m.lamport_cmp(e.id, op.id) > 0) {
                return QueryResult{ QueryResult::FINISH, 0 };
            }
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
