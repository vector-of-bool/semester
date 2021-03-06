#include <semester/data.hpp>

#include <semester/get.hpp>

#include <catch2/catch.hpp>

#include <string>
#include <variant>

struct int_tree_traits {
    template <typename Data>
    struct traits {
        using array_type   = std::vector<Data>;
        using variant_type = std::variant<int, array_type>;
    };
};

TEST_CASE("Create basic data") {
    using data = semester::basic_data<int_tree_traits>;
    static_assert(std::is_same_v<data::variant_type, std::variant<int, std::vector<data>>>);

    data dat;
    CHECK(dat.is_int());  // Defaults to the first base type
    CHECK(semester::holds_alternative<int>(dat));
    CHECK(semester::holds_alternative<int>(dat));

    dat = 44;
    CHECK(dat == 44);
    CHECK(dat != 31);
    CHECK(dat.as_int() == 44);
    CHECK(semester::get<int>(dat) == 44);

    CHECK(std::holds_alternative<int>(dat.variant()));

    data dat2 = dat;
    CHECK(dat == dat2);
    CHECK_FALSE(dat != dat2);

    const data dat3 = dat2;
    CHECK(semester::get<int>(std::move(dat3)) == 44);

    dat = semester::empty_array;

    CHECK(dat.is_array());
    CHECK_FALSE(dat.is_int());
    CHECK_FALSE(dat == 44);
    CHECK(dat != 44);
}
