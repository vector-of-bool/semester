#include <semester/decomp.hpp>
#include <semester/json.hpp>

#include <catch2/catch.hpp>

TEST_CASE("put_into") {
    semester::json_data dat = 34.0;

    double dest = 0;
    semester::decompose(dat, semester::put_into(dest));
    CHECK(dest == 34.0);

    // Use an optional
    std::optional<double> dub;
    semester::decompose(dat, semester::put_into(dub));
    REQUIRE(dub);
    CHECK(*dub == 34.0);
}

TEST_CASE("mapping") {
    semester::json_data dat = semester::json_data::mapping_type{
        {"foo", "bar"},
        {"baz", 33},
    };

    std::string foo_string;
    double      baz_value;
    using namespace semester::decompose_ops;
    semester::decompose(dat,
                        mapping(if_key("foo", put_into(foo_string)),
                                if_key("baz", put_into(baz_value))));
    CHECK(foo_string == "bar");
    CHECK(baz_value == 33.0);
}

TEST_CASE("Type check") {
    semester::json_data dat = semester::json_data::mapping_type{
        {"foo", "bar"},
        {"baz", 66},
    };

    double      number = 0;
    std::string string;
    using namespace semester::decompose_ops;
    semester::decompose(dat,
                        mapping(if_key("foo", if_type<double>(put_into(number))),
                                if_key("foo", if_type<std::string>(put_into(string)))));
    CHECK(number == 0);
    CHECK(string == "bar");

    bool hit = false;
    semester::decompose(dat, if_mapping{[&](auto) {
                            hit = true;
                            return semester::dc_accept;
                        }});
    CHECK(hit);
}

TEST_CASE("Array iteration") {
    semester::json_data dat = semester::json_data::array_type{"some string", 55, false};

    double      number = 0;
    std::string string;
    bool        b = true;
    using namespace semester::decompose_ops;
    semester::decompose(dat,
                        for_each(try_seq(if_type<std::string>(put_into(string)),
                                         if_type<double>(put_into(number)),
                                         if_type<bool>(put_into(b)))));
    CHECK(number == 55);
    CHECK(string == "some string");
    CHECK_FALSE(b);
}

TEST_CASE("Array insertion") {
    semester::json_data dat = semester::json_data::array_type{
        "string 1",
        55,
        "string 2",
        "string 3",
        8,
        9,
    };

    std::vector<std::string> strings;
    std::vector<double>      numbers;
    using namespace semester::decompose_ops;
    semester::decompose(dat,
                        for_each(
                            try_seq{if_type<std::string>(write_to{std::back_inserter(strings)}),
                                    if_type<double>(write_to(std::back_inserter(numbers)))}));
    CHECK(strings == (std::vector<std::string>{"string 1", "string 2", "string 3"}));
    CHECK(numbers == (std::vector<double>{55, 8, 9}));
}

TEST_CASE("Just reject") {
    auto rej = semester::decompose(12, semester::reject_with{"Dummy string"});
    CHECK(std::holds_alternative<semester::dc_reject_t>(rej));
    CHECK(std::get<semester::dc_reject_t>(rej).message == "Dummy string");
}

TEST_CASE("Just accept") {
    using namespace semester::decompose_ops;
    auto acc = semester::decompose(12, just_accept);
    CHECK(std::holds_alternative<semester::dc_accept_t>(acc));

    semester::json_data dat = semester::json_data::mapping_type{
        {"foo", semester::null},
    };

    acc = semester::decompose(dat, mapping{if_key{"foo", just_accept}});
    CHECK(std::holds_alternative<semester::dc_accept_t>(acc));
}

TEST_CASE("Require type") {
    semester::json_data dat = 55;

    auto res
        = semester::decompose(dat, semester::require_type<std::string>{"The data is not a string"});
    auto rej = std::get_if<semester::dc_reject_t>(&res);
    REQUIRE(rej);
    CHECK(rej->message == "The data is not a string");
}

TEST_CASE("Noexcept tests") {
    semester::json_data dat = 1;

    auto throws     = [](auto&&) { return semester::dc_accept; };
    auto nothrows   = [](auto&&) noexcept { return semester::dc_accept; };
    auto throws_2   = [](auto&&, auto&&) { return semester::dc_accept; };
    auto nothrows_2 = [](auto&&, auto&&) noexcept { return semester::dc_accept; };

    // mapping
    static_assert(noexcept(semester::mapping()(dat)));
    static_assert(noexcept(semester::decompose(dat, semester::mapping())));
    static_assert(!noexcept(semester::decompose(dat, semester::mapping(throws_2))));
    static_assert(noexcept(semester::decompose(dat, semester::mapping(nothrows_2))));

    // try_seq
    static_assert(noexcept(semester::try_seq()(dat)));
    static_assert(!noexcept(semester::try_seq(throws)(dat)));
    static_assert(noexcept(semester::try_seq(nothrows)(dat)));
    static_assert(!noexcept(semester::try_seq(throws, nothrows)(dat)));

    // for_each
    static_assert(noexcept(semester::for_each()(dat)));
    static_assert(!noexcept(semester::for_each(throws)(dat)));
    static_assert(noexcept(semester::for_each(nothrows)(dat)));
    static_assert(!noexcept(semester::for_each(throws, nothrows)(dat)));

    // if_array
    static_assert(noexcept(semester::if_array(nothrows)(dat)));
    static_assert(!noexcept(semester::if_array(throws)(dat)));

    // if_mapping
    static_assert(noexcept(semester::if_mapping(nothrows)(dat)));
    static_assert(!noexcept(semester::if_mapping(throws)(dat)));

    // if_type
    static_assert(noexcept(semester::if_type<std::string>(nothrows)(dat)));
    static_assert(!noexcept(semester::if_type<std::string>(throws)(dat)));

    // if_key
    static_assert(noexcept(semester::if_key("key", nothrows)(1, 2)));
    static_assert(!noexcept(semester::if_key("key", throws)(1, 2)));
}
