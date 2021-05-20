#pragma once

#include <neo/fixed_string.hpp>

namespace smstr::dsl {

struct lit_number_base {};

template <neo::basic_fixed_string S>
struct lit_decimal : lit_number_base {
    constexpr static auto str = std::string_view(S);

    constexpr auto eval(auto ctx) const { return ctx.traits.decimal(str); }
};

template <auto S>
struct lit_integer : lit_number_base {
    constexpr static auto str = std::string_view(S);

    constexpr auto eval(auto ctx) const { return ctx.traits.integer(str); }
};

template <typename T>
concept lit_number_c = std::derived_from<T, lit_number_base>;

}  // namespace smstr::dsl
