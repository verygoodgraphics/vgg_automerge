// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include <cassert>

#include "Transaction.h"
#include "../Automerge.h"
#include "../query/Nth.h"
#include "../query/QueryProp.h"
#include "../query/InsertNth.h"

ChangeHash TransactionInner::commit(Automerge& doc, std::optional<std::string>&& message, std::optional<s64>&& time, std::optional<OpObserver*>&& op_observer) {
    if (message) {
        this->message = std::move(message);
    }

    if (time) {
        this->time = *time;
    }

    // TODO: observer

    usize num_ops = pending_ops();
    auto change = export_change(doc.ops.m.actors, doc.ops.m.props);
    auto hash = change.hash;
    doc.update_history(std::move(change), num_ops);

    assert(doc.get_heads() == std::vector<ChangeHash>(1, hash));

    return hash;
}

Change TransactionInner::export_change(const IndexedCache<ActorId>& actors, const IndexedCache<std::string>& props) {
    std::vector<OldOp> old_operations;
    for (auto& [obj, prop, op] : operations) {
        old_operations.push_back(OldOp(op, obj, actors, props));
    }

    return OldChange{
        std::move(old_operations),
        actors.get(actor),
        std::move(hash),
        seq,
        start_op,
        time,
        std::move(message),
        std::move(deps),
        std::move(extra_bytes)
    }.encode();
}

void TransactionInner::put(Automerge& doc, const ExId& ex_obj, Prop&& prop, ScalarValue&& value) {
    ObjId obj = doc.exid_to_obj(ex_obj);
    local_op(doc, obj, std::move(prop), OpType{ OpType::Put, std::move(value) });
}

ExId TransactionInner::put_object(Automerge& doc, const ExId& ex_obj, Prop&& prop, ObjType value) {
    ObjId obj = doc.exid_to_obj(ex_obj);
    OpId id = *local_op(doc, obj, std::move(prop), OpType{ OpType::Make, value });

    return doc.id_to_exid(id);
}

void TransactionInner::insert_local_op(Automerge& doc, Prop&& prop, Op&& op, usize pos, ObjId& obj, VecPos& succ_pos) {
    doc.ops.add_succ(obj, succ_pos, op);

    if (!op.is_delete()) {
        doc.ops.insert(pos, obj, Op(op));
    }

    operations.push_back({ obj, std::move(prop), std::move(op) });
}

void TransactionInner::insert(Automerge& doc, const ExId& ex_obj, usize index, ScalarValue&& value) {
    ObjId obj = doc.exid_to_obj(ex_obj);
    do_insert(doc, obj, index, OpType{ OpType::Put, std::move(value) });
}

ExId TransactionInner::insert_object(Automerge& doc, const ExId& ex_obj, usize index, ObjType value) {
    ObjId obj = doc.exid_to_obj(ex_obj);
    OpId id = do_insert(doc, obj, index, OpType{ OpType::Make, value });
    return doc.id_to_exid(id);
}

OpId TransactionInner::do_insert(Automerge& doc, ObjId& obj, usize index, OpType&& action) {
    OpId id = next_id();

    auto q = InsertNth(index);
    auto& query = static_cast<InsertNth&>(doc.ops.search(obj, q));

    auto key = query.key();

    auto op = Op{
        id,
        std::move(action),
        key,
        {},
        {},
        true
    };

    doc.ops.insert(query.pos(), obj, Op(op));
    operations.push_back({ obj, Prop(index), std::move(op) });

    return id;
}

std::optional<OpId> TransactionInner::local_op(Automerge& doc, ObjId& obj, Prop&& prop, OpType&& action) {
    if (prop.tag == Prop::Map) {
        return local_map_op(doc, obj, std::move(std::get<std::string>(prop.data)), std::move(action));
    }
    else {
        return local_list_op(doc, obj, std::move(std::get<usize>(prop.data)), std::move(action));
    }
}

std::optional<OpId> TransactionInner::local_map_op(Automerge& doc, ObjId& obj, std::string&& prop, OpType&& action) {
    if (prop.empty()) {
        throw AutomergeError{ AutomergeError::EmptyStringKey, (u64)0 };
    }

    OpId id = next_id();
    usize prop_index = doc.ops.m.props.cache(std::string(prop));
    auto q = QueryProp(prop_index);
    auto& query = static_cast<QueryProp&>(doc.ops.search(obj, q));

    // no key present to delete
    if (query.ops.empty() && action.tag == OpType::Delete) {
        return std::nullopt;
    }

    if ((query.ops.size() == 1) && query.ops[0]->is_noop(action)) {
        return std::nullopt;
    }

    // increment operations are only valid against counter values.
    // if there are multiple values (from conflicts) then we just need one of them to be a counter.
    if (action.tag == OpType::Increment) {
        bool no_counter = true;
        for (auto op : query.ops) {
            if (op->is_counter()) {
                no_counter = false;
                break;
            }
        }
        if (no_counter) {
            throw AutomergeError{ AutomergeError::MissingCounter, (u64)0 };
        }
    }

    std::vector<OpId> pred;
    for (auto op : query.ops) {
        pred.push_back(op->id);
    }

    auto op = Op{
        id,
        std::move(action),
        { Key::Map, prop_index },
        {},
        doc.ops.m.sorted_opids(std::move(pred)),
        false
    };

    usize pos = query.pos;
    auto& ops_pos = query.ops_pos;
    insert_local_op(doc, Prop(std::move(prop)), std::move(op), pos, obj, ops_pos);

    return id;
}

std::optional<OpId> TransactionInner::local_list_op(Automerge& doc, ObjId& obj, usize index, OpType&& action) {
    auto q = Nth(index);
    auto& query = static_cast<Nth&>(doc.ops.search(obj, q));

    OpId id = next_id();
    std::vector<OpId> pred;
    for (auto op : query.ops) {
        pred.push_back(op->id);
    }
    auto key = query.key();

    if ((query.ops.size() == 1) && query.ops[0]->is_noop(action)) {
        return std::nullopt;
    }

    // increment operations are only valid against counter values.
    // if there are multiple values (from conflicts) then we just need one of them to be a counter.
    if (action.tag == OpType::Increment) {
        bool no_counter = true;
        for (auto op : query.ops) {
            if (op->is_counter()) {
                no_counter = false;
                break;
            }
        }
        if (no_counter) {
            throw AutomergeError{ AutomergeError::MissingCounter, (u64)0 };
        }
    }

    auto op = Op{
        id,
        std::move(action),
        key,
        {},
        doc.ops.m.sorted_opids(std::move(pred)),
        false
    };

    usize pos = query.pos;
    auto& ops_pos = query.ops_pos;
    insert_local_op(doc, Prop(index), std::move(op), pos, obj, ops_pos);

    return id;
}

void TransactionInner::delete_(Automerge& doc, const ExId& ex_obj, Prop&& prop) {
    auto obj = doc.exid_to_obj(ex_obj);
    local_op(doc, obj, std::move(prop), OpType{ OpType::Delete, {} });
}

/////////////////////////////////////////////////////////

void Transaction::put(const ExId& obj, Prop&& prop, ScalarValue&& value) {
    inner->put(*doc, obj, std::move(prop), std::move(value));
}

ExId Transaction::put_object(const ExId& obj, Prop&& prop, ObjType value) {
    return inner->put_object(*doc, obj, std::move(prop), value);
}

void Transaction::insert(const ExId& obj, usize index, ScalarValue&& value) {
    inner->insert(*doc, obj, index, std::move(value));
}

ExId Transaction::insert_object(const ExId& obj, usize index, ObjType value) {
    return inner->insert_object(*doc, obj, index, value);
}

void Transaction::delete_(const ExId& obj, Prop&& prop) {
    return inner->delete_(*doc, obj, std::move(prop));
}

Keys Transaction::keys(const ExId& obj) const {
    return doc->keys(obj);
}

usize Transaction::length(const ExId& obj) const {
    return doc->length(obj);
}

std::optional<ValuePair> Transaction::get(const ExId& obj, Prop&& prop) const {
    return doc->get(obj, std::move(prop));
}

std::vector<ValuePair> Transaction::get_all(const ExId& obj, Prop&& prop) const {
    return doc->get_all(obj, std::move(prop));
}