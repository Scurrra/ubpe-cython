#ifndef TOP_ELEMENTS
#define TOP_ELEMENTS

#include <functional>
#include <vector>

#include "heapq.hpp"

namespace ubpe {

/// @brief `ubpe::heapq` wrapper for more efficient finding of top n elements in
/// a stream.
///
/// Note: it has a min-heap under the hood.
template <typename T, typename Compare = std::less<T>>
class TopElements {
   private:
    size_t n;
    heapq<T> heap;

   public:
    /// @brief Initialize a TopElements object with a maximum size and a
    /// comparison function.
    TopElements(size_t n, Compare comp = Compare())
        : n(n), heap({.compare = comp}) {}

    TopElements(const TopElements&) = default;
    TopElements(TopElements&&) = default;
    TopElements& operator=(const TopElements&) = default;
    TopElements& operator=(TopElements&&) = default;
    ~TopElements() = default;

    /// @brief Push an element into the heap.
    /// @param element An element to add.
    void push(const T& element) {
        if (this->heap.size() < n) {
            this->heap.push(element);
        } else if (element > this->heap.top()) {
            this->heap.pushpop(element);
        }
    }

    /// @brief Push an element into the heap.
    /// @param element An element to add.
    void push(T&& element) {
        auto _element = std::move(element);
        if (this->heap.size() < n) {
            this->heap.push(_element);
        } else if (element > this->heap.top()) {
            this->heap.pushpop(_element);
        }
    }

    /// @brief Pop the top element from the heap.
    void pop() { auto _ = this->heap.pop(); }

    /// @brief Check if the heap is empty.
    bool empty() const { return this->heap.empty(); }

    /// @brief Get the size of the heap.
    size_t size() const { return this->heap.size(); }

    /// @brief Get the top element from the heap.
    const T& top() const { return this->heap.top(); }

    /// @brief Get sorted top n elements.
    std::vector<T> sorted() const {
        std::vector<T> data(this->heap.size());

        auto heap = this->heap;
        for (int i = data.size() - 1; i >= 0; i--) {
            data[i] = heap.pop();
        }

        return data;
    }
};

}  // namespace ubpe

#endif  // TOP_ELEMENTS
