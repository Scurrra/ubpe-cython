#ifndef UBPE_UTILS
#define UBPE_UTILS

#include <functional>
#include <utility>
#include <variant>

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

template <typename... Types>
class variant {
   private:
    std::variant<Types...> inner;

   public:
    variant() = default;
    ~variant() = default;

    variant(std::variant<Types...> data) : inner(data) {}

    variant(const variant&) = default;
    template <typename T>
    variant(const T& data) {
        static_assert(
            (std::is_same_v<T, Types> || ...),
            "`T` is not convertible to type `ubpe::variant<Types...>`");
        this->inner = data;
    }

    variant(variant&&) = default;
    template <typename T>
    variant(T&& data) {
        static_assert(
            (std::is_same_v<T, Types> || ...),
            "`T` is not convertible to type `ubpe::variant<Types...>`");
        this->inner = data;
    }

    variant& operator=(const variant&) = default;
    template <typename T>
    variant& operator=(const T& data) {
        static_assert(
            (std::is_same_v<T, Types> || ...),
            "`T` is not convertible to type `ubpe::variant<Types...>`");
        this->inner = data;
        return *this;
    }

    variant& operator=(variant&&) = default;
    template <typename T>
    variant& operator=(T&& data) {
        static_assert(
            (std::is_same_v<T, Types> || ...),
            "`T` is not convertible to type `ubpe::variant<Types...>`");
        this->inner = data;
        return *this;
    }

    template <typename T>
    operator T() const {
        static_assert(
            (std::is_same_v<T, Types> || ...),
            "`ubpe::variant<Types...>` is not convertible to type `T`");
        return std::get<T>(this->inner);
    }
};

}  // namespace ubpe

#endif  // UBPE_UTILS
