#include "./token.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Get some tokens") {
    auto tok = smstr::dsl::token::lex("foo.bar[baz]");
    CHECK(tok.spelling == "foo");
    CHECK(tok.kind == tok.ident);
    CHECK(tok.tail == ".bar[baz]");
    tok = tok.next();
    CHECK(tok.spelling == ".");
    CHECK(tok.kind == tok.dot);
    CHECK(tok.tail == "bar[baz]");
    tok = tok.next();
    CHECK(tok.spelling == "bar");
    CHECK(tok.kind == tok.ident);
    CHECK(tok.tail == "[baz]");
    tok = tok.next();
    CHECK(tok.spelling == "[");
    CHECK(tok.kind == tok.square_l);
    CHECK(tok.tail == "baz]");
    tok = tok.next();
    CHECK(tok.spelling == "baz");
    CHECK(tok.kind == tok.ident);
    CHECK(tok.tail == "]");
    tok = tok.next();
    CHECK(tok.spelling == "]");
    CHECK(tok.kind == tok.square_r);
    CHECK(tok.tail == "");
    tok = tok.next();
    CHECK(tok.kind == tok.eof);
}