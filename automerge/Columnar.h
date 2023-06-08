// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include "type.h"
#include "Encoder.h"
#include "Decoder.h"
#include "legacy.h"
#include "helper.h"
#include "OpSet.h"

const usize VALUE_TYPE_NULL = 0;
const usize VALUE_TYPE_FALSE = 1;
const usize VALUE_TYPE_TRUE = 2;
const usize VALUE_TYPE_LEB128_UINT = 3;
const usize VALUE_TYPE_LEB128_INT = 4;
const usize VALUE_TYPE_IEEE754 = 5;
const usize VALUE_TYPE_UTF8 = 6;
const usize VALUE_TYPE_BYTES = 7;
const usize VALUE_TYPE_COUNTER = 8;
const usize VALUE_TYPE_TIMESTAMP = 9;
const usize VALUE_TYPE_CURSOR = 10;
const usize VALUE_TYPE_MIN_UNKNOWN = 11;
const usize VALUE_TYPE_MAX_UNKNOWN = 15;

const u32 COLUMN_TYPE_GROUP_CARD = 0;
const u32 COLUMN_TYPE_ACTOR_ID = 1;
const u32 COLUMN_TYPE_INT_RLE = 2;
const u32 COLUMN_TYPE_INT_DELTA = 3;
const u32 COLUMN_TYPE_BOOLEAN = 4;
const u32 COLUMN_TYPE_STRING_RLE = 5;
const u32 COLUMN_TYPE_VALUE_LEN = 6;
const u32 COLUMN_TYPE_VALUE_RAW = 7;

const u32 COL_OBJ_ACTOR = COLUMN_TYPE_ACTOR_ID;
const u32 COL_OBJ_CTR = COLUMN_TYPE_INT_RLE;
const u32 COL_KEY_ACTOR = 1 << 4 | COLUMN_TYPE_ACTOR_ID;
const u32 COL_KEY_CTR = 1 << 4 | COLUMN_TYPE_INT_DELTA;
const u32 COL_KEY_STR = 1 << 4 | COLUMN_TYPE_STRING_RLE;
const u32 COL_ID_ACTOR = 2 << 4 | COLUMN_TYPE_ACTOR_ID;
const u32 COL_ID_CTR = 2 << 4 | COLUMN_TYPE_INT_DELTA;
const u32 COL_INSERT = 3 << 4 | COLUMN_TYPE_BOOLEAN;
const u32 COL_ACTION = 4 << 4 | COLUMN_TYPE_INT_RLE;
const u32 COL_VAL_LEN = 5 << 4 | COLUMN_TYPE_VALUE_LEN;
const u32 COL_VAL_RAW = 5 << 4 | COLUMN_TYPE_VALUE_RAW;
const u32 COL_PRED_NUM = 7 << 4 | COLUMN_TYPE_GROUP_CARD;
const u32 COL_PRED_ACTOR = 7 << 4 | COLUMN_TYPE_ACTOR_ID;
const u32 COL_PRED_CTR = 7 << 4 | COLUMN_TYPE_INT_DELTA;
const u32 COL_SUCC_NUM = 8 << 4 | COLUMN_TYPE_GROUP_CARD;
const u32 COL_SUCC_ACTOR = 8 << 4 | COLUMN_TYPE_ACTOR_ID;
const u32 COL_SUCC_CTR = 8 << 4 | COLUMN_TYPE_INT_DELTA;
const u32 COL_REF_CTR = 6 << 4 | COLUMN_TYPE_INT_RLE;
const u32 COL_REF_ACTOR = 6 << 4 | COLUMN_TYPE_ACTOR_ID;

const u32 DOC_ACTOR = /* 0 << 4 */ COLUMN_TYPE_ACTOR_ID;
const u32 DOC_SEQ = /* 0 << 4 */ COLUMN_TYPE_INT_DELTA;
const u32 DOC_MAX_OP = 1 << 4 | COLUMN_TYPE_INT_DELTA;
const u32 DOC_TIME = 2 << 4 | COLUMN_TYPE_INT_DELTA;
const u32 DOC_MESSAGE = 3 << 4 | COLUMN_TYPE_STRING_RLE;
const u32 DOC_DEPS_NUM = 4 << 4 | COLUMN_TYPE_GROUP_CARD;
const u32 DOC_DEPS_INDEX = 4 << 4 | COLUMN_TYPE_INT_DELTA;
const u32 DOC_EXTRA_LEN = 5 << 4 | COLUMN_TYPE_VALUE_LEN;
const u32 DOC_EXTRA_RAW = 5 << 4 | COLUMN_TYPE_VALUE_RAW;

struct ObjIterator {
    const std::vector<ActorId>* actors = nullptr;
    RleDecoder<usize> actor = {};
    RleDecoder<u64> ctr = {};

    std::optional<OldObjectId> next();
};

struct DepsIterator {
    RleDecoder<usize> num = {};
    DeltaDecoder dep = {};

    DepsIterator() = delete;
    DepsIterator(const BinSlice& bytes, const std::unordered_map<u32, Range>& ops);

    std::optional<std::vector<usize>> next();
};

struct KeyIterator {
    const std::vector<ActorId>* actors = nullptr;
    RleDecoder<usize> actor = {};
    DeltaDecoder ctr = {};
    RleDecoder<std::string> str = {};

    std::optional<OldKey> next();
};

struct ValueIterator {
    const std::vector<ActorId>* actors = nullptr;
    RleDecoder<usize> val_len = {};
    Decoder val_raw = {};
    RleDecoder<usize> actor = {};
    RleDecoder<u64> ctr = {};

    std::optional<ScalarValue> next();
};

struct PredIterator {
    const std::vector<ActorId>* actors = nullptr;
    RleDecoder<usize> pred_num = {};
    RleDecoder<usize> pred_actor = {};
    DeltaDecoder pred_ctr = {};

    // sorted
    std::optional<std::vector<OldOpId>> next();
};

struct SuccIterator {
    RleDecoder<usize> succ_num = {};
    RleDecoder<usize> succ_actor = {};
    DeltaDecoder succ_ctr = {};

    std::optional<std::vector<OpId>> next();
};

struct ExtraIterator {
    RleDecoder<usize> len = {};
    Decoder extra = {};

    std::optional<std::vector<u8>> next();
};

struct OperationIterator {
    RleDecoder<Action> action;
    ObjIterator objs;
    KeyIterator keys;
    BooleanDecoder insert;
    ValueIterator value;
    PredIterator pred;

    OperationIterator() = delete;
    OperationIterator(const BinSlice& bytes, const std::vector<ActorId>& actors,
        const std::unordered_map<u32, Range>& ops);

    std::optional<OldOp> next();

    usize count();
};

struct DocOp {
    usize actor = 0;
    u64 ctr = 0;
    OpType action = {};
    OldObjectId obj = {};
    OldKey key = {};
    std::vector<OpId> succ = {};
    std::vector<OpId> pred = {};
    bool insert = false;

    bool operator<(const DocOp& other) const {
        return ctr < other.ctr;
    }
};

struct DocChange {
    usize actor = 0;
    u64 seq = 0;
    u64 max_op = 0;
    s64 time = 0;
    std::optional<std::string> message = {};
    std::vector<u8> extra_bytes = {};
    std::vector<DocOp> ops = {};
};

struct DocOpIterator {
    RleDecoder<usize> actor;
    DeltaDecoder ctr;
    RleDecoder<Action> action;
    ObjIterator objs;
    KeyIterator keys;
    BooleanDecoder insert;
    ValueIterator value;
    SuccIterator succ;

    DocOpIterator() = delete;
    DocOpIterator(const BinSlice& bytes, const std::vector<ActorId>& actors,
        const std::unordered_map<u32, Range>& ops);

    std::optional<DocOp> next();

    std::vector<DocOp> collect();
};

struct ChangeIterator {
    RleDecoder<usize> actor;
    DeltaDecoder seq;
    DeltaDecoder max_op;
    DeltaDecoder time;
    RleDecoder<std::string> message;
    ExtraIterator extra;

    ChangeIterator() = delete;
    ChangeIterator(const BinSlice& bytes, const std::unordered_map<u32, Range>& ops);

    std::optional<DocChange> next();

    std::vector<DocChange> collect();
};

struct ValEncoder {
    RleEncoder<usize> len = {};
    RleEncoder<usize> ref_actor = {};
    RleEncoder<u64> ref_counter = {};
    std::vector<u8> raw = {};
    Encoder encoder = Encoder(raw);

    const usize COLUMNS = 4;

    void append_value(const ScalarValue& val, const std::vector<usize>& actors);

    void append_value2(const ScalarValue& val, const std::vector<ActorId>& actors);

    void append_null();

    std::vector<ColData> finish();
};

struct KeyEncoder {
    RleEncoder<usize> actor = {};
    DeltaEncoder ctr = {};
    RleEncoder<std::string> str = {};

    const usize COLUMNS = 3;

    void append(Key&& key, const std::vector<usize>& actors, const std::vector<std::string>& props);

    std::vector<ColData> finish();
};

struct KeyEncoderOld {
    RleEncoder<usize> actor = {};
    DeltaEncoder ctr = {};
    RleEncoder<std::string> str = {};

    const usize COLUMNS = 3;

    void append(OldKey&& key, const std::vector<ActorId>& actors);

    std::vector<ColData> finish();
};

struct SuccEncoder {
    RleEncoder<usize> num = {};
    RleEncoder<usize> actor = {};
    DeltaEncoder ctr = {};

    void append(const std::vector<OpId>& succ, const std::vector<usize>& actors);

    void append_old(const std::vector<OpId>& succ);

    std::vector<ColData> finish();
};

struct PredEncoder {
    RleEncoder<usize> num = {};
    RleEncoder<usize> actor = {};
    DeltaEncoder ctr = {};

    const usize COLUMNS = 3;

    void append(const std::vector<OldOpId>& pred/* sorted */, const std::vector<ActorId>& actors);

    std::vector<ColData> finish();
};

struct ObjEncoder {
    RleEncoder<usize> actor = {};
    RleEncoder<u64> ctr = {};

    const usize COLUMNS = 2;

    void append(const ObjId& obj, const std::vector<usize>& actors);

    std::vector<ColData> finish();
};

struct ObjEncoderOld {
    RleEncoder<usize> actor = {};
    RleEncoder<u64> ctr = {};

    const usize COLUMNS = 2;

    void append(const OldObjectId& obj, const std::vector<ActorId>& actors);

    std::vector<ColData> finish();
};

struct Change;

struct ChangeEncoder {
    RleEncoder<usize> actor = {};
    DeltaEncoder seq = {};
    DeltaEncoder max_op = {};
    DeltaEncoder time = {};
    RleEncoder<std::optional<std::string>> message = {};
    RleEncoder<usize> deps_num = {};
    DeltaEncoder deps_index = {};
    RleEncoder<usize> extra_len = {};
    std::vector<u8> extra_raw = {};

    static auto encode_changes(const std::vector<Change>& changes, const IndexedCache<ActorId>& actors)
        -> std::pair<std::vector<u8>, std::vector<u8>> {
        ChangeEncoder e;

        e.encode(changes, actors);
        return e.finish();
    }

    void encode(const std::vector<Change>& changes, const IndexedCache<ActorId>& actors);

    auto finish() -> std::pair<std::vector<u8>, std::vector<u8>>;
};

struct DocOpEncoder {
    RleEncoder<usize> actor = {};
    DeltaEncoder ctr = {};
    ObjEncoder obj = {};
    KeyEncoder key = {};
    BooleanEncoder insert = {};
    RleEncoder<Action> action = {};
    ValEncoder val = {};
    SuccEncoder succ = {};

    static auto encode_doc_ops(OpSetIter& ops, const std::vector<usize>& actors,
        const std::vector<std::string>& props) -> std::pair<std::vector<u8>, std::vector<u8>>
    {
        DocOpEncoder e;

        e.encode(ops, actors, props);
        return e.finish();
    }

    void encode(OpSetIter& ops, const std::vector<usize>& actors, const std::vector<std::string>& props);

    auto finish() -> std::pair<std::vector<u8>, std::vector<u8>>;
};

struct ColumnEncoder {
    ObjEncoderOld obj = {};
    KeyEncoderOld key = {};
    BooleanEncoder insert = {};
    RleEncoder<Action> action = {};
    ValEncoder val = {};
    PredEncoder pred = {};

    static auto encode_ops(std::vector<OldOp>&& ops, const std::vector<ActorId>& actors)
        -> std::pair<std::vector<u8>, std::unordered_map<u32, Range>>
    {
        ColumnEncoder e;

        e.encode(std::move(ops), actors);
        return e.finish();
    }

    void encode(std::vector<OldOp>&& ops, const std::vector<ActorId>& actors) {
        for (auto& op : ops) {
            append(std::move(op), actors);
        }
    }

    void append(OldOp&& op, const std::vector<ActorId>& actors);

    auto finish() -> std::pair<std::vector<u8>, std::unordered_map<u32, Range>>;
};

template <class T>
T col_iter(const BinSlice& bytes, const std::unordered_map<u32, Range>& ops, u32 col_id) {
    auto range = ops.find(col_id);
    if (range != ops.end()) {
        // reference
        return T(std::make_pair(bytes.first + range->second.first, range->second.second - range->second.first));
    }

    range = ops.find(col_id | COLUMN_TYPE_DEFLATE);
    if (range != ops.end()) {
        return T(deflate_decompress(
            std::make_pair(bytes.first + range->second.first, range->second.second - range->second.first)
        ));
    }

    // empty data
    return T(std::vector<u8>());
}