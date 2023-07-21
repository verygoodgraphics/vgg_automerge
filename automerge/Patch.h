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
#include "ExId.h"
#include "Value.h"

struct PatchPut {
    ExId obj = {};
    Prop key = {};
    ValuePair value = {};
    bool conflict = false;

    bool operator==(const PatchPut& other) const {
        return (obj == other.obj) && (key == other.key) && (value == other.value) && (conflict == other.conflict);
    }
};

struct PatchInsert {
    ExId obj = {};
    usize index = 0;
    ValuePair value = {};

    bool operator==(const PatchInsert& other) const {
        return (obj == other.obj) && (index == other.index) && (value == other.value);
    }
};

struct PatchIncrement {
    ExId obj = {};
    Prop key = {};
    S64Pair value = {};

    bool operator==(const PatchIncrement& other) const {
        return (obj == other.obj) && (key == other.key) && (value == other.value);
    }
};

struct PatchDelete {
    ExId obj = {};
    Prop key = {};

    bool operator==(const PatchDelete& other) const {
        return (obj == other.obj) && (key == other.key);
    }
};

struct Patch {
    enum {
        PUT,
        INSERT,
        INCREMENT,
        DELETE
    } tag = PUT;
    std::variant<PatchPut, PatchInsert, PatchIncrement, PatchDelete> data = {};

    bool operator==(const Patch& other) const {
        return (tag == other.tag) && (data == other.data);
    }
};
