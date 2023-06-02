// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#define SINFL_IMPLEMENTATION
#define SDEFL_IMPLEMENTATION

#include "sdefl.h"
#include "sinfl.h"

#include <random>

#include "type.h"
#include "helper.h"

u64 get_random_64() {
    static std::random_device rd;   // random seed
    static std::mt19937_64 gen(rd()); // random engine
    return gen();
}

BinSlice make_bin_slice(const std::vector<u8>& bin_vec) {
    return { bin_vec.cbegin(), bin_vec.size() };
}

bool bin_slice_cmp(const BinSlice& a, const BinSlice& b) {
    if (a.second != b.second) {
        return false;
    }

    if (a.second == 0) {
        return true;
    }

    BinIter ia = a.first, ib = b.first;
    for (usize i = 0; i < a.second; ++i) {
        if (*ia != *ib) {
            return false;
        }

        ia += 1;
        ib += 1;
    }

    return true;
}

static struct sdefl sdefl;

std::vector<u8> deflate_compress(const BinSlice& data) {
    u8* comp = new u8[data.second * 2]();

    int len = sdeflate(&sdefl, comp, &(*data.first), (int)data.second, SDEFL_LVL_DEF);
    std::vector<u8> res(std::make_move_iterator(comp), std::make_move_iterator(comp + len));

    delete[]comp;

    return res;
}

std::vector<u8> deflate_decompress(const BinSlice& data) {
    u8* decomp = new u8[data.second * 128]();

    int n = sinflate(decomp, (int)data.second * 128, &(*data.first), (int)data.second);
    std::vector<u8> res(std::make_move_iterator(decomp), std::make_move_iterator(decomp + n));

    delete[]decomp;

    return res;
}

static u8 hex_value(u8 hex_digit) {
    static const int hex_values[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
        -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    };
    auto value = hex_values[hex_digit];
    if (value == -1) {
        throw std::invalid_argument("invalid bytes digit");
    }

    return (u8)value;
}

std::vector<u8> hex_from_string(const std::string& hex_str) {
    std::vector<u8> bytes;
    usize len = hex_str.length();

    if (len % 2) {
        throw std::invalid_argument("invalid bytes string");
    }
    bytes.reserve(len / 2);

    for (usize i = 0; i < len; i += 2) {
        u8 hi = hex_value(hex_str[i]);
        u8 lo = hex_value(hex_str[i + 1]);

        bytes.push_back(hi << 4 | lo);
    }

    return bytes;
}
