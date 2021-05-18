#pragma once

#include <semester/get.hpp>
#include <semester/primitives.hpp>

#include <neo/concepts.hpp>
#include <neo/declval.hpp>
#include <neo/fwd.hpp>

#include <cinttypes>
#include <string>
#include <type_traits>
#include <variant>

namespace smstr {

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

template <typename T>
using mapping_type_t = typename std::remove_cvref_t<T>::mapping_type;

template <typename T>
using array_type_t = typename std::remove_cvref_t<T>::array_type;

namespace detail {

/**
 * data_conv_mems_for provides additional convenience methods to a basic_data
 * object based on the base types that it can hold. For each type T that can be
 * stored, a specialization of data_conv_mems_for is derived from.
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
        return smstr::holds_alternative<Type>(AS_C_DERIVED);                                       \
    }                                                                                              \
    constexpr Type&       as_##Name() noexcept { return smstr::get<Type>(AS_DERIVED); }            \
    constexpr const Type& as_##Name() const noexcept { return smstr::get<Type>(AS_C_DERIVED); }    \
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

DECL_CONV_MEMS_FOR_SPEC(null, smstr::null_t);

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
template <typename Traits>
struct data_mapping_part : data_impl<Traits> {
    // Inherit the constructors
    using data_mapping_part::data_impl::data_impl;

    constexpr static bool supports_mappings = false;
};

/// Matches if traits has a `mapping_type`
template <typename Traits>
requires requires {
    typename mapping_type_t<Traits>;
}
struct data_mapping_part<Traits> : data_impl<Traits> {
    // Inherit the constructors
    using data_mapping_part::data_impl::data_impl;

    constexpr static bool supports_mappings = true;

    using mapping_type = mapping_type_t<Traits>;

    constexpr bool is_mapping() const noexcept {
        return smstr::holds_alternative<mapping_type>(*this);
    }

    constexpr mapping_type&       as_mapping() { return smstr::get<mapping_type>(*this); }
    constexpr const mapping_type& as_mapping() const { return smstr::get<mapping_type>(*this); }

    // A constructor for an empty mapping type
    constexpr data_mapping_part(empty_mapping_t)
        : data_mapping_part::data_impl(mapping_type()) {}
};

/**
 * Generate array-access members
 */
template <typename Traits>
struct data_array_part : data_mapping_part<Traits> {
    // Inherit the constructors
    using data_array_part::data_mapping_part::data_mapping_part;
    constexpr static bool supports_arrays = false;
};

/// Matches if traits has an `array_type`
template <typename Traits>
requires requires {
    typename array_type_t<Traits>;
}
struct data_array_part<Traits> : data_mapping_part<Traits> {
    // Inherit the constructors
    using data_array_part::data_mapping_part::data_mapping_part;

    constexpr static bool supports_arrays = true;

    using array_type = array_type_t<Traits>;

    constexpr bool is_array() const noexcept { return smstr::holds_alternative<array_type>(*this); }

    constexpr array_type&       as_array() { return smstr::get<array_type>(*this); }
    constexpr const array_type& as_array() const { return smstr::get<array_type>(*this); }

    // A constructor for an empty array type
    constexpr data_array_part(empty_array_t)
        : data_array_part::data_mapping_part(array_type()) {}
};

/**
 * The initial base class of the data structure.
 */
template <typename Traits>
struct basic_data_base : data_array_part<Traits> {
    using basic_data_base::data_array_part::data_array_part;
};

template <typename Traits, typename T>
concept traits_has_convert = requires(T&& value) {
    { Traits::convert(NEO_FWD(value)) }
    ->neo::convertible_to<typename Traits::variant_type>;
};

template <typename Traits, typename T>
concept traits_has_nothrow_convert = requires(T&& value) {
    { Traits::convert(NEO_FWD(value)) }
    noexcept->neo::convertible_to<typename Traits::variant_type>;
};

/**
 * The class that actually contains the variant, as well as the basis member
 * `try_get()`
 */
template <typename Traits>
class data_impl : public data_conv_mems<data_impl<Traits>, typename Traits::variant_type> {
public:
    /// The variant type that can store this class
    using variant_type = typename Traits::variant_type;
    /// The traits type for this block of data
    using traits_type = Traits;

private:
    /// The actual variant holding our data
    variant_type _var;

    // Check that 'T' can convert to this data type
    template <typename T>
    constexpr static bool _convert_check =       //
        neo::convertible_to<T, variant_type> ||  // Plain conversion
        traits_has_convert<traits_type, T>;      // Or a convert() traits function

    // Check if the conversion (if available) is noexcept
    template <typename T>
    constexpr static bool _convert_noexcept =                //
        std::is_nothrow_constructible_v<variant_type, T> ||  //
        traits_has_nothrow_convert<traits_type, T>;

    template <typename T>
    requires _convert_check<T> constexpr decltype(auto)
    _convert(T&& value) noexcept(_convert_noexcept<T>) {
        if constexpr (traits_has_convert<traits_type, T>) {
            return traits_type::convert(NEO_FWD(value));
        } else {
            return NEO_FWD(value);
        }
    }

public:
    constexpr data_impl() = default;

    /**
     * Support implicit conversions from supported types
     */
    template <typename Arg>
    requires _convert_check<Arg>  //
        constexpr data_impl(Arg&& arg) noexcept(noexcept(variant_type(_convert(arg))))
        : _var(_convert(NEO_FWD(arg))) {}

    /// Check if the data supports a certain type T
    template <typename T>
    constexpr static bool supports = supports_alternative<variant_type, T>;

    template <typename T>
    requires supports<T> constexpr T* try_get() noexcept {
        return smstr::try_get<T>(_var);
    }

    template <typename T>
    requires supports<T> constexpr const T* try_get() const noexcept {
        return smstr::try_get<T>(_var);
    }

    /// Get a reference to the underlying variant
    constexpr variant_type&       variant() & noexcept { return _var; }
    constexpr variant_type&&      variant() && noexcept { return std::move(_var); }
    constexpr const variant_type& variant() const& noexcept { return _var; }

    /// Execute a visitor against the data
    template <typename Func>
    constexpr decltype(auto) visit(Func&& fn) noexcept(noexcept(std::visit(NEO_FWD(fn), _var))) {
        return std::visit(NEO_FWD(fn), _var);
    }

    /// Execute a visitor agains the data
    template <typename Func>
    constexpr decltype(auto) visit(Func&& fn) const
        noexcept(noexcept(std::visit(NEO_FWD(fn), _var))) {
        return std::visit(NEO_FWD(fn), _var);
    }

    template <typename T>
    requires supports<T> constexpr bool operator==(const T& rhs) const
        noexcept(noexcept(NEO_DECLVAL(const T&) == NEO_DECLVAL(const T&))) {
        const auto ptr = smstr::try_get<T>(*this);
        return ptr && (*ptr == rhs);
    }

    constexpr bool operator==(const data_impl& rhs) const
        noexcept(noexcept(NEO_DECLVAL(const variant_type&) == NEO_DECLVAL(const variant_type&))) {
        return variant() == rhs.variant();
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

}  // namespace smstr
