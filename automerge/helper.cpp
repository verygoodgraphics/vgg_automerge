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

bool bin_slice_cmp(const BinSlice& a, const BinSlice& b) {
    if (a.second != b.second) {
        return false;
    }

    if (a.second == 0) {
        return true;
    }

    auto ia = a.first, ib = b.first;
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

std::vector<u8> deflate_compress(const std::vector<u8>& data) {
    u8* comp = new u8[data.size() * 2]();

    int len = sdeflate(&sdefl, comp, data.data(), (int)data.size(), SDEFL_LVL_DEF);
    std::vector<u8> res(std::make_move_iterator(comp), std::make_move_iterator(comp + len));

    delete[]comp;

    return res;
}

std::vector<u8> deflate_decompress(const BinSlice& data) {
    u8* decomp = new u8[data.second * 2]();

    int n = sinflate(decomp, (int)data.second * 2, &(*data.first), (int)data.second);
    std::vector<u8> res(std::make_move_iterator(decomp), std::make_move_iterator(decomp + n));

    delete[]decomp;

    return res;
}