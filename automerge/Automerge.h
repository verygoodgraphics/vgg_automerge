// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <optional>
#include <algorithm>

#include "type.h"
#include "Change.h"
#include "ExId.h"
#include "OpSet.h"
#include "Keys.h"
#include "transaction/Transaction.h"
#include "ChangeGraph.h"
#include "transaction/CommitOptions.h"
#include "Sync.h"
#include "Error.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

struct Actor {
    bool isUsed = false;

    ActorId unused = {};
    usize cached = 0;
};

using TransactionFunc = std::function<std::vector<ExId>(Transaction&)>;
using CommitOptionsFunc = std::function<CommitOptions<OpObserver>(const std::vector<ExId>&)>;

struct Automerge {
    // The list of unapplied changes that are not causally ready.
    std::vector<Change> queue;
    // The history of changes that form this document, topologically sorted too.
    std::vector<Change> histroy;
    // Mapping from change hash to index into the history list.
    std::unordered_map<ChangeHash, usize> histroy_index;
    // Graph of changes
    ChangeGraph change_graph;
    // Mapping from actor index to list of seqs seen for them.
    std::unordered_map<usize, VecPos> states;
    // Current dependencies of this document (heads hashes).
    std::unordered_set<ChangeHash> deps;
    // Heads at the last save.
    std::vector<ChangeHash> saved;
    // The set of operations that form this document.
    OpSet ops;
    // The current actor.
    Actor actor;
    // The maximum operation counter this document has seen.
    u64 max_op;

    Automerge() : queue(), histroy(), histroy_index(), change_graph(), states(), deps(), saved(), ops(),
        actor{ false, ActorId(true), 0 }, max_op(0) {}

    // Set the actor id for this document.
    Automerge& set_actor(ActorId&& _actor) {
        actor.isUsed = false;
        actor.unused = std::move(_actor);

        return *this;
    }

    // Get the current actor id of this document.
    const ActorId& get_actor() const;

    usize get_actor_index();

    // Start a transaction.
    Transaction transaction() {
        return Transaction(transaction_inner(), this);
    }

    TransactionInner transaction_inner();

    // Run a transaction on this document in a closure, automatically handling commit or rollback
    // afterwards.
    // transact

    // Like [`Self::transact`] but with a function for generating the commit options.
    std::pair<std::vector<ExId>, ChangeHash> transact_with(CommitOptionsFunc c, TransactionFunc f);


    // Fork this document at the current point for use by a different actor.
    // This will create a new actor ID for the forked document
    Automerge fork() const {
        Automerge f = *this;
        f.set_actor(ActorId(true));

        return f;
    }

    // throw AutomergeError
    //Automerge fork_at() {}

    // Get the object id of the object that contains this object and the prop that this object is
    // at in that object.
    //std::optional<PropPair> parent_object() {}

    // Get an iterator over the parents of an object.
    // void parents() const {}

    //std::vector<PropPair> path_to_object() {}

    /// Export a key to a prop.
    //Prop export_key(ObjId& obj, Key& key) {}

    // Get the keys of the object `obj`.
    //
    // For a map this returns the keys of the map.
    // For a list this returns the element ids (opids) encoded as strings.
    Keys keys(const ExId& obj) const;

    // keys_at

    // map_range

    // map_range_at

    // list_range

    // list_range_at

    // Values values() {}

    // values_at

    // Get the length of the given object.
    usize length(const ExId& obj) const;

    // Historical version of [`length`](Self::length).
    usize length_at(const ExId& obj, const std::vector<ChangeHash>& heads) const;

    // Get the type of this object, if it is an object.
    // throw AutomergeError
    ObjType object_type(const ExId& obj) const {
        auto [_, obj_type] = exid_to_obj(obj);

        return obj_type;
    }

    // throw AutomergeError
    std::pair<ObjId, ObjType> exid_to_obj(const ExId& id) const;

    ExId id_to_exid(const OpId& id) const;

    // Get the string represented by the given text object.
    // throw AutomergeError
    std::string text(const ExId& obj) const {
        return {};
    }

    // Get the string represented by the given text object.
    // throw AutomergeError
    //std::string text_at() {}

    // throw AutomergeError
    std::optional<ValuePair> get(const ExId& obj, Prop&& prop) const;

    // get_at

    // throw AutomergeError
    std::vector<ValuePair> get_all(const ExId& obj, Prop&& prop) const;

    // get_all_at

    // throw exception
    static Automerge load(const BinSlice& data) {
        return load_with(data, nullptr);
    }

    // throw exception
    static Automerge load_with(const BinSlice& data, OpObserver* options);

    // load_incremental, load_incremental_with

    bool duplicate_seq(const Change& change) const;

    // Apply changes to this document.
    // throw AutomergeError
    void apply_changes(std::vector<Change>&& changes) {
        apply_changes_with(std::move(changes), nullptr);
    }

    // Apply changes to this document.
    // throw AutomergeError
    void apply_changes_with(std::vector<Change>&& changes, OpObserver* options);

    void apply_change(Change&& change, OpObserver* observer);

    bool is_causally_ready(const Change& change) const;

    std::optional<Change> pop_next_causally_ready_change();

    std::vector<std::pair<ObjId, Op>> imports_ops(const Change& change);

    std::vector<ChangeHash> merge(Automerge& other);

    std::vector<ChangeHash> merge(Automerge&& other);

    std::vector<ChangeHash> merge_with(Automerge& other, OpObserver* options);

    std::vector<u8> save();

    // save_incremental
        
    // Filter the changes down to those that are not transitive dependencies of the heads.
    // Thus a graph with these heads has not seen the remaining changes.
    // throw AutomergeError
    void filter_changes(const std::vector<ChangeHash>& heads, std::set<ChangeHash>& changes) const;

    // Get the hashes of the changes in this document that aren't transitive dependencies of the
    // given `heads`.
    std::vector<ChangeHash> get_missing_deps(const std::vector<ChangeHash>& heads) const;

    // Get the changes since `have_deps` in this document using a clock internally.
    // throw AutomergeError
    std::vector<const Change*> get_changes_clock(const std::vector<ChangeHash>& have_deps) const;

    // throw AutomergeError
    std::vector<const Change*> get_changes(const std::vector<ChangeHash>& have_deps) const {
        return get_changes_clock(have_deps);
    }

    // Get the last change this actor made to the document.
    std::optional<Change> get_last_local_change() const;

    // throw AutomergeError
    Clock clock_at(const std::vector<ChangeHash>& heads) const;

    // Get a change by its hash.
    std::optional<const Change*> get_change_by_hash(const ChangeHash& hash) const;

    // Get the changes that the other document added compared to this document.
    std::vector<const Change*> get_changes_added(const Automerge& other) const;

    // Get the heads of this document.
    std::vector<ChangeHash> get_heads() const;

    ChangeHash get_hash(usize actor, u64 seq) const;

    usize update_history(Change&& change, usize num_pos);

    void update_deps(const Change& change);

    // automerge.rs: import
    // throw AutomergeError
    std::pair<ExId, ObjType> import(const std::string_view& str) const;

    // interop.rs: import
    // throw AutomergeError
    std::pair<ExId, ObjType> import_object(const std::string& obj_str) const;

    std::string to_string(Export&& id) const;

    std::string dump(const u8 indent = 0) const {
        return json_doc.dump(indent);
    }

    // visualise_optree

    std::optional<SyncMessage> generate_sync_message(State& sync_state) const;

    // throw
    void receive_sync_message(State& sync_state, SyncMessage&& message) {
        receive_sync_message_with(sync_state, std::move(message), nullptr);
    }

    // throw
    void receive_sync_message_with(State& sync_state, SyncMessage&& message, OpObserver *options);

    Have make_bloom_filter(std::vector<ChangeHash>&& last_sync) const;

    // throw
    std::vector<const Change*> get_changes_to_send(const std::vector<Have>& have, const std::vector<ChangeHash>& need) const;

    // autocommit.rs, wasm/lib.rs: put
    // throw AutomergeError
    void put(const ExId& obj, Prop&& prop, ScalarValue&& value) {
        ensure_transaction_open();

        _transaction->put(obj, std::move(prop), std::move(value));
    }

    // autocommit.rs: put_object
    // throw AutomergeError
    ExId put_object(const ExId& obj, Prop&& prop, ObjType value) {
        ensure_transaction_open();

        return _transaction->put_object(obj, std::move(prop), value);
    }

    // wasm/lib.rs: put_object
    // throw AutomergeError
    ExId put_object(const ExId& obj, Prop&& prop, const std::string& value_str);

    // autocommit.rs, wasm/lib.rs: insert
    // throw AutomergeError
    void insert(const ExId& obj, usize index, ScalarValue&& value) {
        ensure_transaction_open();

        _transaction->insert(obj, index, std::move(value));
    }

    // autocommit.rs: insert_object
    // throw AutomergeError
    ExId insert_object(const ExId& obj, usize index, ObjType value) {
        ensure_transaction_open();

        return _transaction->insert_object(obj, index, value);
    }

    // wasm/lib.rs: insert_object
    // throw AutomergeError
    ExId insert_object(const ExId& obj, usize index, const std::string& value_str);

    // autocommit.rs, wasm/lib.rs: increment
    // throw AutomergeError
    void increment(const ExId& obj, Prop&& prop, s64 value) {
        ensure_transaction_open();

        _transaction->increment(obj, std::move(prop), value);
    }

    // autocommit.rs, wasm/lib.rs: delete
    // throw AutomergeError
    void delete_(const ExId& obj, Prop&& prop) {
        ensure_transaction_open();

        _transaction->delete_(obj, std::move(prop));
    }

    /*!
    @brief add a new item with a new path, the parent path should exist
    @param[in] path     A json path
    @param[in] value    Json value to be added at the path
    @throw

    @note commit() should be called manually after this operation
    */
    void json_add(const json::json_pointer& path, const json& value);

    /*!
    @brief replace an item at an existed path
    @param[in] path     A json path
    @param[in] value    Json value to be replaced at the path
    @throw

    @note commit() should be called manually after this operation.
          The original item will be replaced by the new item entirely. No diff operating internal.
          Change several items in an object, should call replace() separately for each item.
    */
    void json_replace(const json::json_pointer& path, const json& value);

    /*!
    @brief delete a item(scalar or object) of an existed path
    @param[in] path         A json path
    @throw

    @note commit() should be called manually after this operation
    */
    void json_delete(const json::json_pointer& path);

    /*!
    @brief commit a batch of operations together in a transaction

    @note A user operation usually contains several Automerge operations. commit() should be called
          after all add/replace/delete operations applied of one user operation.
    */
    void commit() {
        if (_transaction) {
            _transaction->commit();
            _transaction.reset();
        }
    }

    /*!
    @brief return the current json object of Automerge doc
    @return a const reference of the json object
    */
    const json& json_const_ref() const {
        return json_doc;
    }

private:
    // json object of the whole doc, updated by every operation, always equals to the result of to_json()
    json json_doc;
    std::optional<Transaction> _transaction = {};

    void ensure_transaction_open() {
        if (!_transaction.has_value()) {
            _transaction = transaction();
        }
    }

    ExId json_adding(const PropPair& item, std::pair<Value, std::list<std::pair<Prop, json>>>&& value);
    ExId json_replacing(const PropPair& item, std::pair<Value, std::list<std::pair<Prop, json>>>&& value);

    // interop.rs: import_path
    JsonPathParsed json_pointer_parse(ExId&& prefix_id, ObjType prefix_type, const json::json_pointer& path) const;

    // interop.rs: import_path
    // throw AutomergeError
    std::pair<ExId, ObjType> import_path(ExId&& prefix_id, ObjType prefix_type, const std::string_view& obj_str) const;
};

void to_json(json& j, const Automerge& doc);

void from_json(const json& j, Automerge& doc);
