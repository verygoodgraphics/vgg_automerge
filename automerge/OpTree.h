// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <tuple>
#include <optional>

#include "type.h"
#include "Op.h"
#include "IndexedCache.h"
#include "Query.h"
#include "query/QueryKeys.h"

constexpr usize B = 16;

struct OpTreeNode;
struct OpTreeInternal;

// NodeIter of op_tree/iter.rs
struct OpTreeNodeIter {
    // The node itself
    const OpTreeNode* node = nullptr;
    // The index of the next element we will pull from the node. This means something different
    // depending on whether the node is a leaf node or not. If the node is a leaf node then this
    // index is the index in `node.elements` which will be returned on the next call to `next()`.
    // If the node is not an internal node then this index is the index of `children` which we are
    // currently iterating as well as being the index of the next element of `elements` which we
    // will return once we have finished iterating over the child node.
    usize index = 0;
};

// combine OpTreeIter and Inner of op_tree/iter.rs
struct OpTreeIter {
    bool is_emtpy;

    // A stack of nodes in the optree which we have descended in to to get to the current element.
    std::vector<OpTreeNodeIter> ancestors;
    OpTreeNodeIter current;
    // How far through the whole optree we are
    usize cumulative_index;
    const OpTreeNode* root_node;

    OpTreeIter(const OpTreeInternal& tree);

    std::optional<const Op*> next();

    std::optional<const Op*> nth(usize n);
};

struct OpTreeNode {
public:
    std::vector<OpTreeNode> children;
    std::vector<Op> elements;
    Index index;

    bool search(TreeQuery& query, const OpSetMetadata& m, std::optional<usize> skip) const;

    usize len() const {
        return length;
    }

    void reindex();

    bool is_leaf() const {
        return children.empty();
    }

    bool is_full() const {
        return (elements.size() >= 2 * B - 1);
    }

    // Returns the child index and the given index adjusted for the cumulative index before that
    // child.
    std::pair<usize, usize> find_child_index(usize index) const;

    void insert_into_non_full_node(usize index, Op&& element);

    // A utility function to split the child `full_child_index` of this node
    // Note that `full_child_index` must be full when this function is called.
    void split_child(usize full_child_index);

    Op remove_from_leaf(usize index);

    Op remove_element_from_non_leaf(usize index, usize element_index);

    usize cumulative_index(usize child_index) const;

    Op remove_from_internal_child(usize index, usize child_index);

    usize check() const;

    Op remove(usize index);

    void merge(Op&& middle, OpTreeNode& successor_sibling);

    // Update the operation at the given index using the provided function.
    // This handles updating the indices after the update.
    ReplaceArgs update(usize index, OpFunc f);

    const Op& last() const;

    std::optional<const Op*> get(usize index) const;

private:
    usize length = 0;

    friend struct OpTreeInternal;
};

struct OpTreeInternal {
    std::optional<OpTreeNode> root_node;

    // Get the length of the sequence.
    usize len() const {
        if (root_node) {
            return root_node->len();
        }
        else {
            return 0;
        }
    }

    auto keys() const {
        return root_node ? std::optional<QueryKeys>{ QueryKeys(&*root_node) } : std::nullopt;
    }

    TreeQuery& search(TreeQuery& query, const OpSetMetadata& m) const;

    OpTreeIter iter() const {
        return OpTreeIter(*this);
    }

    // Insert the `element` into the sequence at `index`.
    // Panics if `index > len`.
    void insert(usize index, Op&& element);

    // Get the `element` at `index` in the sequence.
    auto get(usize index) const {
        return root_node ? std::optional<const Op*>{ root_node->get(index) } : std::nullopt;
    }

    // this replaces get_mut() because it allows the indexes to update correctly
    void update(usize index, OpFunc f);

    // Removes the element at `index` from the sequence.
    // throw if `index` is out of bounds.
    Op remove(usize index);
};

struct OpTree {
    OpTreeInternal internal;
    ObjType objtype = ObjType::Map;
    // The id of the parent object, root has no parent.
    std::optional<ObjId> parent;

    OpTreeIter iter() const {
        return internal.iter();
    }

    usize len() const {
        return internal.len();
    }
};
