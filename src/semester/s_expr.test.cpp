#include <semester/s_expr.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Create an s-expr data") {
    semester::s_expr_data tree;

    tree = semester::s_expr_data::traits_type::pair_type("Nope", "Yep");
    CHECK(semester::holds_alternative<semester::s_expr_data::traits_type::pair_type>(tree));
    CHECK_FALSE(tree.is_string());
    CHECK_FALSE(tree == "I am a string");

    auto& pair = semester::get<semester::s_expr_data::traits_type::pair_type>(tree);
    CHECK(pair.left() == "Nope");
    CHECK(pair.right() == "Yep");
}
