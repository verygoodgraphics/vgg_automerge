// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include <cmath>
#include "Bloom.h"
#include "../Decoder.h"

std::vector<u8> BloomFilter::to_bytes() const {
    std::vector<u8> buf;
    Encoder encoder(buf);

    if (num_entries != 0) {
        encoder.encode(num_entries);
        encoder.encode(num_bits_per_entry);
        encoder.encode(num_probes);
        vector_extend(buf, bits);
    }

    return buf;
}

std::optional<BloomFilter> BloomFilter::parse(const BinSlice& bytes) {
    if (bytes.second == 0) {
        return BloomFilter();
    }

    Decoder decoder(bytes);

    auto num_entries = decoder.read<u32>();
    if (!num_entries.has_value()) {
        return {};
    }

    auto num_bits_per_entry = decoder.read<u32>();
    if (!num_bits_per_entry.has_value()) {
        return {};
    }

    auto num_probes = decoder.read<u32>();
    if (!num_probes.has_value()) {
        return {};
    }

    auto bits = decoder.read_bytes(bits_capacity(*num_entries, *num_bits_per_entry));
    if (!bits.has_value()) {
        return {};
    }

    return BloomFilter(
        *num_entries,
        *num_bits_per_entry,
        *num_probes,
        std::vector<u8>(bits->first, bits->first + bits->second)
    );
}

std::vector<u32> BloomFilter::get_probes(const ChangeHash& hash) const {
    const u8* hash_bytes = hash.data;
    u32 modulo = 8 * (u32)bits.size();

    auto bytes_to_u32 = [&](u32 offset) -> u32 {
        auto start = hash_bytes + offset;
        u32 res = 0;

        int num = 1;
        if (*(char*)&num == 1) {
            // Little-Endian
            std::copy(start, start + 4, reinterpret_cast<u8*>(&res));
        }
        else {
            // Big-Endian
            std::copy(std::make_reverse_iterator(start + 4), std::make_reverse_iterator(start), reinterpret_cast<u8*>(&res));
        }

        return res;
    };

    u32 x = bytes_to_u32(0) % modulo;
    u32 y = bytes_to_u32(4) % modulo;
    u32 z = bytes_to_u32(8) % modulo;

    std::vector<u32> probes;
    probes.reserve(num_probes);
    probes.push_back(x);
    for (u32 i = 1; i < num_probes; ++i) {
        x = (x + y) % modulo;
        y = (y + z) % modulo;
        probes.push_back(x);
    }

    return probes;
}

void BloomFilter::add_hash(const ChangeHash& hash) {
    for (auto probe : get_probes(hash)) {
        set_bit((usize)probe);
    }
}

void BloomFilter::set_bit(usize probe) {
    auto index = probe >> 3;
    if (index < bits.size()) {
        bits[index] |= 1 << (probe & 7);
    }
}

std::optional<u8> BloomFilter::get_bit(usize probe) const {
    auto index = probe >> 3;
    if (index >= bits.size()) {
        return {};
    }

    return bits[index] & (1 << (probe & 7));
}

bool BloomFilter::contains_hash(const ChangeHash& hash) const {
    if (num_entries == 0) {
        return false;
    }

    for (auto probe : get_probes(hash)) {
        auto bit = get_bit((usize)probe);
        if (bit && (*bit == 0)) {
            return false;
        }
    }

    return true;
}

BloomFilter::BloomFilter(std::vector<const ChangeHash*>&& hashes) {
    num_entries = (u32)hashes.size();
    num_bits_per_entry = BITS_PER_ENTRY;
    num_probes = NUM_PROBES;
    bits = std::vector<u8>(bits_capacity(num_entries, num_bits_per_entry), 0);

    for (auto hash : hashes) {
        add_hash(*hash);
    }
}

usize BloomFilter::bits_capacity(u32 num_entries, u32 num_bits_per_entry) {
    double f = 1.0 * num_entries * num_bits_per_entry / 8;
    return (usize)std::ceil(f);
}
