// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <vector>
#include <unordered_set>
#include <optional>

#include "type.h"
#include "Change.h"
#include "sync/State.h"

// first byte of a sync message, for identification
constexpr u8 MESSAGE_TYPE_SYNC = 0x42;

// The sync message to be sent.
// #[derive(Clone, Debug, PartialEq)]
struct SyncMessage {
    // The heads of the sender
    std::vector<ChangeHash> heads;
    // The hashes of any changes that are being explicitly requested from the recipient
    std::vector<ChangeHash> need;
    // A summary of the changes that the sender already has
    std::vector<Have> have;
    // The changes for the recipient to apply
    std::vector<Change> changes;

    std::vector<u8> encode();

    std::optional<SyncMessage> decode(const BinSlice& bytes);
};

std::vector<ChangeHash> advance_heads(
    const std::unordered_set<ChangeHash>& my_old_heads,
    const std::unordered_set<ChangeHash>& my_new_heads,
    const std::vector<ChangeHash>& our_old_shared_heads
);