// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <vector>
#include <set>
#include <optional>

#include "../type.h"
#include "../Decoder.h"
#include "Bloom.h"

// first byte of an encoded sync state, for identification
constexpr u8 SYNC_STATE_TYPE = 0x43;

// A summary of the changes that the sender of the message already has.
// This is implicitly a request to the recipient to send all changes that the
// sender does not already have.
// #[derive(Debug, Clone, Default, PartialEq, Eq, Hash)]
struct Have {
    // The heads at the time of the last successful sync with this recipient.
    std::vector<ChangeHash> last_sync;
    // A bloom filter summarising all of the changes that the sender of the message has added
    // since the last sync.
    BloomFilter bloom;
};

// The state of synchronisation with a peer.
// #[derive(Debug, Clone, Default, PartialEq, Eq, Hash)]
struct State{
    std::vector<ChangeHash> shared_heads;
    std::vector<ChangeHash> last_sent_heads;
    std::optional<std::vector<ChangeHash>> their_heads;
    std::optional<std::vector<ChangeHash>> their_need;
    std::optional<std::vector<Have>> their_have;
    std::set<ChangeHash> sent_hashes;

    std::vector<u8> encode() const;
    static std::optional<State> decode(const BinSlice& bytes);
};

std::optional<std::vector<ChangeHash>> decode_hashes(Decoder& decoder);