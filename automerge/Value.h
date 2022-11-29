// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <optional>
#include <variant>

#include "type.h"

struct Value {
    enum {
        OBJECT,
        SCALAR
    } tag = OBJECT;
    // TODO: optimise cow
    std::variant<ObjType, ScalarValue> data = {};

    bool operator==(const Value& other) const {
        return (tag == other.tag) && (data == other.data);
    }
};