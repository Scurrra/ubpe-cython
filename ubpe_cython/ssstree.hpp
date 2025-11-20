#ifndef SUB_SEQUENCES_SEARCH_TREE_HPP
#define SUB_SEQUENCES_SEARCH_TREE_HPP

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>
namespace ubpe {

template <typename T>
concept CanBeKey = std::ranges::range<T> &&
                   !std::is_same_v<std::remove_cvref_t<T>, std::string> &&
                   !std::is_same_v<std::remove_cvref_t<T>, std::wstring> &&
                   !std::is_same_v<std::remove_cvref_t<T>, std::u16string> &&
                   !std::is_same_v<std::remove_cvref_t<T>, std::u32string>;

/// @brief SubSequence Search Tree node.
template <CanBeKey K, typename V>
class SSSTreeNode {
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
        const auto& [key, value] = element;

        auto i = 0;
        auto max_len = std::min(this->key.size(), key.size());
        while (i < max_len && this->key.at(i) == key.at(i)) i++;

        // key to insert is in the tree
        if (i == key.size()) {
            // equal keys
            if (i == this->key.size()) {
                if (!this->value) this->value = value;
                return this;
            }

            // split vertex in two
            auto split = SSSTreeNode<K, V>(
                K(this->key.cbegin() + i, this->key.cend()), this->value);
            split.children = std::move(this->children);
            this->children = {split};
            this->key = key;
            this->value = value;
        }
        // part of a key is in the tree
        else {
            key = K(key.cbegin() + i, key.cend());
            auto child = SSSTreeNode<K, V>(key, value);

            // the new key starts with the old one
            if (i == this->key.size()) {
                for (auto& child : this->children) {
                    if (child.key.at(0) == key.at(0)) {
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
    }

    /// @brief Get the value for `key`.
    /// @param key Key for lookup.
    /// @returns A value in the tree for `key`.
    std::optional<V> operator[](K key) const {
        // `key` matched
        if (key == this->key) return this->value;
        // key in the node is a prefix in `key`
        if (std::vector(key.cbegin(), key.cbegin() + this->key.size()) ==
            this->key) {
            // delete the prefix
            key = std::vector(key.cbegin() + this->key.size(), key.end());
            // search which child contains the rest of `key`
            for (const auto& child : this->children) {
                // if child's key start with the same value as `key`
                if (child.key.at(0) == key.at(0)) {
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
    std::optional<V> operator()(K& key, std::vector<std::pair<K, V>>& stack,
                                size_t start = 0) const {
        // check if the untraced part of `key` is shorter than the key in the
        // node
        if (start + this->key.size() > key.size()) {
            return stack.size() > 0 ? stack.back() : std::nullopt;
        }
        // check if the node's key is in the desired place in `key`
        if (std::vector(key.cbegin() + start,
                        key.cbegin() + start + this->key.size()) == this->key) {
            // add key-value pair to the stack, even if its value is null
            stack.emplace_back({this->key, this->value});
            // move the start in `key` forward
            start += this->key.size();
            // search which child contains the rest of `key`
            for (const auto& child : this->children) {
                // if child's key start with the same value as `key`
                if (child.key.at(0) == key.at(start)) {
                    // recursively search in the child
                    return child(key, stack, start);
                }
            }
        }
        // in case the key is not in the tree
        return stack.size() > 0 ? stack.back() : std::nullopt;
    }
};

/// @brief SubSequence Search Tree.
///
/// Well, it's a version of an optimized trie but with an efficient search
/// operator `()` which return not the full match for the `key`, but all
/// non-null entries which keys are prefixes in the `key`.
template <CanBeKey K, typename V>
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

    /// @brief Add a key-value pair to the tree.
    /// @param element Key-value pair.
    /// @returns The new tree node.
    SSSTreeNode<K, V> operator+(std::pair<K, V> element) {
        // search which child is able to contain `key`
        auto i = 0;
        while (i < this->children.size()) {
            // if child's key starts with the same value as `key`
            if (this->children[i].key.at(0) == element.first.at(0)) {
                // add the element to this child
                return this->children[i] + element;
            }
            // move to the next child
            i++;
        }
        // if no child is able, add a new one
        if (i == this->children.size()) {
            auto child = SSSTreeNode<K, V>(element.first, element.second);
            this->children.emplace_back(child);
            return child;
        }
    }

    /// @brief Get the value for `key`.
    /// @param key Key for lookup.
    /// @returns A value in the tree for `key`.
    std::optional<V> operator[](K& key) const {
        // search which child may contain `key`
        auto i = 0;
        while (i < this->children.size()) {
            // if child's key starts with the same value as `key`
            if (this->children[i].key.at(0) == key.at(0)) {
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
    /// of `key` present in the tree.
    std::vector<std::pair<K, V>> operator()(K key) const {
        // search which child may contain `key`
        auto i = 0;
        while (i < this->children.size()) {
            // if child's key starts with the same value as `key`
            if (this->children[i].key.at(0) == key.at(0)) {
                // create empty stack
                std::vector<std::pair<K, V>> stack;
                // trace the tree
                auto _ = this->children[i](key, stack, 0);
                // remove null values from the stack and accumulate key's parts
                // at the same time
                std::remove_if(stack.begin(), stack.end(),
                               [&, sub_key = K()](auto& element) mutable {
                                   // extend the key with values of the new part
                                   sub_key.insert(sub_key.end(),
                                                  element.first.cbegin(),
                                                  element.first.cend());
                                   // update the key
                                   element.first = sub_key;
                                   // delete if the value is null
                                   return !element.second.has_value();
                               });
                // return the stack
                return stack;
            }
            // move to the next child
            i++;
        }
        // return empty thing if there is not a subtree that may contain `key`
        return {};
    }
};

}  // namespace ubpe

#endif  // SUB_SEQUENCES_SEARCH_TREE_HPP