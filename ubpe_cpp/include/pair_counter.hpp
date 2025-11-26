#ifndef PAIR_COUNTER_HPP
#define PAIR_COUNTER_HPP

#include <algorithm>
#include <cstddef>
#include <map>
#include <queue>
#include <set>
#include <vector>

namespace ubpe {

/// @brief Class for counting of occurences of adjacent pairs in the corpus.
template <typename T>
class PairCounter {
   private:
    std::map<std::pair<T, T>, size_t> pairs_counter;
    std::map<std::pair<T, T>, size_t> docs_counter;

   public:
    PairCounter() = default;
    PairCounter(const std::vector<T>& doc) { this->update(doc); }
    PairCounter(const std::vector<std::vector<T>>& corpus) {
        for (const auto& doc : corpus) {
            this->update(doc);
        }
    }

    PairCounter(const PairCounter&) = default;
    PairCounter(PairCounter&&) = default;
    PairCounter& operator=(const PairCounter&) = default;
    PairCounter& operator=(PairCounter&&) = default;
    ~PairCounter() = default;

    /// @brief Update PairCounter instance with adjacent pairs in `doc`;
    /// @param doc Flat vector.
    void update(const std::vector<T>& doc) {
        // update with adjacent pairs
        for (auto i = 0; i < doc.size() - 1; i++) {
            this->pairs_counter[{doc[i], doc[i + 1]}]++;
        }

        // update with unique adjacent pairs
        std::set<std::pair<T, T>> unique_pairs;
        std::transform(
            doc.cbegin(), doc.cend() - 1, doc.cbegin() + 1,
            std::inserter(unique_pairs, unique_pairs.end()),
            [](const auto& left, const auto& right) -> std::pair<T, T> {
                return {left, right};
            });
        for (const auto& pair : unique_pairs) {
            this->docs_counter[pair]++;
        }
    }

    /// @brief Get `n` most common pairs.
    /// @param n How many pairs together with it's number of occurrences.
    std::vector<std::pair<std::pair<T, T>, size_t>> most_common(
        size_t n) const {
        if (n == 0) return {};

        // comparator
        auto cmp = [](const auto& a, const auto& b) {
            return a.second > b.second;
        };

        // if `n` is greater than naumber of pairs itself then just sort
        if (n >= this->pairs_counter.size()) {
            std::vector<std::pair<std::pair<T, T>, size_t>> mc(
                this->pairs_counter.cbegin(), this->pairs_counter.cend());

            std::sort(mc.begin(), mc.end(), cmp);

            return mc;
        }

        // create priority queue
        std::priority_queue<std::pair<std::pair<T, T>, size_t>,
                            std::vector<std::pair<std::pair<T, T>, size_t>>,
                            decltype(cmp)>
            pq(cmp);

        // add values to `pq` keeping it's size (less or) equal to `n`
        for (auto element : this->pairs_counter) {
            pq.emplace(element);
            // keep max size
            if (pq.size() > n) pq.pop();
        }

        // get values from `pq`
        std::vector<std::pair<std::pair<T, T>, size_t>> mc;
        mc.reserve(n);
        while (!pq.empty()) {
            mc.emplace_back(pq.top());
            pq.pop();
        }

        return mc;
    }

    /// @brief Get counts for a `pair`.
    /// @param pair Pair of elements that should be in counter.
    /// @returns Pair of counts where `.first` is a number of docs the `pair`
    /// occured in corpus and `.second` is a number of occurences of `pair` in
    /// the whole corpus.
    std::pair<size_t, size_t> operator()(const std::pair<T, T>& pair) const {
        // if `pair` was not in corpus
        if (!this->docs_counter.contains(pair)) return {0, 0};

        return {this->docs_counter.at(pair), this->pairs_counter.at(pair)};
    }
};
}  // namespace ubpe

#endif  // PAIR_COUNTER_HPP