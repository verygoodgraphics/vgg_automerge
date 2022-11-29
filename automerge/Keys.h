// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <utility>
#include <optional>

#include "type.h"
#include "ExId.h"
#include "query/QueryKeys.h"

struct Automerge;

struct Keys {
    std::optional<QueryKeys> keys;
    const Automerge* doc = nullptr;

    Keys(const Automerge* d, std::optional<QueryKeys> k) : keys(k), doc(d) {}

    std::optional<std::string> next();

    std::optional<std::string> next_back();

    usize count();
};
