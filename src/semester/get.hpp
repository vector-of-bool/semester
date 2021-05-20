#pragma once

#include <neo/concepts.hpp>
#include <neo/fwd.hpp>
#include <neo/iterator_concepts.hpp>

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
        { *res } -> neo::alike<T>;
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
       !std::is_void_v<T>
    && !std::is_array_v<T>
    && !std::is_const_v<T>
    && !std::is_volatile_v<T>
    && !std::is_reference_v<T>
    && (get_if_nonmember_ptr<Var, T> ||
        get_if_nonmember_ref<Var, T> ||
        try_get_nonmember<Var, T> ||
        try_get_member<Var, T>);
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

/**
 * @brief Try to obtain a pointer to the a `T` held by the given variant.
 * If the variant does not hold an instance of `T`, then returns a null pointer
 *
 * @tparam T The type to try and get out of the variant
 *
 * This call will wrap one of the following expressions, in order:
 *
 * 1. `get_if<T>(std::addressof(var))` if there exists a valid non-member ADL-visible
 *    `get_if<T>` that accepts a pointer to the variant.
 * 2. `get_if<T>(var)` if there exists a valid non-member ADL-visible `get_if<T>`
 *    that accepts the variant by reference.
 * 3. `try_get<T>(var)` if there exists a valid non-member ADL-visible `try_get<T>`
 *    that accepts the variant by reference. (This CPO is not included.)
 * 4. `var.try_get<T>()` if there exists a member function template `try_get<T>` on
 *    the given variant type.
 *
 * If none of the above expressions are valid, `try_get<T>(var)` is not callable.
 *
 * The pointed-to return type has the same cv-qualifiers as the given variant.
 */
template <typename T>
inline constexpr try_get_fn<T> try_get = {};

template <typename T>
struct get_fn {
    template <get_detail::try_get_check<T> Var>
    [[nodiscard]]
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

/**
 * @brief Callable (CPO) that obtains a reference to the `T` held within the given variant.
 * The returned reference will have the same cvref-qualifiers as the given variant. Requires that
 * there exist a valid `try_get<T>` for the given variant type.
 */
template <typename T>
inline constexpr get_fn<T> get = {};

template <typename T>
struct holds_alternative_fn {
    [[nodiscard]]
    constexpr bool operator()(get_detail::try_get_check<T> auto const& v) const noexcept {
        return !!try_get<T>(v);
    }
};

/**
 * @brief Callable (CPO) that determines whether the given variant-like object is currently engaged
 * and holding an instance of `T`. Requires that there exist a valid `try_get<T>` for the given
 * variant type.
 */
template <typename T>
inline constexpr holds_alternative_fn<T> holds_alternative = {};

/**
 * @brief Determine whether the runtime-variant type `Var` supports holding an instance of
 * type `T`.
 *
 * A variant type `Var` "supports alternative" `T` if-and-only-if there exists an
 * implicit conversion from `T` to `Var`, and there is a valid call to
 * `try_get<T>(Var)`. Refer to `try_get<T>` for more details.
 *
 * @tparam Var A variant-like type
 * @tparam T A possible alternative for the variant
 */
template <typename Var, typename T>
concept supports_alternative =
    neo::convertible_to<T, std::remove_cvref_t<Var>> &&
    requires(Var&& var) {
        try_get<T>(var);
    };

template <typename T, typename Var>
concept supported_alternative_of = supports_alternative<Var, T>;

// clang-format on

}  // namespace smstr
