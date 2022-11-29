// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <optional>
#include <variant>
#include <functional>
#include <stdexcept>

#include "type.h"
#include "Value.h"

// #[derive(PartialEq, Debug, Clone)]
struct OpType {
    enum {
        Make,       // ObjType
        Delete,
        Increment,  // s64
        Put         // ScalarValue
    } tag = Make;
    std::variant<ObjType, s64, ScalarValue> data = {};

    bool operator==(const OpType& other) const {
        return (tag == other.tag) && (data == other.data);
    }

    u64 action_index() const;

    // throw
    static OpType from_index_and_value(u64 index, ScalarValue&& value);
};

// #[derive(Debug, Clone, PartialEq, Default)]
struct OpIds {
    std::vector<OpId> v;

    OpIds() = default;
    OpIds(std::vector<OpId>&& opids) : v(std::move(opids)) {}
    OpIds(std::vector<OpId>&& opids, OpIdCmpFunc cmp) : v(std::move(opids)) {
        std::stable_sort(v.begin(), v.end(), cmp);
    }

    // Create a new OpIds if `opids` are sorted with respect to `cmp` and contain no duplicates.
    // Returns `Some(OpIds)` if `opids` is sorted and has no duplicates, otherwise returns `None`
    static std::optional<OpIds> new_if_sorted(std::vector<OpId>&& opids, OpIdCmpFunc cmp);

    // Add an op to this set of OpIds. The `comparator` must provide a
    // consistent ordering between successive calls to `add`.
    void add(OpId opid, OpIdCmpFunc cmp);
};

// #[derive(Debug, Clone, PartialEq)]
struct Op {
    OpId id;
    OpType action;
    Key key;
    OpIds succ;
    OpIds pred;
    bool insert = false;

    void add_succ(const Op& op, OpIdCmpFunc cmp);

    void remove_succ(const Op& op);

    bool visible() const;

    usize incs() const {
        return is_counter() ? std::get<Counter>(std::get<ScalarValue>(action.data).data).increments : 0;
    }

    bool is_delete() const {
        return (action.tag == OpType::Delete);
    }

    bool is_inc() const {
        return (action.tag == OpType::Increment);
    }

    bool is_counter() const {
        return (action.tag == OpType::Put &&
            std::get<ScalarValue>(action.data).tag == ScalarValue::Counter);
    }

    bool is_noop(const OpType& action) const {
        return (this->action.tag == OpType::Put &&
            action.tag == OpType::Put &&
            std::get<ScalarValue>(this->action.data) == std::get<ScalarValue>(action.data));
    }

    bool is_list_op() const {
        return key.is_seq();
    }

    bool overwrites(const Op& other) const {
        for (auto& op : pred.v) {
            if (op == other.id)
                return true;
        }
        return false;
    }

    std::optional<ElemId> elemid() const {
        return elemid_or_key().elemid();
    }

    Key elemid_or_key() const {
        if (insert) {
            return { Key::Seq, id };
        }
        return key;
    }

    auto get_increment_value() const {
        return is_inc() ? std::optional<s64>(std::get<s64>(action.data)) : std::nullopt;
    }

    Value value() const;
};

typedef std::function<void(Op&)> OpFunc;
typedef std::function<int(const Op*)> OpCmpFunc;
