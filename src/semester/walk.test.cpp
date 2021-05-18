#include "./walk.hpp"

#include <semester/json.hpp>

#include <catch2/catch.hpp>

#include <neo/test_concept.hpp>

struct func {
    void operator()(int) {}
};

NEO_TEST_CONCEPT(neo::same_as<int, smstr::detail::vst_arg_t<func>>);

TEST_CASE("Simple walk_seq") {
    smstr::json_data dat = 34.0;

    bool ok = false;
    using smstr::walk;
    walk(
        dat,
        [&](std::string) {
            FAIL("Should not get here");
            return walk.accept;
        },
        [&](double) {
            ok = true;
            return walk.accept;
        },
        [&](auto) {
            FAIL("Nor here");
            return walk.pass;
        });
    CHECK(ok);

    ok = false;
    walk(
        dat,
        [&](auto) {
            ok = true;
            return walk.accept;
        },
        [](auto) {
            FAIL("Bad");
            return walk.pass;
        });
    CHECK(ok);

    ok = false;
    CHECK_THROWS_AS(walk(
                        dat,
                        [&](auto) {
                            ok = true;
                            return walk.pass;
                        },
                        [](auto) { return walk.pass; }),
                    smstr::walk_error);
    CHECK(ok);
}

TEST_CASE("put_into objects") {
    smstr::json_data data = "I am a string";

    std::string                result;
    std::optional<std::string> opt_str;

    using namespace smstr::walk_ops;
    using smstr::walk;
    walk(data, put_into(result));
    CHECK(result == data);

    walk(data, put_into(opt_str));
    CHECK(opt_str.has_value());
    CHECK(opt_str == data);

    // put-into using a projection function
    std::size_t length = 0;
    walk(data, put_into(length, [](std::string const& str) { return str.length(); }));
    CHECK(length == data.as_string().length());

    // put-into with a projection and an optional
    std::optional<std::size_t> opt_length;
    walk(data, put_into(opt_length, [](std::string s) { return s.length(); }));
    CHECK(opt_length.has_value());
    CHECK(opt_length == length);

    // put-into with a bad type will throw
    bool b = false;
    CHECK_THROWS_AS(walk(data, put_into(b)), smstr::walk_error);

    // put-into a back_inserter
    std::vector<std::size_t> vec;
    walk(data, put_into(std::back_inserter(vec), [](std::string s) { return s.length(); }));
    CHECKED_IF(vec.size() == 1) { CHECK(vec[0] == length); }
}

TEST_CASE("Mappings") {
    smstr::json_data dat = smstr::json_data::mapping_type{
        {"foo", "bar"},
        {"baz", 33},
    };

    using namespace smstr::walk_ops;
    using smstr::walk;

    std::string foo_string;
    double      baz_value;
    walk(dat, mapping(if_key("foo", put_into(foo_string)), if_key("baz", put_into(baz_value))));
    CHECK(foo_string == "bar");
    CHECK(baz_value == 33.0);

    // Throws for missing keys that are required
    auto result = walk.try_walk(  //
        dat,
        if_mapping(mapping{
            if_key{"foo", put_into(foo_string)},
            if_key{"baz", just_accept},
            required_key{"quux", "'quux' is required", just_accept},
        }));
    CHECK(result.rejected());
}

TEST_CASE("Array iteration") {
    smstr::json_data dat = smstr::json_data::array_type{"some string", 55, false};

    double      number = 0;
    std::string string;
    bool        b = true;
    using namespace smstr::walk_ops;
    smstr::walk(dat,
                   if_array(for_each(walk_seq(if_type<std::string>(put_into(string)),
                                              if_type<double>(put_into(number)),
                                              if_type<bool>(put_into(b))))),
                   reject_with("Not array"));
    CHECK(number == 55);
    CHECK(string == "some string");
    CHECK_FALSE(b);
}

TEST_CASE("Array insertion") {
    smstr::json_data dat = smstr::json_data::array_type{
        "string 1",
        55,
        "string 2",
        "string 3",
        8,
        9,
    };

    std::vector<std::string> strings;
    std::vector<double>      numbers;
    using namespace smstr::walk_ops;
    smstr::walk(dat,
                   for_each(walk_seq{if_type<std::string>(put_into{std::back_inserter(strings)}),
                                     if_type<double>(put_into(std::back_inserter(numbers)))}));
    CHECK(strings == (std::vector<std::string>{"string 1", "string 2", "string 3"}));
    CHECK(numbers == (std::vector<double>{55, 8, 9}));
}

TEST_CASE("Just reject") {
    auto rej = smstr::walk.try_walk(12, smstr::reject_with("Dummy string"));
    REQUIRE(rej.rejected());
    CHECK(rej.rejection().message == "<root>: Dummy string");
}

TEST_CASE("Just accept") {
    using namespace smstr::walk_ops;
    smstr::walk(12, just_accept);

    smstr::json_data dat = smstr::json_data::mapping_type{
        {"foo", smstr::null},
    };

    smstr::walk(dat, mapping{if_key{"foo", just_accept}});
}