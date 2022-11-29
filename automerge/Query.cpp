// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "Query.h"
#include "OpTree.h"

void Index::replace(const ReplaceArgs& args) {
    if (!(args.old_id == args.new_id)) {
        ops.erase(args.old_id);
        ops.insert(args.new_id);
    }

    if (args.old_visible == args.new_visible)
        return;

    if (args.new_visible) {
        ++visible[args.new_key];
    }
    else {
        visible_remove(args.new_key);
    }
}

void Index::insert(const Op& op) {
    ops.insert(op.id);
    if (op.visible()) {
        ++visible[op.elemid_or_key()];
    }
}

void Index::remove(const Op& op) {
    ops.erase(op.id);
    if (!op.visible())
        return;

    visible_remove(op.elemid_or_key());
}

void Index::merge(const Index& other) {
    for (auto& id : other.ops) {
        ops.insert(id);
    }
    for (auto& kv : other.visible) {
        visible[kv.first] += kv.second;
    }
}

void Index::visible_remove(const Key& key) {
    auto find = visible.find(key);
    if (find == visible.end())
        throw std::out_of_range("remove overun in index");
    if (find->second == 1) {
        visible.erase(find);
    }
    else {
        --find->second;
    }
}

////////////////////////////////////////////////

usize binary_search_by(const OpTreeNode& node, OpCmpFunc f) {
    usize right = node.len();
    usize left = 0;
    while (left < right) {
        usize seq = (left + right) / 2;
        if (f(*(node.get(seq))) < 0) {
            left = seq + 1;
        }
        else {
            right = seq;
        }
    }
    return left;
}