// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include <iomanip>
#include <stdexcept>

#include "type.h"
#include "helper.h"

ChangeHash::ChangeHash(const std::vector<u8>& vec) {
    if (vec.size() != HASH_SIZE) {
        throw std::runtime_error("invalid change hash vector");
    }

    std::copy(vec.cbegin(), vec.cend(), data);
}

ChangeHash::ChangeHash(const BinSlice& bin) {
    if (bin.second != HASH_SIZE) {
        throw std::runtime_error("invalid change hash slice");
    }

    std::copy(bin.first, bin.first + bin.second, data);
}

ChangeHash::ChangeHash(const std::string_view& hex_str) {
    if (hex_str.length() != HASH_SIZE * 2) {
        throw std::runtime_error("invalid change hash string");
    }

    auto vec = hex_from_string(hex_str);
    std::copy(vec.cbegin(), vec.cend(), data);
}

int ChangeHash::cmp(const ChangeHash& other) const {
    for (usize i = 0; i < HASH_SIZE; ++i) {
        if (data[i] < other.data[i])
            return -1;
        if (data[i] > other.data[i])
            return 1;
    }
    return 0;
}

std::string ChangeHash::to_hex() const {
    return hex_to_string<const u8*>({ std::cbegin(data), HASH_SIZE });
}

ActorId::ActorId(bool random) {
    if (!random) {
        return;
    }

    auto r1 = get_random_64();
    auto r2 = get_random_64();
    //std::cout << "ActorId\n" << std::hex << r2 << " " << std::hex << r1 << std::endl;

    auto iter = std::begin(data);
    for (int i = 0; i < 8; ++i) {
        *iter = r1 & 0xFF;
        *(iter + 8) = r2 & 0xFF;

        r1 >>= 8;
        r2 >>= 8;

        ++iter;
    }

    /*for (auto it = std::rbegin(data); it != std::rend(data); ++it) {
        std::cout << std::hex << (int)*it << " ";
    }
    std::cout << std::endl;*/
}

ActorId::ActorId(const std::vector<u8>& slice) {
    for (usize i = 0; i < ACTOR_ID_SIZE; ++i) {
        if (i >= slice.size()) {
            break;
        }

        data[i] = slice[i];
    }
}

ActorId::ActorId(const BinSlice& slice) {
    for (usize i = 0; i < ACTOR_ID_SIZE; ++i) {
        if (i >= slice.second) {
            break;
        }

        data[i] = slice.first[i];
    }
}

ActorId::ActorId(const std::string_view& hex_str) {
    if (hex_str.length() % 2) {
        throw std::runtime_error("invalid actor id string");
    }

    auto vec = hex_from_string(hex_str);
    std::copy(
        vec.cbegin(),
        (vec.size() > ACTOR_ID_SIZE) ? vec.cbegin() + ACTOR_ID_SIZE : vec.cend(),
        data
    );
}

int ActorId::cmp(const ActorId& other) const {
    for (usize i = 0; i < ACTOR_ID_SIZE; ++i) {
        if (data[i] < other.data[i])
            return -1;
        if (data[i] > other.data[i])
            return 1;
    }
    return 0;
}

usize ActorId::actor_index(const std::vector<ActorId>& actors) const {
    for (auto iter = actors.cbegin(); iter != actors.cend(); ++iter) {
        if (cmp(*iter) == 0) {
            return iter - actors.cbegin();
        }
    }
    throw std::runtime_error("actor not found");
}

std::string ActorId::to_hex() const {
    return hex_to_string<const u8*>({ std::cbegin(data), ACTOR_ID_SIZE });
}

std::ostream& operator<<(std::ostream& out, const ActorId& actorId) {
    for (u8 i = 0; i < ACTOR_ID_SIZE; ++i) {
        // type of actorId.data[] is u8, treated as a character, should convert to int
        out << std::setw(2) << std::setfill('0') << std::hex << (u16)actorId.data[i];
    }

    return out;
}

int OpId::succ_ord(const OpId& other, const std::vector<usize>& actors) const {
    if (counter == 0 && other.counter != 0) {
        return -1;
    }
    if (counter == 0 && other.counter == 0) {
        return 0;
    }
    if (counter != 0 && other.counter == 0) {
        return 1;
    }

    if (counter < other.counter) {
        return -1;
    }
    if (counter == other.counter) {
        if (actors[actor] < actors[other.actor]) {
            return -1;
        }
        if (actors[actor] == actors[other.actor]) {
            return 0;
        }
        return 1;
    }
    return 1;
}

std::optional<s64> ScalarValue::to_s64() const {
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

std::string ScalarValue::to_string() const {
    switch (tag) {
    case ScalarValue::Bytes:
        return hex_to_string(make_bin_slice(std::get<std::vector<u8>>(data)));
    case ScalarValue::Str:
        return std::get<std::string>(data);
    case ScalarValue::Int:
    case ScalarValue::Timestamp:
        return std::to_string(std::get<s64>(data));
    case ScalarValue::Uint:
        return std::to_string(std::get<u64>(data));
    case ScalarValue::F64:
        return std::to_string(std::get<double>(data));
    case ScalarValue::Counter:
        return std::to_string(std::get<Counter>(data).current);
    case ScalarValue::Boolean:
        return std::get<bool>(data) ? "true" : "false";
    case ScalarValue::Null:
        return "null";
    default:
        return {};
    }
}

// TODO: need distinguish ObjId, ElemId from OpId
Export::Export(const OpId& op) {
    if (op == ROOT) {
        tag = Export::Special;
        data = ROOT_STR;
    }
    else {
        tag = Export::Id;
        data = op;
    }
}

Export::Export(const Key& key) {
    if (key.is_map()) {
        tag = Export::Prop;
        data = std::get<usize>(key.data);
    }
    else {
        auto e = Export(std::get<ElemId>(key.data));
        tag = e.tag;
        data = e.data;
    }
}
