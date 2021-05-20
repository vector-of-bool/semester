#pragma once

#include "./define.hpp"
#include "./lookup.hpp"
#include "./mapping.hpp"
#include "./string.hpp"
#include "./token.hpp"
#include "./type.hpp"

#include <semester/primitives.hpp>
#include <semester/traits.hpp>

#include <neo/fixed_string.hpp>
#include <neo/tag.hpp>

#include <concepts>
#include <ranges>

#define cx constexpr
#define cxauto constexpr auto
#define guard(Name, ...)                                                                           \
    if cx (cxauto Name = __VA_ARGS__; Name.is_error) {                                             \
        return Name;                                                                               \
    } else

#define tailof(Token) ((Token).tstring_tail(S))

namespace smstr::dsl {

template <auto TStringView, typename Lookup>
struct parser_state {
    static cxauto strv = TStringView;
    using lookup       = Lookup;

    static cxauto peek() { return token::lex(strv); }

    template <auto TS>
    static cxauto resolve() {
        return Lookup::template resolve<TS>();
    }

    static cxauto add_lookup(auto names) {
        return parser_state<TStringView, typename Lookup::template push_front<decltype(names)>>{};
    }
};

#define state_after(State, TokenOrResult)                                                          \
    (parser_state<(State).strv.substr((TokenOrResult).tail.data() - (State).strv.data()),          \
                  typename decltype(State)::lookup>{})

#define StateAfter(TokenOrResult) state_after(State, TokenOrResult)
#define ParseFn(Name, ...) constexpr auto Name(auto State __VA_OPT__(, ) __VA_ARGS__)

template <typename Inner>
struct opt {};

template <typename Inner>
struct array {};

template <typename... Inner>
struct oneof {};

template <>
constexpr null_t define<"null"> = null_t{};

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

cxauto parse_expression(auto state);

template <typename Vars, typename Outer = int>
struct scope_ctx {
    Vars  vars;
    Outer outer{};

    cx auto push_front(auto tup) const noexcept { return detail::scope_ctx{tup, *this}; }

    template <int N>
    cx auto&& get_nth() const noexcept {
        if cx (N == 0) {
            return *this;
        } else {
            return outer.template get_nth<N - 1>();
        }
    }
};

template <typename Traits, typename Scope>
struct evaluation_context {
    Traits& traits;
    Scope&  scope;
};

template <typename T, typename S>
explicit evaluation_context(T&, S&) -> evaluation_context<T, S>;

template <typename Vars>
explicit scope_ctx(Vars) -> scope_ctx<Vars>;

template <typename Vars, typename Outer>
explicit scope_ctx(Vars, Outer) -> scope_ctx<Vars, Outer>;

template <typename Params, typename Body>
struct function;

template <typename... Elems>
struct lit_map_expr {
    cx auto eval(auto ctx) const {
        auto ret = ctx.traits.new_map();
        (Elems{}.add_to_map(ret, ctx), ...);
        return ret;
    }
};

template <typename Key, typename Value>
struct lit_map_entry {
    Key   key;
    Value value;

    cx void add_to_map(auto& map, auto ctx) const {
        auto&& key   = this->key.eval(ctx);
        auto&& value = this->value.eval(ctx);
        ctx.traits.map_insert(map, NEO_FWD(key), NEO_FWD(value));
    }
};

template <typename Expr>
struct map_spread_entry {
    Expr expr;

    cx void add_to_map(auto& map, auto ctx) const {
        auto&& value = expr.eval(ctx);
        ctx.traits.map_spread(map, NEO_FWD(value));
    }
};

template <typename... Entries>
struct lit_array_expr {
    cx auto eval(auto ctx) const {
        auto arr = ctx.traits.new_array();
        (ctx.traits.array_push(arr, Entries{}.eval(ctx)), ...);
        return arr;
    }
};

template <int Scope, int Idx, typename Type>
struct var_ref_expr {
    constexpr auto& eval(auto ctx) const {
        auto& scope = ctx.scope.template get_nth<Scope>();
        return std::get<Idx>(scope.vars);
    }
};

template <typename Left, typename Target>
struct as_expr {
    Left   expr;
    Target type;

    cx decltype(auto) eval(auto ctx) const {
        auto&& value = expr.eval(ctx);
        using type   = lookup_type<std::remove_cvref_t<decltype(value)>, Target>::type;
        return smstr::get<type>(value);
    }
};

template <typename Op, typename Left, typename Right>
struct binop_expr {
    Op    op;
    Left  left;
    Right right;

    cx decltype(auto) eval(auto ctx) const {
        auto&&  left        = this->left.eval(ctx);
        auto&&  right       = this->right.eval(ctx);
        cx bool is_smart_op = requires { op.eval(ctx.traits, NEO_FWD(left), NEO_FWD(right)); };
        if cx (is_smart_op) {
            return op.eval(ctx.traits, NEO_FWD(left), NEO_FWD(right));
        } else {
            return op(NEO_FWD(left), NEO_FWD(right));
        }
    }
};

template <typename Params, typename Body>
struct fn_expr {
    Params params;
    Body   body;

    cx auto eval(auto) const { return function{params, body}; }
};

template <typename Function, typename... Args>
struct call_expr {
    Function          fn;
    neo::tag<Args...> arg_exprs;

    cx decltype(auto) eval(auto ctx) const {
        auto&& func     = fn.eval(ctx);
        cxauto has_call = requires { func.call(ctx, Args{}.eval(ctx)...); };
        if cx (has_call) {
            return func.call(ctx, Args{}.eval(ctx)...);
        } else {
            return func(Args{}.eval(ctx)...);
        }
    }
};

template <typename Func>
struct call_with_ctx_wrapper {
    Func fn;

    cx decltype(auto) call(auto ctx, auto&&... args) const { return fn(ctx, NEO_FWD(args)...); }
};

template <typename T>
struct just_expr {
    T value;

    cx auto& eval(auto) const noexcept { return value; }
};

template <typename Left, auto Ident>
struct dot_expr {
    Left lhs;

    cx auto& eval(auto ctx) const {
        auto&& value = lhs.eval(ctx);
        auto&& map   = ctx.traits.as_map(NEO_FWD(value));
        return ctx.traits.map_lookup(NEO_FWD(map), Ident.string_view());
    }
};

template <typename WithExpr, typename NextExpr>
struct with_expr {
    WithExpr with;
    NextExpr next_expr;

    cx decltype(auto) eval(auto ctx) const {
        auto&&             value     = with.eval(ctx);
        auto&&             tup       = std::tie(value);
        auto               new_scope = ctx.scope.push_front(tup);
        evaluation_context new_ctx{ctx.traits, new_scope};
        return next_expr.eval(new_ctx);
    }
};

template <typename Cond, typename TrueBranch, typename FalseBranch>
struct if_else_expr {
    Cond        condition;
    TrueBranch  true_branch;
    FalseBranch false_branch;

    cx decltype(auto) eval(auto ctx) const {
        auto&& cond        = condition.eval(ctx);
        using true_result  = decltype(true_branch.eval(ctx));
        using false_result = decltype(false_branch.eval(ctx));
        if cx (std::common_reference_with<true_result, false_result>) {
            using comref = std::common_reference_t<true_result, false_result>;
            if (cond) {
                return comref(true_branch.eval(ctx));
            } else {
                return comref(false_branch.eval(ctx));
            }
        } else {
            if (cond) {
                return ctx.traits.normalize(true_branch.eval(ctx));
            } else {
                return ctx.traits.normalize(false_branch.eval(ctx));
            }
        }
    }
};

template <bool B>
struct bool_expr {
    cxauto eval(auto) const { return B; }
};

struct invalid_expr {};

template <typename Expression, typename InitScope>
struct evaluator {
    Expression _expr;
    InitScope  _tup;

    cx decltype(auto) evaluate(auto traits) const {
        scope_ctx          scope{_tup, 0};
        evaluation_context ctx{traits, scope};
        return _expr.eval(ctx);
    }

    template <has_valid_data_traits Data>
    cx operator Data() const&& {
        return this->evaluate(data_traits<Data>{});
    }
};

template <typename Params, typename Body>
struct function {
    Params params;
    Body   body;

    template <typename... Args>
    cx decltype(auto) operator()(Args&&... args) const requires(sizeof...(Args) == Params::count) {
        return evaluator{body, std::tie(args...)};
    }

    cx decltype(auto) call(auto ctx, auto&&... args) const requires requires {
        Params::check_compatible_argv(ctx.traits, NEO_FWD(args)...);
    }
    { return evaluator{body, std::tie(args...)}.evaluate(ctx.traits); }
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

cxauto skip_comma(token t) {
    if (t.kind == t.comma) {
        return t.next();
    } else {
        return t;
    }
}

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
cxauto error() {
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
    if cx (peek.kind == peek.square_l) {
        cxauto peek2 = peek.next();
        if cx (peek2.kind != peek.square_r) {
            return error<"Expected closing bracket ']' following opening bracket in type",
                         peek2.tstring_tok(S)>();
        } else {
            using next = array<Base>;
            return parse_type_suffix<tailof(peek2), next>();
        }
    } else if cx (peek.kind == peek.question) {
        using next = opt<Base>;
        return parse_type_suffix<tailof(peek), next>();
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
        if cx (peek.kind == peek.dot) {
            return parse_dotted_id_1<tailof(peek), Names..., tok.tstring_tok(S)>();
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
    if cx (peek.kind == peek.dot) {
        return parse_dotted_id_1<tailof(peek), tok.tstring_tok(S)>();
    } else {
        return parse_result{name<SM_STR(tok.spelling)>{}, tok.tail};
    }
}

template <auto S>
cxauto parse_base_type() {
    cxauto tok = token::lex(S);
    if cx (tok.kind == tok.invalid) {
        return error<"Invalid leading token", tok.tstring_tok(S)>();
    } else if cx (tok.kind == tok.brace_l) {
        return parse_mapping_type<S>();
    } else if cx (tok.kind == tok.paren_l) {
        cxauto inner = parse_type_definition<tailof(tok)>();
        if cx (inner.is_error) {
            return inner;
        } else {
            cxauto peek = token::lex(inner.tail);
            if cx (peek.kind != peek.paren_r) {
                return error<"Expected closing parenthesis ')' at end of type expression",
                             tailof(inner)>;
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
        return parse_type_suffix<tailof(base), decltype(base.result)>();
    }
}

template <auto S, typename... Alts>
cxauto parse_oneof_next(neo::tag<Alts...>) {
    cxauto new_alt = parse_oneof_alternative<S>();
    if cx (new_alt.is_error) {
        return new_alt;
    } else {
        cxauto peek = token::lex(new_alt.tail);
        if cx (peek.kind == peek.pipe) {
            return parse_oneof_next<tailof(peek)>(neo::tag_v<Alts..., decltype(new_alt.result)>);
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
        if cx (peek.kind == peek.pipe) {
            return parse_oneof_next<tailof(peek)>(neo::tag_v<decltype(single.result)>);
        } else {
            return single;
        }
    }
}

template <auto ErrorMessage>
auto print_error() {
    return ErrorMessage;
}

template <typename R>
cxauto handle_error(R result) {
    if cx (result.is_error) {
        cxauto _ = print_error<R{}>();
        return invalid_expr{};
    } else {
        return result.result;
    }
}

}  // namespace detail
}  // namespace smstr::dsl

#include "./parse.fn.inl"

#include "./parse.expr.inl"

namespace smstr::dsl {

template <neo::basic_fixed_string S>
using parse_type_t = std::remove_cvref_t<decltype(
    detail::handle_error(detail::parse_type_definition<neo::tstring_view_v<SM_S>>()))>;

template <neo::basic_fixed_string S>
struct type {
    using type_ = parse_type_t<SM_S>;
};

template <neo::basic_fixed_string S>
constexpr auto eval_str_defer() {
    using init_lookup = scope_chain<>;
    using expr_type   = decltype(detail::handle_error(
        detail::parse_expression(parser_state<neo::tstring_view_v<S>, init_lookup>{})));
    return detail::evaluator{expr_type{}, std::tuple()};
}

template <neo::basic_fixed_string S>
constexpr auto eval_str() {
    return eval_str_defer<SM_S>().evaluate(default_traits{});
}

}  // namespace smstr::dsl

#undef cxauto
#undef cx
#undef tailof
#undef tail_state
#undef StateAfter
#undef ParseFn