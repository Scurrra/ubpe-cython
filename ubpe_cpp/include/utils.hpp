#ifndef UBPE_UTILS
#define UBPE_UTILS

#include <functional>
#include <utility>

namespace ubpe {

template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

template <typename T>
concept HalfSizeT = std::integral<T> && sizeof(T) * 2 <= sizeof(size_t);

template <HalfSizeT T, std::size_t SHIFT = sizeof(T)>
struct PairHash {
    std::size_t operator()(const std::pair<T, T>& p) const {
        auto h1 = std::hash<T>{}(p.first);
        auto h2 = std::hash<T>{}(p.second);
        return (h1 << SHIFT) ^ h2;
    }
};

}  // namespace ubpe

#endif  // UBPE_UTILS
