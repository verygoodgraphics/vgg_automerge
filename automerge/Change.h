// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <optional>

#include "type.h"
#include "Op.h"
#include "IndexedCache.h"
#include "Encoder.h"
#include "Columnar.h"
#include "legacy.h"

const std::vector<u8> MAGIC_BYTES = { 0x85, 0x6f, 0x4a, 0x83 };
constexpr usize PREAMBLE_BYTES = 8;
constexpr usize HEADER_BYTES = PREAMBLE_BYTES + 1;

constexpr u8 BLOCK_TYPE_DOC = 0;
constexpr u8 BLOCK_TYPE_CHANGE = 1;
constexpr u8 BLOCK_TYPE_DEFLATE = 2;
constexpr usize CHUNK_START = 8;
constexpr Range HASH_RANGE = { 4, 8 };

std::vector<u8> encode_document(std::vector<ChangeHash>&& heads, const std::vector<Change>& changes,
    OpSetIter&& doc_ops, const IndexedCache<ActorId>& actors_index, const std::vector<std::string_view>& props);

struct ChangeBytes {
    bool isCompressed = false;

    std::vector<u8> compressed = {};
    std::vector<u8> uncompressed = {};

    BinSlice get_uncompressed() const {
        return { uncompressed.cbegin(), uncompressed.size() };
    }

    void compress(usize body_start);

    BinSlice raw() const {
        if (isCompressed) {
            return { compressed.cbegin(), compressed.size() };
        }
        else {
            return { uncompressed.cbegin(), uncompressed.size() };
        }
    }
};

struct Change;
struct ChunkIntermediate;

// #[derive(Deserialize, Serialize, Debug, Clone)]
struct OldChange {
    // The operations performed in this change.
    std::vector<OldOp> operations = {};
    // The actor that performed this change.
    ActorId actor_id = {};
    // The hash of this change.
    std::optional<ChangeHash> hash = {};
    // The index of this change in the changes from this actor.
    u64 seq = 0;
    // The start operation index. Starts at 1.
    u64 start_op = 1;  // non-zero
    // The time that this change was committed.
    s64 time = 0;
    // The message of this change.
    std::optional<std::string> message = {};
    // The dependencies of this change.
    std::vector<ChangeHash> deps = {};
    std::vector<u8> extra_bytes = {};

    bool operator==(const OldChange& other) const;

    // When encoding a change we take all the actor IDs referenced by a change and place them in an
    // array. The array has the actor who authored the change as the first element and all remaining
    // actors (i.e. those referenced in object IDs in the target of an operation or in the `pred` of
    // an operation) lexicographically ordered following the change author.
    std::vector<ActorId> actor_ids_in_change() const;

    // note: consume operations
    ChunkIntermediate encode_chunk(const std::vector<ChangeHash>& deps);
};

struct ChunkIntermediate {
    std::vector<u8> bytes = {};
    Range body = {};
    std::vector<ActorId> actors = {};
    Range message = {};
    std::unordered_map<u32, Range> ops = {};
    Range extra_bytes = {};
};

// TryFrom<Vec<u8>> for Change
// throw exception
Change decode_change(std::vector<u8>&& _bytes);

// #[derive(PartialEq, Debug, Clone)]
struct Change {
    ChangeBytes bytes = {};
    usize body_start = 0;
    // Hash of this change.
    ChangeHash hash = {};
    // The index of this change in the changes from this actor.
    u64 seq = 0;
    // The start operation index. Starts at 1.
    u64 start_op = 1;  // non-zero
    // The time that this change was committed.
    s64 time = 0;
    // The message of this change.
    Range message = {};
    // The actors referenced in this change.
    std::vector<ActorId> actors = {};
    // The dependencies of this change.
    std::vector<ChangeHash> deps = {};
    std::unordered_map<u32, Range> ops = {};
    Range extra_bytes = {};
    // The number of operations in this change.
    usize num_ops = 0;

    const ActorId& actor_id() const {
        return actors[0];
    }

    static std::optional<Change> from_bytes(std::vector<u8>&& bytes) {
        // TODO: handle exception
        return decode_change(std::move(bytes));
    }

    static Change from_old_change(OldChange&& change);

    bool is_empty() const {
        return len() == 0;
    }

    usize len() const {
        return num_ops;
    }

    u64 max_op() const {
        return start_op + len() - 1;
    }

    std::optional<std::string> get_message() const;

    OldChange decode() const;

    OperationIterator iter_ops() const;

    BinSlice get_extra_bytes() const {
        return { bytes.uncompressed.cbegin() + extra_bytes.first, extra_bytes.second - extra_bytes.first };
    }

    void compress() {
        bytes.compress(body_start);
    }
};

// throw exception
static Range read_leb128(BinSlice& bytes);

// throw exception
template<class T>
static T read_slice(const BinSlice& bytes, Range& cursor) {
    BinSlice view = { bytes.first + cursor.first, cursor.second - cursor.first };
    usize init_len = view.second;

    std::optional<T> val;
    Decoding::decode(view, val);
    if (!val.has_value()) {
        throw std::runtime_error("no decoded value");
    }

    usize bytes_read = init_len - view.second;
    cursor.first += bytes_read;

    return *val;
}

// throw exception
static Range slice_bytes(const BinSlice& bytes, Range& cursor);

// throw exception
static ChangeBytes decompress_chunk(Range&& preamble, Range&& body, std::vector<u8>&& compressed);

// throw exception
static std::vector<ChangeHash> decode_hashes(const BinSlice& bytes, Range& cursor);

// throw exception
static std::vector<ActorId> decode_actors(const BinSlice& bytes, Range& cursor, std::optional<ActorId>&& first);

// throw exception
static std::vector<std::pair<u32, usize>> decode_column_info(const BinSlice& bytes, Range& cursor, bool allow_compressed_column);

static std::unordered_map<u32, Range> decode_columns(Range& cursor, const std::vector<std::pair<u32, usize>>& columns);

// throw exception
static std::tuple<u8, ChangeHash, Range> decode_header(const BinSlice& bytes);

// throw exception
static std::pair<u8, Range> decode_header_without_hash(const BinSlice& bytes);

// throw exception
static std::vector<Change> load_blocks(const BinSlice& bytes);

// throw exception
static std::vector<BinSlice> split_blocks(const BinSlice& bytes);

// throw exception
static std::optional<Range> pop_block(const BinSlice& bytes);

// throw exception
static void decode_block(const BinSlice& bytes, std::vector<Change>& changes);

// throw exception
static std::vector<Change> decode_document(const BinSlice& bytes);

// throw exception
void group_doc_change_and_doc_ops(std::vector<DocChange>& changes, std::vector<DocOp>&& ops, const std::vector<ActorId>& actors);

static std::optional<std::vector<Change>> compress_doc_changes(std::vector<DocChange>&& uncompressed_changes,
    DepsIterator&& doc_changes_deps, usize num_changes, const std::vector<ActorId>& actors);

static OldChange doc_change_to_uncompressed_change(DocChange&& change, const std::vector<ActorId>& actors);

// throw exception
std::vector<Change> load_document(const BinSlice& bytes);
