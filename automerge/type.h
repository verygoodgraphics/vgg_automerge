// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <optional>
#include <variant>
#include <stdexcept>
#include <ostream>

#include <cstdint>

using u64 = std::uint64_t;
using u32 = std::uint32_t;
using u16 = std::uint16_t;
using u8 = std::uint8_t;
using s64 = std::int64_t;
using s32 = std::int32_t;
using s16 = std::int16_t;
using s8 = std::int8_t;
using usize = std::size_t;

typedef std::pair<usize, usize> Range;
typedef std::vector<usize> VecPos;
typedef std::vector<u8>::const_iterator BinIter;
typedef std::pair<BinIter, usize> BinSlice;

// The number of bytes in a change hash.
constexpr usize HASH_SIZE = 32; // 256 bits = 32 bytes

// #[derive(Eq, PartialEq, Hash, Clone, PartialOrd, Ord, Copy)]
struct ChangeHash {
    u8 data[HASH_SIZE] = { 0 };

    ChangeHash() = default;
    ChangeHash(const std::vector<u8>& vec);
    ChangeHash(const BinSlice& bin);
    ChangeHash(const std::string& hex_str);

    bool operator==(const ChangeHash& other) const {
        return (cmp(other) == 0);
    }

    bool operator<(const ChangeHash& other) const {
        return (cmp(other) < 0);
    }

    int cmp(const ChangeHash& other) const;

    std::string to_hex() const;
};
template<>
struct std::hash<ChangeHash> {
    std::size_t operator()(const ChangeHash& key) const noexcept {
        std::size_t seed = 0;
        for (auto& i : key.data) {
            seed = 31 * seed + i;
        }
        return seed;
    }
};

// The number of bytes in an actor id.
constexpr usize ACTOR_ID_SIZE = 16;

// #[derive(Eq, PartialEq, Hash, Clone, PartialOrd, Ord)]
struct ActorId {
    // little endian
    u8 data[ACTOR_ID_SIZE] = { 0 };

    ActorId(bool random = false);
    ActorId(const std::vector<u8>& slice);
    ActorId(const BinSlice& slice);
    ActorId(const std::string& hex_str);

    bool operator==(const ActorId& other) const {
        return (cmp(other) == 0);
    }

    bool operator<(const ActorId& other) const {
        return (cmp(other) < 0);
    }

    int cmp(const ActorId& other) const;

    usize actor_index(const std::vector<ActorId>& actors) const;

    std::string to_hex() const;

    friend std::ostream& operator<<(std::ostream& out, const ActorId& actorId);
};
template<>
struct std::hash<ActorId> {
    std::size_t operator()(const ActorId& key) const noexcept {
        std::size_t seed = 0;
        for (auto& i : key.data) {
            seed = 31 * seed + i;
        }
        return seed;
    }
};

// #[derive(Debug, Clone, PartialOrd, Ord, Eq, PartialEq, Copy, Hash, Default)]
struct OpId {
    u64 counter = 0;
    usize actor = 0;

    bool operator==(const OpId& other) const {
        return (counter == other.counter && actor == other.actor);
    }

    int succ_ord(const OpId& other, const std::vector<usize>& actors) const;
};
template<>
struct std::hash<OpId> {
    std::size_t operator()(const OpId& id) const noexcept {
        return std::hash<u64>{}(id.counter) ^ (std::hash<usize>{}(id.actor) << 1);
    }
};
typedef std::function<int(const OpId&, const OpId&)> OpIdCmpFunc;

typedef std::vector<ActorId> ActorMap;
// #[derive(Debug, Clone, Copy, PartialOrd, Eq, PartialEq, Ord, Hash, Default)]
typedef OpId ObjId;
typedef OpId ElemId;

constexpr ElemId HEAD = ElemId{ 0, 0 };
constexpr OpId ROOT = OpId{ 0, 0 };
const auto ROOT_STR = std::string("_root");

struct Counter {
    s64 start = 0;
    s64 current = 0;
    usize increments = 0;

    bool operator==(const Counter& other) const {
        return (current == other.current);
    }
};

///////////////////////////////////////////////

enum struct DataType {
    Counter,
    Timestamp,
    Bytes,
    Uint,
    Int,
    F64,
    Undefined
};

struct UnknownValue {
    u8 type_code = 0;
    std::vector<u8> bytes;

    bool operator==(const UnknownValue& other) const {
        return (type_code == other.type_code) && (bytes == other.bytes);
    }
};

struct ScalarValue {
    enum {
        Bytes,          // std::vector<u8>
        Str,            // std::string
        Int,            // s64
        Uint,           // u64
        F64,            // double
        Counter,        // Counter
        Timestamp,      // s64
        Boolean,        // bool
        Unknown,        // UnknownValue
        Null
    } tag = Bytes;
    std::variant<std::vector<u8>, std::string, s64, u64, double, ::Counter, bool, UnknownValue> data = {};

    bool operator==(const ScalarValue& other) const {
        return (tag == other.tag) && (data == other.data);
    }

    std::optional<s64> to_s64() const {
        switch (tag) {
        case ScalarValue::Int:
        case ScalarValue::Timestamp:
            return std::get<s64>(data);
        case ScalarValue::Uint:
            return (s64)std::get<u64>(data);
        case ScalarValue::F64:
            return (s64)std::get<double>(data);
        case ScalarValue::Counter:
            return std::get<Counter>(data).current;
        default:
            return {};
        }
    }
};

///////////////////////////////////////////////

// #[derive(Deserialize, Serialize, Debug, Clone, PartialEq, Eq, Copy, Hash)]
enum struct ObjType {
    Map,
    Table,
    List,
    Text
};

// #[derive(Debug, PartialEq, PartialOrd, Eq, Ord, Clone, Copy, Hash)]
struct Key {
    enum {
        Map,
        Seq
    } tag = Map;
    std::variant<usize, ElemId> data = {};

    auto elemid() const {
        return is_seq() ? std::optional<ElemId>{ std::get<ElemId>(data) } : std::nullopt;
    }

    bool is_map() const {
        return (tag == Map);
    }

    bool is_seq() const {
        return (tag == Seq);
    }

    bool operator==(const Key& other) const {
        return (tag == other.tag) && (data == other.data);
    }
};
template<>
struct std::hash<Key> {
    std::size_t operator()(const Key& key) const noexcept {
        switch (key.tag) {
        case Key::Map:
            return std::hash<usize>{}(std::get<usize>(key.data)) ^ ((usize)1 << key.tag);
        case Key::Seq:
            return std::hash<ElemId>{}(std::get<ElemId>(key.data)) ^ ((usize)1 << key.tag);
        default:
            return 0;
        }
    }
};

// #[derive(Debug, PartialEq, PartialOrd, Eq, Ord, Clone)]
struct Prop {
    enum {
        Map,
        Seq
    } tag = Map;
    std::variant<std::string, usize> data = {};

    Prop() = default;
    Prop(std::string&& map) : tag(Map), data(std::move(map)) {}
    Prop(usize seq) : tag(Seq), data(seq) {}

    bool operator==(const Prop& other) const {
        return (tag == other.tag) && (data == other.data);
    }

    std::string to_string() const {
        return (tag == Map) ? std::get<std::string>(data) : std::to_string(std::get<usize>(data));
    }
};

///////////////////////////////////////////////

// #[derive(PartialEq, Debug, Clone, Copy)]
enum struct Action {
    MakeMap,
    Set,
    MakeList,
    Del,
    MakeText,
    Inc,
    MakeTable,

    BUTT
};

struct Export {
    enum {
        Id,
        Special,
        Prop
    } tag = Id;
    std::variant<OpId, std::string, usize> data = {};

    Export(const OpId& op);

    Export(const Key& key);
};
