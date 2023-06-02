// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <optional>
#include <algorithm>

#include "type.h"
#include "helper.h"

template <class T>
class IndexedCache {
public:
    std::vector<T> _cache;

    IndexedCache() = default;
    IndexedCache(const IndexedCache&) = default;
    IndexedCache(IndexedCache&&) = default;
    IndexedCache& operator=(const IndexedCache&) = default;
    IndexedCache& operator=(IndexedCache&&) = default;

    IndexedCache(const std::vector<T>& other_cache) : _cache(other_cache) {}

    bool operator==(const IndexedCache<T>& other) const {
        return (_cache == other._cache);
    }

    T& operator[](usize index) {
        return _cache[index];
    }
    const T& operator[](usize index) const {
        return _cache[index];
    }

    usize cache(T&& item);

    std::optional<usize> lookup(const T& item) const;

    usize len() const {
        return _cache.size();
    }

    const T& get(usize index) const {
        return _cache[index];
    }

    T remove_last();

    IndexedCache<T> sorted() const;

    std::vector<usize> encode_index() const;

    void print() const;

private:
    std::unordered_map<T, usize> _lookup;
};

template <class T>
usize IndexedCache<T>::cache(T&& item) {
    auto result = _lookup.find(item);
    if (result != _lookup.end()) {
        return result->second;
    }

    usize n = _cache.size();
    _cache.push_back(item);
    _lookup.insert({ std::move(item), n });

    return n;
}

template <class T>
std::optional<usize> IndexedCache<T>::lookup(const T& item) const {
    auto result = _lookup.find(item);
    return (result != _lookup.end()) ? std::optional<usize>{ result->second } : std::nullopt;
}

template <class T>
T IndexedCache<T>::remove_last() {
    T t = vector_pop(_cache);
    _lookup.erase(t);

    return t;
}

template <class T>
IndexedCache<T> IndexedCache<T>::sorted() const {
    IndexedCache<T> res(this->_cache);
    std::sort(res._cache.begin(), res._cache.end());
    for (usize i = 0; i < res._cache.size(); ++i) {
        res._lookup[res._cache[i]] = i;
    }

    return res;
}

template <class T>
std::vector<usize> IndexedCache<T>::encode_index() const {
    std::vector<T> sorted_vec(this->_cache);
    std::sort(sorted_vec.begin(), sorted_vec.end());

    std::vector<usize> res;
    res.reserve(sorted_vec.size());
    for (auto& t : this->_cache) {
        res.push_back(std::lower_bound(sorted_vec.begin(), sorted_vec.end(), t) - sorted_vec.begin());
    }
    return res;
}

template <class T>
void IndexedCache<T>::print() const {
    std::cout << "cache: ";
    for (auto item : _cache) {
        std::cout << item << " ";
    }

    std::cout << std::endl << "lookup: ";
    for (auto item : _lookup) {
        std::cout << item.first << ":" << item.second << " ";
    }
    std::cout << std::endl;
}
