// #include "./generate.hpp"

// #include <semester/json.hpp>

// #include <catch2/catch.hpp>

// TEST_CASE("Generate an empty object") {
//     auto c = smstr::dsl::code<R"(
//         generate(eggs) {
//             foo: "bar",
//             baz: eggs,
//             quux: {
//                 asdf: "44",
//             },
//         }
//     )">;

//     auto value = c.apply<smstr::json_data>(22);

//     {
//         REQUIRE(value.is_mapping());
//         auto& map = value.as_mapping();

//         auto foo_key = map.find("foo");
//         REQUIRE(foo_key != map.end());
//         CHECK(foo_key->second == "bar");

//         auto baz_key = map.find("baz");
//         REQUIRE(baz_key != map.end());
//         CHECK(baz_key->second == 22);

//         auto quux = map.find("quux");
//         REQUIRE(quux != map.end());
//         REQUIRE(quux->second.is_mapping());
//         auto&& quux_map = quux->second.as_mapping();

//         auto asdf = quux_map.find("asdf");
//         REQUIRE(asdf != quux_map.end());
//         CHECK(asdf->second == "44");
//     }

//     value = c("I am a string now");

//     {
//         auto baz_key = value.as_mapping().find("baz");
//         REQUIRE(baz_key != value.as_mapping().end());
//         CHECK(baz_key->second == "I am a string now");
//     }

//     value = smstr::dsl::code<R"(
//         generate(key) {
//             // Dynamically generated map keys (must cast to string)
//             [key as string]: "hello, world!",
//         }
//     )">("message");

//     {
//         REQUIRE(value.is_mapping());
//         auto& map   = value.as_mapping();
//         auto  found = map.find("message");
//         REQUIRE(found != map.end());
//         CHECK(found->second == "hello, world!");
//     }

//     // value = smstr::dsl::code<R"(
//     //     generate(key) {
//     //         // Dynamically generated map keys (typed param is okay without cast)
//     //         [key]: "hello, world!",
//     //     }
//     // )">(66);

//     // {
//     //     REQUIRE(value.is_mapping());
//     //     auto& map   = value.as_mapping();
//     //     auto  found = map.find("message");
//     //     REQUIRE(found != map.end());
//     //     CHECK(found->second == "hello, world!");
//     // }
// }
