#include <semester/get.hpp>

#include <semester/json.hpp>

#include <neo/test_concept.hpp>

#include <catch2/catch.hpp>

#include <variant>

NEO_TEST_CONCEPT(smstr::supports_arrays<smstr::json_data>);
NEO_TEST_CONCEPT(smstr::supports_alternative<std::variant<int, bool>, bool>);
NEO_TEST_CONCEPT(smstr::supports_alternative<std::variant<int, bool>, int>);
NEO_TEST_CONCEPT(!smstr::supports_alternative<std::variant<int, bool>, std::string>);
NEO_TEST_CONCEPT(smstr::supports_alternative<const smstr::json_data, std::string>);
NEO_TEST_CONCEPT(smstr::supports_alternative<const smstr::json_data, std::string>);

TEST_CASE("get for variant<>") {
    std::variant<int, bool> var = 25;
    CHECK(smstr::holds_alternative<int>(var));
    CHECK_FALSE(smstr::holds_alternative<bool>(var));
    CHECK(smstr::get<int>(var) == 25);
    CHECK_THROWS(smstr::get<bool>(var));

    auto maybe_bool = smstr::try_get<bool>(var);
    CHECK_FALSE(maybe_bool);
}

TEST_CASE("get for json_data") {
    smstr::json_data dat = 33;
    CHECK(smstr::get<double>(dat) == 33);
}
