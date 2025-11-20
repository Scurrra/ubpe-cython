#ifndef COUNTER_HPP
#define COUNTER_HPP

#include <algorithm>
#include <cstddef>
#include <map>
#include <queue>
#include <vector>

namespace ubpe {

template <typename T>
class Counter {
   private:
    std::map<T, size_t> counter;

   public:
    Counter() = default;
    Counter(std::vector<T>& doc) { this->update(doc); }
    Counter(std::vector<std::vector<T>>& corpus) {
        for (const auto& doc : corpus) {
            this->update(doc);
        }
    }

    Counter(const Counter&) = default;
    Counter(Counter&&) = default;
    Counter& operator=(const Counter&) = default;
    Counter& operator=(Counter&&) = default;
    ~Counter() = default;

    /// @brief Update PairCounter instance with adjacent pairs in `doc`;
    /// @param doc Flat vector.
    void update(std::vector<T>& doc) {
        for (const auto& element : doc) {
            this->counter[element]++;
        }
    }

    typedef std::map<T, size_t>::const_iterator const_iterator;
    typedef std::map<T, size_t>::iterator iterator;
    typedef std::map<T, size_t>::reference reference;
    typedef std::map<T, size_t>::const_reference const_reference;

    iterator begin() { return this->counter.begin(); }

    iterator end() { return this->counter.end(); }

    const_iterator begin() const { return this->counter.begin(); }

    const_iterator end() const { return this->counter.end(); }

    const_iterator cbegin() const noexcept { return this->counter.cbegin(); }

    const_iterator cend() const noexcept { return this->counter.cend(); }

    /// @brief Get counts for `element`.
    /// @param element Key that should be in counter.
    /// @returns Count of occuriences of `element`.
    size_t& operator[](T element) {
        // if `pair` was not in corpus
        if (!this->counter.contains(element)) {
            this->counter[element] = 0;
        }

        return this->counter[element];
    }

    /// @brief Get counts for `element`.
    /// @param element Key that should be in counter.
    /// @returns Count of occuriences of `element`.
    const size_t& operator[](T element) const {
        // if `pair` was not in corpus
        if (!this->counter.contains(element)) {
            this->counter[element] = 0;
        }

        return this->counter[element];
    }

    /// @brief Get counts for `element`, if not present first inits it with 0.
    /// @param element Key that should be in counter.
    /// @returns Count of occuriences of `element`.
    const size_t& at(T element) const noexcept {
        // if `pair` was not in corpus
        assert(this->counter.contains(element) ||
               "Counter does not contain this element");

        return this->counter[element];
    }

    /// @brief Get `n` most common pairs.
    /// @param n How many pairs together with it's number of occurrences.
    std::vector<std::pair<T, size_t>> most_common(size_t n) const {
        if (n == 0) return {};

        // comparator
        auto cmp = [](const auto& a, const auto& b) { return a > b; };

        // if `n` is greater than naumber of pairs itself then just sort
        if (n >= this->counter.size()) {
            std::vector<std::pair<T, size_t>> mc(this->counter.begin(),
                                                 this->counter.end());

            std::sort(mc.begin(), mc.end(), cmp);

            return mc;
        }

        // create priority queue
        std::priority_queue<std::pair<T, size_t>,
                            std::vector<std::pair<T, size_t>>, decltype(cmp)>
            pq(cmp);

        // add values to `pq` keeping it's size (less or) equal to `n`
        for (auto element : this->counter) {
            pq.emplace(element);
            // keep max size
            if (pq.size() > n) pq.pop();
        }

        // get values from `pq`
        std::vector<std::pair<T, size_t>> mc;
        mc.reserve(n);
        while (!pq.empty()) {
            mc.emplace_back(pq.top());
            pq.pop();
        }

        return mc;
    }
};

}  // namespace ubpe

#endif
