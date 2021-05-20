#include "./parse.hpp"

#include <semester/json.hpp>

#include <catch2/catch.hpp>

namespace smstr {

// template <>
// struct dsl::define<"int"> : dsl::primitive_type<int> {};

// template <>
// struct dsl::define<"empty_map"> : dsl::type<"{}"> {};

// template <>
// struct dsl::define<"simple_map"> : dsl::type<"{foo: int, bar: foo.int, baz: empty_map}"> {};

// template <>
// struct dsl::define<"map1"> : dsl::type<"{foo: simple_map, bar: simple_map, baz: simple_map}"> {};

// template <>
// struct dsl::define<"map2"> : dsl::type<"{foo: map1, bar: map1, baz: map1}"> {};

// template <>
// struct dsl::define<"map3"> : dsl::type<"{foo: map2, bar: map2, baz: map2}"> {};

// template <>
// struct dsl::define<"map4"> : dsl::type<R"(
//     {
//         foo: map3[][][][],
//         bar: map3[],
//         "baz": null,
//         asdf: "hello" | "goodbye",
//     }
// )"> {};

// template <>
// struct dsl::define<"recursive"> : dsl::type<R"(
//     {
//         foo: foodint,
//         child: recursive?,
//     }
// )"> {};

// static_assert(std::same_as<dsl::type_named_t<"empty_map">, dsl::mapping<>>);
// static_assert(std::same_as<
//               dsl::type_named_t<"simple_map">,
//               dsl::mapping<dsl::kv_pair<dsl::lit_string<"foo">, dsl::name<"int">>,
//                            dsl::kv_pair<dsl::lit_string<"bar">, dsl::qualified_name<"foo",
//                            "int">>, dsl::kv_pair<dsl::lit_string<"baz">,
//                            dsl::name<"empty_map">>>>);

// static_assert(std::same_as<dsl::parse_type_t<"int?">, dsl::opt<dsl::name<"int">>>);
// static_assert(std::same_as<dsl::parse_type_t<"int[]">, dsl::array<dsl::name<"int">>>);
// static_assert(std::same_as<dsl::parse_type_t<"int[]?">, dsl::opt<dsl::array<dsl::name<"int">>>>);
// static_assert(std::same_as<dsl::parse_type_t<"int?[]">, dsl::array<dsl::opt<dsl::name<"int">>>>);

// using oneof_type_1 = dsl::parse_type_t<"int|empty_map">;
// static_assert(std::same_as<oneof_type_1, dsl::oneof<dsl::name<"int">, dsl::name<"empty_map">>>);

// using oneof_type_2 = dsl::parse_type_t<"(int|empty_map)[]">;
// static_assert(
//     std::same_as<oneof_type_2, dsl::array<dsl::oneof<dsl::name<"int">,
//     dsl::name<"empty_map">>>>);

// static_assert(smstr::dsl::detail::realize_string_literal<neo::tstring_view_v<"\"hello\"">>()
//               == "hello");
// static_assert(smstr::dsl::detail::realize_string_literal<neo::tstring_view_v<"\"he\\llo\"">>()
//               == "hello");
// static_assert(smstr::dsl::detail::realize_string_literal<neo::tstring_view_v<"\"hel\\\"lo\"">>()
//               == "hel\"lo");

// constexpr auto func = smstr::dsl::eval_str<R"(fn 9)">;

// static_assert(int(func()) == 9);
// static_assert(int(smstr::dsl::eval_str<"fn foo (foo + 9)">(5)) == 14);
// static_assert(int(smstr::dsl::eval_str<"fn bar foo (bar - foo)">(2, 55)) == -53);

// static_assert(int(smstr::dsl::eval_str<R"(
//     fn foo bar
//     with sum foo + bar,
//         diff foo - bar
//     sum * diff
// )">(3, 5)) == -16);

static_assert(smstr::string_like<std::string>);

extern "C" std::int64_t foo(std::int64_t i) {
    auto fun = smstr::dsl::eval_str<"fn val is val + 66 end">();
    return fun(i).evaluate(smstr::default_traits{});
}

TEST_CASE("Simple expressions") {
    constexpr auto val = smstr::dsl::eval_str<"6 * 7">();
    static_assert(std::same_as<decltype(val), const std::int64_t>);
    CHECK(val == 42);

    auto string = smstr::dsl::eval_str<"'foo' + 'bar'">();
    CHECK(string == "foobar");
}

TEST_CASE("Length-of expression") {
    auto val = smstr::dsl::eval_str<"len('some_string')">();
    CHECK(val == 11);

    auto           fn = smstr::dsl::eval_str<"fn v is len(v) end">();
    constexpr auto l  = fn(std::array<int, 4>{}).evaluate(smstr::default_traits{});
    static_assert(l == 4);
}

TEST_CASE("Create an array") {
    auto             func = smstr::dsl::eval_str<"fn is [1, 'two', 3, null] end">();
    smstr::json_data arr  = func();
    REQUIRE(is_array(arr));

    auto& vec = as_array(arr);
    REQUIRE(vec.size() == 4);

    auto  is_num = smstr::holds_alternative<double>;
    auto& one    = vec[0];
    REQUIRE(is_num(one));
    CHECK(one == 1);

    auto& two = vec[1];
    REQUIRE(is_string(two));
    CHECK(two == "two");

    auto& three = vec[2];
    REQUIRE(is_num(three));
    CHECK(three == 3);

    auto& four = vec[3];
    CHECK(four == smstr::null);
}

TEST_CASE("Concat arrays") {
    auto func = smstr::dsl::eval_str<R"(
        fn is
            with first is [1, 2, 3],
                second is [4, 5, 6]
            then first ++ second
        end
    )">();

    smstr::json_data arr = func();
    REQUIRE(smstr::is_array(arr));
    CHECK(as_array(arr).size() == 6);
    CHECK(as_array(arr) == array_type_t<json_data>({1, 2, 3, 4, 5, 6}));
}

TEST_CASE("Create a map") {
    auto make_pkg = smstr::dsl::eval_str<R"(
        fn name, version, namespace is { name, version, namespace, } end
    )">();

    json_data pkg = make_pkg("boost", "1.66.0", "boost");
    REQUIRE(is_map(pkg));
    auto& map = as_map(pkg);

    auto name = map.find("name");
    REQUIRE(name != map.end());
    CHECK(is_string(name->second));
    CHECK(as_string(name->second) == "boost");

    auto version = map.find("version");
    REQUIRE(version != map.end());
    CHECK(is_string(version->second));
    CHECK(version->second == "1.66.0");

    // Key lookup
    auto get_version = smstr::dsl::eval_str<R"(
        fn map is map.version end
    )">();
    // Get it
    json_data& v = get_version(pkg).evaluate(smstr::data_traits<json_data>{});
    CHECK(v == "1.66.0");
}

TEST_CASE("Map an array") {
    smstr::json_data arr     = smstr::dsl::eval_str_defer<"[1, 2, 3, 4, 5]">();
    auto             doubler = smstr::dsl::eval_str<"fn val is val as number * 2 end">();
    auto             func    = smstr::dsl::eval_str<R"(
        fn array, func is
            array |> transform(func)
        end
    )">();
    smstr::json_data arr2    = func(arr, doubler);

    REQUIRE(is_array(arr2));
    auto& vec = as_array(arr2);
    CHECK(vec.size() == 5);
    CHECK(vec == array_type_t<json_data>({2, 4, 6, 8, 10}));
}

}  // namespace smstr
