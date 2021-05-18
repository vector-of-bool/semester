#include "./lookup.hpp"

#include "./number.hpp"

#define cxauto constexpr auto
#define cx constexpr

namespace smstr::dsl::detail {

template <auto S, typename Lookup>
constexpr auto parse_generate_expr() {
    constexpr auto gen_kw = token::lex(S);
    static_assert(gen_kw.spelling == "generate");

    if constexpr (constexpr auto open_paren = gen_kw.next(); open_paren.spelling != "(") {
        return error<"Expected opening parenthesis `(` following `generate` keyword",
                     open_paren.tstring_tok(S)>();
    } else if constexpr (constexpr auto params = parse_param_list<open_paren.tstring_tail(S)>();
                         params.is_error) {
        return params;
    } else if constexpr (constexpr auto body = parse_expression<
                             params.tstring_tail(S),
                             typename Lookup::template push_front<decltype(params.result)>>();
                         body.is_error) {
        return body;
    } else {
        return parse_result{generator<params.result.count, decltype(body.result)>{}, body.tail};
    }
}

}  // namespace smstr::dsl::detail
