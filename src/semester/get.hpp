#pragma once

#include <neo/concepts.hpp>
#include <neo/fwd.hpp>
#include <neo/iterator_concepts.hpp>
#include <neo/opt_ref.hpp>
#include <neo/ref.hpp>

#include <variant>

namespace smstr {

// clang-format off
namespace get_detail {

template <typename>
void get_if(...) = delete;

template <typename>
void try_get(...) = delete;

template <typename Result, typename T>
concept try_get_result_of =
    neo::simple_boolean<Result> &&
    neo::indirectly_readable<Result> &&
    requires (Result res) {
        { *res } -> neo::convertible_to<T>;
    };

template <typename Var, typename T>
concept try_get_member = requires(Var&& v) {
    { NEO_FWD(v).template try_get<T>() } noexcept
        -> try_get_result_of<T>;
};

template <typename Var, typename T>
concept try_get_nonmember = requires(Var&& v) {
    { try_get_if<T>(NEO_FWD(v)) } noexcept
        -> try_get_result_of<T>;
};

template <typename Var, typename T>
concept get_if_nonmember_ref = requires(Var&& v) {
    { get_if<T>(v) } noexcept -> try_get_result_of<T>;
};

template <typename Var, typename T>
concept get_if_nonmember_ptr = requires(std::remove_reference_t<Var>* v) {
    { get_if<T>(v) } noexcept -> try_get_result_of<T>;
};

template <typename Var, typename T>
concept try_get_check =
    get_if_nonmember_ptr<Var, T> ||
    get_if_nonmember_ref<Var, T> ||
    try_get_nonmember<Var, T> ||
    try_get_member<Var, T>;

}

template <typename T>
struct try_get_fn {
    template <get_detail::try_get_check<T> Var>
    constexpr decltype(auto) operator()(Var&& var) const noexcept {
        using namespace get_detail;
        if constexpr (get_if_nonmember_ptr<Var, T>) {
            return get_if<T>(std::addressof(var));
        } else if constexpr (get_if_nonmember_ref<Var, T>) {
            return get_if<T>(var);
        } else if constexpr (try_get_nonmember<Var, T>) {
            return try_get<T>(var);
        } else {
            static_assert(try_get_member<Var, T>);
            return var.template try_get<T>();
        }
    }
};

template <typename T>
inline constexpr try_get_fn<T> try_get = {};

template <typename T>
struct get_fn {
    template <typename Var>
        requires get_detail::try_get_check<Var, T>
    constexpr decltype(auto) operator()(Var&& var) const {
        auto&& result = try_get<T>(var);
        if (!result) {
            throw std::bad_variant_access();
        }
        if constexpr (std::is_rvalue_reference_v<Var&&>) {
            return std::move(*result);
        } else {
            return *result;
        }
    }
};

template <typename T>
inline constexpr get_fn<T> get = {};

template <typename T>
struct holds_alternative_fn {
    template <get_detail::try_get_check<T> Var>
    constexpr bool operator()(const Var& v) const noexcept {
        return !!try_get<T>(v);
    }
};

template <typename T>
inline constexpr holds_alternative_fn<T> holds_alternative = {};

template <typename Var, typename T>
concept supports_alternative =
    neo::convertible_to<T, std::remove_cvref_t<Var>> &&
    requires(Var&& var) {
        try_get<T>(var);
    };

// clang-format on

}  // namespace smstr
