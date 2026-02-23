#ifndef HEAPQ_HPP
#define HEAPQ_HPP

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <vector>

namespace ubpe {

// 1. Concept to check for the required iterator operations.
template <typename Iter>
concept RequiredIteratorOps =
    std::incrementable<Iter> &&        // Checks for pre/post-increment (++)
    std::equality_comparable<Iter> &&  // Checks for equality/inequality (==,
                                       // !=)
    requires(Iter it) {
        // Dereferenceable to some type
        { *it };
    };

// 2. Concept to check if a type has a valid const_iterator type.
template <typename T>
concept HasConstIterator = requires {
    typename T::const_iterator;  // Checks for the existence of the nested type
    requires RequiredIteratorOps<
        typename T::const_iterator>;  // Checks if the const_iterator meets
                                      // requirements
};

// 3. Combined concept for all requirements.
template <typename T>
concept ContainerConcept = HasConstIterator<T>;

/// C++ implementation of the heapq module (w/o the merge function).
/// https://github.com/python/cpython/blob/main/Lib/heapq.py
template <typename V, typename K = V,
          ContainerConcept Container = std::vector<V>>
class heapq {
   public:
    /// @brief Comparator object to hold a comparator function and optional
    /// functions to extract a key for comparison from a value and vice versa.
    struct Comparator {
        std::function<bool(const K&, const K&)> compare;
        std::function<K(const V&)> key;
        // I don't know yet where to apply value extracting function, but...
        std::function<V(const K&)> value;
    };

    /// @brief Constructor for a heap from a container.
    /// @param comp The comparison function to use for the heap.
    heapq(Comparator comp = {.compare = std::less<V>{}}) : comparator(comp) {}

    /// @brief Constructor for a heap from a container.
    /// @param data The container to build the heap from.
    /// @param comp The comparison function to use for the heap.
    heapq(const Container& data, Comparator comp = {.compare = std::less<V>{}})
        : data(data), comparator(comp) {
        if (!this->comparator.compare) {
            this->comparator.compare = std::less<K>{};
        }
        auto n = this->data.size();
        // Transform bottom-up.
        for (int64_t i = n / 2 - 1; i >= 0; --i) {
            this->siftup(i);
        }
    }

    /// @brief Constructor for a heap from a container.
    /// @param data The container to build the heap from.
    /// @param comp The comparison function to use for the heap.
    heapq(Container&& data, Comparator comp = {.compare = std::less<V>{}})
        : data(std::move(data)), comparator(comp) {
        if (!this->comparator.compare) {
            this->comparator.compare = std::less<K>{};
        }
        auto n = this->data.size();
        // Transform bottom-up.
        for (int64_t i = n / 2 - 1; i >= 0; --i) {
            this->siftup(i);
        }
    }

    heapq(const heapq&) = default;
    heapq(heapq&&) = default;
    heapq& operator=(const heapq&) = default;
    heapq& operator=(heapq&&) = default;
    ~heapq() = default;

    /// @brief Returns the number of elements in the heap.
    size_t size() const { return this->data.size(); }

    /// @brief Checks if the heap is empty.
    bool empty() const { return this->data.empty(); }

    /// @brief Returns the top element of the heap.
    const V& top() const { return this->data.front(); }

    /// @brief Returns the underlying container of the heap.
    const Container& container() const { return this->data; }

    /// @brief Pushes a new element onto the heap, maintaining the heap
    /// invariant.
    /// @param element The new element to add to the heap.
    void push(const V& element) {
        this->data.emplace_back(element);
        this->siftdown(0, this->data.size() - 1);
    }

    /// @brief Pushes a new element onto the heap, maintaining the heap
    /// invariant.
    /// @param element The new element to add to the heap.
    void push(V&& element) {
        this->data.emplace_back(std::move(element));
        this->siftdown(0, this->data.size() - 1);
    }

    /// @brief Pop the top element off the heap, maintaining the heap invariant.
    /// @return The current top value before the element was popped.
    V pop() {
        if (this->data.empty()) {
            throw std::out_of_range("Heap is empty");
        }
        auto bottom_element = this->data.back();
        this->data.pop_back();
        if (!this->data.empty()) {
            auto top_element = this->data[0];
            this->data[0] = bottom_element;
            this->siftup(0);
            return top_element;
        }
        return std::move(bottom_element);
    }

    /// @brief Pop and return the current top value, and add the new `element`.
    ///
    /// This is more efficient than `pop()` followed by `push()`, and can be
    /// more appropriate when using a fixed-size heap.  Note that the value
    /// returned may be !topper than item!
    ///
    /// @param element The new element to add to the heap.
    /// @return The current top value before the new element was added.
    V replace(const V& element) {
        if (this->data.empty()) {
            throw std::out_of_range("Heap is empty");
        }
        auto return_element = this->data[0];
        this->data[0] = element;
        this->siftup(0);
        return return_element;
    }

    /// @brief Pop and return the current top value, and add the new `element`.
    ///
    /// This is more efficient than `.pop()` followed by `.push()`, and can be
    /// more appropriate when using a fixed-size heap.  Note that the value
    /// returned may be !topper than item!
    ///
    /// @param element The new element to add to the heap.
    /// @return The current top value before the new element was added.
    V replace(V&& element) {
        if (this->data.empty()) {
            throw std::out_of_range("Heap is empty");
        }
        auto return_element = this->data[0];
        this->data[0] = std::move(element);
        this->siftup(0);
        return return_element;
    }

    /// @brief Fast version of a `.push()` followed by a `.pop()`.
    /// @param element The new element to add to the heap.
    /// @return The current top value after the new element was added.
    V pushpop(V element) {
        if (!this->data.empty() &&
            this->comparator.compare(this->data[0], element)) {
            std::swap(this->data[0], element);
            this->siftup(0);
        }
        return element;
    }

   private:
    Container data;
    Comparator comparator;

    void siftdown(size_t start_pos, size_t pos) {
        if (pos >= this->data.size())
            throw std::invalid_argument("OOB in siftdown");
        auto new_element = this->data[pos];
        // Follow the path to the root, moving parents down until finding a
        // place `new_element` fits.
        if (this->comparator.key) {
            while (pos > start_pos) {
                auto parent_pos = (pos - 1) >> 1;
                auto parent = this->data[parent_pos];
                if (this->comparator.compare(this->comparator.key(new_element),
                                             this->comparator.key(parent))) {
                    this->data[pos] = parent;
                    pos = parent_pos;
                    continue;
                }
                break;
            }
        } else {
            while (pos > start_pos) {
                auto parent_pos = (pos - 1) >> 1;
                auto parent = this->data[parent_pos];
                if (this->comparator.compare(new_element, parent)) {
                    this->data[pos] = parent;
                    pos = parent_pos;
                    continue;
                }
                break;
            }
        }

        this->data[pos] = new_element;
    }

    void siftup(size_t pos) {
        if (pos >= this->data.size())
            throw std::invalid_argument("OOB in siftup");
        auto end_pos = this->data.size();
        auto start_pos = pos;
        auto new_element = this->data[pos];
        // Bubble up the top child until hitting a leaf.
        auto child_pos = 2 * pos + 1;

        if (this->comparator.key) {
            while (child_pos < end_pos) {
                // Set child_pos to index of smaller child.
                auto right_pos = child_pos + 1;
                if ((right_pos < end_pos) &&
                    !this->comparator.compare(
                        this->comparator.key(this->data[child_pos]),
                        this->comparator.key(this->data[right_pos]))) {
                    child_pos = right_pos;
                }
                // Move the top child up.
                this->data[pos] = this->data[child_pos];
                pos = child_pos;
                child_pos = 2 * pos + 1;
            }
        } else {
            while (child_pos < end_pos) {
                // Set child_pos to index of smaller child.
                auto right_pos = child_pos + 1;
                if ((right_pos < end_pos) &&
                    !this->comparator.compare(this->data[child_pos],
                                              this->data[right_pos])) {
                    child_pos = right_pos;
                }
                // Move the top child up.
                this->data[pos] = this->data[child_pos];
                pos = child_pos;
                child_pos = 2 * pos + 1;
            }
        }

        // The leaf at `pos` is empty now.  Put `new_element` there, and bubble
        // it up to its final resting place (by sifting its parents down).
        this->data[pos] = new_element;
        this->siftdown(start_pos, pos);
    }
};

/// @brief Returns the n smallest elements from the container.
/// @param cbegin The beginning of the container.
/// @param cend The end of the container.
/// @param n The number of elements to extract.
/// @param comp The comparison function object to determine the smallest of two
/// elements.
/// @return A container with the n smallest elements.
///
/// Note: The function assumes that you have passed a key extraction function.
/// If a comparison function is passed, it is considered to work as a "less
/// than" comparison.
template <typename V, typename K, ContainerConcept Container = std::vector<V>>
    requires(!ContainerConcept<K>)
std::vector<V> nsmallest(typename Container::const_iterator cbegin,
                         typename Container::const_iterator cend, size_t n,
                         typename heapq<V, K>::Comparator comp = {}) {
    if (n > static_cast<size_t>(std::numeric_limits<int64_t>::max())) {
        throw std::overflow_error("`n` should be positive and fit into int64_t");
    }
    if (!comp.key) {
        throw std::logic_error("`key` function is expected but not provided");
    }

    bool default_comparison = false;
    if (!comp.compare) {
        comp.compare = std::greater<K>{};
        default_comparison = true;
    }

    // Check if the container is empty
    if (cbegin == cend) return {};

    // If n is 0, return an empty container
    if (n == 0) return {};

    // If n is 1, return the smallest element using `std::min_element`
    if (n == 1) {
        if (default_comparison) {
            return {*std::min_element(cbegin, cend,
                                      [&comp](const V& a, const V& b) {
                                          return comp.key(a) < comp.key(b);
                                      })};
        } else {
            return {*std::min_element(
                cbegin, cend, [&comp](const V& a, const V& b) {
                    return comp.compare(comp.key(a), comp.key(b));
                })};
        }
    }

    // When `n >= data.size()`, it's faster to use `std::stable_sort`
    if (n >= static_cast<size_t>(std::distance(cbegin, cend))) {
        std::vector<V> result(cbegin, cend);
        if (default_comparison) {
            std::stable_sort(result.begin(), result.end(),
                             [&comp](const V& a, const V& b) {
                                 return comp.key(a) < comp.key(b);
                             });
        } else {
            std::stable_sort(result.begin(), result.end(),
                             [&comp](const V& a, const V& b) {
                                 return comp.compare(comp.key(a), comp.key(b));
                             });
        }
        return result;
    }

    // Else use a max-heap with ordering
    auto it = cbegin;
    auto order = static_cast<int64_t>(0);

    // Prepare the data to start with
    std::vector<std::tuple<K, int, V>> _data;
    _data.reserve(n);
    std::generate_n(std::back_inserter(_data), n,
                    [&comp, &it, &order]() -> std::tuple<K, int, V> {
                        auto value = *(it++);
                        return {comp.key(value), order++, value};
                    });

    // Prepare the comparison object (only the .compare function)
    typename heapq<std::tuple<K, int, V>>::Comparator _comp;
    if (default_comparison) {
        _comp.compare = std::greater<std::tuple<K, int, V>>{};
    } else {
        _comp.compare = [&compare = comp.compare](
                            const std::tuple<K, int, V>& a,
                            const std::tuple<K, int, V>& b) {
            auto a_comp_b = compare(std::get<0>(a), std::get<0>(b));
            auto b_comp_a = compare(std::get<0>(b), std::get<0>(a));
            // If a == b, compare their indices
            if (!a_comp_b && !b_comp_a) return std::get<1>(a) > std::get<1>(b);
            return b_comp_a;
        };
    }

    // Build the initial heap
    auto _heap = heapq<std::tuple<K, int, V>>(_data, _comp);

    // Update the heap, if the new element is smaller than the top element
    auto top = std::get<0>(_heap.top());
    for (; it != cend; ++it) {
        auto it_key = comp.key(*it);
        if (default_comparison) {
            if (it_key < top) {
                _heap.replace({it_key, order++, *it});
                top = std::get<0>(_heap.top());
            }
        } else {
            if (comp.compare(it_key, top)) {
                _heap.replace({it_key, order++, *it});
                top = std::get<0>(_heap.top());
            }
        }
    }

    // Element extraction
    std::vector<V> result(n);
    for (int64_t i = n - 1; i >= 0; i--) {
        result[i] = std::get<2>(_heap.pop());
    }
    return result;
}

/// @brief Returns the n smallest elements from the container.
/// @param data The container to extract the elements from.
/// @param n The number of elements to extract.
/// @param comp The comparison function object to determine the smallest of two
/// elements.
/// @return A container with the n smallest elements.
///
/// Note: The function assumes that you have passed a key extraction function.
/// If a comparison function is passed, it is considered to work as a "less
/// than" comparison.
template <typename V, typename K, ContainerConcept Container = std::vector<V>>
    requires(!ContainerConcept<K>)
std::vector<V> nsmallest(const Container& data, size_t n,
                         typename heapq<V, K>::Comparator comp = {}) {
    return nsmallest<V, K, Container>(data.cbegin(), data.cend(), n, comp);
}

/// @brief Returns the n smallest elements from the container.
/// @param il The initializer list to extract the elements from.
/// @param n The number of elements to extract.
/// @param comp The comparison function object to determine the smallest of two
/// elements.
/// @return A container with the n smallest elements.
///
/// Note: The function assumes that you have passed a key extraction function.
/// If a comparison function is passed, it is considered to work as a "less
/// than" comparison.
template <typename V, typename K, ContainerConcept Container = std::vector<V>>
    requires(!ContainerConcept<K>)
std::vector<V> nsmallest(std::initializer_list<V> il, size_t n,
                         typename heapq<V, K>::Comparator comp = {}) {
    Container data(il);
    return nsmallest<V, K, Container>(std::move(data), n, comp);
}

/// @brief Returns the n smallest elements from the container.
/// @param cbegin The beginning of the container to extract the elements from.
/// @param cend The end of the container to extract the elements from.
/// @param n The number of elements to extract.
/// @param comp The comparison function object to determine the smallest of two
/// elements.
/// @return A container with the n smallest elements.
///
/// Note: The function assumes that you have not passed a key extraction
/// function; if it is passed, arguments are automatically forwarded to a proper
/// function. If a comparison function is passed, it is considered to work as a
/// "less than" comparison.
template <typename V, ContainerConcept Container = std::vector<V>>
std::vector<V> nsmallest(typename Container::const_iterator cbegin,
                         typename Container::const_iterator cend, size_t n,
                         typename heapq<V>::Comparator comp = {}) {
    if (n > static_cast<size_t>(std::numeric_limits<int64_t>::max())) {
        throw std::overflow_error("`n` should be positive and fit into int64_t");
    }

    if (comp.key) {
        using K = decltype(comp.key)::result_type;
        return nsmallest<V, K, Container>(cbegin, cend, n, comp);
    }
    bool default_comparison = false;
    if (!comp.compare) {
        comp.compare = std::greater<V>{};
        default_comparison = true;
    }

    // Check if the container is empty
    if (cbegin == cend) return {};

    // If n is 0, return an empty container
    if (n == 0) return {};

    // If n is 1, return the smallest element using `std::min_element`
    if (n == 1) {
        if (default_comparison) {
            return {*std::min_element(cbegin, cend)};
        } else {
            return {*std::min_element(cbegin, cend, comp.compare)};
        }
    }

    // When `n >= data.size()`, it's faster to use `std::stable_sort`
    if (n >= static_cast<size_t>(std::distance(cbegin, cend))) {
        std::vector<V> result(cbegin, cend);
        if (default_comparison) {
            std::stable_sort(result.begin(), result.end(), std::less<>{});
        } else {
            std::stable_sort(result.begin(), result.end(),
                             [&compare = comp.compare](const V& a, const V& b) {
                                 return compare(b, a);
                             });
        }

        return result;
    }

    // Else use a max-heap with ordering
    auto it = cbegin;
    auto order = static_cast<int64_t>(0);

    // Prepare the data to start with
    std::vector<std::tuple<V, int, V>> _data;
    _data.reserve(n);
    std::generate_n(std::back_inserter(_data), n,
                    [&it, &order]() -> std::tuple<V, int, V> {
                        auto value = *(it++);
                        return {value, order++, value};
                    });

    // Prepare the comparison object (only the .compare function)
    typename heapq<std::tuple<V, int, V>>::Comparator _comp;
    if (default_comparison) {
        _comp.compare = std::greater<std::tuple<V, int, V>>{};
    } else {
        _comp.compare = [compare = comp.compare](
                            const std::tuple<V, int, V>& a,
                            const std::tuple<V, int, V>& b) {
            auto a_comp_b = compare(std::get<0>(a), std::get<0>(b));
            auto b_comp_a = compare(std::get<0>(b), std::get<0>(a));
            // If a == b, compare their indices
            if (!a_comp_b && !b_comp_a) return std::get<1>(a) > std::get<1>(b);
            return b_comp_a;
        };
    }

    // Build the initial heap
    auto _heap = heapq<std::tuple<V, int, V>>(_data, _comp);

    // Update the heap, if the new element is smaller than the top element
    auto top = std::get<0>(_heap.top());
    for (; it != cend; ++it) {
        if (default_comparison) {
            if (*it < top) {
                _heap.replace({*it, order++, *it});
                top = std::get<0>(_heap.top());
            }
        } else {
            if (comp.compare(*it, top)) {
                _heap.replace({*it, order++, *it});
                top = std::get<0>(_heap.top());
            }
        }
    }

    // Element extraction
    std::vector<V> result(n);
    for (int64_t i = n - 1; i >= 0; i--) {
        result[i] = std::get<2>(_heap.pop());
    }
    return result;
}

/// @brief Returns the n smallest elements from the container.
/// @param data The container to extract the elements from.
/// @param n The number of elements to extract.
/// @param comp The comparison function object to determine the smallest of two
/// elements.
/// @return A container with the n smallest elements.
///
/// Note: The function assumes that you have not passed a key extraction
/// function; if it is passed, arguments are automatically forwarded to a proper
/// function. If a comparison function is passed, it is considered to work as a
/// "less than" comparison.
template <typename V, ContainerConcept Container = std::vector<V>>
std::vector<V> nsmallest(const Container& data, size_t n,
                         typename heapq<V>::Comparator comp = {}) {
    return nsmallest<V, Container>(data.cbegin(), data.cend(), n, comp);
}

/// @brief Returns the n smallest elements from the container.
/// @param il The initializer list to extract the elements from.
/// @param n The number of elements to extract.
/// @param comp The comparison function object to determine the smallest of two
/// elements.
/// @return A container with the n smallest elements.
///
/// Note: The function assumes that you have not passed a key extraction
/// function; if it is passed, arguments are automatically forwarded to a proper
/// function. If a comparison function is passed, it is considered to work as a
/// "less than" comparison.
template <typename V, ContainerConcept Container = std::vector<V>>
std::vector<V> nsmallest(std::initializer_list<V> il, size_t n,
                         typename heapq<V>::Comparator comp = {}) {
    Container data(il);
    return nsmallest<V, Container>(std::move(data), n, comp);
}

/// @brief Returns the n smallest elements from the container.
/// @param cbegin The beginning of the container to extract the elements from.
/// @param cend The end of the container to extract the elements from.
/// @param n The number of elements to extract.
/// @param comp The comparison function object to determine the smallest of two
/// elements.
/// @return A container with the n smallest elements.
///
/// Note: The function assumes that you have passed a key extraction function.
/// If a comparison function is passed, it is considered to work as a "greater
/// than" comparison.
template <typename V, typename K, ContainerConcept Container = std::vector<V>>
    requires(!ContainerConcept<K>)
std::vector<V> nlargest(typename Container::const_iterator cbegin,
                        typename Container::const_iterator cend, size_t n,
                        typename heapq<V, K>::Comparator comp = {}) {
    if (n > static_cast<size_t>(std::numeric_limits<int64_t>::max())) {
        throw std::overflow_error("`n` should be positive and fit into int64_t");
    }
    if (!comp.key) {
        throw std::logic_error("`key` function is expected but not provided");
    }

    bool default_comparison = false;
    if (!comp.compare) {
        comp.compare = std::less<K>{};
        default_comparison = true;
    }

    // Check if the container is empty
    if (cbegin == cend) return {};

    // If n is 0, return an empty container
    if (n == 0) return {};

    // If n is 1, return the smallest element using `std::max_element`
    if (n == 1) {
        if (default_comparison) {
            return {*std::max_element(cbegin, cend,
                                      [&comp](const V& a, const V& b) {
                                          return comp.key(a) > comp.key(b);
                                      })};
        } else {
            return {*std::max_element(
                cbegin, cend, [&comp](const V& a, const V& b) {
                    return comp.compare(comp.key(a), comp.key(b));
                })};
        }
    }

    // When `n >= data.size()`, it's faster to use `std::stable_sort`
    if (n >= static_cast<size_t>(std::distance(cbegin, cend))) {
        std::vector<V> result(cbegin, cend);
        if (default_comparison) {
            std::stable_sort(result.begin(), result.end(),
                             [&comp](const V& a, const V& b) {
                                 return comp.key(a) > comp.key(b);
                             });
        } else {
            std::stable_sort(result.begin(), result.end(),
                             [&comp](const V& a, const V& b) {
                                 return comp.compare(comp.key(a), comp.key(b));
                             });
        }
        return result;
    }

    // Else use a min-heap with ordering
    auto it = cbegin;
    auto order = static_cast<int64_t>(0);

    // Prepare the data to start with
    std::vector<std::tuple<K, int, V>> _data;
    _data.reserve(n);
    std::generate_n(std::back_inserter(_data), n,
                    [&comp, &it, &order]() -> std::tuple<K, int, V> {
                        auto value = *(it++);
                        return {comp.key(value), order--, value};
                    });

    // Prepare the comparison object (only the .compare function)
    typename heapq<std::tuple<K, int, V>>::Comparator _comp;
    if (default_comparison) {
        _comp.compare = std::less<std::tuple<K, int, V>>{};
    } else {
        _comp.compare = [&compare = comp.compare](
                            const std::tuple<K, int, V>& a,
                            const std::tuple<K, int, V>& b) {
            auto a_comp_b = compare(std::get<0>(a), std::get<0>(b));
            auto b_comp_a = compare(std::get<0>(b), std::get<0>(a));
            // If a == b, compare their indices
            if (!a_comp_b && !b_comp_a) return std::get<1>(a) < std::get<1>(b);
            return b_comp_a;
        };
    }

    // Build the initial heap
    auto _heap = heapq<std::tuple<K, int, V>>(_data, _comp);

    // Update the heap, if the new element is greater than the top element
    auto top = std::get<0>(_heap.top());
    for (; it != cend; ++it) {
        auto it_key = comp.key(*it);
        if (default_comparison) {
            if (it_key > top) {
                _heap.replace({it_key, order--, *it});
                top = std::get<0>(_heap.top());
            }
        } else {
            if (comp.compare(it_key, top)) {
                _heap.replace({it_key, order--, *it});
                top = std::get<0>(_heap.top());
            }
        }
    }

    // Element extraction
    std::vector<V> result(n);
    for (int64_t i = n - 1; i >= 0; i--) {
        result[i] = std::get<2>(_heap.pop());
    }
    return result;
}

/// @brief Returns the n smallest elements from the container.
/// @param data The container to extract the elements from.
/// @param n The number of elements to extract.
/// @param comp The comparison function object to determine the smallest of two
/// elements.
/// @return A container with the n smallest elements.
///
/// Note: The function assumes that you have passed a key extraction function.
/// If a comparison function is passed, it is considered to work as a "greater
/// than" comparison.
template <typename V, typename K, ContainerConcept Container = std::vector<V>>
    requires(!ContainerConcept<K>)
std::vector<V> nlargest(const Container& data, size_t n,
                        typename heapq<V, K>::Comparator comp = {}) {
    return nlargest<V, K, Container>(data.cbegin(), data.cend(), n, comp);
}

/// @brief Returns the n smallest elements from the container.
/// @param il The initializer list to extract the elements from.
/// @param n The number of elements to extract.
/// @param comp The comparison function object to determine the smallest of two
/// elements.
/// @return A container with the n smallest elements.
///
/// Note: The function assumes that you have passed a key extraction function.
/// If a comparison function is passed, it is considered to work as a "greater
/// than" comparison.
template <typename V, typename K, ContainerConcept Container = std::vector<V>>
    requires(!ContainerConcept<K>)
std::vector<V> nlargest(std::initializer_list<V> il, size_t n,
                        typename heapq<V, K>::Comparator comp = {}) {
    Container data(il);
    return nlargest<V, K, Container>(std::move(data), n, comp);
}

/// @brief Returns the n smallest elements from the container.
/// @param cbegin The beginning of the container to extract the elements from.
/// @param cend The end of the container to extract the elements from.
/// @param n The number of elements to extract.
/// @param comp The comparison function object to determine the smallest of two
/// elements.
/// @return A container with the n smallest elements.
///
/// Note: The function assumes that you have not passed a key extraction
/// function; if it is passed, arguments are automatically forwarded to a proper
/// function. If a comparison function is passed, it is considered to work as a
/// "greater than" comparison.
template <typename V, ContainerConcept Container = std::vector<V>>
std::vector<V> nlargest(typename Container::const_iterator cbegin,
                        typename Container::const_iterator cend, size_t n,
                        typename heapq<V>::Comparator comp = {}) {
    if (n > static_cast<size_t>(std::numeric_limits<int64_t>::max())) {
        throw std::overflow_error("`n` should be positive and fit into int64_t");
    }

    if (comp.key) {
        using K = decltype(comp.key)::result_type;
        return nlargest<V, K, Container>(cbegin, cend, n, comp);
    }

    bool default_comparison = false;
    if (!comp.compare) {
        comp.compare = std::less<V>{};
        default_comparison = true;
    }

    // Check if the container is empty
    if (cbegin == cend) return {};

    // If n is 0, return an empty container
    if (n == 0) return {};

    // If n is 1, return the smallest element using `std::max_element`
    if (n == 1) {
        if (default_comparison) {
            return {*std::max_element(cbegin, cend)};
        } else {
            return {*std::max_element(cbegin, cend, comp.compare)};
        }
    }

    // When `n >= data.size()`, it's faster to use `std::stable_sort`
    if (n >= static_cast<size_t>(std::distance(cbegin, cend))) {
        std::vector<V> result(cbegin, cend);
        if (default_comparison) {
            std::stable_sort(result.begin(), result.end(), std::greater<>{});
        } else {
            std::stable_sort(result.begin(), result.end(),
                             [&compare = comp.compare](const V& a, const V& b) {
                                 return compare(b, a);
                             });
        }

        return result;
    }

    // Else use a min-heap with ordering
    auto it = cbegin;
    auto order = static_cast<int64_t>(0);

    // Prepare the data to start with
    std::vector<std::tuple<V, int, V>> _data;
    _data.reserve(n);
    std::generate_n(std::back_inserter(_data), n,
                    [&it, &order]() -> std::tuple<V, int, V> {
                        auto value = *(it++);
                        return {value, order--, value};
                    });

    // Prepare the comparison object (only the .compare function)
    typename heapq<std::tuple<V, int, V>>::Comparator _comp;
    if (default_comparison) {
        _comp.compare = std::less<std::tuple<V, int, V>>{};
    } else {
        _comp.compare = [compare = comp.compare](
                            const std::tuple<V, int, V>& a,
                            const std::tuple<V, int, V>& b) {
            auto a_comp_b = compare(std::get<0>(a), std::get<0>(b));
            auto b_comp_a = compare(std::get<0>(b), std::get<0>(a));
            // If a == b, compare their indices
            if (!a_comp_b && !b_comp_a) return std::get<1>(a) < std::get<1>(b);
            return b_comp_a;
        };
    }

    // Build the initial heap
    auto _heap = heapq<std::tuple<V, int, V>>(_data, _comp);

    // Update the heap, if the new element is greater than the top element
    auto top = std::get<0>(_heap.top());
    for (; it != cend; ++it) {
        if (default_comparison) {
            if (*it > top) {
                _heap.replace({*it, order--, *it});
                top = std::get<0>(_heap.top());
            }
        } else {
            if (comp.compare(*it, top)) {
                _heap.replace({*it, order--, *it});
                top = std::get<0>(_heap.top());
            }
        }
    }

    // Element extraction
    std::vector<V> result(n);
    for (int64_t i = n - 1; i >= 0; i--) {
        result[i] = std::get<2>(_heap.pop());
    }
    return result;
}

/// @brief Returns the n smallest elements from the container.
/// @param data The container to extract the elements from.
/// @param n The number of elements to extract.
/// @param comp The comparison function object to determine the smallest of two
/// elements.
/// @return A container with the n smallest elements.
///
/// Note: The function assumes that you have not passed a key extraction
/// function; if it is passed, arguments are automatically forwarded to a proper
/// function. If a comparison function is passed, it is considered to work as a
/// "greater than" comparison.
template <typename V, ContainerConcept Container = std::vector<V>>
std::vector<V> nlargest(const Container& data, size_t n,
                        typename heapq<V>::Comparator comp = {}) {
    return nlargest<V, Container>(data.cbegin(), data.cend(), n, comp);
}

/// @brief Returns the n smallest elements from the container.
/// @param il The initializer list to extract the elements from.
/// @param n The number of elements to extract.
/// @param comp The comparison function object to determine the smallest of two
/// elements.
/// @return A container with the n smallest elements.
///
/// Note: The function assumes that you have not passed a key extraction
/// function; if it is passed, arguments are automatically forwarded to a proper
/// function. If a comparison function is passed, it is considered to work as a
/// "greater than" comparison.
template <typename V, ContainerConcept Container = std::vector<V>>
std::vector<V> nlargest(std::initializer_list<V> il, size_t n,
                        typename heapq<V>::Comparator comp = {}) {
    Container data(il);
    return nlargest<V, Container>(std::move(data), n, comp);
}

}  // namespace ubpe

#endif  // HEAPQ_HPP
