#include <neo/utility.hpp>

namespace smstr::dsl::detail {

ParseFn(parse_var_decl) {
    cxauto tok = State.peek();
    if cx (tok.kind != tok.ident) {
        return error<"Expected an identifier for a variable declaration",
                     tok.tstring_tok(State.strv)>();
    } else {
        cxauto peek = tok.next();
        if cx (peek.kind == tok.colon) {
            cxauto type = parse_type_definition<peek.tstring_tail(State.strv)>();
            return parse_result{typed_name<SM_STR(tok.spelling), decltype(type.result)>{},
                                type.tail};
        } else {
            return parse_result{name<SM_STR(tok.spelling)>{}, tok.tail};
        }
    }
}

/**
 * @brief Parse a parameter list. Expects `S` to start _after_ the opening paren
 *
 * @tparam S
 * @tparam Params
 * @return cxauto
 */
ParseFn(parse_param_inner, auto... acc) {
    guard(vardecl, parse_var_decl(State)) {
        cxauto peek = vardecl.next_token();
        if cx (peek.kind == token::comma) {
            return parse_param_inner(StateAfter(peek), acc..., vardecl.result);
        } else {
            return parse_result{scoped_names<decltype(acc)..., decltype(vardecl.result)>{},
                                vardecl.tail};
        }
    }
}

ParseFn(parse_param_list) {
    cxauto tok = State.peek();
    if cx (tok.kind == token::kw_is) {
        return parse_result{scoped_names<>{}, State.strv};
    } else if cx (tok.kind == token::ident) {
        return parse_param_inner(State);
    } else {
        return error<"Expected parameter list or `is` keyword", tok.tstring_tok(State.strv)>();
    }
}

ParseFn(parse_fn_expression) {
    guard(params, parse_param_list(State)) {
        cxauto new_state = State.add_lookup(params.result);
        cxauto is        = params.next_token();
        if cx (is.kind != token::kw_is) {
            return error<"Expected `is` keyword following parameter list and before function body",
                         is.tstring_in(State)>();
        } else {
            guard(body, parse_expression(state_after(new_state, is))) {
                cxauto end = body.next_token();
                if cx (end.kind != token::kw_end) {
                    return error<"Expected `end` keyword following function body",
                                 end.tstring_in(State)>();
                } else {
                    return parse_result{fn_expr{params.result, body.result}, end.tail};
                }
            }
        }
    }
}

}  // namespace smstr::dsl::detail
