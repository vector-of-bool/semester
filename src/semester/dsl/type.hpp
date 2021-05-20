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

#define DECL_LOOKUP(NameString, NestedType)                                                        \
    template <typename Data>                                                                       \
    requires requires {                                                                            \
        typename Data::traits_type::NestedType;                                                    \
    }                                                                                              \
    struct lookup_type<Data, name<NameString>> {                                                   \
        using type = Data::traits_type::NestedType;                                                \
    }

DECL_LOOKUP("string", string_type);
DECL_LOOKUP("integer", integer_type);
DECL_LOOKUP("integer", number_type);
DECL_LOOKUP("real", real_type);
DECL_LOOKUP("real", number_type);
DECL_LOOKUP("number", number_type);
DECL_LOOKUP("number", real_type);
DECL_LOOKUP("number", integer_type);
DECL_LOOKUP("null", null_type);
DECL_LOOKUP("bool", bool_type);
DECL_LOOKUP("array", array_type);
DECL_LOOKUP("mapping", mapping_type);
#undef DECL_LOOKUP

template <typename Data, typename Name>
using lookup_type_t = lookup_type<Data, Name>::type;

}  // namespace smstr::dsl
