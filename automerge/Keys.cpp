// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "Keys.h"
#include "Automerge.h"

std::optional<std::string> Keys::next() {
    if (!keys) {
        return {};
    }

    auto key = keys->next();
    if (!key) {
        return {};
    }

    return doc->to_string(Export(*key));
}

std::optional<std::string> Keys::next_back() {
    auto key = keys->next_back();
    if (!key) {
        return {};
    }

    return doc->to_string(Export(*key));
}

usize Keys::count() {
    usize res = 0;
    while (next()) {
        ++res;
    }

    return res;
}