#include "./number.hpp"

#include "./builtins.inl"

#include <neo/utility.hpp>

#include <functional>

namespace smstr::dsl::detail {

ParseFn(parse_map_literal, auto... elems);

ParseFn(parse_array_literal, auto... elems);

ParseFn(parse_mapping_key_expression) {
    cxauto tok = State.peek();
    if cx (tok.kind == tok.ident) {
        return parse_result{lit_string<SM_STR(tok.spelling)>{}, tok.tail};
    } else if cx (tok.kind == tok.string) {
        cxauto key = realize_string_literal<tok.tstring_in(State)>();
        return parse_result{lit_string<SM_STR(key)>{}, tok.tail};
    } else if cx (tok.kind == tok.square_l) {
        cxauto expr = parse_expression(StateAfter(tok));
        if cx (expr.is_error) {
            return expr;
        } else {
            cxauto close_bracket = expr.next_token();
            if cx (close_bracket.kind != token::square_r) {
                return error<"Expected closing bracket following key expression",
                             close_bracket.tstring_in(State)>();
            } else {
                return parse_result{expr.result, close_bracket.tail};
            }
        }
    } else {
        return error<
            "Expected a string literal, identifier, or bracketed expression for map literal "
            "key",
            tok.tstring_in(State)>();
    }
}

ParseFn(parse_map_entry) {
    cxauto tok = State.peek();
    if cx (tok.kind == tok.ident && tok.next().kind == neo::oper::any_of(tok.brace_r, tok.comma)) {
        guard(expr, parse_expression(State)) {
            cxauto lit_str = lit_string<SM_STR(tok.spelling)>{};
            return parse_result{lit_map_entry{lit_str, expr.result}, tok.tail};
        }
    } else if cx (tok.kind == tok.elipse) {
        guard(expr, parse_expression(StateAfter(tok))) {
            return parse_result{map_spread_entry{expr.result}, expr.tail};
        }
    } else if cx (cxauto key = parse_mapping_key_expression(State); key.is_error) {
        return key;
    } else if cx (cxauto colon = key.next_token(); colon.kind != colon.colon) {
        return error<"Expected a colon `:` following map key", colon.tstring_in(State)>();
    } else if cx (cxauto value = parse_expression(StateAfter(colon)); value.is_error) {
        return value;
    } else {
        return parse_result{lit_map_entry{key.result, value.result}, value.tail};
    }
}

ParseFn(parse_map_literal, auto... elems) {
    cxauto tok = State.peek();
    if cx (tok.kind == tok.brace_r) {
        return parse_result{lit_map_expr<decltype(elems)...>{}, tok.tail};
    } else if cx (cxauto entry = parse_map_entry(State); entry.is_error) {
        return entry;
    } else if cx (cxauto peek = entry.next_token(); peek.kind == peek.brace_r) {
        return parse_map_literal(StateAfter(entry), elems..., entry.result);
    } else if cx (peek.kind != peek.comma) {
        return error<"Expect a comma `,` or closing brace `}` following map literal value",
                     peek.tstring_in(State)>();
    } else {
        return parse_map_literal(StateAfter(peek), elems..., entry.result);
    }
}

ParseFn(parse_expression_with) {
    cxauto ident = State.peek();
    if cx (ident.kind != token::ident) {
        return error<"Expected identifier for scope variable name", ident.tstring_in(State)>();
    } else if cx (cxauto is = ident.next(); is.kind != token::kw_is) {
        return error<"Expected `is` keyword following identifier of `with` expression variable",
                     is.tstring_in(State)>();
    } else {
        guard(with, parse_expression(StateAfter(is))) {
            using scope       = scoped_names<name<SM_STR(ident.spelling)>>;
            cxauto peek       = with.next_token();
            cxauto new_lookup = State.add_lookup(scope{});
            cxauto new_state  = state_after(new_lookup, peek);
            if cx (peek.kind == token::comma) {
                guard(next_expr, parse_expression_with(new_state)) {
                    return parse_result{with_expr{with.result, next_expr.result}, next_expr.tail};
                }
            } else if cx (peek.kind == token::kw_then) {
                guard(final_expr, parse_expression(new_state)) {
                    return parse_result{with_expr{with.result, final_expr.result}, final_expr.tail};
                }
            } else {
                return error<"Expected a comma or keyword `then` following with-expression",
                             peek.tstring_in(State)>();
            }
        }
    }
}

ParseFn(parse_if_else_expr) {
    guard(cond_expr, parse_expression(State)) {
        cxauto then_tok = cond_expr.next_token();
        if cx (then_tok.kind != token::kw_then) {
            return error<"Expected `then` keyword following conditional expression",
                         then_tok.tstring_in(State)>();
        } else {
            guard(true_expr, parse_expression(StateAfter(then_tok))) {
                cxauto else_tok = true_expr.next_token();
                if cx (else_tok.kind != token::kw_else) {
                    return error<
                        "Expected `else` keyword following true-branch expression in "
                        "conditional",
                        else_tok.tstring_in(State)>();
                } else {
                    guard(false_expr, parse_expression(StateAfter(else_tok))) {
                        cxauto end_tok = false_expr.next_token();
                        if cx (end_tok.kind != token::kw_end) {
                            return error<"Expected `end` marker for if/else branch",
                                         end_tok.tstring_in(State)>();
                        } else {
                            return parse_result{if_else_expr{cond_expr.result,
                                                             true_expr.result,
                                                             false_expr.result},
                                                end_tok.tail};
                        }
                    }
                }
            }
        }
    }
}

ParseFn(parse_primary_expr) {
    cxauto tok = State.peek();
    if cx (tok.kind == tok.kw_fn) {
        return parse_fn_expression(StateAfter(tok));
    } else if cx (tok.kind == tok.kw_with) {
        return parse_expression_with(StateAfter(tok));
    } else if cx (tok.kind == tok.ident) {
        cxauto resolve = State.template resolve<tok.tstring_in(State)>();
        if cx (resolve.scope_idx < 0) {
            if cx (defined<SM_STR(tok.spelling)>) {
                return parse_result{just_expr{define<SM_STR(tok.spelling)>}, tok.tail};
            } else {
                cxauto message = "Name `" + SM_STR(tok.spelling)
                    + "` does not name a variable or an externally-defined name.";
                return error<SM_STR(message), tok.tstring_in(State)>();
            }
        } else {
            using ptype = decltype(resolve)::param_type::type;
            using pref  = var_ref_expr<resolve.scope_idx, resolve.name_idx, ptype>;
            return parse_result{pref{}, tok.tail};
        }
    } else if cx (tok.kind == tok.kw_true) {
        return parse_result{bool_expr<true>{}, tok.tail};
    } else if cx (tok.kind == tok.kw_false) {
        return parse_result{bool_expr<false>{}, tok.tail};
    } else if cx (tok.kind == tok.kw_if) {
        return parse_if_else_expr(StateAfter(tok));
    } else if cx (tok.kind == tok.string) {
        cxauto str = realize_string_literal<tok.tstring_in(State)>();
        return parse_result{lit_string<SM_STR(str)>{}, tok.tail};
    } else if cx (tok.kind == tok.brace_l) {
        return parse_map_literal(StateAfter(tok));
    } else if cx (tok.kind == tok.square_l) {
        return parse_array_literal(StateAfter(tok));
    } else if cx (tok.kind == tok.integer) {
        return parse_result{lit_integer<SM_STR(tok.spelling)>{}, tok.tail};
    } else if cx (tok.kind == tok.decimal) {
        return parse_result{lit_decimal<SM_STR(tok.spelling)>{}, tok.tail};
    } else if cx (tok.kind == tok.paren_l) {
        guard(inner_expr, parse_expression(StateAfter(tok))) {
            if cx (cxauto next = inner_expr.next_token(); next.kind != token::paren_r) {
                return error<"Expected closing parenthesis following expression",
                             next.tstring_in(State)>();
            } else {
                return parse_result{inner_expr.result, next.tail};
            }
        }
    } else {
        return error<"Expected an expression (string literal, map literal, array literal, or name)",
                     tok.tstring_in(State)>();
    }
}

ParseFn(parse_call_args_inner, auto... args) {
    cxauto tok = State.peek();
    if cx (tok.kind == tok.paren_r) {
        return parse_result{neo::tag_v<decltype(args)...>, tok.tail};
    } else {
        guard(expr, parse_expression(State)) {
            cxauto peek = expr.next_token();
            if cx (peek.kind == token::comma) {
                return parse_call_args_inner(StateAfter(peek), args..., expr.result);
            } else if cx (peek.kind == token::paren_r) {
                return parse_call_args_inner(StateAfter(expr), args..., expr.result);
            } else {
                return error<
                    "Expected a comma `,` or closing parenthesis `)` following call argument "
                    "expression",
                    peek.tstring_in(State)>();
            }
        }
    }
}

template <auto LHS>
ParseFn(parse_expr_suffixes) {
    cxauto peek = State.peek();
    if cx (peek.kind == token::paren_l) {
        // We're a function call
        guard(arglist, parse_call_args_inner(StateAfter(peek))) {
            cxauto call = call_expr{LHS, arglist.result};
            return parse_expr_suffixes<call>(StateAfter(arglist));
        }
    } else if cx (peek.kind == token::dot) {
        cxauto ident = peek.next();
        if cx (ident.kind != token::ident) {
            return error<"Expected identifier following dot `.`", ident.tstring_in(State)>();
        } else {
            return parse_expr_suffixes<dot_expr<decltype(LHS), SM_STR(ident.spelling)>{}>(
                StateAfter(ident));
        }
    } else {
        return parse_result{LHS, State.strv};
    }
}

template <auto LHS>
ParseFn(parse_cast_tail) {
    cxauto peek = State.peek();
    if cx (peek.spelling != "as") {
        return parse_result{LHS, State.strv};
    } else {
        guard(type, parse_type_definition<peek.tstring_tail(State.strv)>()) {
            cxauto as = as_expr{LHS, type.result};
            return parse_cast_tail<as>(StateAfter(type));
        }
    }
}

ParseFn(parse_cast_expr) {
    guard(primary, parse_primary_expr(State)) {
        guard(with_suffixes, parse_expr_suffixes<primary.result>(StateAfter(primary))) {
            return parse_cast_tail<with_suffixes.result>(StateAfter(with_suffixes));
        }
    }
}

template <typename Parser>
ParseFn(parse_binop_expr_left_assoc);

template <typename T>
concept has_inner_binop = requires {
    typename T::inner_binop;
};

template <typename Parser, auto LHS>
ParseFn(parse_binop_tail) {
    cxauto peek = State.peek();
    if cx (!Parser::template should_join<peek.kind>) {
        return parse_result{LHS, State.strv};
    } else {
        if cx (has_inner_binop<Parser>) {
            guard(rhs,
                  parse_binop_expr_left_assoc<typename Parser::inner_binop>(StateAfter(peek))) {
                using oper   = Parser::template oper_t<peek.kind>;
                cxauto binop = binop_expr{oper{}, LHS, rhs.result};
                return parse_binop_tail<Parser, binop>(StateAfter(rhs));
            }
        } else {
            guard(rhs, Parser::template parse_inner(StateAfter(peek))) {
                using oper   = Parser::template oper_t<peek.kind>;
                cxauto binop = binop_expr{oper{}, LHS, rhs.result};
                return parse_binop_tail<Parser, binop>(StateAfter(rhs));
            }
        }
    }
}

template <typename Parser>
ParseFn(parse_binop_expr_left_assoc) {
    if cx (has_inner_binop<Parser>) {
        guard(expr, parse_binop_expr_left_assoc<typename Parser::inner_binop>(State)) {
            return parse_binop_tail<Parser, expr.result>(StateAfter(expr));
        }
    } else {
        guard(expr, Parser::parse_inner(State)) {
            return parse_binop_tail<Parser, expr.result>(StateAfter(expr));
        }
    }
}

struct mult_binop {
    static cxauto parse_inner(auto state) { return parse_cast_expr(state); }

    template <auto K>
    static cxauto should_join = K == neo::oper::any_of(token::star, token::slash);

    template <auto K>
    using oper_t = std::conditional_t<K == token::star, std::multiplies<>, std::divides<>>;
};

struct sum_binop {
    using inner_binop = mult_binop;

    template <auto K>
    static cxauto should_join = K == neo::oper::any_of(token::plus, token::minus);

    template <auto K>
    using oper_t = std::conditional_t<K == token::plus, std::plus<>, std::minus<>>;
};

struct concat_binop {
    using inner_binop = sum_binop;
    template <auto K>
    static cxauto should_join = K == token::plusplus;

    struct concat {
        cx decltype(auto) eval(auto traits, auto&& left, auto&& right) const {
            return traits.concat(NEO_FWD(left), NEO_FWD(right));
        }
    };

    template <auto>
    using oper_t = concat;
};

struct eq_binop {
    using inner_binop = concat_binop;

    template <auto K>
    static cxauto should_join = K == neo::oper::any_of(token::equal_to, token::not_equal_to);

    template <auto K>
    using oper_t = std::conditional_t<K == token::equal_to, std::equal_to<>, std::not_equal_to<>>;
};

template <typename Left, typename Func, typename... Args>
cxauto rewrite_pipeline(Left, call_expr<Func, Args...>, auto State) {
    return parse_result{call_expr<Func, Left, Args...>{}, State.strv};
}

cxauto rewrite_pipeline(auto, auto, auto State) {
    return error<"Expect a call-expression on the right-hand side of pipeline `|>` operator",
                 State.peek().tstring_in(State)>();
}

template <auto LHS>
ParseFn(parse_pipeline_arrow_tail) {
    cxauto peek = State.peek();
    if cx (peek.kind != token::right_pipe) {
        return parse_result{LHS, State.strv};
    } else {
        guard(rhs, parse_binop_expr_left_assoc<eq_binop>(StateAfter(peek))) {
            guard(rewritten, rewrite_pipeline(LHS, rhs.result, State)) {
                return parse_pipeline_arrow_tail<rewritten.result>(StateAfter(rhs));
            }
        }
    }
}

ParseFn(parse_pipeline_arrow_expr) {
    guard(expr, parse_binop_expr_left_assoc<eq_binop>(State)) {
        return parse_pipeline_arrow_tail<expr.result>(StateAfter(expr));
    }
}

struct logic_binop {
    static cxauto parse_inner(auto state) { return parse_pipeline_arrow_expr(state); }

    template <auto K>
    static cxauto should_join = K == neo::oper::any_of(token::kw_and, token::kw_or, token::kw_xor);

    struct logical_xor {
        constexpr bool operator()(auto&& left, auto&& right) const noexcept {
            return bool(left) ^ bool(right);
        }
    };

    template <auto K>
    using oper_t = std::conditional_t<  //
        K == token::kw_and,
        std::logical_and<>,
        std::conditional_t<  //
            K == token::kw_or,
            std::logical_or<>,
            logical_xor>>;
};

ParseFn(parse_expression) { return parse_binop_expr_left_assoc<logic_binop>(State); }

}  // namespace smstr::dsl::detail

#include "./parse.expr.array.inl"
#include "./parse.expr.map.inl"
