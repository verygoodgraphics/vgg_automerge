// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <vector>
#include <iterator>
#include <algorithm> 
#include <cassert>
#include <string_view>

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

template <class T>
std::vector<const T*> vector_to_vector_of_pointer(const std::vector<T>& source) {
    std::vector<const T*> target(source.size());
    std::transform(source.begin(), source.end(), target.begin(), [](const T& t) { return &t; });
    return target;
}

template <class T>
std::vector<T> vector_of_pointer_to_vector(const std::vector<const T*>& source) {
    std::vector<T> target;
    target.reserve(source.size());

    std::transform(source.begin(), source.end(), 
        std::back_inserter(target),
        [](const T* t) { return *t; });

    return target;
}

u64 get_random_64();

BinSlice make_bin_slice(const std::vector<u8>& bin_vec);

bool bin_slice_cmp(const BinSlice& a, const BinSlice& b);

std::vector<u8> deflate_compress(const BinSlice& data);

std::vector<u8> deflate_decompress(const BinSlice& data);

template<class T>
std::string hex_to_string(const std::pair<T, usize>& hex_bytes) {
    const std::string_view hex_char = "0123456789abcdef";
    std::string str;

    str.reserve(hex_bytes.second * 2);

    auto iter = hex_bytes.first;
    for (usize i = 0; i < hex_bytes.second; ++i) {
        str.push_back(hex_char[*iter >> 4]);
        str.push_back(hex_char[*iter & 0x0f]);

        ++iter;
    }

    return str;
}

std::vector<u8> hex_from_string(const std::string_view& hex_str);
