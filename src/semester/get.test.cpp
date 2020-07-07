#include <semester/get.hpp>

#include <semester/json.hpp>

#include <neo/test_concept.hpp>

#include <catch2/catch.hpp>

#include <variant>

NEO_TEST_CONCEPT(semester::supports_alternative<std::variant<int, bool>, bool>);
NEO_TEST_CONCEPT(semester::supports_alternative<std::variant<int, bool>, int>);
NEO_TEST_CONCEPT(!semester::supports_alternative<std::variant<int, bool>, std::string>);
NEO_TEST_CONCEPT(semester::supports_alternative<const semester::json_data, std::string>);
NEO_TEST_CONCEPT(semester::supports_alternative<const semester::json_data, std::string>);

TEST_CASE("get for variant<>") {
    std::variant<int, bool> var = 25;
    CHECK(semester::holds_alternative<int>(var));
    CHECK_FALSE(semester::holds_alternative<bool>(var));
    CHECK(semester::get<int>(var) == 25);
    CHECK_THROWS(semester::get<bool>(var));

    auto maybe_bool = semester::try_get<bool>(var);
    CHECK_FALSE(maybe_bool);
}

TEST_CASE("get for json_data") {
    semester::json_data dat = 33;
    CHECK(semester::get<double>(dat) == 33);
}
