// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "Len.h"
#include "../OpTree.h"

QueryResult Len::query_node(const OpTreeNode& child) {
    len = child.index.visible_len();
    return QueryResult{ QueryResult::FINISH, 0 };
}
