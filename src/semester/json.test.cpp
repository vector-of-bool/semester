#include <semester/json.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Create a JSON node") {
    smstr::json_data d1;
    smstr::json_data d2 = 6;
    CHECK(d1 != d2);
    d2 = "I am a string";
    CHECK(smstr::holds_alternative<std::string>(d2));
    CHECK(d2 == "I am a string");
    CHECK(d2 != "I am a different string");

    d1 = smstr::empty_map;
    CHECK(d1 != d2);
    d2 = smstr::empty_map;
    CHECK(d1 == d2);
}
