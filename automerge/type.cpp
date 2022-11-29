// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include <iomanip>
#include <stdexcept>

#include "type.h"
#include "helper.h"

ChangeHash::ChangeHash(const std::vector<u8>& vec) {
    if (vec.size() != HASH_SIZE) {
        throw std::runtime_error("invalid change hash slice");
    }

    std::copy(vec.cbegin(), vec.cend(), data);
}

ChangeHash::ChangeHash(const BinSlice& bin) {
    if (bin.second != HASH_SIZE) {
        throw std::runtime_error("invalid change hash slice");
    }

    std::copy(bin.first, bin.first + bin.second, data);
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
    for (int i = 0; i < 16; ++i) {
        if (i >= slice.size()) {
            break;
        }

        data[i] = slice[i];
    }
}

ActorId::ActorId(const BinSlice& slice) {
    for (int i = 0; i < 16; ++i) {
        if (i >= slice.second) {
            break;
        }

        data[i] = slice.first[i];
    }
}

std::ostream& operator<<(std::ostream& out, const ActorId& actorId) {
    for (u8 i = 0; i < 16; ++i) {
        // type of actorId.data[] is u8, treated as a character, should convert to int
        out << std::setw(2) << std::setfill('0') << std::hex << (u16)actorId.data[15 - i];
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