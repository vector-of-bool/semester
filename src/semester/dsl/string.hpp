#pragma once

#include <neo/fixed_string.hpp>

namespace smstr::dsl {

struct lit_string_base {};

template <neo::basic_fixed_string S>
struct lit_string : lit_string_base {
    constexpr static auto str = S;
};

template <typename T>
concept lit_string_c = std::derived_from<T, lit_string_base>;

}  // namespace smstr::dsl
