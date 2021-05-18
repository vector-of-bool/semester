#pragma once

#include <neo/fixed_string.hpp>

#include <concepts>

#define SM_STR(Str)                                                                                \
    ::neo::basic_fixed_string<std::ranges::range_value_t<decltype(Str)>, (Str).size()>(Str)
#define SM_S SM_STR(S)

namespace smstr::dsl {

struct undefined_t {
    constexpr bool operator==(undefined_t) const noexcept { return true; }

    template <typename Other>
    constexpr bool operator==(Other&&) const noexcept {
        return false;
    }
};

template <neo::basic_fixed_string>
struct define : undefined_t {};

template <neo::basic_fixed_string S>
concept undefined = std::derived_from<define<SM_S>, undefined_t>;

template <neo::basic_fixed_string S>
concept defined = !undefined<SM_S>;

template <neo::basic_fixed_string S>
using type_named_t = typename define<SM_S>::type_;

template <neo::basic_fixed_string Name>
struct name {
    constexpr static std::string_view spelling = Name;
    using type                                 = undefined_t;
};

template <neo::basic_fixed_string Scope, neo::basic_fixed_string Name>
struct qualified_name {};

template <neo::basic_fixed_string Name, typename Type>
struct typed_name {
    constexpr static std::string_view spelling = Name;
    using type                                 = Type;
};

}  // namespace smstr::dsl
