// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <vector>
#include <iterator>
#include <cassert>

#include "type.h"

template <class T>
std::vector<T> vector_split_off(std::vector<T>& source, usize index) {
    assert(source.size() >= index);

    std::vector<T> back(
        std::make_move_iterator(source.begin() + index),
        std::make_move_iterator(source.end())
    );
    source.erase(source.begin() + index, source.end());

    return back;
}

template <class T>
T vector_pop(std::vector<T>& v) {
    assert(!v.empty());

    T item(std::move(v.back()));
    v.pop_back();

    return item;
}

template <class T>
T vector_remove(std::vector<T>& v, usize index) {
    assert(v.size() > index);

    auto iter = std::next(v.begin(), index);
    T item(std::move(*iter));
    v.erase(iter);

    return item;
}

template <class T>
void vector_extend(std::vector<T>& dest, std::vector<T>&& src) {
    dest.insert(dest.end(), std::make_move_iterator(src.begin()), std::make_move_iterator(src.end()));
}

template <class T>
void vector_extend(std::vector<T>& dest, const std::vector<T>& src) {
    dest.insert(dest.end(), src.cbegin(), src.cend());
}

u64 get_random_64();

bool bin_slice_cmp(const BinSlice& a, const BinSlice& b);

std::vector<u8> deflate_compress(const BinSlice& data);

std::vector<u8> deflate_decompress(const BinSlice& data);
