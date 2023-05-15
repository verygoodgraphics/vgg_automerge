// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <stdexcept>

#include "OpSet.h"
#include "query/OpId.h"
#include "query/SeekOp.h"
#include "query/SeekOpWithPatch.h"

std::optional<std::pair<const ObjId*, const OpTree*>> TreesIter::next() {
    if (current >= trees.size()) {
        return {};
    }

    return trees[current++];
}

std::optional<std::pair<const ObjId*, const Op*>> OpSetIter::next() {
    if (current) {
        auto tree = current->second.next();
        if (tree) {
            return std::make_pair(current->first, *tree);
        }
    }

    while (true) {
        auto tree_next = trees.next();
        if (!tree_next) {
            return {};
        }

        current = { tree_next->first, tree_next->second->iter() };
        auto next = current->second.next();
        if (next) {
            return std::make_pair(current->first, *next);
        }
    }
}

/////////////////////////////////////////////////////////

int OpSetMetadata::key_cmp(const Key& left, const Key& right) const {
    if (!left.is_map() || !right.is_map())
        throw std::invalid_argument("left or right is not map");

    return props[std::get<usize>(left.data)].compare(props[std::get<usize>(right.data)]);
}

int OpSetMetadata::lamport_cmp(const OpId& left, const OpId& right) const {
    if (left.counter == right.counter) {
        return actors[left.actor].cmp(actors[right.actor]);
    }

    if (left.counter < right.counter)
        return -1;
    if (left.counter > right.counter)
        return 1;
    return 0;
}

OpIds OpSetMetadata::import_opids(std::vector<OldOpId>&& external_opids) {
    std::vector<OpId> result;
    result.reserve(external_opids.size());

    for (auto& opid : external_opids) {
        auto actor_idx = actors.cache(std::move(opid.actor));
        result.push_back({ opid.counter, actor_idx });
    }

    return OpIds(std::move(result), [&](const OpId& left, const OpId& right) {
        return lamport_cmp(left, right);
        });
}

OpSetIter OpSetInternal::iter() const {
    std::vector<std::pair<const ObjId*, const OpTree*>> objs;
    objs.reserve(trees.size());
    for (auto& tree : trees) {
        objs.push_back({ &tree.first, &tree.second });
    }

    // TODO: why need stable sort?
    std::sort(objs.begin(), objs.end(),
        [&](std::pair<const ObjId*, const OpTree*>& a, std::pair<const ObjId*, const OpTree*>& b) {
            return m.lamport_cmp(*a.first, *b.first) < 0;
        });
    return { TreesIter{std::move(objs), 0}, {} };
}

std::optional<std::pair<ObjId, Key>> OpSetInternal::parent_object(const ObjId& obj) const {
    if (trees.count(obj) == 0 || !trees.at(obj).parent)
        return std::nullopt;
    auto& parent = *(trees.at(obj).parent);
    auto query = OpIdSearch(obj);
    auto& key = static_cast<OpIdSearch&>(search(parent, query)).key;
    if (!key) {
        throw std::runtime_error("not found");
    }

    return { {parent, *key} };
}

Prop OpSetInternal::export_key(const ObjId& obj, const Key& key) const {
    if (key.tag == Key::Map) {
        return Prop(std::string(m.props.get(std::get<usize>(key.data))));
    }

    // TODO: Key::Seq
    return Prop();
}

std::optional<QueryKeys> OpSetInternal::keys(const ObjId& obj) const {
    try {
        auto& tree = trees.at(obj);
        return tree.internal.keys();
    }
    catch (std::out_of_range) {
        return std::nullopt;
    }
}

TreeQuery& OpSetInternal::search(const ObjId& obj, TreeQuery& query) const {
    try {
        auto& tree = trees.at(obj);
        return tree.internal.search(query, m);
    }
    catch (std::out_of_range) {
        return query;
    }
}

void OpSetInternal::replace(const ObjId& obj, usize index, OpFunc f) {
    try {
        auto& tree = trees.at(obj);
        tree.internal.update(index, f);
    }
    catch (std::out_of_range) {
    }
}

void OpSetInternal::add_succ(const ObjId& obj, const std::vector<usize>& op_indices, const Op& op) {
    try {
        auto& tree = trees.at(obj);
        for (auto index : op_indices) {
            tree.internal.update(index, [&](Op& old_op) {
                old_op.add_succ(op, [&](const OpId& left, const OpId& right) {
                    return m.lamport_cmp(left, right);
                    });
                });
        }
    }
    catch (std::out_of_range) {
    }
}

Op OpSetInternal::remove(const ObjId& obj, usize index) {
    // this happens on rollback - be sure to go back to the old state
    auto& tree = trees.at(obj);
    --length;
    Op op = tree.internal.remove(index);
    if (op.action.tag == OpType::Make) {
        trees.erase(op.id);
    }
    return op;
}

void OpSetInternal::insert(usize index, const ObjId& obj, Op&& element) {
    if (element.action.tag == OpType::Make) {
        trees.insert({
            element.id,
            OpTree{
                OpTreeInternal(),
                std::get<ObjType>(element.action.data),
                std::optional<ObjId>(obj)
            }
            });
    }

    try {
        auto& tree = trees.at(obj);
        tree.internal.insert(index, std::move(element));
        ++length;
    }
    catch (std::out_of_range) {
        // throw tracing::warn!("attempting to insert op for unknown object");
    }
}

Op&& OpSetInternal::insert_op(const ObjId& obj, Op&& op) {
    auto query = SeekOp(op);
    auto& q = static_cast<SeekOp&>(search(obj, query));

    auto& succ = q.succ;
    usize pos = q.pos;

    add_succ(obj, succ, op);

    if (!op.is_delete()) {
        insert(pos, obj, Op(op));
    }

    return std::move(op);
}

// TODO: add parents param to observer
Op&& OpSetInternal::insert_op_with_observer(const ObjId& obj, Op&& op, OpObserver& observer) {
    auto query = SeekOpWithPatch(op);
    auto& q = static_cast<SeekOpWithPatch&>(search(obj, query));

    usize pos = q.pos;
    auto& succ = q.succ;
    usize seen = q.seen;
    auto& values = q.values;
    bool had_value_before = q.had_value_before;

    ExId ex_obj = id_to_exid(obj);
    Prop key;
    if (op.key.tag == Key::Map) {
        key.tag = Prop::Map;
        key.data = m.props[std::get<usize>(op.key.data)];
    }
    else {
        key.tag = Prop::Seq;
        key.data = seen;
    }

    if (op.insert) {
        ValuePair value = { id_to_exid(op.id), op.value() };
        observer.insert(ex_obj, seen, value);
    }
    else if (op.is_delete()) {
        if (!values.empty()) {
            auto& winner = *values.end();
            ValuePair value = { id_to_exid(winner->id), winner->value() };
            bool conflict = values.size() > 1;
            observer.put(ex_obj, key, value, conflict);
        }
        else {
            observer._delete(ex_obj, key);
        }
    }
    else {
        auto increment_value = op.get_increment_value();
        if (increment_value) {
            // only observe this increment if the counter is visible, i.e. the counter's
            // create op is in the values
            for (auto& value : values) {
                bool found = false;
                for (auto& opid : op.pred.v) {
                    if (opid == value->id) {
                        found = true;
                        break;
                    }
                }

                if (found) {
                    // we have observed the value
                    observer.increment(ex_obj, key, { id_to_exid(op.id), *increment_value });
                    break;
                }
            }
        }
        else {
            const Op* winner = &op;
            if (!values.empty()) {
                auto& last_value = *values.end();
                if (m.lamport_cmp(op.id, last_value->id) <= 0) {
                    winner = last_value;
                }
            }
            ValuePair value = { id_to_exid(winner->id), winner->value() };
            if (op.is_list_op() && !had_value_before) {
                observer.insert(ex_obj, seen, value);
            }
            else {
                observer.put(ex_obj, key, value, !values.empty());
            }
        }
    }

    add_succ(obj, succ, op);

    if (!op.is_delete()) {
        insert(pos, obj, Op(op));
    }

    return std::move(op);
}

std::optional<ObjType> OpSetInternal::object_type(const ObjId& id) const {
    try {
        auto& tree = trees.at(id);
        return std::optional<ObjType>(tree.objtype);
    }
    catch (std::out_of_range) {
        return std::nullopt;
    }
}