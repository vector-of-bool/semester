#pragma once

#include "./get.hpp"

#include <neo/tag.hpp>
#include <neo/ufmt.hpp>

#include <cassert>
#include <concepts>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <tuple>

namespace smstr {

/**
 * @brief Access information about the semistructured data type `T`
 *
 * @tparam T
 */
template <typename T>
struct data_traits {
    using _is_default_definition_ = void;
};

template <typename T>
using as_const_t = decltype(std::as_const(NEO_DECLVAL(T&)));

// clang-format off
/**
 * @brief Match types that are string-view-like. That is, they are a random-access
 * sequence of characters that provides a .length(), .size(), and .data(). NOTE that
 * string-like types are also string-view-like (e.g. std::string).
 *
 * @tparam View A (cvr-qualified) type that can be used like a string_view
 */
template <typename View>
concept string_view_like =
    std::ranges::random_access_range<View> &&
    std::regular<std::remove_cvref_t<View>> &&
    requires(View&                            view,
             std::ranges::range_value_t<View> chr,
             std::ranges::range_size_t<View>  idx) {
        requires std::same_as<
            typename std::remove_cvref_t<View>::traits_type::char_type,
            std::ranges::range_value_t<View>>;
        view.length();
        view.size();
        view[idx];
        view[idx] == chr;
        { std::ranges::cdata(view) }
            -> std::convertible_to<std::add_pointer_t<const std::ranges::range_value_t<View>>>;
    };
// clang-format on

/**
 * @brief Create a base class that defines several common operations for common `T` types
 *
 * `T` is already specialized for integral types, floating point types, string-like types,
 * array-like types, and map-like types. These specializations are based on correspoding
 * concepts `std::integral`, `std::floating_point`, `smstr::string_like`, smstr::array_like`,
 * and `smstr::map_like`.
 *
 * @tparam T
 */
template <typename T>
struct partial_data_traits {
    using _is_default_definition_ = void;
};

template <std::integral I>
struct partial_data_traits<I> {
    using integer_type = I;
    static constexpr integer_type integer(string_view_like auto&& str) {
        integer_type ret = 0;
        for (char32_t c : str) {
            assert(c >= '0' && c <= '9');
            auto val = c - '0';
            ret *= 10;
            ret += val;
        }
        return ret;
    }
};

template <std::floating_point F>
struct partial_data_traits<F> {
    using float_type = F;

    static constexpr float_type decimal(string_view_like auto&& str) {
        if constexpr (std::same_as<float_type, double>) {
            return std::stod(std::string(str));
        } else {
            return std::stof(std::string(str));
        }
    }
};

// clang-format off
/**
 * @brief Concept matching types that are string-like. That is, the type provides
 * owning-semantics over a ranges of characters. Must also satisfy string_view_like.
 *
 * @tparam String
 */
template <typename String>
concept string_like =
    string_view_like<String> &&
    requires(std::remove_cvref_t<String> string,
             as_const_t<String> const_string,
             const std::ranges::range_value_t<String> chr,
             const std::ranges::range_size_t<String> idx) {
        string[idx] = chr;
        string.push_back(chr);
        string.append(const_string);
        string.resize(idx);
        { std::ranges::data(string) }
            -> std::convertible_to<std::add_pointer_t<std::ranges::range_value_t<String>>>;
        { std::ranges::data(const_string) }
            -> std::convertible_to<std::add_pointer_t<const std::ranges::range_value_t<String>>>;
        { std::ranges::cdata(string) }
            -> std::convertible_to<std::add_pointer_t<const std::ranges::range_value_t<String>>>;
    };
// clang-format on

template <string_like String>
struct partial_data_traits<String> {
    using string_type = String;

    static constexpr String string(string_view_like auto&& str) noexcept {
        String ret;
        ret.resize(str.size());
        std::copy(str.begin(), str.end(), ret.begin());
        return ret;
    }
};

// clang-format off
/**
 * @brief Concept for null-like types. The type MUST be named `null_t`, empty,
 * must always be `constexpr` equal to itself, and must be convertible from
 * `nullptr`.
 *
 * @tparam Null
 */
template <typename Null>
concept null_like =
    std::regular<Null> &&
    std::convertible_to<std::nullptr_t, Null> &&
    requires(Null null) {
        requires std::same_as<Null, typename Null::null_t>;
        requires std::same_as<Null, typename Null::null_t::null_t>;
        requires null == null;
        requires std::is_empty_v<Null>;
    };
// clang-format on

template <null_like Null>
struct partial_data_traits<Null> {
    using null_type = Null;
    constexpr static Null null() noexcept { return nullptr; }
};

/// Specialization for bool
template <>
struct partial_data_traits<bool> {
    using bool_type = bool;
    constexpr static bool boolean(bool b) noexcept { return b; }
};

// clang-format off
/// Specialization for array types
template <typename Array>
concept array_like =
    std::ranges::forward_range<Array> &&
    std::regular<Array> &&
    requires(as_const_t<Array>                       c_array,
             std::remove_cvref_t<Array>&             array,
             const std::ranges::range_value_t<Array> item,
             std::ranges::iterator_t<Array>          iter,
             std::ranges::iterator_t<as_const_t<Array>> c_iter) {
        array.push_back(item);
        array.emplace_back(item);
        array.insert(iter, item);
        array.insert(iter, c_iter, c_iter);
        array.erase(iter);
        array.erase(iter, iter);
    };
// clang-format on

template <array_like Array>
struct partial_data_traits<Array> {
    using array_type = Array;

    constexpr static array_type new_array() noexcept { return array_type(); }

    constexpr static void
    array_push(array_type&                                                        arr,
               neo::convertible_to<std::ranges::range_value_t<array_type>> auto&& value) {
        arr.push_back(NEO_FWD(value));
    }

    constexpr static decltype(auto) as_array(supports_alternative<array_type> auto&& data) {
        return smstr::get<array_type>(NEO_FWD(data));
    }

    constexpr static decltype(auto) as_array(neo::alike<array_type> auto&& arr) noexcept {
        return NEO_FWD(arr);
    }

    constexpr static array_type concat(auto&& left_, auto&& right_) {
        auto   left  = as_array(NEO_FWD(left_));
        auto&& right = as_array(NEO_FWD(right_));
        left.insert(left.end(), right.begin(), right.end());
        return left;
    }
};

template <typename Map>
using key_type_t = std::remove_cvref_t<Map>::key_type;
template <typename Map>
using mapped_type_t = std::remove_cvref_t<Map>::mapped_type;

// clang-format off
/**
 * @brief Concept for map-like types. That is, associative array containers,
 * which have a key_type and a mapped_type
 */
template <typename Map>
concept map_like =
    std::ranges::forward_range<Map> &&
    requires(as_const_t<Map>                       c_map,
             std::remove_cvref_t<Map>              map,
             const key_type_t<Map>                 key,
             const mapped_type_t<Map>              value,
             const std::ranges::range_value_t<Map> entry_pair,
             const std::ranges::iterator_t<Map>    iter,
             const std::ranges::iterator_t<
                    as_const_t<Map>>               c_iter) {
    typename key_type_t<Map>;
    typename mapped_type_t<Map>;
    requires std::tuple_size_v<std::ranges::range_value_t<Map>> == 2;
    { c_map.find(key) }
        -> std::same_as<std::ranges::iterator_t<as_const_t<Map>>>;
    { map.find(key) }
        -> std::same_as<std::ranges::iterator_t<Map>>;
    map.erase(iter);
    { map.insert(entry_pair).first }
        -> std::convertible_to<std::ranges::iterator_t<Map>>;
    { map.insert(entry_pair).second }
        -> std::convertible_to<bool>;
    map.emplace(key, value);
};
// clang-format on

template <map_like Map>
struct partial_data_traits<Map> {
    using map_type = Map;

    static constexpr Map  new_map() noexcept { return Map(); }
    static constexpr void map_insert(Map&                                           map,
                                     std::convertible_to<key_type_t<Map>> auto&&    key,
                                     std::convertible_to<mapped_type_t<Map>> auto&& value) {
        map.insert(typename Map::value_type(NEO_FWD(key), NEO_FWD(value)));
    }

    static constexpr decltype(auto) as_map(supports_alternative<map_type> auto&& data) {
        return smstr::get<Map>(NEO_FWD(data));
    }

    static constexpr decltype(auto) as_map(neo::alike<map_type> auto&& map) noexcept {
        return NEO_FWD(map);
    }

    static constexpr void map_spread(Map& map, const auto& other) {
        for (const auto& pair : other) {
            const auto& [key, value] = pair;
            map.insert_or_assign(key, value);
        }
    }

    static constexpr auto& map_lookup(Map& map, std::string_view key) {
        auto found = map.find(key);
        if (found != map.cend()) {
            return found->second;
        } else {
            throw std::runtime_error(neo::ufmt("Map object has no key '{}'", key));
        }
    }
};

template <typename T>
concept has_partial_data_traits = !requires {
    typename partial_data_traits<T>::_is_default_definition_;
};

template <typename Var>
struct data_traits_for_variant;

template <template <class...> class Variant, typename... Ts>
struct data_traits_for_variant<Variant<Ts...>> : partial_data_traits<Ts>... {};

struct default_traits : data_traits_for_variant<neo::tag<double, std::int64_t, std::string, bool>> {
};

template <typename T>
concept has_valid_data_traits = !requires {
    typename data_traits<T>::_is_default_definition_;
};

/// The map type of the given data type
template <typename T>
using map_type_t = data_traits<std::remove_cvref_t<T>>::map_type;

/// The array type of the given data type
template <typename T>
using array_type_t = data_traits<std::remove_cvref_t<T>>::array_type;

/// The string type of the given data type
template <typename T>
using string_type_t = data_traits<std::remove_cvref_t<T>>::string_type;

/// The integer type of the given data type
template <typename T>
using integer_type_t = data_traits<std::remove_cvref_t<T>>::integer_type;

/// The floating-point-number type of the given data type
template <typename T>
using float_type_t = data_traits<std::remove_cvref_t<T>>::float_type;

/// The null-value type of the given data type
template <typename T>
using null_type_t = data_traits<std::remove_cvref_t<T>>::null_type;

/// Constrain to types that have a mapping type
template <typename T>
concept supports_maps = requires {
    typename map_type_t<T>;
};

/// Constraint to types that have an array type
template <typename T>
concept supports_arrays = requires {
    typename array_type_t<T>;
};

/// Constraint to types that have a string type
template <typename T>
concept supports_strings = requires {
    typename string_type_t<T>;
};

/// If the given type contains a map, return that map
template <supports_maps Data>
[[nodiscard]] constexpr decltype(auto) as_map(Data&& d) {
    return smstr::get<map_type_t<Data>>(NEO_FWD(d));
}

/// If the given type contains an array, return that array
template <supports_arrays Data>
[[nodiscard]] constexpr decltype(auto) as_array(Data&& d) {
    return smstr::get<array_type_t<Data>>(NEO_FWD(d));
}

/// If the given type contains a string, return that string
template <supports_strings Data>
[[nodiscard]] constexpr decltype(auto) as_string(Data&& d) {
    return smstr::get<string_type_t<Data>>(NEO_FWD(d));
}

/// Test if the given data contains a map
template <supports_maps Data>
[[nodiscard]] constexpr bool is_map(Data&& d) {
    return smstr::holds_alternative<map_type_t<Data>>(d);
}

/// Test if the given data is an array
template <supports_arrays Data>
[[nodiscard]] constexpr bool is_array(Data&& d) {
    return smstr::holds_alternative<array_type_t<Data>>(d);
}

/// Test if the given data is a string
template <supports_strings Data>
[[nodiscard]] constexpr bool is_string(Data&& d) {
    return smstr::holds_alternative<string_type_t<Data>>(d);
}

}  // namespace smstr
