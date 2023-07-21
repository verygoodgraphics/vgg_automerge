// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <numeric>

#include "Columnar.h"
#include "Change.h"
#include "leb128.h"

std::optional<OldObjectId> ObjIterator::next() {
    auto actor_next = actor.next();
    if (!actor_next.has_value()) {
        return {};
    }
    auto ctr_next = ctr.next();
    if (!ctr_next.has_value()) {
        return {};
    }

    if (!(*actor_next).has_value() || !(*ctr_next).has_value()) {
        return OldObjectId(true);
    };

    if (**actor_next >= actors->size()) {
        return {};
    }

    return OldObjectId(OldOpId(**ctr_next, (*actors)[**actor_next]));
}

DepsIterator::DepsIterator(const BinSlice& bytes, const std::unordered_map<u32, Range>& ops) {
    num = col_iter<RleDecoder<usize>>(bytes, ops, DOC_DEPS_NUM);
    dep = col_iter<DeltaDecoder>(bytes, ops, DOC_DEPS_INDEX);
}

std::optional<std::vector<usize>> DepsIterator::next() {
    auto num_next = num.next();
    if (!num_next.has_value() || !(*num_next).has_value()) {
        return {};
    }

    std::vector<usize> p;
    p.reserve(**num_next);
    for (usize i = 0; i < **num_next; ++i) {
        auto dep_next = dep.next();
        if (!dep_next.has_value() || !(*dep_next).has_value()) {
            return {};
        }

        p.push_back((usize)(**dep_next));
    }

    return p;
}

std::optional<OldKey> KeyIterator::next() {
    auto actor_next = actor.next();
    if (!actor_next.has_value()) {
        return {};
    }
    auto ctr_next = ctr.next();
    if (!ctr_next.has_value()) {
        return {};
    }
    auto str_next = str.next();
    if (!str_next.has_value()) {
        return {};
    }

    if (!(*actor_next).has_value() && !(*ctr_next).has_value() && (*str_next).has_value()) {
        return OldKey{ OldKey::MAP, **str_next };
    }

    if (!(*actor_next).has_value() && (*ctr_next).has_value() && !(*str_next).has_value() && **ctr_next == 0) {
        return OldKey::head();
    }

    if ((*actor_next).has_value() && (*ctr_next).has_value() && !(*str_next).has_value()) {
        if (**actor_next >= actors->size()) {
            return {};
        }
        return OldKey{ OldKey::SEQ, OldElementId(OldOpId(**ctr_next, (*actors)[**actor_next])) };
    }

    return {};
}

std::optional<ScalarValue> ValueIterator::next() {
    auto val_len_next = val_len.next();
    if (!val_len_next.has_value() || !(*val_len_next).has_value()) {
        return {};
    }
    auto val_type = **val_len_next;
    auto actor_next = actor.next();
    if (!actor_next.has_value()) {
        return {};
    }
    auto ctr_next = ctr.next();
    if (!ctr_next.has_value()) {
        return {};
    }

    if (val_type == VALUE_TYPE_NULL) {
        return ScalarValue{ ScalarValue::Null, {} };
    }

    if (val_type == VALUE_TYPE_FALSE) {
        return ScalarValue{ ScalarValue::Boolean, false };
    }

    if (val_type == VALUE_TYPE_TRUE) {
        return ScalarValue{ ScalarValue::Boolean, true };
    }

    usize len = val_type >> 4;
    if ((val_type % 16 >= VALUE_TYPE_MIN_UNKNOWN) && (val_type % 16 <= VALUE_TYPE_MAX_UNKNOWN)) {
        auto data = val_raw.read_bytes(len);
        if (!data.has_value()) {
            return {};
        }
        throw std::runtime_error("unimplemented");
    }

    switch (val_type % 16) {
    case VALUE_TYPE_COUNTER: {
        auto val = val_raw.read<s64>();
        if (!val.has_value()) {
            return {};
        }
        if (len != val_raw.last_read) {
            return {};
        }
        return ScalarValue{ ScalarValue::Counter, Counter(*val) };
    }
    case VALUE_TYPE_TIMESTAMP: {
        auto val = val_raw.read<s64>();
        if (!val.has_value()) {
            return {};
        }
        if (len != val_raw.last_read) {
            return {};
        }
        return ScalarValue{ ScalarValue::Timestamp, *val };
    }
    case VALUE_TYPE_LEB128_UINT: {
        auto val = val_raw.read<u64>();
        if (!val.has_value()) {
            return {};
        }
        if (len != val_raw.last_read) {
            return {};
        }
        return ScalarValue{ ScalarValue::Uint, *val };
    }
    case VALUE_TYPE_LEB128_INT: {
        auto val = val_raw.read<s64>();
        if (!val.has_value()) {
            return {};
        }
        if (len != val_raw.last_read) {
            return {};
        }
        return ScalarValue{ ScalarValue::Int, *val };
    }
    case VALUE_TYPE_UTF8: {
        auto data = val_raw.read_bytes(len);
        if (!data.has_value()) {
            return {};
        }
        return ScalarValue{ ScalarValue::Str, std::string(data->first, data->first + data->second) };
    }
    case VALUE_TYPE_BYTES: {
        auto data = val_raw.read_bytes(len);
        if (!data.has_value()) {
            return {};
        }
        return ScalarValue{ ScalarValue::Bytes, std::vector<u8>(data->first, data->first + data->second) };
    }
    case VALUE_TYPE_IEEE754: {
        if (len == 8) {
            // confirm only 8 bytes read
            auto val = val_raw.read<double>();
            if (!val.has_value()) {
                return {};
            }
            return ScalarValue{ ScalarValue::F64, *val };
        }
        else {
            return {};
        }
    }
    default:
        return {};
    }
}

std::optional<std::vector<OldOpId>> PredIterator::next() {
    auto num = pred_num.next();
    if (!num.has_value() || !(*num).has_value()) {
        return {};
    }

    std::vector<OldOpId> p;
    p.reserve(**num);
    for (usize i = 0; i < **num; i++) {
        auto actor = pred_actor.next();
        if (!actor.has_value() || !(*actor).has_value()) {
            return {};
        }
        auto ctr = pred_ctr.next();
        if (!ctr.has_value() || !(*ctr).has_value()) {
            return {};
        }
        if (**actor >= actors->size()) {
            return {};
        }

        p.emplace_back(**ctr, (*actors)[**actor]);
    }

    std::sort(p.begin(), p.end());

    return p;
}

std::optional<std::vector<OpId>> SuccIterator::next() {
    auto num = succ_num.next();
    if (!num.has_value() || !(*num).has_value()) {
        return {};
    }

    std::vector<OpId> p;
    p.reserve(**num);
    for (usize i = 0; i < **num; i++) {
        auto actor = succ_actor.next();
        if (!actor.has_value() || !(*actor).has_value()) {
            return {};
        }
        auto ctr = succ_ctr.next();
        if (!ctr.has_value() || !(*ctr).has_value()) {
            return {};
        }

        p.push_back({ **ctr, **actor });
    }

    return p;
}

std::optional<std::vector<u8>> ExtraIterator::next() {
    auto v = len.next();
    if (!v.has_value() || !(*v).has_value()) {
        return {};
    }

    usize len = **v >> 4;
    auto bytes = extra.read_bytes(len);
    if (!bytes.has_value()) {
        return {};
    }

    return std::vector<u8>(bytes->first, bytes->first + bytes->second);
}

/////////////////////////////////////////////////////////

OperationIterator::OperationIterator(const BinSlice& bytes, const std::vector<ActorId>& actors,
    const std::unordered_map<u32, Range>& ops) {
    objs = ObjIterator{
        &actors,
        col_iter<RleDecoder<usize>>(bytes, ops, COL_OBJ_ACTOR),
        col_iter<RleDecoder<u64>>(bytes, ops, COL_OBJ_CTR)
    };

    keys = KeyIterator{
        &actors,
        col_iter<RleDecoder<usize>>(bytes, ops, COL_KEY_ACTOR),
        col_iter<DeltaDecoder>(bytes, ops, COL_KEY_CTR),
        col_iter<RleDecoder<std::string_view>>(bytes, ops, COL_KEY_STR)
    };

    value = ValueIterator{
        &actors,
        col_iter<RleDecoder<usize>>(bytes, ops, COL_VAL_LEN),
        col_iter<Decoder>(bytes, ops, COL_VAL_RAW),
        col_iter<RleDecoder<usize>>(bytes, ops, COL_REF_ACTOR),
        col_iter<RleDecoder<u64>>(bytes, ops, COL_REF_CTR)
    };

    pred = PredIterator{
        &actors,
        col_iter<RleDecoder<usize>>(bytes, ops, COL_PRED_NUM),
        col_iter<RleDecoder<usize>>(bytes, ops, COL_PRED_ACTOR),
        col_iter<DeltaDecoder>(bytes, ops, COL_PRED_CTR)
    };

    insert = col_iter<BooleanDecoder>(bytes, ops, COL_INSERT);

    action = col_iter<RleDecoder<Action>>(bytes, ops, COL_ACTION);
}

std::optional<OldOp> OperationIterator::next() {
    auto action_next = action.next();
    if (!action_next.has_value() || !(*action_next).has_value()) {
        return {};
    }
    auto insert_next = insert.next();
    if (!insert_next.has_value()) {
        return {};
    }
    auto obj = objs.next();
    if (!obj.has_value()) {
        return {};
    }
    auto key = keys.next();
    if (!key.has_value()) {
        return {};
    }
    auto pred_next = pred.next();
    if (!pred_next.has_value()) {
        return {};
    }
    auto value_next = value.next();
    if (!value_next.has_value()) {
        return {};
    }

    OpType act;
    switch (**action_next) {
    case Action::Set:
        act = { OpType::Put, std::move(*value_next) };
        break;
    case Action::MakeList:
        act = { OpType::Make, ObjType::List };
        break;
    case Action::MakeText:
        act = { OpType::Make, ObjType::Text };
        break;
    case Action::MakeMap:
        act = { OpType::Make, ObjType::Map };
        break;
    case Action::MakeTable:
        act = { OpType::Make, ObjType::Table };
        break;
    case Action::Del:
        act = { OpType::Delete };
        break;
    case Action::Inc: {
        auto num = value_next->to_s64();
        if (!num.has_value()) {
            return {};
        }
        act = { OpType::Increment, *num };
        break;
    }
    default:
        break;
    }

    return OldOp(
        std::move(act),
        std::move(*obj),
        std::move(*key),
        std::move(*pred_next),
        *insert_next
    );
}

usize OperationIterator::count() {
    usize res = 0;
    while (next()) {
        ++res;
    }

    return res;
}

DocOpIterator::DocOpIterator(const BinSlice& bytes, const std::vector<ActorId>& actors,
    const std::unordered_map<u32, Range>& ops) {
    actor = col_iter<RleDecoder<usize>>(bytes, ops, COL_ID_ACTOR);

    ctr = col_iter<DeltaDecoder>(bytes, ops, COL_ID_CTR);

    objs = ObjIterator{
        &actors,
        col_iter<RleDecoder<usize>>(bytes, ops, COL_OBJ_ACTOR),
        col_iter<RleDecoder<u64>>(bytes, ops, COL_OBJ_CTR)
    };

    keys = KeyIterator{
        &actors,
        col_iter<RleDecoder<usize>>(bytes, ops, COL_KEY_ACTOR),
        col_iter<DeltaDecoder>(bytes, ops, COL_KEY_CTR),
        col_iter<RleDecoder<std::string_view>>(bytes, ops, COL_KEY_STR)
    };

    value = ValueIterator{
        &actors,
        col_iter<RleDecoder<usize>>(bytes, ops, COL_VAL_LEN),
        col_iter<Decoder>(bytes, ops, COL_VAL_RAW),
        col_iter<RleDecoder<usize>>(bytes, ops, COL_REF_ACTOR),
        col_iter<RleDecoder<u64>>(bytes, ops, COL_REF_CTR)
    };

    succ = SuccIterator{
        col_iter<RleDecoder<usize>>(bytes, ops, COL_SUCC_NUM),
        col_iter<RleDecoder<usize>>(bytes, ops, COL_SUCC_ACTOR),
        col_iter<DeltaDecoder>(bytes, ops, COL_SUCC_CTR)
    };

    insert = col_iter<BooleanDecoder>(bytes, ops, COL_INSERT);

    action = col_iter<RleDecoder<Action>>(bytes, ops, COL_ACTION);
}

std::optional<DocOp> DocOpIterator::next() {
    auto action_next = action.next();
    if (!action_next.has_value() || !(*action_next).has_value()) {
        return {};
    }
    auto actor_next = actor.next();
    if (!actor_next.has_value() || !(*actor_next).has_value()) {
        return {};
    }
    auto ctr_next = ctr.next();
    if (!ctr_next.has_value() || !(*ctr_next).has_value()) {
        return {};
    }
    auto insert_next = insert.next();
    if (!insert_next.has_value()) {
        return {};
    }
    auto obj = objs.next();
    if (!obj.has_value()) {
        return {};
    }
    auto key = keys.next();
    if (!key.has_value()) {
        return {};
    }
    auto succ_next = succ.next();
    if (!succ_next.has_value()) {
        return {};
    }
    auto value_next = value.next();
    if (!value_next.has_value()) {
        return {};
    }

    OpType act;
    switch (**action_next) {
    case Action::Set:
        act = { OpType::Put, std::move(*value_next) };
        break;
    case Action::MakeList:
        act = { OpType::Make, ObjType::List };
        break;
    case Action::MakeText:
        act = { OpType::Make, ObjType::Text };
        break;
    case Action::MakeMap:
        act = { OpType::Make, ObjType::Map };
        break;
    case Action::MakeTable:
        act = { OpType::Make, ObjType::Table };
        break;
    case Action::Del:
        act = { OpType::Delete };
        break;
    case Action::Inc: {
        auto num = value_next->to_s64();
        if (!num.has_value()) {
            return {};
        }
        act = { OpType::Increment, *num };
        break;
    }
    default:
        break;
    }

    return DocOp{
        **actor_next,
        **ctr_next,
        std::move(act),
        std::move(*obj),
        std::move(*key),
        std::move(*succ_next),
        {},
        *insert_next
    };
}

std::vector<DocOp> DocOpIterator::collect() {
    std::vector<DocOp> res;
    std::optional<DocOp> item;
    while ((item = next())) {
        res.push_back(std::move(*item));
    }

    return res;
}

ChangeIterator::ChangeIterator(const BinSlice& bytes, const std::unordered_map<u32, Range>& ops) {
    actor = col_iter<RleDecoder<usize>>(bytes, ops, DOC_ACTOR);
    seq = col_iter<DeltaDecoder>(bytes, ops, DOC_SEQ);
    max_op = col_iter<DeltaDecoder>(bytes, ops, DOC_MAX_OP);
    time = col_iter<DeltaDecoder>(bytes, ops, DOC_TIME);
    message = col_iter<RleDecoder<std::string>>(bytes, ops, DOC_MESSAGE);
    extra = {
        col_iter<RleDecoder<usize>>(bytes, ops, DOC_EXTRA_LEN),
        col_iter<Decoder>(bytes, ops, DOC_EXTRA_RAW)
    };
}

std::optional<DocChange> ChangeIterator::next() {
    auto actor_next = actor.next();
    if (!actor_next.has_value() || !(*actor_next).has_value()) {
        return {};
    }
    auto seq_next = seq.next();
    if (!seq_next.has_value() || !(*seq_next).has_value()) {
        return {};
    }
    auto max_op_next = max_op.next();
    if (!max_op_next.has_value() || !(*max_op_next).has_value()) {
        return {};
    }
    auto time_next = time.next();
    if (!time_next.has_value() || !(*time_next).has_value()) {
        return {};
    }
    auto message_next = message.next();
    if (!message_next.has_value()) {
        return {};
    }
    auto extra_next = extra.next();

    return DocChange{
        **actor_next,
        **seq_next,
        **max_op_next,
        (s64) * *time_next,
        std::move(*message_next),
        extra_next.value_or(std::vector<u8>()),
        {}
    };
}

std::vector<DocChange> ChangeIterator::collect() {
    std::vector<DocChange> res;
    std::optional<DocChange> item;
    while ((item = next())) {
        res.push_back(std::move(*item));
    }

    return res;
}

/////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////

void ValEncoder::append_value(const ScalarValue& val, const std::vector<usize>& actors) {
    // It may seem weird to have two consecutive matches on the same value. The reason is so
    // that we don't have to repeat the `append_null` calls on ref_actor and ref_counter in
    // every arm of the next match
    ref_actor.append_null();
    ref_counter.append_null();
    switch (val.tag) {
    case ScalarValue::Null: {
        len.append_value(u64(VALUE_TYPE_NULL));
        break;
    }
    case ScalarValue::Boolean: {
        if (std::get<bool>(val.data)) {
            len.append_value(u64(VALUE_TYPE_TRUE));
        }
        else {
            len.append_value(u64(VALUE_TYPE_FALSE));
        }
        break;
    }
    case ScalarValue::Bytes: {
        auto& bytes = std::get<std::vector<u8>>(val.data);
        vector_extend(raw, bytes);
        len.append_value((bytes.size() << 4) | VALUE_TYPE_BYTES);
        break;
    }
    case ScalarValue::Str: {
        auto& str = std::get<std::string>(val.data);
        raw.insert(raw.end(), str.begin(), str.end());
        len.append_value((str.size() << 4) | VALUE_TYPE_UTF8);
        break;
    }
    case ScalarValue::Counter: {
        auto& count = std::get<Counter>(val.data);
        len.append_value((encoder.encode(count.start) << 4) | VALUE_TYPE_COUNTER);
        break;
    }
    case ScalarValue::Timestamp: {
        auto time = std::get<s64>(val.data);
        len.append_value((encoder.encode(time) << 4) | VALUE_TYPE_TIMESTAMP);
        break;
    }
    case ScalarValue::Int: {
        auto n = std::get<s64>(val.data);
        len.append_value((encoder.encode(n) << 4) | VALUE_TYPE_LEB128_INT);
        break;
    }
    case ScalarValue::Uint: {
        auto n = std::get<u64>(val.data);
        len.append_value((encoder.encode(n) << 4) | VALUE_TYPE_LEB128_UINT);
        break;
    }
    case ScalarValue::F64: {
        auto n = std::get<double>(val.data);
        len.append_value((encoder.encode(n) << 4) | VALUE_TYPE_IEEE754);
        break;
    }
    default:
        break;
    }
}

void ValEncoder::append_value2(const ScalarValue& val, const std::vector<ActorId>& actors) {
    // It may seem weird to have two consecutive matches on the same value. The reason is so
    // that we don't have to repeat the `append_null` calls on ref_actor and ref_counter in
    // every arm of the next match
    ref_actor.append_null();
    ref_counter.append_null();
    switch (val.tag) {
    case ScalarValue::Null: {
        len.append_value(u64(VALUE_TYPE_NULL));
        break;
    }
    case ScalarValue::Boolean: {
        if (std::get<bool>(val.data)) {
            len.append_value(u64(VALUE_TYPE_TRUE));
        }
        else {
            len.append_value(u64(VALUE_TYPE_FALSE));
        }
        break;
    }
    case ScalarValue::Bytes: {
        auto& bytes = std::get<std::vector<u8>>(val.data);
        vector_extend(raw, bytes);
        len.append_value((bytes.size() << 4) | VALUE_TYPE_BYTES);
        break;
    }
    case ScalarValue::Str: {
        auto& str = std::get<std::string>(val.data);
        raw.insert(raw.end(), str.begin(), str.end());
        len.append_value((str.size() << 4) | VALUE_TYPE_UTF8);
        break;
    }
    case ScalarValue::Counter: {
        auto& count = std::get<Counter>(val.data);
        len.append_value((encoder.encode(count.start) << 4) | VALUE_TYPE_COUNTER);
        break;
    }
    case ScalarValue::Timestamp: {
        auto time = std::get<s64>(val.data);
        len.append_value((encoder.encode(time) << 4) | VALUE_TYPE_TIMESTAMP);
        break;
    }
    case ScalarValue::Int: {
        auto n = std::get<s64>(val.data);
        len.append_value((encoder.encode(n) << 4) | VALUE_TYPE_LEB128_INT);
        break;
    }
    case ScalarValue::Uint: {
        auto n = std::get<u64>(val.data);
        len.append_value((encoder.encode(n) << 4) | VALUE_TYPE_LEB128_UINT);
        break;
    }
    case ScalarValue::F64: {
        auto n = std::get<double>(val.data);
        len.append_value((encoder.encode(n) << 4) | VALUE_TYPE_IEEE754);
        break;
    }
    default:
        break;
    }
}

void ValEncoder::append_null() {
    ref_counter.append_null();
    ref_actor.append_null();
    len.append_value(u64(VALUE_TYPE_NULL));
}

std::vector<ColData> ValEncoder::finish() {
    return {
        ref_counter.finish(COL_REF_CTR),
        ref_actor.finish(COL_REF_ACTOR),
        len.finish(COL_VAL_LEN),
        ColData{ COL_VAL_RAW, std::move(raw), false }
    };
}

/////////////////////////////////////////////////////////

void KeyEncoder::append(Key&& key, const std::vector<usize>& actors, const std::vector<std::string_view>& props) {
    if (key.is_map()) {
        actor.append_null();
        ctr.append_null();
        str.append_value(std::string_view(props[std::get<usize>(key.data)]));

        return;
    }

    auto& elem = std::get<ElemId>(key.data);
    if (elem == HEAD) {
        actor.append_null();
        ctr.append_value(0);
        str.append_null();

        return;
    }

    usize act = actors[elem.actor];
    actor.append_value(std::move(act));
    ctr.append_value(elem.counter);
    str.append_null();
}

std::vector<ColData> KeyEncoder::finish() {
    return {
        actor.finish(COL_KEY_ACTOR),
        ctr.finish(COL_KEY_CTR),
        str.finish(COL_KEY_STR)
    };
}

void KeyEncoderOld::append(OldKey&& key, const std::vector<ActorId>& actors) {
    if (key.is_map_key()) {
        actor.append_null();
        ctr.append_null();
        str.append_value(std::move(std::get<std::string_view>(key.data)));

        return;
    }

    auto& elem = std::get<OldElementId>(key.data);
    if (elem.isHead) {
        actor.append_null();
        ctr.append_value(0);
        str.append_null();

        return;
    }

    actor.append_value(elem.id.actor.actor_index(actors));
    ctr.append_value(elem.id.counter);
    str.append_null();
}

std::vector<ColData> KeyEncoderOld::finish() {
    return {
        actor.finish(COL_KEY_ACTOR),
        ctr.finish(COL_KEY_CTR),
        str.finish(COL_KEY_STR)
    };
}

/////////////////////////////////////////////////////////

void SuccEncoder::append(const std::vector<OpId>& succ, const std::vector<usize>& actors) {
    num.append_value(succ.size());
    for (auto& s : succ) {
        ctr.append_value(s.counter);
        usize actor_index = actors[s.actor];
        actor.append_value(std::move(actor_index));
    }
}

void SuccEncoder::append_old(const std::vector<OpId>& succ) {
    num.append_value(succ.size());

    for (auto& s : succ) {
        ctr.append_value(s.counter);
        usize actor_index = s.actor;
        actor.append_value(std::move(actor_index));
    }
}

std::vector<ColData> SuccEncoder::finish() {
    return {
        num.finish(COL_SUCC_NUM),
        actor.finish(COL_SUCC_ACTOR),
        ctr.finish(COL_SUCC_CTR)
    };
}

void PredEncoder::append(const std::vector<OldOpId>& pred/* sorted */, const std::vector<ActorId>& actors) {
    num.append_value(pred.size());
    for (auto& p : pred) {
        ctr.append_value(p.counter);
        actor.append_value(p.actor.actor_index(actors));
    }
}

std::vector<ColData> PredEncoder::finish() {
    return {
        num.finish(COL_PRED_NUM),
        actor.finish(COL_PRED_ACTOR),
        ctr.finish(COL_PRED_CTR)
    };
}

/////////////////////////////////////////////////////////

void ObjEncoder::append(const ObjId& obj, const std::vector<usize>& actors) {
    if (obj.counter == 0) {
        actor.append_null();
        ctr.append_null();
    }
    else {
        usize act = actors[obj.actor];
        actor.append_value(std::move(act));
        u64 c = obj.counter;
        ctr.append_value(std::move(c));
    }
}

std::vector<ColData> ObjEncoder::finish() {
    return {
        actor.finish(COL_OBJ_ACTOR),
        ctr.finish(COL_OBJ_CTR)
    };
}

void ObjEncoderOld::append(const OldObjectId& obj, const std::vector<ActorId>& actors) {
    if (obj.isRoot) {
        actor.append_null();
        ctr.append_null();
    }
    else {
        actor.append_value(obj.id.actor.actor_index(actors));
        usize counter = obj.id.counter;
        ctr.append_value(std::move(counter));
    }
}

std::vector<ColData> ObjEncoderOld::finish() {
    return {
        actor.finish(COL_OBJ_ACTOR),
        ctr.finish(COL_OBJ_CTR)
    };
}

/////////////////////////////////////////////////////////

void ChangeEncoder::encode(const std::vector<Change>& changes, const IndexedCache<ActorId>& actors) {
    std::unordered_map<ChangeHash, usize> index_by_hash;
    for (usize index = 0; index < changes.size(); ++index) {
        auto& change = changes[index];
        index_by_hash.insert({ change.hash, index });
        actor.append_value(actors.lookup(change.actor_id()).value());
        seq.append_value(change.seq);
        max_op.append_value(change.start_op + change.iter_ops().count() - 1);
        time.append_value((u64)change.time);
        message.append_value(change.get_message());
        deps_num.append_value(change.deps.size());

        for (auto& dep : change.deps) {
            auto dep_index = index_by_hash.find(dep);
            if (dep_index != index_by_hash.end()) {
                deps_index.append_value(dep_index->second);
            }
            else {
                throw std::runtime_error("Missing dependency for hash");
            }
        }

        auto change_extra_bytes = change.get_extra_bytes();
        extra_len.append_value(change_extra_bytes.second << 4 | VALUE_TYPE_BYTES);
        extra_raw.insert(extra_raw.end(), change_extra_bytes.first,
            change_extra_bytes.first + change_extra_bytes.second);
    }
}

std::pair<std::vector<u8>, std::vector<u8>> ChangeEncoder::finish() {
    std::vector<ColData> coldata{
        actor.finish(DOC_ACTOR),
        seq.finish(DOC_SEQ),
        max_op.finish(DOC_MAX_OP),
        time.finish(DOC_TIME),
        message.finish(DOC_MESSAGE),
        deps_num.finish(DOC_DEPS_NUM),
        deps_index.finish(DOC_DEPS_INDEX),
        extra_len.finish(DOC_EXTRA_LEN),
        ColData{ DOC_EXTRA_RAW, std::move(extra_raw), false }
    };
    std::sort(coldata.begin(), coldata.end());

    std::vector<u8> data;
    std::vector<u8> info;
    Encoder encoder(info);

    usize non_empty_data_count = std::count_if(coldata.begin(), coldata.end(), [](const ColData& d) {
        return !(d.data.empty());
        });
    encoder.encode(non_empty_data_count);

    usize len = 0;
    for (auto& d : coldata) {
        d.deflate();
        d.encode_col_len(encoder);

        len += d.data.size();
    }

    data.reserve(len);
    for (auto& d : coldata) {
        vector_extend(data, std::move(d.data));
    }

    return { std::move(data), std::move(info) };
}

/////////////////////////////////////////////////////////

void DocOpEncoder::encode(OpSetIter& ops, const std::vector<usize>& actors, const std::vector<std::string_view>& props) {
    while (true) {
        auto ops_next = ops.next();
        if (!ops_next.has_value()) {
            break;
        }
        auto obj = ops_next->first;
        auto op = ops_next->second;

        usize actor_index = actors[op->id.actor];
        actor.append_value(std::move(actor_index));
        ctr.append_value(op->id.counter);
        this->obj.append(*obj, actors);
        key.append(Key(op->key), actors, props);
        insert.append(op->insert);
        succ.append(op->succ.v, actors);

        Action action;
        auto& op_action = op->action;
        switch (op_action.tag) {
        case OpType::Put:
            val.append_value(std::get<ScalarValue>(op_action.data), actors);
            action = Action::Set;
            break;
        case OpType::Increment:
            val.append_value(ScalarValue{ ScalarValue::Int, std::get<s64>(op_action.data) }, actors);
            action = Action::Inc;
            break;
        case OpType::Delete:
            val.append_null();
            action = Action::Del;
            break;
        case OpType::Make:
            val.append_null();
            switch (std::get<ObjType>(op_action.data)) {
            case ObjType::Map:
                action = Action::MakeMap;
                break;
            case ObjType::Table:
                action = Action::MakeTable;
                break;
            case ObjType::List:
                action = Action::MakeList;
                break;
            case ObjType::Text:
                action = Action::MakeText;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
        this->action.append_value(std::move(action));
    }
}

std::pair<std::vector<u8>, std::vector<u8>> DocOpEncoder::finish() {
    std::vector<ColData> coldata{
        actor.finish(COL_ID_ACTOR),
        ctr.finish(COL_ID_CTR),
        insert.finish(COL_INSERT),
        action.finish(COL_ACTION)
    };
    vector_extend(coldata, obj.finish());
    vector_extend(coldata, key.finish());
    vector_extend(coldata, val.finish());
    vector_extend(coldata, succ.finish());
    std::sort(coldata.begin(), coldata.end());

    std::vector<u8> info;
    std::vector<u8> data;
    Encoder encoder(info);

    usize non_empty_data_count = std::count_if(coldata.begin(), coldata.end(), [](const ColData& d) {
        return !(d.data.empty());
        });
    encoder.encode(non_empty_data_count);

    usize len = 0;
    for (auto& d : coldata) {
        d.deflate();
        d.encode_col_len(encoder);

        len += d.data.size();
    }

    data.reserve(len);
    for (auto& d : coldata) {
        vector_extend(data, std::move(d.data));
    }

    return { std::move(data), std::move(info) };
}

void ColumnEncoder::append(OldOp&& op, const std::vector<ActorId>& actors) {
    obj.append(op.obj, actors);
    key.append(std::move(op.key), actors);
    insert.append(op.insert);

    pred.append(op.pred, actors);
    Action action = Action::MakeMap;
    switch (op.action.tag) {
    case OpType::Put:
        val.append_value2(std::get<ScalarValue>(op.action.data), actors);
        action = Action::Set;
        break;
    case OpType::Increment:
        val.append_value2(ScalarValue{ ScalarValue::Int, std::get<s64>(op.action.data) }, actors);
        action = Action::Inc;
        break;
    case OpType::Delete:
        val.append_null();
        action = Action::Del;
        break;
    case OpType::Make:
        val.append_null();
        switch (std::get<ObjType>(op.action.data)) {
        case ObjType::Map:
            action = Action::MakeMap;
            break;
        case ObjType::Table:
            action = Action::MakeTable;
            break;
        case ObjType::List:
            action = Action::MakeList;
            break;
        case ObjType::Text:
            action = Action::MakeText;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    this->action.append_value(std::move(action));
}

auto ColumnEncoder::finish()->std::pair<std::vector<u8>, std::unordered_map<u32, Range>> {
    std::vector<ColData> coldata;
    coldata.reserve(2 + obj.COLUMNS + key.COLUMNS + val.COLUMNS + pred.COLUMNS);
    coldata.push_back(insert.finish(COL_INSERT));
    coldata.push_back(action.finish(COL_ACTION));
    vector_extend(coldata, obj.finish());
    vector_extend(coldata, key.finish());
    vector_extend(coldata, val.finish());
    vector_extend(coldata, pred.finish());
    std::sort(coldata.begin(), coldata.end());

    usize non_empty_column_count = std::count_if(coldata.begin(), coldata.end(), [](const ColData& d) {
        return !(d.data.empty());
        });
    usize data_len = std::accumulate(coldata.begin(), coldata.end(), usize(0), [](usize sum, const ColData& d) {
        return sum + d.data.size();
        });
    // 1 for the non_empty_column_count, 2 for each non_empty column (encode_col_len), data_len
    //   for all the actual data
    std::vector<u8> data;
    data.reserve(1 + non_empty_column_count * 2 + data_len);
    Encoder encoder(data);

    encoder.encode(non_empty_column_count);
    for (auto& d : coldata) {
        d.encode_col_len(encoder);
    }

    std::unordered_map<u32, Range> rangemap;
    rangemap.reserve(non_empty_column_count);
    for (auto& d : coldata) {
        if (!d.data.empty()) {
            usize begin = data.size();
            vector_extend(data, std::move(d.data));
            rangemap.insert({ d.col, { begin, data.size() } });
        }
    }

    return { std::move(data), std::move(rangemap) };
}
