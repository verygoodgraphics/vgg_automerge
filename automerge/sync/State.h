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
// #[derive(Debug, Clone, Default, PartialEq, Eq, Hash, serde::Serialize)]
struct Have {
    // The heads at the time of the last successful sync with this recipient.
    std::vector<ChangeHash> last_sync;
    // A bloom filter summarising all of the changes that the sender of the message has added
    // since the last sync.
    BloomFilter bloom;
};

// The state of synchronisation with a peer.
//
// This should be persisted using [`Self::encode`] when you know you will be interacting with the
// same peer in multiple sessions. [`Self::encode`] only encodes state which should be reused
// across connections.
// #[derive(Debug, Clone, Default, PartialEq, Eq, Hash)]
struct State{
    // The hashes which we know both peers have
    std::vector<ChangeHash> shared_heads;
    // The heads we last sent
    std::vector<ChangeHash> last_sent_heads;
    // The heads we last received from them
    std::optional<std::vector<ChangeHash>> their_heads;
    // Any specific changes they last said they needed
    std::optional<std::vector<ChangeHash>> their_need;
    // The bloom filters summarising what they said they have
    std::optional<std::vector<Have>> their_have;
    // The hashes we have sent in this session
    std::set<ChangeHash> sent_hashes;

    // `generate_sync_message` should return `None` if there are no new changes to send. In
    // particular, if there are changes in flight which the other end has not yet acknowledged we
    // do not wish to generate duplicate sync messages. This field tracks whether the changes we
    // expect to send to the peer based on this sync state have been sent or not. If
    // `in_flight` is `false` then `generate_sync_message` will return a new message (provided
    // there are in fact changes to send). If it is `true` then we don't. This flag is cleared
    // in `receive_sync_message`.
    bool in_flight = false;

    std::vector<u8> encode() const;
    static std::optional<State> decode(const BinSlice& bytes);
};

std::optional<std::vector<ChangeHash>> decode_hashes(Decoder& decoder);
