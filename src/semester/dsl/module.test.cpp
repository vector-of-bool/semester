#include "./module.hpp"

#include <catch2/catch.hpp>

namespace smstr::dsl {

template <>
struct module<"test1"> : module_def<R"(
    type MyThing {
        foo: bar,
    };
)"> {};

using MyThing_def = module_lookup<"test1", "MyThing">;
static_assert(MyThing_def::name == "MyThing");
static_assert(std::same_as<MyThing_def::type, mapping<kv_pair<lit_string<"foo">, name<"bar">>>>);

// using MyThing_def_2 = type_named_t<"test1.MyThing">;

}  // namespace smstr::dsl
