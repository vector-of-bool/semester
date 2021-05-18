#include "./parse.hpp"

namespace smstr {

template <>
struct dsl::define<"int"> : dsl::primitive_type<int> {};

template <>
struct dsl::define<"empty_map"> : dsl::type<"{}"> {};

template <>
struct dsl::define<"simple_map"> : dsl::type<"{foo: int, bar: foo.int, baz: empty_map}"> {};

template <>
struct dsl::define<"map1"> : dsl::type<"{foo: simple_map, bar: simple_map, baz: simple_map}"> {};

template <>
struct dsl::define<"map2"> : dsl::type<"{foo: map1, bar: map1, baz: map1}"> {};

template <>
struct dsl::define<"map3"> : dsl::type<"{foo: map2, bar: map2, baz: map2}"> {};

template <>
struct dsl::define<"map4"> : dsl::type<R"(
    {
        foo: map3[][][][],
        bar: map3[],
        "baz": null,
        asdf: "hello" | "goodbye",
    }
)"> {};

template <>
struct dsl::define<"recursive"> : dsl::type<R"(
    {
        foo: foodint,
        child: recursive?,
    }
)"> {};

static_assert(std::same_as<dsl::type_named_t<"empty_map">, dsl::mapping<>>);
static_assert(std::same_as<
              dsl::type_named_t<"simple_map">,
              dsl::mapping<dsl::kv_pair<dsl::lit_string<"foo">, dsl::name<"int">>,
                           dsl::kv_pair<dsl::lit_string<"bar">, dsl::qualified_name<"foo", "int">>,
                           dsl::kv_pair<dsl::lit_string<"baz">, dsl::name<"empty_map">>>>);

static_assert(std::same_as<dsl::parse_type_t<"int?">, dsl::opt<dsl::name<"int">>>);
static_assert(std::same_as<dsl::parse_type_t<"int[]">, dsl::array<dsl::name<"int">>>);
static_assert(std::same_as<dsl::parse_type_t<"int[]?">, dsl::opt<dsl::array<dsl::name<"int">>>>);
static_assert(std::same_as<dsl::parse_type_t<"int?[]">, dsl::array<dsl::opt<dsl::name<"int">>>>);

using oneof_type_1 = dsl::parse_type_t<"int|empty_map">;
static_assert(std::same_as<oneof_type_1, dsl::oneof<dsl::name<"int">, dsl::name<"empty_map">>>);

using oneof_type_2 = dsl::parse_type_t<"(int|empty_map)[]">;
static_assert(
    std::same_as<oneof_type_2, dsl::array<dsl::oneof<dsl::name<"int">, dsl::name<"empty_map">>>>);

static_assert(smstr::dsl::detail::realize_string_literal<neo::tstring_view_v<"\"hello\"">>()
              == "hello");
static_assert(smstr::dsl::detail::realize_string_literal<neo::tstring_view_v<"\"he\\llo\"">>()
              == "hello");
static_assert(smstr::dsl::detail::realize_string_literal<neo::tstring_view_v<"\"hel\\\"lo\"">>()
              == "hel\"lo");

constexpr auto func = smstr::dsl::eval_str<R"(fn 9)">;

static_assert(func() == 9);
static_assert(smstr::dsl::eval_str<"fn foo (foo + 9)">(5) == 14);
static_assert(smstr::dsl::eval_str<"fn bar foo (bar - foo)">(2, 55) == -53);

static_assert(smstr::dsl::eval_str<R"(
    fn foo bar
    with sum foo + bar,
        diff foo - bar
    sum * diff
)">(3, 5));

}  // namespace smstr
