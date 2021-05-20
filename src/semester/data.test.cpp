#include <semester/data.hpp>

#include <semester/get.hpp>

#include <catch2/catch.hpp>

#include <string>
#include <variant>

using namespace smstr;

struct int_tree_traits {
    template <typename Data>
    struct traits {
        using array_type   = std::vector<Data>;
        using variant_type = std::variant<int, array_type>;
    };
};

TEST_CASE("Create basic data") {
    using data = smstr::basic_data<int_tree_traits>;
    static_assert(std::is_same_v<data::variant_type, std::variant<int, std::vector<data>>>);

    data dat;
    auto is_integer = smstr::holds_alternative<int>;
    auto as_integer = smstr::get<int>;
    CHECK(is_integer(dat));  // Defaults to the first base type

    dat = 44;
    CHECK(dat == 44);
    CHECK(dat != 31);
    CHECK(as_integer(dat) == 44);

    CHECK(is_integer(dat.variant()));

    data dat2 = dat;
    CHECK(dat == dat2);
    CHECK_FALSE(dat != dat2);

    const data dat3 = dat2;
    CHECK(as_integer(std::move(dat3)) == 44);

    dat = smstr::empty_array;

    CHECK(is_array(dat));
    CHECK_FALSE(is_integer(dat));
    CHECK_FALSE(dat == 44);
    CHECK(dat != 44);
}
