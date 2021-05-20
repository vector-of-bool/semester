#pragma once

#include "./traits.hpp"
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
 * Tag type used to construct an empty map in a data object.
 */
constexpr inline struct empty_map_t {
} empty_map;

namespace detail {

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

}  // namespace detail

/**
 * The root template of the entire library. Stores a (possible recursive) data
 * structure of some unknown number of basis types. The type of each value in
 * the structure can be queried and switched on at runtime, and generic
 * algorithms can be written to traverse the structure.
 */
template <typename TraitsTemplate>
class basic_data {
private:
    using this_t = basic_data;

public:
    /// The traits type for this block of data
    using traits_type = TraitsTemplate::template traits<this_t>;
    /// The variant type that can store this class
    using variant_type = traits_type::variant_type;

private:
    /// The actual variant holding our data
    variant_type _var;

    // Check that 'T' can convert to this data type
    template <typename T>
    constexpr static bool _convert_check =           //
        neo::convertible_to<T, variant_type> ||      // Plain conversion
        detail::traits_has_convert<traits_type, T>;  // Or a convert() traits function

    // Check if the conversion (if available) is noexcept
    template <typename T>
    constexpr static bool _convert_noexcept =                //
        std::is_nothrow_constructible_v<variant_type, T> ||  //
        detail::traits_has_nothrow_convert<traits_type, T>;

    template <typename T>
    requires _convert_check<T> constexpr decltype(auto)
    _convert(T&& value) noexcept(_convert_noexcept<T>) {
        if constexpr (detail::traits_has_convert<traits_type, T>) {
            return traits_type::convert(NEO_FWD(value));
        } else {
            return NEO_FWD(value);
        }
    }

public:
    constexpr basic_data() = default;

    /**
     * Support implicit conversions from supported types
     */
    template <typename Arg>
    requires _convert_check<Arg>  //
        constexpr basic_data(Arg&& arg) noexcept(_convert_noexcept<Arg>)
        : _var(_convert(NEO_FWD(arg))) {}

    // A constructor for an empty array type
    constexpr basic_data(empty_array_t) noexcept requires supports_arrays<this_t>  //
        : basic_data(array_type_t<this_t>()) {}

    constexpr basic_data(empty_map_t) noexcept requires supports_maps<this_t>  //
        : basic_data(map_type_t<this_t>()) {}

    template <supported_alternative_of<variant_type> T>
    [[nodiscard]] constexpr T* try_get() noexcept {
        return smstr::try_get<T>(_var);
    }

    template <supported_alternative_of<variant_type> T>
    [[nodiscard]] constexpr const T* try_get() const noexcept {
        return smstr::try_get<T>(_var);
    }

    /// Get a reference to the underlying variant
    [[nodiscard]] constexpr variant_type&       variant() & noexcept { return _var; }
    [[nodiscard]] constexpr variant_type&&      variant() && noexcept { return std::move(_var); }
    [[nodiscard]] constexpr const variant_type& variant() const& noexcept { return _var; }

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

    template <supported_alternative_of<variant_type> T>
    [[nodiscard]] constexpr bool operator==(const T& rhs) const
        noexcept(noexcept(NEO_DECLVAL(const T&) == NEO_DECLVAL(const T&))) {
        const auto ptr = smstr::try_get<T>(*this);
        return ptr && (*ptr == rhs);
    }

    [[nodiscard]] constexpr bool operator==(const basic_data& rhs) const
        noexcept(noexcept(NEO_DECLVAL(const variant_type&) == NEO_DECLVAL(const variant_type&))) {
        return variant() == rhs.variant();
    }

    // [[nodiscard]] constexpr bool is_array() const noexcept requires supports_arrays<this_t> {
    //     return smstr::holds_alternative<array_type_t<this_t>>(*this);
    // }

    // [[nodiscard]] constexpr decltype(auto) as_array() const& requires supports_arrays<this_t> {
    //     return smstr::get<array_type_t<this_t>>(*this);
    // }

    // [[nodiscard]] constexpr decltype(auto) as_array() const&& requires supports_arrays<this_t> {
    //     return smstr::get<array_type_t<this_t>>(std::move(*this));
    // }

    // [[nodiscard]] constexpr decltype(auto) as_array() & requires supports_arrays<this_t> {
    //     return smstr::get<array_type_t<this_t>>(*this);
    // }

    // [[nodiscard]] constexpr decltype(auto) as_array() && requires supports_arrays<this_t> {
    //     return smstr::get<array_type_t<this_t>>(std::move(*this));
    // }

    // [[nodiscard]] constexpr bool is_map() const noexcept requires supports_maps<this_t> {
    //     return smstr::holds_alternative<map_type_t<this_t>>(*this);
    // }

    // [[nodiscard]] constexpr decltype(auto) as_map() const& requires supports_maps<this_t> {
    //     return smstr::get<map_type_t<this_t>>(*this);
    // }

    // [[nodiscard]] constexpr decltype(auto) as_map() const&& requires supports_maps<this_t> {
    //     return smstr::get<map_type_t<this_t>>(std::move(*this));
    // }

    // [[nodiscard]] constexpr decltype(auto) as_map() & requires supports_maps<this_t> {
    //     return smstr::get<map_type_t<this_t>>(*this);
    // }

    // [[nodiscard]] constexpr decltype(auto) as_map() && requires supports_maps<this_t> {
    //     return smstr::get<map_type_t<this_t>>(std::move(*this));
    // }
};

template <typename TraitsTemplate>
struct data_traits<basic_data<TraitsTemplate>>
    : data_traits_for_variant<typename basic_data<TraitsTemplate>::variant_type> {
    // Convert a value to a basic_data
    using data_type = basic_data<TraitsTemplate>;
    constexpr static basic_data<TraitsTemplate> normalize(auto&& arg) { return arg; }

    // clang-format off
    constexpr static auto integer(auto string) noexcept
        requires requires { data_traits::decimal(string); }
              && (not requires { data_traits::data_traits_for_variant::integer(string); })
    {
        return data_traits::decimal(string);
    }
    // clang-format on
};

}  // namespace smstr
