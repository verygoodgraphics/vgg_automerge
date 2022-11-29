// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "Op.h"

u64 OpType::action_index() const {
    if (tag == OpType::Make) {
        auto& obj_type = std::get<ObjType>(data);
        switch (obj_type) {
        case ObjType::Map:
            return 0;
        case ObjType::List:
            return 2;
        case ObjType::Text:
            return 4;
        case ObjType::Table:
            return 6;
        }
    }

    if (tag == OpType::Put) {
        return 1;
    }

    if (tag == OpType::Delete) {
        return 3;
    }

    if (tag == OpType::Increment) {
        return 5;
    }

    return 0;
}

OpType OpType::from_index_and_value(u64 index, ScalarValue&& value) {
    switch (index) {
    case 0:
        return { OpType::Make, ObjType::Map };
    case 1:
        return { OpType::Put, std::move(value) };
    case 2:
        return { OpType::Make, ObjType::List };
    case 3:
        return { OpType::Delete, 0 };
    case 4:
        return { OpType::Make, ObjType::Text };
    case 5: {
        if (value.tag == ScalarValue::Int) {
            return { OpType::Increment, std::get<s64>(value.data) };
        }
        else if (value.tag == ScalarValue::Int) {
            return { OpType::Increment, (s64)std::get<u64>(value.data) };
        }
        else {
            throw std::runtime_error("error::InvalidOpType::NonNumericInc");
        }
    }
    case 6:
        return { OpType::Make, ObjType::Table };
    default:
        throw std::runtime_error("error::InvalidOpType::UnknownAction");
    }
}

/////////////////////////////////////////////////////////

std::optional<OpIds> OpIds::new_if_sorted(std::vector<OpId>&& opids, OpIdCmpFunc cmp) {
    auto last = opids.cbegin();
    if (last == opids.cend()) {
        return OpIds();
    }

    for (auto next = std::next(last); next != opids.cend(); ++next) {
        if (cmp(*last, *next) >= 0) {
            return {};
        }
    }

    return OpIds(std::move(opids));
}

void OpIds::add(OpId opid, OpIdCmpFunc cmp) {
    if (v.empty()) {
        v.push_back(std::move(opid));
        return;
    }

    for (auto iter = v.cbegin(); iter != v.cend(); ++iter) {
        auto res = cmp(*iter, opid);
        if (res >= 0) {
            if (res > 0) {
                v.insert(iter, std::move(opid));
            }
            return;
        }
    }

    v.push_back(std::move(opid));
}

/////////////////////////////////////////////////////////

void Op::add_succ(const Op& op, OpIdCmpFunc cmp) {
    succ.add(op.id, cmp);

    if (!is_counter() || !op.is_inc())
        return;

    std::get<Counter>(std::get<ScalarValue>(action.data).data).current +=
        std::get<s64>(op.action.data);
    ++(std::get<Counter>(std::get<ScalarValue>(action.data).data).increments);
}

void Op::remove_succ(const Op& op) {
    succ.v.erase(std::remove(succ.v.begin(), succ.v.end(), op.id), succ.v.end());

    if (!is_counter() || !op.is_inc())
        return;

    std::get<Counter>(std::get<ScalarValue>(action.data).data).current -=
        std::get<s64>(op.action.data);
    --(std::get<Counter>(std::get<ScalarValue>(action.data).data).increments);
}

bool Op::visible() const {
    if (is_inc())
        return false;
    if (is_counter())
        return (succ.v.size() <= incs());
    return succ.v.empty();
}

Value Op::value() const {
    if (action.tag == OpType::Make) {
        return Value{ Value::OBJECT, std::get<ObjType>(action.data) };
    }
    if (action.tag == OpType::Put) {
        return Value{ Value::SCALAR, std::get<ScalarValue>(action.data) };
    }
    throw std::runtime_error("cant convert op into a value");
}