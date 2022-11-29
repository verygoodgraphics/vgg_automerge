// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <utility>
#include <optional>

#include "../type.h"
#include "../Query.h"

class QueryKeys {
public:
    QueryKeys(const OpTreeNode* r);

    std::optional<Key> next();

    std::optional<Key> next_back();

private:
    usize index = 0;
    std::optional<Key> last_key;
    usize index_back = 0;
    std::optional<Key> last_key_back;
    const OpTreeNode* root_child = nullptr;
};