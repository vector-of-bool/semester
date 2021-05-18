#pragma once

#include <neo/fixed_string.hpp>

namespace smstr::dsl {

struct lit_number_base {};

template <neo::basic_fixed_string S>
struct lit_number : lit_number_base {
    constexpr static auto str = S;
};

template <typename T>
concept lit_number_c = std::derived_from<T, lit_number_base>;

}  // namespace smstr::dsl
