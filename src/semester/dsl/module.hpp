#pragma once

#include "./parse.hpp"

#include <neo/tag.hpp>

namespace smstr::dsl {

template <neo::basic_fixed_string Name>
struct module {};

template <typename... Content>
struct module_definition {
    using decls = neo::tag<Content...>;

    struct nonesuch {};

    template <auto S>
    constexpr static auto find_decl_for_name(neo::tag<>) {
        return nonesuch{};
    }

    template <auto S, typename Head, typename... Tail>
    constexpr static auto find_decl_for_name(neo::tag<Head, Tail...>) {
        if constexpr (Head::name == S) {
            return Head{};
        } else {
            return find_decl_for_name<S>(neo::tag_v<Tail...>);
        }
    }

    template <auto S>
    using decl_for_name = decltype(find_decl_for_name<S>(neo::tag_v<Content...>));
};

template <neo::basic_fixed_string Name, typename T>
struct type_def {
    static constexpr auto name = Name;
    using type                 = T;
};

namespace detail {

template <auto S>
constexpr auto parse_type_decl() {
    constexpr auto name = token::lex(S);
    if constexpr (name.kind != token::ident) {
        return error<"Expected an identifier following `type` keyword", name.tstring_tok(S)>();
    } else {
        constexpr auto tdef = parse_type_definition<name.tstring_tail(S)>();
        if constexpr (tdef.is_error) {
            return tdef;
        } else {
            using def = type_def<SM_STR(name.spelling), decltype(tdef.result)>;
            return parse_result{def{}, tdef.tail};
        }
    }
}

template <auto S>
constexpr auto parse_one_module_decl() {
    constexpr auto tok = token::lex(S);
    if constexpr (tok.spelling == "type") {
        return parse_type_decl<tok.tstring_tail(S)>();
    } else {
        return error<"Unknown token at module-level. Expected a declaration", tok.tstring_tok(S)>();
    }
}

template <auto S, typename... Acc>
constexpr auto parse_next_module_decl(neo::tag<Acc...>) {
    constexpr auto tok = token::lex(S);
    if constexpr (tok.kind == tok.eof) {
        return parse_result{module_definition<Acc...>{}, tok.tail};
    } else {
        constexpr auto decl = parse_one_module_decl<S>();
        if constexpr (decl.is_error) {
            return decl;
        } else {
            constexpr auto semi = token::lex(decl.tail);
            if constexpr (semi.spelling != ";") {
                return error<"Expect a semicolon `;` following module-level declaration",
                             semi.tstring_tok(S)>();
            } else {
                return parse_next_module_decl<semi.tstring_tail(S)>(
                    neo::tag_v<Acc..., decltype(decl.result)>);
            }
        }
    }
}

template <auto S>
constexpr auto parse_module_body() {
    using decls = neo::tag<>;
    return parse_next_module_decl<S>(decls{});
}

}  // namespace detail

template <neo::basic_fixed_string Body>
using module_def
    = decltype(detail::handle_error(detail::parse_module_body<neo::tstring_view_v<Body>>()));

template <neo::basic_fixed_string Name, neo::basic_fixed_string Inner>
using module_lookup = typename module<SM_STR(Name)>::template decl_for_name<SM_STR(Inner)>;

}  // namespace smstr::dsl