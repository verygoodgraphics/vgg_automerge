// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "legacy.h"

OldOp::OldOp(const Op& op, const ObjId& obj, const IndexedCache<ActorId>& actors, const IndexedCache<std::string>& props) {
    this->action = op.action;

    this->obj = OldObjectId(obj, actors);

    if (op.key.tag == Key::Map) {
        this->key = OldKey{ OldKey::MAP, props.get(std::get<usize>(op.key.data)) };
    }
    else {
        this->key = OldKey{ OldKey::SEQ, OldElementId(std::get<ElemId>(op.key.data), actors) };
    }

    for (auto& id : op.pred.v) {
        this->pred.push_back(OldOpId(id, actors));
    }
    std::sort(this->pred.begin(), this->pred.end());

    this->insert = op.insert;
}

bool OldOp::operator==(const OldOp& other) const {
    return (action == other.action) &&
        (obj == other.obj) &&
        (key == other.key) &&
        (pred == other.pred) &&
        (insert == other.insert);
}

std::vector<const ActorId*> OldOp::opids_in_operation() const {
    std::vector<const ActorId*> res;

    if (!obj.isRoot) {
        res.push_back(&obj.id.actor);
    }

    if (key.is_seq()) {
        auto& element_id = key.get_element_id();
        if (!element_id.isHead) {
            res.push_back(&element_id.id.actor);
        }
    }

    for (auto& opid : pred) {
        res.push_back(&opid.actor);
    }

    return res;
}