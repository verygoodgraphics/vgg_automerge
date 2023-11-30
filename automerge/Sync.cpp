// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include "Sync.h"
#include "Decoder.h"

std::vector<u8> SyncMessage::encode() {
    std::vector<u8> buf;
    buf.push_back(MESSAGE_TYPE_SYNC);

    Encoder encoder(buf);

    encoder.encode(heads);
    encoder.encode(need);

    encoder.encode((u64)have.size());    
    for (auto& h : have) {
        encoder.encode(h.last_sync);
        encoder.encode(h.bloom.to_bytes());
    }

    encoder.encode((u64)changes.size());
    for (auto& change : changes) {
        change.compress();
        encoder.encode(change.bytes.raw());
    }

    return buf;
}

std::optional<SyncMessage> SyncMessage::decode(const BinSlice& bytes) {
    Decoder decoder(bytes);

    auto message_type = decoder.read<u8>();
    if (!message_type.has_value()) {
        return {};
    }
    if (*message_type != MESSAGE_TYPE_SYNC) {
        // throw WrongType
        return {};
    }

    auto heads = decode_hashes(decoder);
    if (!heads.has_value()) {
        return {};
    }

    auto need = decode_hashes(decoder);
    if (!need.has_value()) {
        return {};
    }

    auto have_count = decoder.read<u64>();
    if (!have_count.has_value()) {
        return {};
    }

    std::vector<Have> have;
    have.reserve(*have_count);
    for (usize i = 0; i < *have_count; ++i) {
        auto last_sync = decode_hashes(decoder);
        if (!last_sync.has_value()) {
            return {};
        }

        auto bloom_bytes = decoder.read<std::vector<u8>>();
        if (!bloom_bytes.has_value()) {
            return {};
        }

        auto bloom = BloomFilter::parse(BinSlice(bloom_bytes->cbegin(), bloom_bytes->size()));
        if (!bloom.has_value()) {
            return {};
        }

        have.push_back(Have{
            std::move(*last_sync),
            std::move(*bloom)
            });
    }

    auto change_count = decoder.read<u64>();
    if (!change_count.has_value()) {
        return {};
    }

    std::vector<Change> changes;
    changes.reserve(*change_count);
    for (usize i = 0; i < *change_count; ++i) {
        auto change_bytes = decoder.read<std::vector<u8>>();
        if (!change_bytes.has_value()) {
            return {};
        }

        auto change = Change::from_bytes(std::move(*change_bytes));
        if (!change.has_value()) {
            return {};
        }

        changes.push_back(std::move(*change));
    }

    return SyncMessage{
        std::move(*heads),
        std::move(*need),
        std::move(have),
        std::move(changes)
    };
}

// TODO: optimise, use pointer in set
std::vector<ChangeHash> advance_heads(
    std::unordered_set<ChangeHash>&& my_old_heads,
    std::unordered_set<ChangeHash>&& my_new_heads,
    std::vector<ChangeHash>&& our_old_shared_heads
) {
    std::vector<ChangeHash> advanced_heads;
    for (auto& head : my_new_heads) {
        if (!my_old_heads.count(head)) {
            // items in set can't move
            advanced_heads.push_back(head);
        }
    }

    for (auto& head : our_old_shared_heads) {
        if (my_new_heads.count(head)) {
            advanced_heads.push_back(std::move(head));
        }
    }

    std::sort(advanced_heads.begin(), advanced_heads.end());

    auto last = std::unique(advanced_heads.begin(), advanced_heads.end());
    advanced_heads.erase(last, advanced_heads.end());

    return advanced_heads;
}
