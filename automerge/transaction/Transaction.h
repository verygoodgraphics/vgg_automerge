// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <utility>
#include <optional>

#include "../type.h"
#include "../Op.h"
#include "../ExId.h"
#include "../Change.h"
#include "../OpObserver.h"
#include "../IndexedCache.h"
#include "Transactable.h"
#include "CommitOptions.h"

struct Automerge;

struct TransactionInner {
    usize actor = 0;
    u64 seq = 0;
    u64 start_op = 0;
    s64 time = 0;
    std::optional<std::string> message = {};
    std::vector<u8> extra_bytes = {};
    std::optional<ChangeHash> hash = {};
    std::vector<ChangeHash> deps = {};
    std::vector<std::tuple<ObjId, Prop, Op>> operations = {};

    usize pending_ops() const {
        return operations.size();
    }

    ChangeHash commit(Automerge& doc, std::optional<std::string>&& message, std::optional<s64>&& time, std::optional<OpObserver*>&& op_observer);

    Change export_change(const IndexedCache<ActorId>& actors, const IndexedCache<std::string>& props);

    // throw AutomergeError
    void put(Automerge& doc, const ExId& ex_obj, Prop&& prop, ScalarValue&& value);

    // throw AutomergeError
    ExId put_object(Automerge& doc, const ExId& ex_obj, Prop&& prop, ObjType value);

    OpId next_id() {
        return OpId{ start_op + pending_ops(), actor };
    }

    void insert_local_op(Automerge& doc, Prop&& prop, Op&& op, usize pos, ObjId& obj, VecPos& succ_pos);

    // throw AutomergeError
    void insert(Automerge& doc, const ExId& ex_obj, usize index, ScalarValue&& value);

    // throw AutomergeError
    ExId insert_object(Automerge& doc, const ExId& ex_obj, usize index, ObjType value);

    // throw AutomergeError
    OpId do_insert(Automerge& doc, ObjId& obj, usize index, OpType&& action);

    // throw AutomergeError
    std::optional<OpId> local_op(Automerge& doc, ObjId& obj, Prop&& prop, OpType&& action);

    // throw AutomergeError
    std::optional<OpId> local_map_op(Automerge& doc, ObjId& obj, std::string&& prop, OpType&& action);

    // throw AutomergeError::InvalidIndex, AutomergeError::MissingCounter
    std::optional<OpId> local_list_op(Automerge& doc, ObjId& obj, usize index, OpType&& action);

    // throw AutomergeError
    void increment(Automerge& doc, const ExId& ex_obj, Prop&& prop, s64 value);

    // throw AutomergeError
    void delete_(Automerge& doc, const ExId& ex_obj, Prop&& prop);
};

struct Transaction : public Transactable {
    std::optional<TransactionInner> inner = {};
    Automerge* doc = nullptr;

    Transaction(TransactionInner t, Automerge* d) : inner(t), doc(d) {}

    // Commit the operations performed in this transaction, returning the hashes corresponding to the new heads.
    ChangeHash commit() {
        return inner->commit(*doc, {}, {}, {});
    }

    // Commit the operations in this transaction with some options.
    ChangeHash commit_with(CommitOptions<OpObserver>&& options) {
        return inner->commit(*doc, std::move(options.message), std::move(options.time), std::move(options.op_observer));
    }

    // throw AutomergeError
    void put(const ExId& obj, Prop&& prop, ScalarValue&& value) override;

    // throw AutomergeError
    ExId put_object(const ExId& obj, Prop&& prop, ObjType value) override;

    // throw AutomergeError
    void insert(const ExId& obj, usize index, ScalarValue&& value) override;

    // throw AutomergeError
    ExId insert_object(const ExId& obj, usize index, ObjType value) override;

    // throw AutomergeError
    void increment(const ExId& obj, Prop&& prop, s64 value) override;

    // throw AutomergeError
    void delete_(const ExId& obj, Prop&& prop) override;

    Keys keys(const ExId& obj) const override;

    usize length(const ExId& obj) const override;

    // throw AutomergeError
    std::optional<ValuePair> get(const ExId& obj, Prop&& prop) const override;

    // throw AutomergeError
    std::vector<ValuePair> get_all(const ExId& obj, Prop&& prop) const override;
};
