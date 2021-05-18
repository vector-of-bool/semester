#pragma once

#include "./define.hpp"
#include "./lookup.hpp"
#include "./mapping.hpp"
#include "./string.hpp"
#include "./token.hpp"
#include "./type.hpp"

#include <semester/primitives.hpp>

#include <neo/fixed_string.hpp>
#include <neo/tag.hpp>

#include <concepts>
#include <ranges>

#define cx constexpr
#define cxauto constexpr auto
#define errcheck(Name)                                                                             \
    if cx (Name.is_error) {                                                                        \
        return Name;                                                                               \
    } else
#define guard(Name, ...)                                                                           \
    if cx (cxauto Name = __VA_ARGS__; Name.is_error) {                                             \
        return Name;                                                                               \
    } else

namespace smstr::dsl {

template <typename Inner>
struct opt {};

template <typename Inner>
struct array {};

template <typename... Inner>
struct oneof {};

template <>
struct define<"null"> : primitive_type<null_t> {};

namespace detail {

/*
########  ########  ######  ##        ######
##     ## ##       ##    ## ##       ##    ##
##     ## ##       ##       ##       ##
##     ## ######   ##       ##        ######
##     ## ##       ##       ##             ##
##     ## ##       ##    ## ##       ##    ##
########  ########  ######  ########  ######
*/

template <auto S>
cxauto parse_type_definition();

template <auto S>
cxauto parse_mapping_type();

template <auto S, typename Lookup>
cxauto parse_expression();

cx decltype(auto) evaluate_expression(auto traits, auto body, auto&& ctx);

struct default_traits {
    static constexpr std::int64_t number(auto str) {
        std::int64_t ret = 0;
        for (char32_t c : str) {
            assert(c >= '0' && c <= '9');
            auto val = c - '0';
            ret *= 10;
            ret += val;
        }
        return ret;
    }
};

template <int Scope, int Idx, typename Type>
struct param_ref {};

template <typename... Elems>
struct map_literal {};

template <typename Key, typename Value>
struct map_entry {
    Key   key;
    Value value;
};

template <typename Left, typename Target>
struct as_expr {};

template <typename Op, typename Left, typename Right>
struct binop_expr {
    Op    op;
    Left  left;
    Right right;
};

template <typename Params, typename Body>
struct fn_expr {
    Params params;
    Body   body;
};

template <typename WithExpr, typename NextExpr>
struct with_expr {
    WithExpr with;
    NextExpr next_expr;
};

template <typename Params, typename Body>
struct function {
    Params params;
    Body   body;

    template <typename... Args>
    cx decltype(auto) operator()(Args&&... args) const {
        return evaluate_expression(default_traits{},
                                   body,
                                   std::forward_as_tuple(std::forward_as_tuple(NEO_FWD(args)...)));
    }
};

template <typename T>
struct parse_result {
    T           result;
    string_view tail;

    template <typename TS>
    cxauto tstring_tail(TS ts) const noexcept {
        const auto off = tail.data() - ts.data();
        return ts.substr(off, tail.length());
    }

    constexpr token next_token() const noexcept { return token::lex(tail); }

    constexpr static inline bool is_error = false;
};

template <neo::basic_fixed_string Message>
struct parse_error {
    constexpr static inline bool is_error = true;
};

/*
########  ######## ######## ##    ##  ######
##     ## ##       ##       ###   ## ##    ##
##     ## ##       ##       ####  ## ##
##     ## ######   ######   ## ## ##  ######
##     ## ##       ##       ##  ####       ##
##     ## ##       ##       ##   ### ##    ##
########  ######## ##       ##    ##  ######
*/

template <auto N>
cxauto make_lineno_string() {
    if cx (N > 9) {
        auto prefix = make_lineno_string<N / 10>();
        auto s      = make_lineno_string<N % 10>();
        return prefix + s;
    } else {
        neo::basic_fixed_string s = "_";
        *s.begin()                = '0' + (N % 10);
        return s;
    }
}

template <auto Full, auto Where>
cxauto extract_line() {
    cxauto offset = Where.data() - Full.data();
    static_assert(offset >= 0);
    cxauto line_it = std::find(std::make_reverse_iterator(Full.begin() + offset),
                               std::make_reverse_iterator(Full.begin()),
                               '\n')
                         .base();
    cxauto line_end = std::find(line_it + 1, Full.end(), '\n');
    cxauto size     = line_end - line_it;
    return Full.substr(line_it - Full.begin(), size);
}

template <neo::basic_fixed_string Message, auto TStringWhere>
auto error() {
    cxauto Full     = neo::tstring_view_v<decltype(TStringWhere)::tstring_type::string>;
    cxauto offset   = TStringWhere.data() - Full.data();
    cxauto nth_line = std::ranges::count_if(Full.string_view().substr(0, offset),
                                            [](char32_t c) { return c == '\n'; });
    constexpr neo::basic_fixed_string prefix = "Parse error on line ";
    constexpr neo::basic_fixed_string lno    = make_lineno_string<nth_line>();
    // Build the line:
    cxauto line       = extract_line<Full, TStringWhere>();
    cxauto col_offset = TStringWhere.begin() - line.begin();
    cxauto col_end    = col_offset + TStringWhere.size();
    cxauto line_pre   = line.substr(0, col_offset);
    cxauto line_inner = line.substr(col_offset, TStringWhere.size());
    cxauto line_after = line.substr(col_end);

    cxauto full_message = SM_STR(prefix) + lno + ": " + Message + ": `" + SM_STR(line_pre)
        + "<HERE->" + SM_STR(line_inner) + "<-HERE>" + SM_STR(line_after) + "`";
    return parse_error<full_message>{};
}

template <auto S>
cxauto realize_string_literal_1() {
    cxauto escape_pos = S.find_first_of("\\");
    if cx (escape_pos == S.npos) {
        // No escape chars. Just return the string body
        return S;
    } else if cx (S.empty()) {
        // End of-string
        return neo::basic_fixed_string{""};
    } else {
        // Get the string up to the next escape character,
        cxauto part = S.substr(0, escape_pos);
        // The the single escaped character
        cxauto esc_char = S.substr(escape_pos + 1, 1);
        // Then the rest of the string
        cxauto tail = realize_string_literal_1<S.substr(escape_pos + 2)>();
        // Concatenate
        return SM_STR(part) + SM_STR(esc_char) + SM_STR(tail);
    }
}

template <auto S>
cxauto realize_string_literal() {
    return realize_string_literal_1<S.substr(1, S.size() - 2)>();
}

template <auto S, typename Base>
cxauto parse_type_suffix() {
    cxauto peek = token::lex(S);
    if cx (peek.spelling == "[") {
        cxauto peek2 = peek.next();
        if cx (peek2.spelling != "]") {
            return error<"Expected closing bracket ']' following opening bracket in type",
                         peek2.tstring_tok(S)>();
        } else {
            using next = array<Base>;
            return parse_type_suffix<peek2.tstring_tail(S), next>();
        }
    } else if cx (peek.spelling == "?") {
        using next = opt<Base>;
        return parse_type_suffix<peek.tstring_tail(S), next>();
    } else {
        return parse_result{Base{}, S};
    }
}

template <auto S, auto... Tail>
cxauto join_dotted_ids() {
    if cx (sizeof...(Tail)) {
        return S + "." + join_dotted_ids<Tail...>();
    } else {
        return S;
    }
}

template <auto S, auto... Names>
cxauto parse_dotted_id_1() {
    cxauto tok = token::lex(S);
    if cx (tok.kind != tok.ident) {
        return error<"Expected an identifier following dot `.`", tok.tstring_tok(S)>();
    } else {
        cxauto peek = tok.next();
        if cx (peek.spelling == ".") {
            return parse_dotted_id_1<peek.tstring_tail(S), Names..., tok.tstring_tok(S)>();
        } else {
            // End of dotted-id
            cxauto ns = join_dotted_ids<Names...>();
            return parse_result{qualified_name<SM_STR(ns), SM_STR(tok.spelling)>{}, tok.tail};
        }
    }
}

template <auto S>
cxauto parse_dotted_id() {
    cxauto tok = token::lex(S);
    static_assert(tok.kind == tok.ident);
    cxauto peek = tok.next();
    if cx (peek.spelling == ".") {
        return parse_dotted_id_1<peek.tstring_tail(S), tok.tstring_tok(S)>();
    } else {
        return parse_result{name<SM_STR(tok.spelling)>{}, tok.tail};
    }
}

template <auto S>
cxauto parse_base_type() {
    cxauto tok = token::lex(S);
    if cx (tok.kind == tok.invalid) {
        return error<"Invalid leading token", tok.tstring_tok(S)>();
    } else if cx (tok.spelling == "{") {
        return parse_mapping_type<S>();
    } else if cx (tok.spelling == "(") {
        cxauto inner = parse_type_definition<tok.tstring_tail(S)>();
        if cx (inner.is_error) {
            return inner;
        } else {
            cxauto peek = token::lex(inner.tail);
            if cx (peek.spelling != ")") {
                return error<"Expected closing parenthesis ')' at end of type expression",
                             inner.tstring_tail(S)>;
            } else {
                return parse_result{inner.result, peek.tail};
            }
        }
    } else if cx (tok.kind == tok.ident) {
        return parse_dotted_id<S>();
    } else if cx (tok.kind == tok.string) {
        cxauto str = realize_string_literal<tok.tstring_tok(S)>();
        return parse_result{lit_string<SM_STR(str)>{}, tok.tail};
    } else {
        return error<"Unexpected token beginning type defintion", tok.tstring_tok(S)>();
    }
}

template <auto S>
cxauto parse_oneof_alternative() {
    cxauto base = parse_base_type<S>();
    if cx (base.is_error) {
        return base;
    } else {
        return parse_type_suffix<base.tstring_tail(S), decltype(base.result)>();
    }
}

template <auto S, typename... Alts>
cxauto parse_oneof_next(neo::tag<Alts...>) {
    cxauto new_alt = parse_oneof_alternative<S>();
    if cx (new_alt.is_error) {
        return new_alt;
    } else {
        cxauto peek = token::lex(new_alt.tail);
        if cx (peek.spelling == "|") {
            return parse_oneof_next<peek.tstring_tail(S)>(
                neo::tag_v<Alts..., decltype(new_alt.result)>);
        } else {
            return parse_result{oneof<Alts..., decltype(new_alt.result)>{}, new_alt.tail};
        }
    }
}

template <auto S>
cxauto parse_type_definition() {
    cxauto single = parse_oneof_alternative<S>();
    if cx (single.is_error) {
        return single;
    } else {
        cxauto peek = token::lex(single.tail);
        if cx (peek.spelling == "|") {
            return parse_oneof_next<peek.tstring_tail(S)>(neo::tag_v<decltype(single.result)>);
        } else {
            return single;
        }
    }
}

template <auto ErrorMessage>
auto print_error() {
    return ErrorMessage;
}

template <auto S>
cxauto parse_type_definition_with_err_handling_1() {
    cxauto result = parse_type_definition<S>();
    if cx (result.is_error) {
        return print_error<result>();
    } else {
        return result;
    }
}

template <typename R>
cxauto handle_error(R result) {
    if cx (result.is_error) {
        cxauto _ = print_error<R{}>();
    } else {
        return result.result;
    }
}

}  // namespace detail
}  // namespace smstr::dsl

#include "./parse.fn.inl"

#include "./parse.expr.inl"
#include "./parse_mapping.inl"

#include "./exec.inl"

namespace smstr::dsl {

namespace detail {

template <auto S>
cxauto do_eval_str() {
    using init_lookup = scope_chain<>;
    cxauto expr       = detail::handle_error(detail::parse_expression<S, init_lookup>());
    return evaluate_expression(default_traits{}, expr, std::tuple(std::forward_as_tuple()));
}

}  // namespace detail

template <neo::basic_fixed_string S>
using parse_type_t = std::remove_cvref_t<decltype(
    detail::handle_error(detail::parse_type_definition<neo::tstring_view_v<SM_S>>()))>;

template <neo::basic_fixed_string S>
struct type {
    using type_ = parse_type_t<SM_S>;
};

template <neo::basic_fixed_string S>
cxauto eval_str = detail::do_eval_str<neo::tstring_view_v<S>>();

}  // namespace smstr::dsl

#undef cxauto
#undef cx
