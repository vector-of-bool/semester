#include <semester/json.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Create a JSON node") {
    semester::json_data d1;
    semester::json_data d2 = 6;
    CHECK(d1 != d2);
    d2 = "I am a string";
    CHECK(d2.holds_alternative<std::string>());
    CHECK(d2 == "I am a string");
    CHECK(d2 != "I am a different string");

    d1 = semester::empty_mapping;
    CHECK(d1 != d2);
    d2 = semester::empty_mapping;
    CHECK(d1 == d2);
}
