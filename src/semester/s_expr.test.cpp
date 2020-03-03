#include <semester/s_expr.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Create an s-expr data") {
    semester::s_expr_data tree;

    tree = semester::s_expr_data::traits_type::pair_type("Nope", "Yep");
    CHECK(tree.holds_alternative<semester::s_expr_data::traits_type::pair_type>());
    CHECK_FALSE(tree.is_string());
    CHECK_FALSE(tree == "I am a string");

    auto& pair = tree.as<semester::s_expr_data::traits_type::pair_type>();
    CHECK(pair.left() == "Nope");
    CHECK(pair.right() == "Yep");
}
