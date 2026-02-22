#ifndef PAIR_COUNTER_HPP
#define PAIR_COUNTER_HPP

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "heapq.hpp"
#include "utils.hpp"

namespace ubpe {

/// @brief Class for counting of occurences of adjacent pairs in the corpus.
template <Hashable T>
class PairCounter {
   private:
    // `.first` --- count of documents
    // `.second` --- count of pairs
    std::unordered_map<std::pair<T, T>, std::pair<size_t, size_t>, PairHash<T>>
        counter;

   public:
    /// @brief Constructor that updates the PairCounter instance with adjacent
    /// pairs in `doc`.
    /// @param doc Flat vector.
    PairCounter(const std::vector<T>& doc) { this->update(doc); }

    /// @brief Constructor that updates the PairCounter instance with adjacent
    /// pairs in each document of `corpus`.
    /// @param corpus Vector of vectors.
    PairCounter(const std::vector<std::vector<T>>& corpus) {
        for (const auto& doc : corpus) {
            this->update(doc);
        }
    }

    PairCounter() = default;
    PairCounter(const PairCounter&) = default;
    PairCounter(PairCounter&&) = default;
    PairCounter& operator=(const PairCounter&) = default;
    PairCounter& operator=(PairCounter&&) = default;
    ~PairCounter() = default;

    /// @brief Update PairCounter instance with adjacent pairs in `doc`;
    /// @param doc Flat vector.
    void update(const std::vector<T>& doc) {
        // update with adjacent pairs
        for (size_t i = 0; i < doc.size() - 1; i++) {
            this->counter[{doc[i], doc[i + 1]}].second++;
        }

        // update with unique adjacent pairs
        std::unordered_set<std::pair<T, T>, PairHash<T>> unique_pairs;
        std::transform(
            doc.cbegin(), doc.cend() - 1, doc.cbegin() + 1,
            std::inserter(unique_pairs, unique_pairs.end()),
            [](const auto& left, const auto& right) -> std::pair<T, T> {
                return {left, right};
            });
        for (const auto& pair : unique_pairs) {
            this->counter[pair].first++;
        }
    }

    /// @brief Get `n` most common pairs.
    /// @param n How many pairs together with it's number of occurrences.
    std::vector<std::pair<std::pair<T, T>, size_t>> most_common(
        size_t n) const {
        if (n == 0) return {};

        std::vector<std::pair<std::pair<T, T>, std::pair<size_t, int>>> data;
        data.reserve(this->counter.size());
        std::transform(
            this->counter.cbegin(), this->counter.cend(),
            std::back_inserter(data),
            [](const std::pair<std::pair<T, T>, std::pair<size_t, size_t>>&
                   value)
                -> std::pair<std::pair<T, T>, std::pair<size_t, int>> {
                return {value.first,
                        {value.second.second, -value.second.first}};
            });

        // `ubpe::nlargest` doesn't work with std::unordered_map iterators yet,
        // so we need to convert it to a vector first both of the following
        // work, but the second one *should be* more efficient

        // auto mc =
        //     nlargest<std::pair<std::pair<T, T>, std::pair<size_t, int>>,
        //              std::pair<std::pair<size_t, int>,
        //                        std::pair<T, T>>>(
        //                        int>>(
        //         data, n,
        //         {.key = [](const std::pair<std::pair<T, T>,
        //                                    std::pair<size_t, int>>& value)
        //              -> std::pair<std::pair<size_t, int>, std::pair<T, T>> {
        //             return {value.second, value.first};
        //         }});

        auto mc = nlargest<std::pair<std::pair<T, T>, std::pair<size_t, int>>>(
            data, n,
            {.compare =
                 [](const std::pair<std::pair<T, T>, std::pair<size_t, int>>& a,
                    const std::pair<std::pair<T, T>, std::pair<size_t, int>>&
                        b) {
                     if (a.second == b.second) return a.first > b.first;
                     return a.second > b.second;
                 }});

        std::vector<std::pair<std::pair<T, T>, size_t>> result(mc.size());
        for (auto i = 0; i < result.size(); i++) {
            result[i] = {mc[i].first, mc[i].second.first};
        }
        return result;
    }

    /// @brief Get counts for a `pair`.
    /// @param pair Pair of elements that should be in counter.
    /// @returns Pair of counts where `.first` is a number of docs the `pair`
    /// occured in corpus and `.second` is a number of occurences of `pair` in
    /// the whole corpus.
    std::pair<size_t, size_t> operator()(
        const std::pair<T, T>& pair) const noexcept {
        // if `pair` was not in corpus
        if (!this->counter.contains(pair)) return {0, 0};

        return this->counter.at(pair);
    }
};
}  // namespace ubpe

#endif  // PAIR_COUNTER_HPP
