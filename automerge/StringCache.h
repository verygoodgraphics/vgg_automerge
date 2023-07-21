// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <optional>

#include "type.h"
#include "IndexedCache.h"

constexpr usize BUFFER_BLOCK_SIZE = 64 * 1024;

class BufferBlcok {
public:
    BufferBlcok(usize len = BUFFER_BLOCK_SIZE);

    BufferBlcok(BufferBlcok&& b) noexcept :
        data(b.data), size(b.size), begin(b.begin), end(b.end) {
        b.data = nullptr;
    }

    BufferBlcok(const BufferBlcok&) = delete;
    BufferBlcok& operator=(const BufferBlcok&) = delete;
    BufferBlcok& operator=(BufferBlcok&&) = delete;

    ~BufferBlcok() {
        delete[] data;
    }

    static BufferBlcok& get_buffer_block(usize len);

    usize rest_size() const {
        return end - begin;
    }

    std::string_view cache_string(const std::string& str);

private:
    char* data = nullptr;
    usize size = 0;
    usize begin = 0;
    usize end = 0;
};

using StringCache = std::pair<usize, std::string_view>;

StringCache g_cache_string(const std::string& str);
