#include <semester/s_expr.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Create an s-expr data") {
    smstr::s_expr_data tree;

    tree = smstr::s_expr_data::traits_type::pair_type("Nope", "Yep");
    CHECK(smstr::holds_alternative<smstr::s_expr_data::traits_type::pair_type>(tree));
    CHECK_FALSE(tree.is_string());
    CHECK_FALSE(tree == "I am a string");

    auto& pair = smstr::get<smstr::s_expr_data::traits_type::pair_type>(tree);
    CHECK(pair.left() == "Nope");
    CHECK(pair.right() == "Yep");
}
