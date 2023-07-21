// Copyright (c) 2022 the VGG Automerge contributors
// This code is licensed under MIT license (see LICENSE for details)

#include "OpTree.h"

#include <cassert>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include "helper.h"

OpTreeIter::OpTreeIter(const OpTreeInternal& tree) {
    if (!tree.root_node.has_value()) {
        is_emtpy = true;
        return;
    }

    is_emtpy = false;

    // This is a guess at the average depth of an OpTree
    ancestors.reserve(6);
    current = { &(*tree.root_node), 0 };
    cumulative_index = 0;
    root_node = &(*tree.root_node);
}

std::optional<const Op*> OpTreeIter::next() {
    if (is_emtpy) {
        return {};
    }

    if (current.node->is_leaf()) {
        // If we're in a leaf node and we haven't exhausted it yet we just return the elements
        // of the leaf node
        if (current.index < current.node->len()) {
            auto& result = current.node->elements[current.index];
            current.index += 1;
            cumulative_index += 1;
            return &result;
        }

        // We've exhausted the leaf node, we must find the nearest non-exhausted parent
        OpTreeNodeIter node_iter;
        while (true) {
            if (ancestors.empty()) {
                // No parents left, we're done
                return {};
            }

            auto iter = vector_pop(ancestors);
            if (iter.index >= iter.node->elements.size()) {
                // We've exhausted this parent
                continue;
            }
            else {
                // This parent still has elements to process, let's use it!
                node_iter = iter;
                break;
            }
        }

        // if we've finished the elements in a leaf node and there's a parent node then we
        // return the element from the parent node which is one after the index at which we
        // descended into the child
        current = node_iter;
        auto& result = current.node->elements[current.index];
        current.index += 1;
        cumulative_index += 1;
        return &result;
    }
    else {
        // If we're in a non-leaf node then the last iteration returned an element from the
        // current nodes `elements`, so we must now descend into a leaf child
        ancestors.push_back(current);
        while (true) {
            auto& child = current.node->children[current.index];
            current.index = 0;
            if (!child.is_leaf()) {
                ancestors.push_back({ &child, 0 });
                current.node = &child;
            }
            else {
                current.node = &child;
                break;
            }
        }
        return next();
    }
}

std::optional<const Op*> OpTreeIter::nth(usize n) {
    if (is_emtpy) {
        return {};
    }

    // Make sure that we don't rewind when calling nth more than once
    if (n < cumulative_index) {
        return {};
    }

    if (n >= root_node->len()) {
        cumulative_index = root_node->len() - 1;
        return {};
    }

    // rather than trying to go back up through the ancestors to find the right
    // node we just start at the root.
    current = { root_node, n };
    cumulative_index = 0;
    ancestors.clear();
    while (!current.node->is_leaf()) {
        auto& children = current.node->children;
        for (usize child_index = 0; child_index < children.size(); ++child_index) {
            auto& child = children[child_index];
            auto sum = cumulative_index + child.len();
            if (sum < n) {
                cumulative_index += child.len() + 1;
                current.index = child_index + 1;
            }
            else if (sum == n) {
                cumulative_index += child.len() + 1;
                current.index = child_index + 1;
                return &current.node->elements[child_index];
            }
            else {
                current.index = child_index;
                ancestors.push_back(std::move(current));
                current = { &child, 0 };
                break;
            }
        }
    }
    // we're in a leaf node and we kept track of the cumulative index as we went,
    usize index_in_this_node = (n > cumulative_index) ? n - cumulative_index : 0;
    current.index = index_in_this_node + 1;
    return &current.node->elements[index_in_this_node];
}

/////////////////////////////////////////////////////////

bool OpTreeNode::search_element(TreeQuery& query, const OpSetMetadata& m, usize index) const {
    if (index < elements.size() &&
        query.query_element_with_metadata(elements[index], m).tag == QueryResult::FINISH) {
        return true;
    }
    return false;
}

bool OpTreeNode::search(TreeQuery& query, const OpSetMetadata& m, std::optional<usize> skip) const {
    if (is_leaf()) {
        usize skip_value = skip.value_or(0);
        if (skip_value >= elements.size())
            return false;
        for (auto iter = std::next(elements.begin(), skip_value); iter != elements.end(); ++iter) {
            if (query.query_element_with_metadata(*iter, m).tag == QueryResult::FINISH)
                return true;
        }
        return false;
    }

    for (usize child_index = 0; child_index < children.size(); ++child_index) {
        auto& child = children[child_index];
        if (!skip.has_value()) {
            // descend and try find it
            auto res = query.query_node_with_metadata(child, m);
            switch (res.tag) {
            case QueryResult::DESCEND:
                if (child.search(query, m, {})) {
                    return true;
                }
                break;
            case QueryResult::FINISH:
                return true;
            case QueryResult::NEXT:
                break;
            case QueryResult::SKIP:
                throw std::runtime_error("had skip from non-root node");
                break;
            default:
                break;
            }
            if (search_element(query, m, child_index)) {
                return true;
            }
        }
        else if (*skip > child.len()) {
            skip = *skip - child.len() - 1;
        }
        else if (*skip == child.len()) {
            // important to not be None so we never call query_node again
            skip = 0;
            if (search_element(query, m, child_index)) {
                return true;
            }
        }
        else {
            if (child.search(query, m, skip)) {
                return true;
            }
            // important to not be None so we never call query_node again
            skip = 0;
            if (search_element(query, m, child_index)) {
                return true;
            }
        }
    }
    return false;
}

void OpTreeNode::reindex() {
    Index index;
    for (auto& c : children) {
        index.merge(c.index);
    }
    for (auto& e : elements) {
        index.insert(e);
    }
    this->index = index;
}

std::pair<usize, usize> OpTreeNode::find_child_index(usize index) const {
    usize cumulative_len = 0;
    for (usize i = 0; i < children.size(); ++i) {
        if (cumulative_len + children[i].len() >= index) {
            return { i, index - cumulative_len };
        }
        else {
            cumulative_len += children[i].len() + 1;
        }
    }
    throw std::runtime_error("index not found");
}

void OpTreeNode::insert_into_non_full_node(usize index, Op&& element) {
    assert(!is_full());

    this->index.insert(element);

    if (is_leaf()) {
        ++length;
        elements.insert(std::next(elements.begin(), index), std::move(element));
        return;
    }

    auto [child_index, sub_index] = find_child_index(index);
    auto& child = children[child_index];

    if (child.is_full()) {
        split_child(child_index);

        // child structure has changed so we need to find the index again
        auto [child_index, sub_index] = find_child_index(index);
        children[child_index].insert_into_non_full_node(sub_index, std::move(element));
    }
    else {
        child.insert_into_non_full_node(sub_index, std::move(element));
    }
    ++length;
}

void OpTreeNode::split_child(usize full_child_index) {
    usize original_len_self = len();
    auto& full_child = children[full_child_index];

    // Create a new node which is going to store (B-1) keys
    // of the full child.
    OpTreeNode successor_sibling;

    usize original_len = full_child.len();
    assert(full_child.is_full());

    successor_sibling.elements = vector_split_off(full_child.elements, B);
    if (!full_child.is_leaf()) {
        successor_sibling.children = vector_split_off(full_child.children, B);
    }

    Op middle = vector_pop(full_child.elements);

    auto children_len_sum = [](const OpTreeNode& node) {
        return std::accumulate(node.children.cbegin(), node.children.cend(), usize(0),
            [](usize length, const OpTreeNode& c) {
                return length + c.len();
            });
    };
    full_child.length = full_child.elements.size() + children_len_sum(full_child);
    successor_sibling.length = successor_sibling.elements.size() + children_len_sum(successor_sibling);

    usize z_len = successor_sibling.len();
    usize full_child_len = full_child.len();
    full_child.reindex();
    successor_sibling.reindex();

    children.insert(std::next(children.begin(), full_child_index + 1), successor_sibling);
    elements.insert(std::next(elements.begin(), full_child_index), middle);

    assert(full_child_len + z_len + 1 == original_len);
    assert(original_len_self == len());
}

Op OpTreeNode::remove_from_leaf(usize index) {
    --length;
    return vector_remove(elements, index);
}

Op OpTreeNode::remove_element_from_non_leaf(usize index, usize element_index) {
    --length;
    if (children[element_index].elements.size() >= B) {
        usize total_index = cumulative_index(element_index);
        // recursively delete index - 1 in predecessor_node
        Op predecessor = children[element_index].remove(index - 1 - total_index);
        // replace element with that one
        std::swap(elements[element_index], predecessor);

        return predecessor;
    }
    else if (children[element_index + 1].elements.size() >= B) {
        // recursively delete index + 1 in successor_node
        usize total_index = cumulative_index(element_index + 1);
        Op successor = children[element_index + 1].remove(index + 1 - total_index);
        // replace element with that one
        std::swap(elements[element_index], successor);

        return successor;
    }
    else {
        Op middle_element = vector_remove(elements, element_index);
        OpTreeNode successor_child = vector_remove(children, element_index + 1);
        children[element_index].merge(std::move(middle_element), successor_child);

        usize total_index = cumulative_index(element_index);
        return children[element_index].remove(index - total_index);
    }
}

usize OpTreeNode::cumulative_index(usize child_index) const {
    usize sum = 0;
    for_each(children.cbegin(), std::next(children.cbegin(), child_index), [&](auto& c) {
        sum += c.len() + 1;
        });

    return sum;
}

Op OpTreeNode::remove_from_internal_child(usize index, usize child_index) {
    if ((children[child_index].elements.size() < B) &&
        (child_index == 0 || (children[child_index - 1].elements.size() < B)) &&
        ((child_index + 1 >= children.size()) || (children[child_index + 1].elements.size() < B))) {
        // if the child and its immediate siblings have B-1 elements merge the child
        // with one sibling, moving an element from this node into the new merged node
        // to be the median
        if (child_index > 0) {
            Op middle = vector_remove(elements, child_index - 1);

            // use the predessor sibling
            OpTreeNode successor = vector_remove(children, child_index);
            --child_index;

            children[child_index].merge(std::move(middle), successor);
        }
        else {
            Op middle = vector_remove(elements, child_index);

            // use the sucessor sibling
            OpTreeNode successor = vector_remove(children, child_index + 1);

            children[child_index].merge(std::move(middle), successor);
        }
    }
    else if (children[child_index].elements.size() < B) {
        if ((child_index > 0) && (child_index - 1 < children.size()) &&
            (children[child_index - 1].elements.size() >= B)) {
            Op last_element = vector_pop(children[child_index - 1].elements);
            assert(!children[child_index - 1].elements.empty());
            --children[child_index - 1].length;
            children[child_index - 1].index.remove(last_element);

            std::swap(elements[child_index - 1], last_element);
            Op& parent_element = last_element;

            children[child_index].index.insert(parent_element);
            children[child_index].elements.insert(elements.begin(), parent_element);
            ++children[child_index].length;

            if (!children[child_index - 1].children.empty()) {
                OpTreeNode last_child = vector_pop(children[child_index - 1].children);

                children[child_index - 1].length -= last_child.len();
                children[child_index - 1].reindex();
                children[child_index].length += last_child.len();
                children[child_index].children.insert(
                    children[child_index].children.begin(), last_child);
                children[child_index].reindex();
            }
        }
        else if ((child_index + 1 < children.size()) &&
            (children[child_index + 1].elements.size() >= B)) {
            Op first_element = vector_remove(children[child_index + 1].elements, 0);
            children[child_index + 1].index.remove(first_element);
            --children[child_index + 1].length;

            assert(!children[child_index + 1].elements.empty());

            std::swap(elements[child_index], first_element);
            Op& parent_element = first_element;

            ++children[child_index].length;
            children[child_index].index.insert(parent_element);
            children[child_index].elements.push_back(std::move(parent_element));

            if (!children[child_index + 1].is_leaf()) {
                OpTreeNode first_child = vector_remove(children[child_index + 1].children, 0);
                children[child_index + 1].length -= first_child.len();
                children[child_index + 1].reindex();
                children[child_index].length += first_child.len();

                children[child_index].children.push_back(std::move(first_child));
                children[child_index].reindex();
            }
        }
    }
    --length;
    usize total_index = cumulative_index(child_index);
    return children[child_index].remove(index - total_index);
}

usize OpTreeNode::check() const {
    usize l = elements.size();
    for (auto& c : children) {
        l += c.check();
    }

    assert(len() == l);

    return l;
}

Op OpTreeNode::remove(usize index) {
    usize original_len = len();

    if (is_leaf()) {
        Op v = remove_from_leaf(index);
        this->index.remove(v);

        assert(original_len == len() + 1);
        assert(check() == len());

        return v;
    }

    usize total_index = 0;
    for (usize child_index = 0; child_index < children.size(); ++child_index) {
        auto& child = children[child_index];
        usize tmp_index = total_index + child.len();
        if (tmp_index < index) {
            // should be later on in the loop
            total_index = tmp_index + 1;
            continue;
        }
        else if (tmp_index > index) {
            Op v = remove_from_internal_child(index, child_index);
            this->index.remove(v);

            assert(original_len == len() + 1);
            assert(check() == len());

            return v;
        }
        else {
            Op v = remove_element_from_non_leaf(index, std::min(child_index, elements.size() - 1));
            this->index.remove(v);

            assert(original_len == len() + 1);
            assert(check() == len());

            return v;
        }
    }

    throw std::runtime_error("index not found to remove");
}

void OpTreeNode::merge(Op&& middle, OpTreeNode& successor_sibling) {
    index.insert(middle);
    index.merge(successor_sibling.index);
    elements.push_back(std::move(middle));
    for (auto& e : successor_sibling.elements) {
        elements.push_back(std::move(e));
    }
    for (auto& c : successor_sibling.children) {
        children.push_back(std::move(c));
    }
    length += successor_sibling.length + 1;

    assert(is_full());
}

ReplaceArgs OpTreeNode::update(usize index, OpFunc f) {
    if (is_leaf()) {
        Op& new_element = elements.at(index);
        OpId old_id = new_element.id;
        bool old_visible = new_element.visible();
        f(new_element);
        ReplaceArgs replace_args = {
            old_id, new_element.id, old_visible, new_element.visible(), new_element.elemid_or_key()
        };
        this->index.replace(replace_args);
        return replace_args;
    }

    usize cumulative_len = 0;
    usize len = this->len();
    for (usize child_index = 0; child_index < children.size(); ++child_index) {
        auto& child = children[child_index];
        usize tmp_len = cumulative_len + child.len();
        if (tmp_len < index) {
            cumulative_len = tmp_len + 1;
        }
        else if (tmp_len > index) {
            auto replace_args = child.update(index - cumulative_len, f);
            this->index.replace(replace_args);
            return replace_args;
        }
        else {
            Op& new_element = elements.at(child_index);
            OpId old_id = new_element.id;
            bool old_visible = new_element.visible();
            f(new_element);
            ReplaceArgs replace_args = {
                old_id, new_element.id, old_visible, new_element.visible(), new_element.elemid_or_key()
            };
            this->index.replace(replace_args);
            return replace_args;
        }
    }
    throw std::runtime_error("Invalid index to set: {} but len was {}");
}

const Op& OpTreeNode::last() const {
    if (is_leaf()) {
        // node is never empty so this is safe
        return elements.back();
    }

    // if not a leaf then there is always at least one child
    return children.back().last();
}

std::optional<const Op*> OpTreeNode::get(usize index) const {
    if (is_leaf()) {
        return (index < elements.size()) ? std::optional<const Op*>{ &elements[index] } : std::nullopt;
    }

    usize cumulative_len = 0;
    for (usize child_index = 0; child_index < children.size(); ++child_index) {
        auto& child = children[child_index];
        usize tmp_index = cumulative_len + child.len();
        if (tmp_index < index) {
            cumulative_len = tmp_index + 1;
        }
        else if (tmp_index > index) {
            return child.get(index - cumulative_len);
        }
        else {
            return (child_index < elements.size()) ? std::optional<const Op*>{ &elements[child_index] } : std::nullopt;
        }
    }

    return std::nullopt;
}

//////////////////////////////////////////////////////

TreeQuery& OpTreeInternal::search(TreeQuery& query, const OpSetMetadata& m) const {
    if (!root_node.has_value())
        return query;

    auto res = query.query_node_with_metadata(*root_node, m);
    if (res.tag == QueryResult::DESCEND) {
        root_node->search(query, m, {});
    }
    else if (res.tag == QueryResult::SKIP) {
        root_node->search(query, m, { res.skip });
    }

    return query;
}

void OpTreeInternal::insert(usize index, Op&& element) {
    assert(index <= len());

    usize old_len = len();
    if (!root_node.has_value()) {
        OpTreeNode root;
        root.insert_into_non_full_node(index, std::move(element));
        root_node = root;

        assert(len() == old_len + 1);
        return;
    }

#ifndef NDEBUG
    root_node->check();
#endif
    if (!root_node->is_full()) {
        root_node->insert_into_non_full_node(index, std::move(element));

        assert(len() == old_len + 1);
        return;
    }

    usize original_len = root_node->len();
    OpTreeNode old_root;

    // move a new root to root position
    std::swap(*root_node, old_root);

    root_node->length += old_root.len();
    root_node->index = old_root.index;
    root_node->children.push_back(std::move(old_root));
    root_node->split_child(0);

    assert(original_len == root_node->len());

    // after splitting the root has one element and two children, find which child the
    // index is in
    usize first_child_len = root_node->children[0].len();
    ++root_node->length;
    root_node->index.insert(element);
    if (first_child_len < index) {
        root_node->children[1].insert_into_non_full_node(index - (first_child_len + 1), std::move(element));
    }
    else {
        root_node->children[0].insert_into_non_full_node(index, std::move(element));
    }

    assert(len() == old_len + 1);
    return;
}

void OpTreeInternal::update(usize index, OpFunc f) {
    if (len() > index) {
        if (!root_node.has_value()) {
            throw std::runtime_error("update from empty tree");
        }

        root_node->update(index, f);
    }
}

Op OpTreeInternal::remove(usize index) {
    if (!root_node.has_value()) {
        throw std::runtime_error("remove from empty tree");
    }

#ifndef NDEBUG
    usize len = root_node->check();
#endif
    Op old = root_node->remove(index);

    if (root_node->elements.empty()) {
        if (root_node->is_leaf()) {
            root_node.reset();
        }
        else {
            root_node = vector_remove(root_node->children, 0);
        }
    }

#ifndef NDEBUG
    if (root_node) {
        assert(len == (root_node->check() + 1));
    }
    else {
        assert(len == 1);
    }
#endif

    return old;
}
