#include "./number.hpp"

namespace smstr::dsl::detail {

template <auto S, typename Lookup, typename... Elems>
cxauto parse_map_literal();

template <auto S, typename Lookup>
cxauto parse_mapping_key_expression() {
    cxauto tok = token::lex(S);
    if cx (tok.kind == tok.ident) {
        return parse_result{lit_string<SM_STR(tok.spelling)>{}, tok.tail};
    } else if cx (tok.kind == tok.string) {
        cxauto key = realize_string_literal<tok.tstring_tok(S)>();
        return parse_result{lit_string<SM_STR(key)>{}, tok.tail};
    } else if cx (tok.kind == tok.square_l) {
        cxauto expr = parse_expression<tok.tstring_tail(S), Lookup>();
        if cx (expr.is_error) {
            return expr;
        } else {
            cxauto close_bracket = expr.next_token();
            if cx (close_bracket.kind != token::square_r) {
                return error<"Expected closing bracket following key expression",
                             close_bracket.tstring_tok(S)>();
            } else {
                return parse_result{expr.result, close_bracket.tail};
            }
        }
    } else {
        return error<
            "Expected a string literal, identifier, or bracketed expression for map literal key",
            token::lex(S).tstring_tok(S)>();
    }
}

template <auto S, typename Lookup>
cxauto parse_map_entry() {
    if cx (cxauto key = parse_mapping_key_expression<S, Lookup>(); key.is_error) {
        return key;
    } else if cx (cxauto colon = key.next_token(); colon.kind != colon.colon) {
        return error<"Expected a colon `:` following map key", colon.tstring_tok(S)>();
    } else if cx (cxauto value = parse_expression<colon.tstring_tail(S), Lookup>();
                  value.is_error) {
        return value;
    } else {
        return parse_result{map_entry{key.result, value.result}, value.tail};
    }
}

template <auto S, typename Lookup, typename... Elems>
cxauto parse_map_literal() {
    cxauto tok = token::lex(S);
    if cx (tok.kind == tok.brace_r) {
        return parse_result{map_literal<Elems...>{}, tok.tail};
    } else if cx (cxauto entry = parse_map_entry<S, Lookup>(); entry.is_error) {
        return entry;
    } else if cx (cxauto peek = entry.next_token(); peek.kind == peek.brace_r) {
        return parse_map_literal<entry.tstring_tail(S), Lookup, Elems..., decltype(entry.result)>();
    } else if cx (peek.kind != peek.comma) {
        return error<"Expect a comma `,` or closing brace `}` following map literal value",
                     peek.tstring_tok(S)>();
    } else {
        return parse_map_literal<peek.tstring_tail(S), Lookup, Elems..., decltype(entry.result)>();
    }
}

template <auto S, typename Lookup>
cxauto parse_expression_with() {
    cxauto tok = token::lex(S);
    if cx (tok.kind != token::ident) {
        return error<"Expected identifier for scope variable tok", tok.tstring_tok(S)>();
    } else {
        guard(with, parse_expression<tok.tstring_tail(S), Lookup>()) {
            using scope      = scoped_names<name<SM_STR(tok.spelling)>>;
            using new_lookup = Lookup::template push_front<scope>;
            cxauto peek      = with.next_token();
            if cx (peek.kind == token::comma) {
                guard(next_expr, parse_expression_with<peek.tstring_tail(S), new_lookup>()) {
                    return parse_result{with_expr{with.result, next_expr.result}, next_expr.tail};
                }
            } else {
                guard(final_expr, parse_expression<with.tstring_tail(S), new_lookup>()) {
                    return parse_result{with_expr{with.result, final_expr.result}, final_expr.tail};
                }
            }
        }
    }
}

template <auto S, typename Lookup>
cxauto parse_expression_prefix() {
    cxauto tok = token::lex(S);
    if cx (tok.spelling == "fn") {
        return parse_fn_expression<tok.tstring_tail(S), Lookup>();
    } else if cx (tok.spelling == "with") {
        return parse_expression_with<tok.tstring_tail(S), Lookup>();
    } else if cx (tok.kind == tok.ident) {
        cxauto resolve = Lookup::template resolve<tok.tstring_tok(S)>();
        if cx (resolve.scope_idx < 0) {
            cxauto message = "Name `" + SM_STR(tok.spelling)
                + "` does not name a parameter in any enclosing scope.";
            return error<SM_STR(message), tok.tstring_tok(S)>();
        } else {
            using ptype = typename decltype(resolve)::param_type::type;
            using pref  = param_ref<resolve.scope_idx, resolve.name_idx, ptype>;
            return parse_result{pref{}, tok.tail};
        }
    } else if cx (tok.kind == tok.string) {
        cxauto str = realize_string_literal<tok.tstring_tok(S)>();
        return parse_result{lit_string<SM_STR(str)>{}, tok.tail};
    } else if cx (tok.kind == tok.brace_l) {
        return parse_map_literal<tok.tstring_tail(S), Lookup>();
    } else if cx (tok.kind == tok.number) {
        return parse_result{lit_number<SM_STR(tok.spelling)>{}, tok.tail};
    } else if cx (tok.kind == tok.paren_l) {
        guard(inner_expr, parse_expression<tok.tstring_tail(S), Lookup>()) {
            if cx (cxauto next = inner_expr.next_token(); next.kind != token::paren_r) {
                return error<"Expected closing parenthesis following expression",
                             next.tstring_tok(S)>();
            } else {
                return parse_result{inner_expr.result, next.tail};
            }
        }
    } else {
        return error<
            "Expected an expression (string literal, map literal, or named parameter reference)",
            tok.tstring_tok(S)>();
    }
}  // namespace smstr::dsl::detail

template <typename Left, auto S, typename Lookup>
cxauto parse_expression_suffix() {
    cxauto tok = token::lex(S);
    if cx (tok.spelling == "as") {
        cxauto type = parse_type_definition<tok.tstring_tail(S)>();
        if cx (type.is_error) {
            return type;
        } else {
            using as = as_expr<Left, decltype(type.result)>;
            return parse_expression_suffix<as, type.tstring_tail(S), Lookup>();
        }
    } else {
        return parse_result{Left{}, S};
    }
}

template <auto S, typename Lookup>
cxauto parse_primary_expr() {
    return parse_expression_prefix<S, Lookup>();
}

template <auto S, auto LHS>
cxauto parse_cast_tail() {
    cxauto peek = token::lex(S);
    if cx (peek.spelling != "as") {
        return parse_result{LHS, S};
    } else {
        guard(type, parse_type_definition<peek.tstring_tail(S)>()) {
            cxauto as = as_expr<decltype(LHS), decltype(type.result)>{};
            return parse_cast_tail<type.tstring_tail(S), as>();
        }
    }
}

template <auto S, typename Lookup>
cxauto parse_cast_expr() {
    guard(lhs, parse_primary_expr<S, Lookup>()) {
        return parse_cast_tail<lhs.tstring_tail(S), lhs.result>();
    }
}

template <auto S, auto LHS, typename Lookup>
cxauto parse_multiply_tail() {
    cxauto peek = token::lex(S);
    if cx (peek.kind != token::star && peek.kind != token::slash) {
        return parse_result{LHS, S};
    } else {
        guard(next, parse_cast_expr<peek.tstring_tail(S), Lookup>()) {
            using op
                = std::conditional_t<peek.kind == token::star, std::multiplies<>, std::divides<>>;
            cxauto binop = binop_expr{op{}, LHS, next.result};
            return parse_multiply_tail<next.tstring_tail(S), binop, Lookup>();
        }
    }
}

template <auto S, typename Lookup>
cxauto parse_multiply_expr() {
    guard(lhs, parse_cast_expr<S, Lookup>()) {
        return parse_multiply_tail<lhs.tstring_tail(S), lhs.result, Lookup>();
    }
}

template <auto S, auto LHS, typename Lookup>
cxauto parse_sum_tail() {
    cxauto peek = token::lex(S);
    if cx (peek.kind != token::plus && peek.kind != token::minus) {
        return parse_result{LHS, S};
    } else {
        guard(next, parse_multiply_expr<peek.tstring_tail(S), Lookup>()) {
            using op     = std::conditional_t<peek.kind == token::plus, std::plus<>, std::minus<>>;
            cxauto binop = binop_expr{op{}, LHS, next.result};
            return parse_sum_tail<next.tstring_tail(S), binop, Lookup>();
        }
    }
}

template <auto S, typename Lookup>
cxauto parse_sum_expr() {
    guard(lhs, parse_multiply_expr<S, Lookup>()) {
        return parse_sum_tail<lhs.tstring_tail(S), lhs.result, Lookup>();
    }
}

template <auto S, typename Lookup>
cxauto parse_expression() {
    return parse_sum_expr<S, Lookup>();
}

}  // namespace smstr::dsl::detail