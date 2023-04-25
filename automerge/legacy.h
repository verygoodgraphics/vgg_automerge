// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include "type.h"
#include "Op.h"
#include "IndexedCache.h"

// #[derive(Eq, PartialEq, Hash, Clone)]
struct OldOpId {
    u64 counter = 0;
    ActorId actor;

    OldOpId() = default;
    OldOpId(u64 counter, const ActorId& actor) : counter(counter), actor(actor) {}
    OldOpId(const OpId& id, const IndexedCache<ActorId>& actors)
        : counter(id.counter), actor(actors.get(id.actor)) {}

    bool operator==(const OldOpId& other) const {
        return (counter == other.counter && actor == other.actor);
    }
    bool operator<(const OldOpId& other) const {
        return (counter < other.counter) || (counter == other.counter && actor < other.actor);
    }
};

// #[derive(Eq, PartialEq, Debug, Hash, Clone)]
struct OldObjectId {
    bool isRoot;
    OldOpId id;

    OldObjectId(bool isRoot = false) : isRoot(isRoot), id() {}
    OldObjectId(const OldOpId& id) : isRoot(false), id(id) {}
    OldObjectId(const ObjId& id, const IndexedCache<ActorId>& actors) {
        if (id == ROOT) {
            isRoot = true;
        }
        else {
            isRoot = false;
            this->id = OldOpId(id, actors);
        }
    }

    bool operator==(const OldObjectId& other) const {
        return (isRoot == other.isRoot && (isRoot || id == other.id));
    }
};

// #[derive(PartialEq, Eq, Debug, Hash, Clone)]
struct OldElementId {
    bool isHead;
    OldOpId id;

    OldElementId(bool isHead = true) :isHead(isHead), id() {}
    OldElementId(const OldOpId& id) :isHead(false), id(id) {}
    OldElementId(const ElemId& id, const IndexedCache<ActorId>& actors) {
        if (id == HEAD) {
            isHead = true;
        }
        else {
            isHead = false;
            this->id = OldOpId(id, actors);
        }
    }

    bool operator==(const OldElementId& other) const {
        return (isHead == other.isHead && (isHead || id == other.id));
    }
};

// #[derive(Serialize, PartialEq, Eq, Debug, Hash, Clone)]
struct OldKey {
    enum {
        MAP,
        SEQ
    } tag = MAP;
    std::variant<std::string, OldElementId> data = {};

    static OldKey head() {
        return { SEQ, OldElementId(true) };
    }

    bool is_map_key() const {
        return (tag == MAP);
    }

    const OldElementId& get_element_id() const {
        return std::get<OldElementId>(data);
    }

    auto as_element_id() const {
        return is_seq() ? std::optional<OldElementId>{ std::get<OldElementId>(data) } : std::nullopt;
    }

    bool is_seq() const {
        return (tag == SEQ);
    }

    bool operator==(const OldKey& other) const {
        return (tag == other.tag) && (data == other.data);
    }
};

// #[derive(PartialEq, Debug, Clone)]
struct OldOp {
    OpType action;
    OldObjectId obj;
    OldKey key;
    std::vector<OldOpId> pred; // sorted
    bool insert;

    OldOp() = default;
    OldOp(OpType&& action, OldObjectId&& obj, OldKey&& key, std::vector<OldOpId>&& pred, bool insert)
        : action(std::move(action)), obj(std::move(obj)), key(std::move(key)), pred(std::move(pred)), insert(insert) {}
    OldOp(const Op& op, const ObjId& obj, const IndexedCache<ActorId>& actors, const IndexedCache<std::string>& props);

    bool operator==(const OldOp& other) const;

    std::vector<const ActorId*> opids_in_operation() const;
};
