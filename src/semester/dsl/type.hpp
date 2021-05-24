#pragma once

#include "./define.hpp"

#include "./mapping.hpp"
#include "./token.hpp"
#include "./type_fwd.hpp"

#include <neo/tag.hpp>

namespace smstr::dsl {

template <typename T>
struct primitive_type {
    using type_ = primitive_type;
};

template <typename Data, typename Name>
struct lookup_type {};

template <typename Data, typename Name>
using lookup_type_t = lookup_type<Data, Name>::type;

}  // namespace smstr::dsl
