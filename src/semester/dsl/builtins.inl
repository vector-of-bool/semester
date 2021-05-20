#include <ranges>

#include "./define.hpp"

namespace smstr::dsl {

template <>
auto define<"len"> =
    [](std::ranges::forward_range auto&& arg) { return std::ranges::distance(arg); };

template <>
auto define<"transform"> = detail::call_with_ctx_wrapper{[](auto ctx, auto&& array_, auto&& func) {
    auto   ret   = ctx.traits.new_array();
    auto&& array = ctx.traits.as_array(array_);
    for (auto&& elem : array) {
        ctx.traits.array_push(ret, func(NEO_FWD(elem)));
    }
    return ret;
}};

}  // namespace smstr::dsl
