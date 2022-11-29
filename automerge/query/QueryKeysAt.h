// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <utility>
#include <optional>

#include "../type.h"
#include "../Query.h"
#include "../Clock.h"

class QueryKeysAt {
public:
    QueryKeysAt(OpTreeNode* r, Clock&& clock);

private:
    Clock clock;
    // TODO: VisWindow
    usize index = 0;
    std::optional<Key> last_key;
    usize index_back = 0;
    std::optional<Key> last_key_back;
    OpTreeNode* root_child = nullptr;
};