// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <optional>
#include <variant>
#include <functional>
#include <stdexcept>

#include "type.h"

// #[derive(Default, Debug, Clone, Copy, PartialEq)]
struct ClockData {
    // Maximum operation counter of the actor at the point in time.
    u64 max_op = 0;
    // Sequence number of the change from this actor.
    u64 seq = 0;

    bool operator==(const ClockData& other) const {
        return cmp(other) == 0;
    }

    bool operator<(const ClockData& other) const {
        return cmp(other) < 0;
    }

    int cmp(const ClockData& other) const {
        if (max_op < other.max_op)
            return -1;
        if (max_op == other.max_op)
            return 0;
        return 1;
    }
};

// Vector clock mapping actor indices to the max op counter of the changes created by that actor.
// #[derive(Default, Debug, Clone, PartialEq)]
struct Clock {
    std::unordered_map<usize, ClockData> clock;

    // A general clock is greater if it has one element the other does not or has a counter higher than
    // the other for a given actor.
    //
    // It is equal with another clock if it has the same entries everywhere.
    //
    // It is less than another clock otherwise.
    std::optional<int> cmp(const Clock& other) const {
        if (clock == other.clock)
            return { 0 };
        if (is_greater(other))
            return { 1 };
        if (other.is_greater(*this)) {
            return { -1 };
        }

        // concurrent
        return std::nullopt;
    }

    void include(usize actor_index, ClockData data) {
        try {
            auto& d = clock.at(actor_index);
            if (data.max_op > d.max_op) {
                d = data;
            }
        }
        catch (std::out_of_range&) {
            clock.insert({ actor_index, data });
        }
    }

    bool covers(const OpId& id) const {
        try {
            auto& data = clock.at(id.actor);
            return data.max_op >= id.counter;
        }
        catch (std::out_of_range&) {
            return false;
        }
    }

    std::optional<ClockData> get_for_actor(usize actor_index) {
        try {
            return clock.at(actor_index);
        }
        catch (std::out_of_range&) {
            return {};
        }
    }

private:
    bool is_greater(const Clock& other) const {
        bool has_greater = false;
        usize others_found = 0;

        for (auto& [actor, data] : clock) {
            try {
                auto& other_data = other.clock.at(actor);
                if (data < other_data) {
                    // may be concurrent or less
                    return false;
                }
                else if (other_data < data) {
                    has_greater = true;
                }
                ++others_found;
            }
            catch (std::out_of_range&) {
                // other doesn't have this so effectively has a greater element
                has_greater = true;
            }
        }

        if (has_greater) {
            // if they are equal then we have seen every key in the other clock and have at least
            // one greater element so our clock is greater
            //
            // If they aren't the same then we haven't seen every key but have a greater element
            // anyway so are concurrent
            return (others_found == other.clock.size());
        }
        else {
            // our clock doesn't have anything greater than the other clock so can't be greater but
            // could still be concurrent
            return false;
        }
    }
};
