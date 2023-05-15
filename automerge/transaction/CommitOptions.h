// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <optional>

#include "../type.h"

template<class Obs>
struct CommitOptions {
    std::optional<std::string> message;
    std::optional<s64> time;
    std::optional<Obs*> op_observer;

    CommitOptions() = default;
    CommitOptions(std::optional<std::string>&& m, std::optional<s64>&& t, std::optional<Obs*>&& o)
        : message(std::move(m)), time(std::move(t)), op_observer(std::move(o)) {}
};