#pragma once

#include <neo/concepts.hpp>
#include <neo/fwd.hpp>
#include <neo/opt_ref.hpp>
#include <neo/ref.hpp>

#include <variant>

namespace semester {

// clang-format off
namespace get_detail {

template <typename>
void get(...) = delete;

template <typename Var, typename T>
constexpr inline bool variant_check = true;

template <typename T, typename... Types>
constexpr inline bool variant_check<std::variant<Types...>, T> =
    (neo::same_as<T, Types> || ...);

template <typename Var, typename T>
concept as_member = requires(std::remove_cvref_t<Var> v, neo::make_cref_t<Var&> cv) {
    { v.template as<T>() } -> neo::alike<T>;
    { cv.template as<T>() } -> neo::alike<T>;
};

template <typename Var, typename T>
concept get_nonmember =
    variant_check<T, Var> &&
    requires(std::remove_cvref_t<Var> v,
             neo::make_cref_t<Var&> cv) {
        { get<T>(v) } -> neo::alike<T>;
        { get<T>(cv) } -> neo::alike<T>;
    };

}

template <typename T>
struct get_fn {
    template <typename Var>
        requires (
            get_detail::as_member<Var, T> ||
            get_detail::get_nonmember<Var, T>
        )
    constexpr decltype(auto) operator()(Var&& var) const {
        if constexpr (get_detail::as_member<Var, T>) {
            return NEO_FWD(var).template as<T>();
        } else {
            using get_detail::get;
            return get<T>(NEO_FWD(var));
        }
    }
};

template <typename T>
inline constexpr get_fn<T> get = {};

namespace holds_alt_detail {

template <typename>
void holds_alternative(...) = delete;

template <typename Var, typename T>
concept has_member = requires(const Var& v) {
    { v.template holds_alternative<T>() } noexcept -> neo::simple_boolean;
};

template <typename Var, typename T>
concept has_nonmember =
    requires(const Var& v) {
        { holds_alternative<T>(v) } noexcept -> neo::simple_boolean;
    };

template <typename Var,
          typename T>
struct variant_guard {
    static constexpr bool value =
        holds_alt_detail::has_member<Var, T> ||
        holds_alt_detail::has_nonmember<Var, T>;
};

template <typename... Ts, typename T>
struct variant_guard<std::variant<Ts...>, T> {
    static constexpr bool value = (neo::same_as<Ts, T> || ...);
};

} // namespace holds_alt_detail

template <typename T>
struct holds_alternative_fn {
    template <typename Var>
        requires (holds_alt_detail::variant_guard<Var, T>::value)
    constexpr bool operator()(const Var& v) const noexcept {
        if constexpr (holds_alt_detail::has_member<Var, T>) {
            return v.template holds_alternative<T>();
        } else {
            using holds_alt_detail::holds_alternative;
            return holds_alternative<T>(v);
        }
    }
};

template <typename T>
inline constexpr holds_alternative_fn<T> holds_alternative = {};

template <typename T, typename Var>
    requires requires(Var v) {
        { semester::holds_alternative<T>(v) };
        { semester::get<T>(v) };
    }
constexpr
std::conditional_t<std::is_const_v<std::remove_reference_t<Var>>,
    neo::opt_ref<const T>,
    neo::opt_ref<T>>
try_get(Var& v) noexcept {
    if (semester::holds_alternative<T>(v)) {
        return semester::get<T>(NEO_FWD(v));
    } else {
        return {};
    }
}

template <typename Var, typename T>
concept supports_alternative =
    neo::convertible_to<T, std::remove_cvref_t<Var>> &&
    requires(Var var) {
        try_get<T>(var);
    };

// clang-format on

}  // namespace semester