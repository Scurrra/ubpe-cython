#ifndef TOP_ELEMENTS
#define TOP_ELEMENTS

#include <functional>
#include <queue>
#include <vector>

namespace ubpe {

/// @brief `std::priority_queue` wrapper for more efficient finding of top n
/// elements in a stream.
template <typename T, typename Compare = std::greater<T>>
class TopElements {
   private:
    size_t n;
    std::priority_queue<T, std::vector<T>, Compare> pq;

   public:
    TopElements(size_t n, Compare comp = Compare()) : n(n), pq(comp) {}
    TopElements(const TopElements&) = default;
    TopElements(TopElements&&) = default;
    TopElements& operator=(const TopElements&) = default;
    TopElements& operator=(TopElements&&) = default;
    ~TopElements() = default;

    void push(const T& element) {
        this->pq.push(element);
        if (this->pq.size() > this->n) {
            this->pq.pop();
        }
    }

    void push(T&& element) {
        this->pq.push(element);
        if (this->pq.size() > this->n) {
            this->pq.pop();
        }
    }

    void pop() { this->pq.pop(); }

    bool empty() const { return this->pq.empty(); }

    size_t size() const { return this->pq.size(); }

    const T& top() const { return this->pq.top(); }

    /// @brief Get sorted top n elements.
    std::vector<T> sorted() const {
        std::vector<T> data(this->pq.size());

        auto pq = this->pq;
        for (int i = data.size() - 1; i >= 0 && !pq.empty(); i--, pq.pop()) {
            data[i] = pq.top();
        }

        return data;
    }
};

}  // namespace ubpe

#endif  // TOP_ELEMENTS