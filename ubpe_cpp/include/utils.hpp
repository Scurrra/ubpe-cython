#ifndef UBPE_UTILS
#define UBPE_UTILS

#include <functional>
#include <string>
#include <utility>
#include <variant>

namespace ubpe {

/// Concept for a type that can be used as a document.
template <typename T>
concept DocumentT = std::ranges::range<T> ||
                    std::is_same_v<std::remove_cvref_t<T>,
                                   std::basic_string<typename T::value_type>>;

/// Hashable concept.
template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

/// HalfSizeT concept to check if a type is half the size of a `size_t`.
template <typename T>
concept HalfSizeT = std::integral<T> && sizeof(T) * 2 <= sizeof(size_t);

/// Hash function for pairs of HalfSizeT types.
template <HalfSizeT T, std::size_t SHIFT = sizeof(T)>
struct PairHash {
    std::size_t operator()(const std::pair<T, T>& p) const {
        auto h1 = std::hash<T>{}(p.first);
        auto h2 = std::hash<T>{}(p.second);
        return (h1 << SHIFT) ^ h2;
    }
};

/// `std::variant` wrapper without the need for using `std::get` to access
/// contained data.
///
/// Note: This class was designed to mimic the Pythons syntax: with it one can
/// return data of various types from functions (determined by some flag
/// argument or smth) without the need to use `std::get` for unpacking returned
/// data. The trick is that the deduced type is `ubpe::variant<Types...>`, but
/// if one provides the needed type as a type of a variable or with
/// `static_cast` the data is unpacked automatically.
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

/// `enum class` wrapper that extends the enum with `|` and `&` operators and
/// `.has` method.
template <typename Enum>
    requires std::is_enum_v<Enum>
class EnumWrapper {
    using UnderlyingType = std::underlying_type_t<Enum>;
    UnderlyingType value_{0};

   public:
    constexpr EnumWrapper() = default;

    /// Construct a wrapper from the value of the underlying `Enum`.
    constexpr EnumWrapper(Enum e) : value_(static_cast<UnderlyingType>(e)) {}

    constexpr EnumWrapper& operator|=(const EnumWrapper& other) {
        this->value_ |= other.value_;
        return *this;
    }

    constexpr EnumWrapper& operator&=(const EnumWrapper& other) {
        this->value_ &= other.value_;
        return *this;
    }

    constexpr EnumWrapper operator|(const EnumWrapper& other) const {
        return EnumWrapper(*this) |= other;
    }

    constexpr EnumWrapper operator&(const EnumWrapper& other) const {
        return EnumWrapper(*this) &= other;
    }

    constexpr explicit operator bool() const { return value_ != 0; }
    constexpr explicit operator Enum() const {
        return static_cast<Enum>(value_);
    }

    constexpr UnderlyingType get_value() const { return this->value_; }

    /// Check including of an instance of type `Enum`.
    constexpr bool has(Enum e) const {
        return (*this & EnumWrapper(e)).value_ != 0;
    }

    /// Check including of an instance of a type derived from this wrapper.
    template <typename T>
        requires std::is_base_of_v<EnumWrapper<Enum>, std::decay_t<T>>
    constexpr bool has(const T& flag) const {
        return (*this & flag).value_ != 0;
    }
};

/// A wrapper that converts the passed `enum class` into a flag type that
/// supports `|` and `&` operations, `.has` check, converting from underlying
/// enum and creating new flags by combining multiple elements of the underlying
/// enum.
template <typename E>
    requires std::is_enum_v<E>
class Flags : public EnumWrapper<E> {
   public:
    using EnumWrapper<E>::EnumWrapper;

    template <E Value>
    static constexpr Flags value() {
        return Flags(Value);
    }

    template <E... Values>
        requires(sizeof...(Values) > 0)
    static constexpr Flags combine() {
        using Underlying = std::underlying_type_t<E>;
        constexpr auto combined_value = (static_cast<Underlying>(Values) | ...);
        return static_cast<E>(combined_value);
    }
};

}  // namespace ubpe

#endif  // UBPE_UTILS
