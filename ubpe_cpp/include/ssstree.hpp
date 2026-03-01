#ifndef SUB_SEQUENCES_SEARCH_TREE_HPP
#define SUB_SEQUENCES_SEARCH_TREE_HPP

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "utils.hpp"

namespace ubpe {

/// @brief SubSequence Search Tree.
///
/// Well, it's a version of an optimized trie but with an efficient search
/// operator `()` which return not the full match for the `key`, but all
/// non-null entries which keys are prefixes in the `key`.
template <DocumentT K, typename V>
class SSSTree;

/// @brief SubSequence Search Tree node.
template <DocumentT K, typename V>
class SSSTreeNode {
    friend class SSSTree<K, V>;

   private:
    K key;
    std::optional<V> value;
    std::vector<SSSTreeNode<K, V>> children;

   public:
    SSSTreeNode(K key, std::optional<V> value = std::nullopt)
        : key(key), value(value) {}
    SSSTreeNode(const SSSTreeNode&) = default;
    SSSTreeNode(SSSTreeNode&&) = default;
    SSSTreeNode& operator=(const SSSTreeNode&) = default;
    SSSTreeNode& operator=(SSSTreeNode&&) = default;
    ~SSSTreeNode() = default;

    /// @brief Add a key-value pair to the tree.
    /// @param element Key-value pair.
    /// @returns The new tree node.
    SSSTreeNode operator+(std::pair<K, V> element) {
        auto [key, value] = element;

        // find common prefix for the node's key and `key`
        size_t i = 0;
        auto max_len = std::min(this->key.size(), key.size());
        while (i < max_len && this->key[i] == key[i]) i++;

        // key to insert is in the tree
        if (i == key.size()) {
            // equal keys
            if (i == this->key.size()) {
                // if node was empty, set the value
                if (!this->value) this->value = value;
                // current node is returned with the old value if it was present
                // or with the new value if the node was empty
                return *this;
            }

            // split vertex in two
            auto split = SSSTreeNode<K, V>(
                K(this->key.cbegin() + i, this->key.cend()), this->value);
            split.children = std::move(this->children);
            this->children = {split};
            this->key = key;
            this->value = value;

            return *this;
        }

        // part of a key is in the tree
        key = K(key.cbegin() + i, key.cend());
        auto child = SSSTreeNode<K, V>(key, value);

        // the new key starts with the old one
        if (i == this->key.size()) {
            for (auto& child : this->children) {
                if (child.key[0] == key[0]) {
                    return child + std::make_pair(key, value);
                }
            }

            this->children.emplace_back(child);
        }
        // the new and the old keys have common first i elements
        else {
            auto split = SSSTreeNode<K, V>(
                K(this->key.cbegin() + i, this->key.cend()), this->value);
            split.children = std::move(this->children);
            this->children = {split, child};
            this->key = K(this->key.cbegin(), this->key.cbegin() + i);
            this->value = std::nullopt;
        }

        return child;
    }

    /// @brief Get the value for `key`.
    /// @param key Key for lookup.
    /// @returns A value in the tree for `key`.
    std::optional<V> operator[](K key) const {
        // `key` matched
        if (key == this->key) return this->value;
        // key in the node is a prefix in `key`
        if (K(key.cbegin(), key.cbegin() + this->key.size()) == this->key) {
            // delete the prefix
            key = K(key.cbegin() + this->key.size(), key.cend());
            // search which child contains the rest of `key`
            for (const auto& child : this->children) {
                // if child's key start with the same value as `key`
                if (child.key[0] == key[0]) {
                    // recursively search in the child
                    return child[key];
                }
            }
        }
        // in case the key is not in the tree
        return std::nullopt;
    }

    /// @brief Get the value for `key`, and all key-value pairs which keys are
    /// prefixes of `key` that are also in the tree and have value.
    /// @param key Key for lookup.
    /// @param stack History of lookup.
    /// @param start The position in key to start lookup with.
    /// @returns A value in the tree which key is the longest prefix of `key`
    /// present in the tree.
    std::optional<V> operator()(
        const K& key, std::vector<std::pair<K, std::optional<V>>>& stack,
        size_t start = 0) const {
        if (start >= key.size())
            throw std::out_of_range("`start` is out of range");

        // check if the untraced part of `key` is shorter than the key in the
        // node
        if (start + this->key.size() > key.size()) {
            return stack.size() > 0 ? stack.back().second : std::nullopt;
        }
        // check if the node's key is in the desired place in `key`
        if (K(key.cbegin() + start, key.cbegin() + start + this->key.size()) ==
            this->key) {
            // add key-value pair to the stack, even if its value is null
            stack.emplace_back(std::make_pair(this->key, this->value));
            // move the start in `key` forward
            start += this->key.size();
            // search which child contains the rest of `key`
            for (const auto& child : this->children) {
                // if child's key start with the same value as `key`
                if (child.key[0] == key[start]) {
                    // recursively search in the child
                    return child(key, stack, start);
                }
            }
        }
        // in case the key is not in the tree
        return stack.size() > 0 ? stack.back().second : std::nullopt;
    }
};

template <DocumentT K, typename V>
class SSSTree {
   private:
    std::vector<SSSTreeNode<K, V>> children;

   public:
    SSSTree() = default;
    SSSTree(const SSSTree&) = default;
    SSSTree(SSSTree&&) = default;
    SSSTree& operator=(const SSSTree&) = default;
    SSSTree& operator=(SSSTree&&) = default;
    ~SSSTree() = default;

    /// @brief Check if the tree is empty.
    bool empty() const { return this->children.size() == 0; }

    /// @brief Add a key-value pair to the tree.
    /// @param element Key-value pair.
    /// @returns The new tree node.
    SSSTreeNode<K, V> operator+(std::pair<K, V> element) {
        // search which child is able to contain `key`
        size_t i = 0;
        while (i < this->children.size()) {
            // if child's key starts with the same value as `key`
            if (this->children[i].key[0] == element.first[0]) {
                // add the element to this child
                return this->children[i] + element;
            }
            // move to the next child
            i++;
        }
        // if no child is able, add a new one
        auto child = SSSTreeNode<K, V>(element.first, element.second);
        this->children.emplace_back(child);
        return child;
    }

    /// @brief Get the value for `key`.
    /// @param key Key for lookup.
    /// @returns A value in the tree for `key`.
    std::optional<V> operator[](K& key) const {
        // search which child may contain `key`
        size_t i = 0;
        while (i < this->children.size()) {
            // if child's key starts with the same value as `key`
            if (this->children[i].key[0] == key[0]) {
                // search for `key` in this child
                return this->children[i][key];
            }
            // move to the next child
            i++;
        }
        // `key` is not in the tree
        return std::nullopt;
    }

    /// @brief Get all the key-value pairs in the tree where keys are prefixes
    /// of `key` present in the tree.
    /// @param key Key for lookup.
    /// @returns A vector of key-value pairs in the tree where keys are prefixes
    /// or lengths of prefixes of `key` present in the tree.
    variant<std::vector<std::pair<K, V>>, std::vector<std::pair<size_t, V>>>
    operator()(const K& key, size_t start = 0, bool fast = false) const {
        if (start >= key.size())
            throw std::out_of_range("`start` is out of range");

        // search which child may contain `key`
        size_t i = 0;
        while (i < this->children.size()) {
            // if child's key starts with the same value as `key`
            if (this->children[i].key[0] == key[start]) {
                // create empty stack
                std::vector<std::pair<K, std::optional<V>>> stack;
                // trace the tree
                auto _ = this->children[i](key, stack, start);
                // remove null values from the stack and accumulate key's parts
                // at the same time
                std::erase_if(stack, [&, sub_key = K()](auto& element) mutable {
                    // extend the key with values of the new part
                    sub_key.insert(sub_key.end(), element.first.cbegin(),
                                   element.first.cend());
                    // update the key
                    element.first = sub_key;
                    // delete if the value is null
                    return !element.second.has_value();
                });

                if (!fast) {
                    std::vector<std::pair<K, V>> prefixes;
                    prefixes.reserve(stack.size());
                    std::transform(stack.cbegin(), stack.cend(),
                                   std::back_inserter(prefixes),
                                   [](const auto& prefix) -> std::pair<K, V> {
                                       return {prefix.first,
                                               prefix.second.value()};
                                   });

                    // return the prefixes
                    return prefixes;
                } else {
                    std::vector<std::pair<size_t, V>> prefixes;
                    prefixes.reserve(stack.size());
                    std::transform(
                        stack.cbegin(), stack.cend(),
                        std::back_inserter(prefixes),
                        [](const auto& prefix) -> std::pair<size_t, V> {
                            return {prefix.first.size(), prefix.second.value()};
                        });

                    // return the prefixes
                    return prefixes;
                }
            }
            // move to the next child
            i++;
        }

        // return empty thing if there is not a subtree that may contain `key`
        if (fast)
            return std::vector<std::pair<size_t, V>>{};
        else
            return std::vector<std::pair<K, V>>{};
    }
};

}  // namespace ubpe

#endif  // SUB_SEQUENCES_SEARCH_TREE_HPP
