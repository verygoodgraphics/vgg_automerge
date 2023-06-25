// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <optional>

#include "type.h"
#include "ExId.h"
#include "Query.h"
#include "IndexedCache.h"
#include "legacy.h"
#include "OpTree.h"
#include "query/QueryKeys.h"
#include "OpObserver.h"

// std::vec::IntoIter<(&'a ObjId, &'a op_tree::OpTree)>
struct TreesIter {
    std::vector<std::pair<const ObjId*, const OpTree*>> trees;
    usize current = 0;

    std::optional<std::pair<const ObjId*, const OpTree*>> next();
};

// Iter of op_set.rs
struct OpSetIter {
    TreesIter trees;
    std::optional<std::pair<const ObjId*, OpTreeIter>> current;

    std::optional<std::pair<const ObjId*, const Op*>> next();
};

struct OpSetMetadata {
    IndexedCache<ActorId> actors;
    IndexedCache<std::string_view> props;

    int key_cmp(const Key& left, const Key& right) const;

    int lamport_cmp(const OpId& left, const OpId& right) const;

    OpIds sorted_opids(std::vector<OpId>&& opids) const {
        return OpIds(std::move(opids), [&](const OpId& left, const OpId& right) {
            return lamport_cmp(left, right);
            });
    }

    std::optional<OpIds> try_sorted_opids(std::vector<OpId>&& opids) const {
        return OpIds::new_if_sorted(std::move(opids), [&](const OpId& left, const OpId& right) {
            return lamport_cmp(left, right);
            });
    }

    usize import_prop(std::string&& key) {
        return props.cache(std::move(key));
    }

    OpIds import_opids(std::vector<OldOpId>&& external_opids);
};

class OpSetInternal {
public:
    // Metadata about the operations in this opset.
    OpSetMetadata m;

    OpSetInternal() {
        trees.insert({ ROOT, OpTree() });
    }

    ExId id_to_exid(const OpId& id) const {
        if (id == ROOT)
            return ExId();
        else
            return ExId(id.counter, m.actors._cache[id.actor], id.actor);
    }

    OpSetIter iter() const;

    std::optional<std::pair<ObjId, Key>> parent_object(const ObjId& obj) const;

    Prop export_key(const ObjId& obj, const Key& key) const;

    std::optional<QueryKeys> keys(const ObjId& obj) const;

    TreeQuery& search(const ObjId& obj, TreeQuery& query) const;

    void replace(const ObjId& obj, usize index, OpFunc f);

    void add_succ(const ObjId& obj, const std::vector<usize>& op_indices, const Op& op);

    Op remove(const ObjId& obj, usize index);

    usize len() const {
        return length;
    }

    void insert(usize index, const ObjId& obj, Op&& element);

    Op&& insert_op(const ObjId& obj, Op&& op);

    Op&& insert_op_with_observer(const ObjId& obj, Op&& op, OpObserver& observer);

    std::optional<ObjType> object_type(const ObjId& id) const;

private:
    // The map of objects to their type and ops.
    std::unordered_map<ObjId, OpTree> trees;
    // The number of operations in the opset.
    usize length = 0;
};

typedef OpSetInternal OpSet;
