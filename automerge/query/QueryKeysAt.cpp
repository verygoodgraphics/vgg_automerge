// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "QueryKeysAt.h"
#include "../OpTree.h"

QueryKeysAt::QueryKeysAt(OpTreeNode* r, Clock&& clock) :
    clock(std::move(clock)), index(0), last_key(std::nullopt), index_back(r->len()),
    last_key_back(std::nullopt), root_child(r) {}