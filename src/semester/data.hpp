#pragma once

#include <neo/concepts.hpp>
#include <neo/fwd.hpp>

#include <cinttypes>
#include <string>
#include <type_traits>
#include <variant>

namespace semester {

/**
 * Type and value to represent "null" values.
 */
constexpr inline struct null_t {
    friend constexpr bool operator==(null_t, null_t) noexcept { return true; }
    friend constexpr bool operator!=(null_t, null_t) noexcept { return false; }
} null;

/**
 * Tag type used to construct an empty array in a data object.
 */
constexpr inline struct empty_array_t {
} empty_array;

/**
 * Tag type used to construct an empty mapping in a data object.
 */
constexpr inline struct empty_mapping_t {
} empty_mapping;

namespace detail {

/// Simple trait to check if a `T` is one of the valid alternative in a variant.
/// (Base case undefined)
template <typename Variant, typename T>
constexpr bool variant_supports_v = false;

/// Implementation. Checks that any of the variant types is `T`
template <typename... Ts, typename T>
constexpr bool variant_supports_v<std::variant<Ts...>, T> = (std::is_same_v<T, Ts> || ...);

/**
 * data_conv_mems_for provides additional convenience methods to a basic_data
 * object based on the base types that it can hold. For each type T that can be
 * stored, a specialization fo data_conv_mems_for is derived from.
 *
 * In the primary definition (below), there are no convenience members.
 * Partial specializations are provided below that add the appropriate members.
 */
template <typename Derived, typename T>
struct data_conv_mems_for {};

/**
 * Like data_conv_mems_for, but add an additional layer that provides
 * convenience members for the language-intrinsic types that often
 * vary (int, unsigned, etc).
 */
template <typename Derived, typename T>
struct data_conv_mems_for_intrin {};

// Convenience macros to static_cast to the derived type (from CRTP)
#define AS_DERIVED static_cast<Derived&>(*this)
#define AS_C_DERIVED static_cast<const Derived&>(*this)

// Convenience macros to declare the convenience methods.
#define DECL_CONV_MEMS(Name, Type)                                                                 \
    constexpr bool is_##Name() const noexcept {                                                    \
        return AS_C_DERIVED.template holds_alternative<Type>();                                    \
    }                                                                                              \
    constexpr Type&       as_##Name() noexcept { return AS_DERIVED.template as<Type>(); }          \
    constexpr const Type& as_##Name() const noexcept { return AS_C_DERIVED.template as<Type>(); }  \
    static_assert(true)

// Helpers for `int`
template <typename Derived>
struct data_conv_mems_for_intrin<Derived, int> {
    DECL_CONV_MEMS(int, int);
};

// Helpers for `unsigned`
template <typename Derived>
struct data_conv_mems_for_intrin<Derived, unsigned> {
    DECL_CONV_MEMS(unsigned, unsigned);
};

// Declares a partial specialization of data_conv_mems_for for the given type.
#define DECL_CONV_MEMS_FOR_SPEC(Name, Type)                                                        \
    template <typename Derived>                                                                    \
    struct data_conv_mems_for<Derived, Type> : data_conv_mems_for_intrin<Derived, Type> {          \
        DECL_CONV_MEMS(Name, Type);                                                                \
    }

DECL_CONV_MEMS_FOR_SPEC(double, double);
DECL_CONV_MEMS_FOR_SPEC(float, float);
DECL_CONV_MEMS_FOR_SPEC(bool, bool);
DECL_CONV_MEMS_FOR_SPEC(char, char);

DECL_CONV_MEMS_FOR_SPEC(int8, std::int8_t);
DECL_CONV_MEMS_FOR_SPEC(int16, std::int16_t);
DECL_CONV_MEMS_FOR_SPEC(int32, std::int32_t);
DECL_CONV_MEMS_FOR_SPEC(int64, std::int64_t);

DECL_CONV_MEMS_FOR_SPEC(uint8, std::uint8_t);
DECL_CONV_MEMS_FOR_SPEC(uint16, std::uint16_t);
DECL_CONV_MEMS_FOR_SPEC(uint32, std::uint32_t);
DECL_CONV_MEMS_FOR_SPEC(uint64, std::uint64_t);

DECL_CONV_MEMS_FOR_SPEC(null, semester::null_t);

#undef DECL_CONV_MEMS_FOR_SPEC

/**
 * Convenience methods for std::basic_string for `char`
 */
template <typename Derived, typename Traits, typename Alloc>
struct data_conv_mems_for<Derived, std::basic_string<char, Traits, Alloc>> {
    using string_type = std::basic_string<char, Traits, Alloc>;

    DECL_CONV_MEMS(string, string_type);
};

/**
 * Convenience members for std::basic_string for `wchar_t`
 */
template <typename Derived, typename Traits, typename Alloc>
struct data_conv_mems_for<Derived, std::basic_string<wchar_t, Traits, Alloc>> {
    using wstring_type = std::basic_string<wchar_t, Traits, Alloc>;

    DECL_CONV_MEMS(wstring, wstring_type);
};

#undef DECL_CONV_MEMS
#undef AS_C_DERIVED
#undef AS_DERIVED

/**
 * Declare convenience members for the types that are available in the data
 * node. The primary is undefined:
 */
template <typename Base, typename Variant>
struct data_conv_mems;

// Forward-decl for data_conv_mems partial specialization
template <typename Traits>
class data_impl;

/**
 * For every type `T` that can be held in the data node, inherit from a
 * data_conv_mems_for<Derived, T>. The individual specializations of
 * data_conv_mems_for will provide the convenience members that will be
 * attached to the class.
 */
template <typename Traits, typename... Ts>
struct data_conv_mems<data_impl<Traits>, std::variant<Ts...>>
    : data_conv_mems_for<data_impl<Traits>, Ts>... {};

/**
 * Generate mapping-access members
 */
template <typename Traits, typename = void>
struct data_mapping_part : data_impl<Traits> {
    // Inherit the constructors
    using data_mapping_part::data_impl::data_impl;

    constexpr static bool supports_mappings = false;
};

/// Matches if traits has an `mapping_type`
template <typename Traits>
struct data_mapping_part<Traits, std::void_t<typename Traits::mapping_type>> : data_impl<Traits> {
    // Inherit the constructors
    using data_mapping_part::data_impl::data_impl;

    constexpr static bool supports_mappings = true;

    using mapping_type = typename Traits::mapping_type;

    constexpr bool is_mapping() const noexcept {
        return this->template holds_alternative<mapping_type>();
    }

    constexpr mapping_type&       as_mapping() { return this->template as<mapping_type>(); }
    constexpr const mapping_type& as_mapping() const { return this->template as<mapping_type>(); }

    // A constructor for an empty mapping type
    data_mapping_part(empty_mapping_t)
        : data_mapping_part::data_impl(mapping_type()) {}
};

/**
 * Generate array-access members
 */
template <typename Traits, typename = void>
struct data_array_part : data_mapping_part<Traits> {
    // Inherit the constructors
    using data_array_part::data_mapping_part::data_mapping_part;
    constexpr static bool supports_arrays = false;
};

/// Matches if traits has an `array_type`
template <typename Traits>
struct data_array_part<Traits,  //
                       std::void_t<typename Traits::array_type>> : data_mapping_part<Traits> {
    // Inherit the constructors
    using data_array_part::data_mapping_part::data_mapping_part;

    constexpr static bool supports_arrays = true;

    using array_type = typename Traits::array_type;

    constexpr bool is_array() const noexcept {
        return this->template holds_alternative<array_type>();
    }

    constexpr array_type&       as_array() { return this->template as<array_type>(); }
    constexpr const array_type& as_array() const { return this->template as<array_type>(); }

    // A constructor for an empty array type
    data_array_part(empty_array_t)
        : data_array_part::data_mapping_part(array_type()) {}
};

/**
 * The initial base class of the data structure.
 */
template <typename Traits>
struct basic_data_base : data_array_part<Traits> {
    using basic_data_base::data_array_part::data_array_part;
};

/**
 * The class that actually contains the variant, as well as the basis members
 * `as()`, `holds_alternative()`, and `supports`
 */
template <typename Traits>
class data_impl : public data_conv_mems<data_impl<Traits>, typename Traits::variant_type> {
public:
    /// The variant type that can store this class
    using variant_type = typename Traits::variant_type;
    using traits_type  = Traits;

private:
    /// The actual variant holding our data
    variant_type _var;

    template <typename T, typename U = T>
    static variant_type
    _convert(U&& value) noexcept(std::is_nothrow_constructible_v<variant_type, U>) {
        static_assert(neo::convertible_to<T, variant_type>,
                      "The given value is not implicitly convertible to the any underlying data "
                      "type, and no conversion function was provided");
        return static_cast<variant_type>(NEO_FWD(value));
    }

    template <typename T, typename U = T, typename TraitsDep = traits_type>
    static variant_type
    _convert(U&& value) noexcept(noexcept(traits_type::convert(NEO_FWD(value)))) requires requires {
        TraitsDep::convert(NEO_FWD(value));
    }
    { return traits_type::convert(NEO_FWD(value)); }

public:
    data_impl()                 = default;
    data_impl(const data_impl&) = default;
    data_impl(data_impl&&)      = default;

    data_impl& operator=(const data_impl&) = default;
    data_impl& operator=(data_impl&&) = default;

    /**
     * Support implicit conversions from supported types
     */
    template <typename Arg>
    requires(!std::is_base_of_v<data_impl, std::decay_t<Arg>>)  //
        data_impl(Arg&& arg) noexcept(noexcept(variant_type(_convert<Arg>(arg))))
        : _var(_convert<Arg>(arg)) {}

    /// Check if the data supports a certain type T
    template <typename T>
    constexpr static bool supports = variant_supports_v<variant_type, T>;

    /// Check whether we currently contain the type T
    template <typename T>
    requires supports<T> constexpr bool holds_alternative() const noexcept {
        return std::holds_alternative<T>(_var);
    }

    /// Obtain a reference to the underlying T
    template <typename T>
    requires supports<T> constexpr T& as() & {
        return std::get<T>(_var);
    }

    /// Obtain an rvalue-reference to the underlying T
    template <typename T>
    requires supports<T> constexpr T&& as() && {
        return std::get<T>(std::move(_var));
    }

    /// Obtain a const-reference to the underlying T
    template <typename T>
    requires supports<T> constexpr const T& as() const& {
        return std::get<T>(_var);
    }

    /// Get a reference to the underlying variant
    variant_type&       variant() & noexcept { return _var; }
    variant_type&&      variant() && noexcept { return std::move(_var); }
    const variant_type& variant() const& noexcept { return _var; }

    /// Execute a visitor against the data
    template <typename Func>
    decltype(auto) visit(Func&& fn) noexcept(noexcept(std::visit(NEO_FWD(fn), _var))) {
        return std::visit(NEO_FWD(fn), _var);
    }

    /// Execute a visitor agains the data
    template <typename Func>
    decltype(auto) visit(Func&& fn) const noexcept(noexcept(std::visit(NEO_FWD(fn), _var))) {
        return std::visit(NEO_FWD(fn), _var);
    }

    // For the "transparent"-ish equality operators, inhibit conversions that unintentionally
    // add them to the overload set when comparing disparate types. MSVC/GCC rank these overloads
    // differently, and MSVC will consider it ambiguous.
    template <typename T,
              neo::derived_from<data_impl> SelfType,
              typename = std::enable_if_t<supports<T>>>
    friend constexpr bool operator==(const SelfType& lhs,
                                     const T&        rhs) noexcept(noexcept(std::declval<const T&>()
                                                                     == std::declval<const T&>())) {
        return lhs.template holds_alternative<T>() && lhs.template as<T>() == rhs;
    }

    template <typename T,
              neo::derived_from<data_impl> SelfType,
              typename = std::enable_if_t<supports<T>>>
    friend constexpr bool operator==(const T&        lhs,
                                     const SelfType& rhs) noexcept(noexcept(rhs == lhs)) {
        return rhs == lhs;
    }

    template <typename T,
              neo::derived_from<data_impl> SelfType,
              typename = std::enable_if_t<supports<T>>>
    friend constexpr bool operator!=(const SelfType& lhs,
                                     const T&        rhs) noexcept(noexcept(lhs == rhs)) {
        return !(lhs == rhs);
    }

    template <typename T,
              neo::derived_from<data_impl> SelfType,
              typename = std::enable_if_t<supports<T>>>
    friend constexpr bool operator!=(const T&        lhs,
                                     const SelfType& rhs) noexcept(noexcept(rhs == lhs)) {
        return !(rhs == lhs);
    }

    friend constexpr bool operator==(const data_impl& lhs, const data_impl& rhs) noexcept(
        noexcept(std::declval<const variant_type&>() == std::declval<const variant_type&>())) {
        return lhs.variant() == rhs.variant();
    }

    friend constexpr bool operator!=(const data_impl& lhs, const data_impl& rhs) noexcept(
        noexcept(std::declval<const variant_type&>() != std::declval<const variant_type&>())) {
        return lhs.variant() != rhs.variant();
    }
};

}  // namespace detail

/**
 * The root template of the entire library. Stores a (possible recursive) data
 * structure of some unknown number of basis types. The type of each value in
 * the structure can be queried and switched on at runtime, and generic
 * algorithms can be written to traverse the structure.
 */
template <typename TraitsTemplate>
class basic_data : public detail::basic_data_base<
                       typename TraitsTemplate::template traits<basic_data<TraitsTemplate>>> {
public:
    using basic_data::basic_data_base::basic_data_base;
};

}  // namespace semester
