// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include "../type.h"
#include "../ExId.h"
#include "../Keys.h"
#include "../Value.h"

struct Transactable {
    // throw AutomergeError
    virtual void put(const ExId& obj, Prop&& prop, ScalarValue&& value) = 0;

    // throw AutomergeError
    virtual ExId put_object(const ExId& obj, Prop&& prop, ObjType object) = 0;

    // throw AutomergeError
    virtual void insert(const ExId& obj, usize index, ScalarValue&& value) = 0;

    // throw AutomergeError
    virtual ExId insert_object(const ExId& obj, usize index, ObjType object) = 0;

    // throw AutomergeError
    virtual void increment(const ExId& obj, Prop&& prop, s64 value) = 0;

    // throw AutomergeError
    virtual void delete_(const ExId& obj, Prop&& prop) = 0;

    virtual Keys keys(const ExId& obj) const = 0;

    virtual usize length(const ExId& obj) const = 0;

    // throw AutomergeError
    virtual std::optional<ValuePair> get(const ExId& obj, Prop&& prop) const = 0;

    // throw AutomergeError
    virtual std::vector<ValuePair> get_all(const ExId& obj, Prop&& prop) const = 0;
};