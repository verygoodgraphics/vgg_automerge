// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "StringCache.h"

BufferBlcok::BufferBlcok(usize len) {
    size = std::max(len, BUFFER_BLOCK_SIZE);
    begin = 0;
    end = size;
    data = new char[size]();
}

BufferBlcok& BufferBlcok::get_buffer_block(usize len) {
    static std::vector<BufferBlcok> string_buffer;

    auto iter = string_buffer.begin();
    for (; iter != string_buffer.end(); ++iter) {
        if (iter->rest_size() >= len * 2) {
            break;
        }
    }

    if (iter == string_buffer.end()) {
        string_buffer.emplace_back(len);
        iter = std::prev(string_buffer.end());
    }

    return *iter;
}

std::string_view BufferBlcok::cache_string(const std::string& str) {
    auto len = str.size();
    if (len > rest_size()) {
        return {};
    }

    auto dest = data + begin;

    std::copy(str.cbegin(), str.cend(), dest);
    begin += len;

    return std::string_view(dest, len);
}

IndexedCache<std::string_view> StringIndexedCache;

StringCache g_cache_string(const std::string& str) {
    if (str.size() == 0) {
        return {};
    }

    std::string_view item_view = str;

    auto result = StringIndexedCache._lookup.find(item_view);
    if (result != StringIndexedCache._lookup.end()) {
        return { result->second, result->first };
    }

    auto persistent_view = BufferBlcok::get_buffer_block(str.size()).cache_string(str);

    usize n = StringIndexedCache._cache.size();
    StringIndexedCache._lookup.emplace(persistent_view, n);
    StringIndexedCache._cache.push_back(persistent_view);

    return { n, persistent_view };
}
