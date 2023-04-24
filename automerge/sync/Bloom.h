// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <vector>
#include <optional>

#include "../type.h"
#include "../helper.h"
#include "../Encoder.h"

// These constants correspond to a 1% false positive rate. The values can be changed without
// breaking compatibility of the network protocol, since the parameters used for a particular
// Bloom filter are encoded in the wire format.
constexpr u32 BITS_PER_ENTRY = 10;
constexpr u32 NUM_PROBES = 7;

// #[derive(Debug, Clone, PartialEq, Eq, Hash, serde::Serialize)]
struct BloomFilter {
    u32 num_entries = 0;
    u32 num_bits_per_entry = BITS_PER_ENTRY;
    u32 num_probes = NUM_PROBES;
    std::vector<u8> bits;

    BloomFilter() = default;
    BloomFilter(u32 num_entries, u32 num_bits_per_entry, u32 num_probes, std::vector<u8>&& bits) :
        num_entries(num_entries), num_bits_per_entry(num_bits_per_entry), num_probes(num_probes), bits(std::move(bits)) {}
    // from_hashes
    BloomFilter(std::vector<const ChangeHash*>&& hashes);

    // try_from &[u8]
    static std::optional<BloomFilter> decode(const BinSlice& bytes);

    static usize bits_capacity(u32 num_entries, u32 num_bits_per_entry);

    std::vector<u8> to_bytes() const;

    std::vector<u32> get_probes(const ChangeHash& hash) const;

    void add_hash(const ChangeHash& hash);

    void set_bit(usize probe);

    std::optional<u8> get_bit(usize probe) const;

    bool contains_hash(const ChangeHash& hash) const;
};
