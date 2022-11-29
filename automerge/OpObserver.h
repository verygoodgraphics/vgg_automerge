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
#include "Patch.h"

class OpObserver {
public:
    virtual void insert(ExId objid, usize index, ValuePair tagged_value) = 0;

    virtual void put(ExId objid, Prop key, ValuePair tagged_value, bool conflict) = 0;

    virtual void increment(ExId objid, Prop key, S64Pair tagged_value) = 0;

    virtual void _delete(ExId objid, Prop key) = 0;
};

class VecOpObserver : public OpObserver {
public:
    /// Take the current list of patches, leaving the internal list empty and ready for new
    /// patches.
    std::vector<Patch>&& take_patches() {
        return std::move(patches);
    }

    void insert(ExId objid, usize index, ValuePair tagged_value) override {
        patches.push_back(Patch{
            Patch::INSERT,
            PatchInsert{
                objid,
                index,
                tagged_value
            }
            });
    }

    void put(ExId objid, Prop key, ValuePair tagged_value, bool conflict) override {
        patches.push_back(Patch{
            Patch::PUT,
            PatchPut{
                objid,
                key,
                tagged_value,
                conflict
            }
            });
    }

    void increment(ExId objid, Prop key, S64Pair tagged_value) override {
        patches.push_back(Patch{
            Patch::INCREMENT,
            PatchIncrement{
                objid,
                key,
                tagged_value
            }
            });
    }

    void _delete(ExId objid, Prop key) override {
        patches.push_back(Patch{
            Patch::DELETE,
            PatchDelete{
                objid,
                key
            }
            });
    }

private:
    std::vector<Patch> patches = {};
};
