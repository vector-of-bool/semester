#pragma once

#include "./string.hpp"
#include "./token.hpp"
#include "./type_fwd.hpp"

#include <neo/tag.hpp>

namespace smstr::dsl {

template <typename... Entries>
struct mapping {
    using entries_tag = neo::tag<Entries...>;
};

template <typename Key, typename Type>
struct kv_pair {
    using key  = Key;
    using type = Type;
};

namespace detail {}  // namespace detail

}  // namespace smstr::dsl
