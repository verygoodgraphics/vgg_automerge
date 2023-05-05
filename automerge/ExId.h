// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <tuple>

#include "type.h"
#include "Value.h"

struct ExId {
    bool isRoot;
    std::tuple<u64, ActorId, usize> id;

    ExId() :isRoot(true) {}
    ExId(u64 _1, ActorId _2, usize _3) : isRoot(false), id{ _1, _2, _3 } {}

    bool operator==(const ExId& other) const {
        return cmp(other) == 0;
    }

    bool eq(const ExId& other) const {
        return cmp(other) == 0;
    }

    int cmp(const ExId& other) const {
        if (isRoot && other.isRoot)
            return 0;
        if (isRoot && !other.isRoot)
            return -1;
        if (!isRoot && other.isRoot)
            return 1;

        if (std::get<0>(id) == std::get<0>(other.id))
            return std::get<1>(id).cmp(std::get<1>(other.id));

        if (std::get<0>(id) < std::get<0>(other.id))
            return -1;

        return 1;
    }

    std::string to_string() const {
        if (isRoot) {
            return "_root";
        }

        std::string str = std::to_string(std::get<0>(id)) + '@';
        str.append(std::get<1>(id).to_hex());

        return str;
    }
};

typedef std::pair<ExId, Prop> PropPair;
typedef std::pair<ExId, Value> ValuePair;
typedef std::tuple<ExId, Value, PropPair> ValueTuple;
typedef std::pair<ExId, s64> S64Pair;

struct JsonPathParsed {
    enum {
        // not a valid path: its parent isn't found or isn't an object
        // variant: std::string(the root path which not found or not an object)
        Invalid,
        // path is not found, its parent is found
        // variant: PropPair<ExId, Prop>(parent's ExId, new path's Prop)
        NewPath,
        // path found
        // variant: ValueTuple<ExId, Value, PropPair>
        //          (path's Value and PropPair(parent's ExId, path's Prop); path's ExId only for object)
        ExistedPath
    } tag = Invalid;
    std::variant<std::string, PropPair, ValueTuple> data = {};
};
