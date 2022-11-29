// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <set>
#include <stdexcept>
#include <unordered_set>

#include "Change.h"
#include "Columnar.h"
#include "helper.h"
#include "picosha2.h"

std::vector<u8> encode_document(std::vector<ChangeHash>&& heads, const std::vector<Change>& changes,
    OpSetIter&& doc_ops, const IndexedCache<ActorId>& actors_index, const std::vector<std::string>& props)
{
    std::vector<u8> bytes;
    Encoder bytes_encoder(bytes);

    auto actors_map = actors_index.encode_index();
    auto actors = actors_index.sorted();

    auto [change_bytes, change_info] = ChangeEncoder::encode_changes(changes, actors);

    auto [ops_bytes, ops_info] = DocOpEncoder::encode_doc_ops(doc_ops, actors_map, props);

    vector_extend(bytes, MAGIC_BYTES);
    vector_extend(bytes, { 0, 0, 0, 0 });
    bytes.push_back(BLOCK_TYPE_DOC);

    std::vector<u8> chunk;
    Encoder chunk_encoder(chunk);

    chunk_encoder.encode(actors.len());

    for (auto& a : actors._cache) {
        chunk_encoder.encode(a);
    }

    chunk_encoder.encode(heads.size());
    for (auto& head : heads) {
        std::move(std::begin(head.data), std::end(head.data), std::back_inserter(chunk));
    }

    vector_extend(chunk, std::move(change_info));
    vector_extend(chunk, std::move(ops_info));

    vector_extend(chunk, std::move(change_bytes));
    vector_extend(chunk, std::move(ops_bytes));

    bytes_encoder.encode(chunk.size());
    vector_extend(bytes, std::move(chunk));

    std::vector<u8> hash_result(picosha2::k_digest_size);
    picosha2::hash256(bytes.begin() + CHUNK_START, bytes.end(), hash_result.begin(), hash_result.end());

    std::copy(hash_result.cbegin(), hash_result.cbegin() + (HASH_RANGE.second - HASH_RANGE.first),
        bytes.begin() + HASH_RANGE.first);

    return bytes;
}

/////////////////////////////////////////////////////////

bool OldChange::operator==(const OldChange& other) const {
    return (operations == other.operations) &&
        (actor_id == other.actor_id) &&
        (seq == other.seq) &&
        (start_op == other.start_op) &&
        (time == other.time) &&
        (message == other.message) &&
        (deps == other.deps) &&
        (extra_bytes == other.extra_bytes);
}

std::vector<ActorId> OldChange::actor_ids_in_change() const {
    std::set<ActorId> unique;
    std::vector<const ActorId*> other_ids;
    for (auto& op : operations) {
        auto opids = op.opids_in_operation();
        for (auto& opid : opids) {
            if (*opid == actor_id || unique.count(*opid)) {
                continue;
            }
            other_ids.push_back(opid);
            unique.insert(*opid);
        }
    }

    std::stable_sort(other_ids.begin(), other_ids.end(), [](const ActorId* a, const ActorId* b) {
        return *a < *b;
        });

    std::vector<ActorId> res;
    res.reserve(other_ids.size() + 1);
    res.push_back(actor_id);
    for (auto& id : other_ids) {
        res.push_back(*id);
    }

    return res;
}

void increment_range(Range& range, usize len) {
    range.first += len;
    range.second += len;
}

void increment_range_map(std::unordered_map<u32, Range>& ranges, usize len) {
    for (auto& range : ranges) {
        increment_range(range.second, len);
    }
}

ChunkIntermediate OldChange::encode_chunk(const std::vector<ChangeHash>& deps) const {
    std::vector<u8> bytes;
    Encoder encoder(bytes);
    bytes.reserve(256); // guessed minimum length of encoded Change bytes

    // encode deps
    encoder.encode(deps.size());
    for (auto& hash : deps) {
        bytes.insert(bytes.end(), std::begin(hash.data), std::end(hash.data));
    }

    auto actors = actor_ids_in_change();
    encoder.encode(actor_id);

    // encode seq, start_op, time, message
    encoder.encode(seq);
    encoder.encode(start_op);
    encoder.encode(time);
    // TODO: why need +1
    usize message_start = bytes.size() + 1;
    encoder.encode(message);
    Range message_range{ message_start, bytes.size() };

    // encode ops into a side buffer - collect all other actors
    auto [ops_buf, ops] = ColumnEncoder::encode_ops(operations, actors);

    // encode all other actors
    encoder.encode(actors, 1);

    // now we know how many bytes ops are offset by so we can adjust the ranges
    increment_range_map(ops, bytes.size());

    // write out the ops
    vector_extend(bytes, std::move(ops_buf));

    // write out the extra bytes
    Range extra_bytes_range{ bytes.size(), bytes.size() + extra_bytes.size() };
    vector_extend(bytes, extra_bytes);
    Range body_range{ 0, bytes.size() };

    return {
        std::move(bytes),
        std::move(body_range),
        std::move(actors),
        std::move(message_range),
        std::move(ops),
        std::move(extra_bytes_range)
    };
}

Change OldChange::encode() const {
    std::vector<ChangeHash> deps(this->deps);
    std::sort(deps.begin(), deps.end());

    auto chunk = encode_chunk(deps);

    std::vector<u8> bytes;
    bytes.reserve(MAGIC_BYTES.size() + 4 + 1 + 10 + chunk.bytes.size());
    Encoder encoder(bytes);

    vector_extend(bytes, MAGIC_BYTES);
    vector_extend(bytes, { 0, 0, 0, 0 }); // we dont know the hash yet so fill in a fake
    bytes.push_back(BLOCK_TYPE_CHANGE);
    encoder.encode(chunk.bytes.size());

    usize body_start = bytes.size();
    increment_range(chunk.body, body_start);
    increment_range(chunk.message, body_start);
    increment_range(chunk.extra_bytes, body_start);
    increment_range_map(chunk.ops, body_start);

    // TODO: why not move?
    vector_extend(bytes, std::move(chunk.bytes));

    std::vector<u8> hash_result(picosha2::k_digest_size);
    picosha2::hash256(bytes.begin() + CHUNK_START, bytes.end(), hash_result.begin(), hash_result.end());
    ChangeHash hash(hash_result);

    std::copy(hash_result.cbegin(), hash_result.cbegin() + (HASH_RANGE.second - HASH_RANGE.first),
        bytes.begin() + HASH_RANGE.first);

    // any time I make changes to the encoder decoder its a good idea
    // to run it through a round trip to detect errors the tests might not
    // catch
    // let c0 = Change::from_bytes(bytes.clone()).unwrap();
    // std::assert_eq!(c1, c0);
    // perhaps we should add something like this to the test suite
    return Change{
        ChangeBytes{ false, {}, std::move(bytes) },
        body_start,
        std::move(hash),
        seq,
        start_op,
        time,
        std::move(chunk.message),
        std::move(chunk.actors),
        std::move(deps),
        std::move(chunk.ops),
        std::move(chunk.extra_bytes),
        operations.size()
    };
}

/////////////////////////////////////////////////////////

std::optional<std::string> Change::get_message() const {
    if (message.first == message.second) {
        return {};
    }
    return std::string(bytes.uncompressed.cbegin() + message.first,
        bytes.uncompressed.cbegin() + message.second);
}

OldChange Change::decode() const {
    // TODO: Change decode to OldChange
    return OldChange();
}

OperationIterator Change::iter_ops() const {
    return OperationIterator({ bytes.uncompressed.cbegin(), bytes.uncompressed.size() }, actors, ops);
}

/////////////////////////////////////////////////////////

Range read_leb128(BinSlice& bytes) {
    BinSlice buf = bytes;
    std::optional<usize> val;

    Decoding::decode(buf, val);
    if (!val) {
        throw std::overflow_error("decode u64");
    }

    return { *val, bytes.second - buf.second };
}

Range slice_bytes(const BinSlice& bytes, Range& cursor) {
    BinSlice slice = { bytes.first + cursor.first, cursor.second - cursor.first };
    auto [val, len] = read_leb128(slice);
    usize start = cursor.first + len;
    usize end = start + val;
    cursor.first = end;

    return { start, end };
}

Change decode_change(std::vector<u8>&& _bytes) {
    auto [chunktype, body] = decode_header_without_hash({ _bytes.cbegin(), _bytes.size() });
    ChangeBytes bytes;
    if (chunktype == BLOCK_TYPE_DEFLATE) {
        decompress_chunk({ 0, PREAMBLE_BYTES }, std::move(body), std::move(_bytes));
    }
    else {
        bytes.isCompressed = true;
        bytes.uncompressed = std::move(_bytes);
    }
    auto uncompressed = bytes.get_uncompressed();

    ChangeHash hash;
    std::tie(chunktype, hash, body) = decode_header(uncompressed);

    if (chunktype != BLOCK_TYPE_CHANGE) {
        throw std::runtime_error("wrong chunk type");
    }

    usize body_start = body.first;
    Range cursor = body;

    auto deps = decode_hashes(uncompressed, cursor);

    auto actor_range = slice_bytes(uncompressed, cursor);
    ActorId actor({ uncompressed.first + actor_range.first, actor_range.second - actor_range.first });
    u64 seq = read_slice<u64>(uncompressed, cursor);
    u64 start_op = read_slice<u64>(uncompressed, cursor);
    s64 time = read_slice<s64>(uncompressed, cursor);
    auto message = slice_bytes(uncompressed, cursor);

    auto actors = decode_actors(uncompressed, cursor, std::move(actor));

    auto ops_info = decode_column_info(uncompressed, cursor, false);
    auto ops = decode_columns(cursor, ops_info);

    Change change = {
        std::move(bytes),
        body_start,
        std::move(hash),
        seq,
        start_op,
        time,
        std::move(message),
        std::move(actors),
        std::move(deps),
        std::move(ops),
        std::move(cursor),
        0
    };

    usize len = change.iter_ops().count();
    change.num_ops = len;

    return change;
}

ChangeBytes decompress_chunk(Range&& preamble, Range&& body, std::vector<u8>&& compressed) {
    auto decompressed = deflate_decompress({ compressed.cbegin() + body.first, body.second - body.first });
    std::vector<u8> result;
    result.reserve(decompressed.size() + (preamble.second - preamble.first));
    result.insert(result.end(), compressed.begin() + preamble.first, compressed.begin() + preamble.second);
    result.push_back(BLOCK_TYPE_CHANGE);
    Encoder encoder(result);
    encoder.encode((u64)decompressed.size());
    vector_extend(result, std::move(decompressed));

    return { true, {}, std::move(result) };
}

std::vector<ChangeHash> decode_hashes(const BinSlice& bytes, Range& cursor) {
    usize num_hashes = read_slice<usize>(bytes, cursor);
    std::vector<ChangeHash> hashes;
    hashes.reserve(num_hashes);

    for (usize i = 0; i < num_hashes; ++i) {
        Range hash = { cursor.first, cursor.first + HASH_SIZE };
        cursor.first = hash.second;

        if (bytes.second < hash.second) {
            throw std::runtime_error("no enough bytes");
        }
        hashes.push_back(ChangeHash(BinSlice{ bytes.first + hash.first, hash.second - hash.first }));
    }

    return hashes;
}

std::vector<ActorId> decode_actors(const BinSlice& bytes, Range& cursor, std::optional<ActorId>&& first) {
    usize num_actors = read_slice<usize>(bytes, cursor);
    std::vector<ActorId> actors;
    actors.reserve(num_actors + 1);

    if (first) {
        actors.push_back(*first);
    }

    for (usize i = 0; i < num_actors; ++i) {
        Range range = slice_bytes(bytes, cursor);
        if (bytes.second < range.second) {
            throw std::runtime_error("no enough bytes");
        }
        actors.push_back(ActorId(BinSlice{ bytes.first + range.first, range.second - range.first }));
    }

    return actors;
}

std::vector<std::pair<u32, usize>> decode_column_info(const BinSlice& bytes, Range& cursor, bool allow_compressed_column) {
    usize num_columns = read_slice<usize>(bytes, cursor);
    std::vector<std::pair<u32, usize>> columns;
    columns.reserve(num_columns);
    u32 last_id = 0;

    for (usize i = 0; i < num_columns; ++i) {
        u32 id = read_slice<u32>(bytes, cursor);

        if ((id & ~COLUMN_TYPE_DEFLATE) <= (last_id & ~COLUMN_TYPE_DEFLATE)) {
            throw std::runtime_error("ColumnsNotInAscendingOrder");
        }
        if ((id & COLUMN_TYPE_DEFLATE) != 0 && !allow_compressed_column) {
            throw std::runtime_error("ChangeContainedCompressedColumns");
        }

        last_id = id;
        columns.push_back({ id, read_slice<usize>(bytes, cursor) });
    }

    return columns;
}

std::unordered_map<u32, Range> decode_columns(Range& cursor, const std::vector<std::pair<u32, usize>>& columns) {
    std::unordered_map<u32, Range> ops;
    for (auto& [id, length] : columns) {
        usize start = cursor.first;
        usize end = start + length;
        cursor.first = end;
        ops.insert({ id, Range{start, end} });
    }
    return ops;
}

std::tuple<u8, ChangeHash, Range> decode_header(const BinSlice& bytes) {
    auto [chunktype, body] = decode_header_without_hash(bytes);

    std::vector<u8> calculated_hash(picosha2::k_digest_size);
    picosha2::hash256(bytes.first + PREAMBLE_BYTES, bytes.first + bytes.second,
        calculated_hash.begin(), calculated_hash.end());

    BinSlice checksum = { bytes.first + 4, 4 };
    if (!bin_slice_cmp(checksum, { calculated_hash.cbegin(), 4 })) {
        throw std::runtime_error("invalid check sum");
    }

    return { chunktype, ChangeHash(calculated_hash), body };
}

std::pair<u8, Range> decode_header_without_hash(const BinSlice& bytes) {
    if (bytes.second <= HEADER_BYTES) {
        throw std::runtime_error("not enough bytes");
    }
    if (!bin_slice_cmp({ bytes.first, 4 }, { MAGIC_BYTES.cbegin(), MAGIC_BYTES.size() })) {
        throw std::runtime_error("wrong magic bytes");
    }

    BinSlice after_header = { bytes.first + HEADER_BYTES, bytes.second - HEADER_BYTES };
    auto [val, len] = read_leb128(after_header);
    Range body = { HEADER_BYTES + len, HEADER_BYTES + len + val };
    if (bytes.second != body.second) {
        throw std::runtime_error("wrong bytes length");
    }

    return { bytes.first[PREAMBLE_BYTES], body };
}

std::vector<Change> load_blocks(const BinSlice& bytes) {
    std::vector<Change> changes;
    for (auto& slice : split_blocks(bytes)) {
        decode_block(slice, changes);
    }

    return changes;
}

std::vector<BinSlice> split_blocks(const BinSlice& bytes) {
    std::vector<BinSlice> blocks;
    BinSlice cursor = bytes;
    while (true) {
        auto block = pop_block(cursor);
        if (!block) {
            break;
        }

        blocks.push_back({ cursor.first + block->first, block->second - block->first });

        if (cursor.second <= block->second) {
            break;
        }

        cursor.first += block->second;
        cursor.second -= block->second;
    }

    return blocks;
}

std::optional<Range> pop_block(const BinSlice& bytes) {
    if (bytes.second < 4 || !bin_slice_cmp({ bytes.first, 4 }, { MAGIC_BYTES.cbegin(), MAGIC_BYTES.size() })) {
        return {};
    }

    if (bytes.second < HEADER_BYTES + 1) {
        throw std::runtime_error("not enough bytes");
    }

    BinSlice body = { bytes.first + HEADER_BYTES, bytes.second - HEADER_BYTES };
    auto [val, len] = read_leb128(body);

    if (UINT64_MAX - val < HEADER_BYTES + len) {
        throw std::runtime_error("overflow");
    }

    usize end = HEADER_BYTES + len + val;
    if (end > bytes.second) {
        return {};
    }

    return Range{ 0, end };
}

void decode_block(const BinSlice& bytes, std::vector<Change>& changes) {
    if (bytes.first[PREAMBLE_BYTES] == BLOCK_TYPE_DOC) {
        vector_extend(changes, decode_document(bytes));
        return;
    }
    if ((bytes.first[PREAMBLE_BYTES] == BLOCK_TYPE_CHANGE) ||
        (bytes.first[PREAMBLE_BYTES] == BLOCK_TYPE_DEFLATE)) {
        changes.push_back(decode_change(std::vector<u8>(bytes.first, bytes.first + bytes.second)));
        return;
    }

    throw std::runtime_error("wrong chunk type");
}

std::vector<Change> decode_document(const BinSlice& bytes) {
    auto [chunktype, _hash, cursor] = decode_header(bytes);

    if (chunktype > 0) {
        throw std::runtime_error("wrong chunk type");
    }

    auto actors = decode_actors(bytes, cursor, {});

    auto heads = decode_hashes(bytes, cursor);

    auto changes_info = decode_column_info(bytes, cursor, true);
    auto ops_info = decode_column_info(bytes, cursor, true);

    auto changes_data = decode_columns(cursor, changes_info);
    auto doc_changes = ChangeIterator(bytes, changes_data).collect();
    DepsIterator doc_changes_deps(bytes, changes_data);

    usize doc_changes_len = doc_changes.size();

    auto ops_data = decode_columns(cursor, ops_info);
    auto doc_ops = DocOpIterator(bytes, actors, ops_data).collect();

    group_doc_change_and_doc_ops(doc_changes, std::move(doc_ops), actors);

    /* let uncompressed_changes = doc_changes_to_uncompressed_changes(doc_changes.into_iter(), &actors);
       let changes = compress_doc_changes(uncompressed_changes, doc_changes_deps, doc_changes_len).ok_or(decoding::Error::NoDocChanges) ? ;
    */
    auto changes = compress_doc_changes(std::move(doc_changes), std::move(doc_changes_deps), doc_changes_len, actors);
    if (!changes) {
        throw std::runtime_error("no doc changes");
    }

    std::unordered_set<ChangeHash> calculated_heads;
    for (auto& change : *changes) {
        for (auto& dep : change.deps) {
            calculated_heads.erase(dep);
        }
        calculated_heads.insert(change.hash);
    }

    if (calculated_heads != std::unordered_set<ChangeHash>(
        std::make_move_iterator(heads.begin()), std::make_move_iterator(heads.end()))) {
        throw std::runtime_error("MismatchedHeads");
    }

    return *changes;
}

void group_doc_change_and_doc_ops(std::vector<DocChange>& changes, std::vector<DocOp>&& ops, const std::vector<ActorId>& actors) {
    std::unordered_map<usize, std::vector<usize>> changes_by_actor;

    for (usize i = 0; i < changes.size(); ++i) {
        auto& change = changes[i];
        auto& actor_change_index = changes_by_actor[change.actor];
        if (change.seq != (u64)(actor_change_index.size() + 1)) {
            throw std::runtime_error("Doc Seq Invalid");
        }
        if (change.actor >= actors.size()) {
            throw std::runtime_error("Doc Actor Invalid");
        }
        actor_change_index.push_back(i);
    }

    std::unordered_map<OpId, usize> op_by_id;
    for (usize i = 0; i < ops.size(); ++i) {
        op_by_id.insert({ OpId{ ops[i].ctr, ops[i].actor }, i });
    }

    for (usize i = 0; i < ops.size(); ++i) {
        DocOp op = ops[i];
        for (auto& succ : op.succ) {
            auto find = op_by_id.find(succ);
            if (find != op_by_id.end()) {
                ops[find->second].pred.push_back({ op.ctr, op.actor });
            }
            else {
                DocOp del = {
                    succ.actor,
                    succ.counter,
                    {OpType::Delete, {}},
                    op.obj,
                    op.insert ? OldKey{ OldKey::SEQ, OldElementId(OldOpId(op.ctr, actors[op.actor])) } : op.key,
                    {},
                    {OpId{op.ctr, op.actor}},
                    false
                };
                op_by_id.insert({ succ, ops.size() });
                ops.push_back(std::move(del));
            }
        }
    }

    for (auto& op : ops) {
        auto& actor_change_index = changes_by_actor[op.actor];
        usize left = 0;
        usize right = actor_change_index.size();
        while (left < right) {
            usize seq = (left + right) / 2;
            if (changes[actor_change_index[seq]].max_op < op.ctr) {
                left = seq + 1;
            }
            else {
                right = seq;
            }
        }
        if (left >= actor_change_index.size()) {
            throw std::runtime_error("Doc MaxOp Invalid");
        }
        changes[actor_change_index[left]].ops.push_back(std::move(op));
    }

    for (auto& change : changes) {
        std::sort(change.ops.begin(), change.ops.end());
    }
}

std::optional<std::vector<Change>> compress_doc_changes(std::vector<DocChange>&& uncompressed_changes,
    DepsIterator&& doc_changes_deps, usize num_changes, const std::vector<ActorId>& actors) {
    std::vector<Change> changes;
    changes.reserve(num_changes);

    for (auto& doc_change : uncompressed_changes) {
        auto deps = doc_changes_deps.next();
        if (!deps) {
            throw std::runtime_error("num not match");
        }

        auto uncompressed_change = doc_change_to_uncompressed_change(std::move(doc_change), actors);

        for (auto idx : *deps) {
            if (idx >= changes.size()) {
                return {};
            }
            uncompressed_change.deps.push_back(changes[idx].hash);
        }

        changes.push_back(uncompressed_change.encode());
    }

    return changes;
}

OldChange doc_change_to_uncompressed_change(DocChange&& change, const std::vector<ActorId>& actors) {
    std::vector<OldOp> oprations;
    oprations.reserve(change.ops.size());
    for (auto& op : change.ops) {
        std::vector<OldOpId> pred;
        pred.reserve(op.pred.size());
        for (auto& [ctr, actor] : op.pred) {
            pred.push_back(OldOpId{ ctr, actors[actor] });
        }

        oprations.push_back(OldOp(
            std::move(op.action),
            std::move(op.obj),
            std::move(op.key),
            std::move(pred),
            op.insert
        ));
    }

    return {
        std::move(oprations),
        actors[change.actor],
        {},
        change.seq,
        change.max_op - change.ops.size() + 1,
        change.time,
        std::move(change.message),
        {},
        std::move(change.extra_bytes)
    };
}

std::vector<Change> load_document(const BinSlice& bytes) {
    return load_blocks(bytes);
}