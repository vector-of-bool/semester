namespace smstr::dsl::detail {

ParseFn(parse_array_literal, auto... elems) {
    cxauto tok = State.peek();
    if cx (tok.kind == tok.square_r) {
        return parse_result{lit_array_expr<decltype(elems)...>{}, tok.tail};
    } else {
        guard(expr, parse_expression(State)) {
            cxauto peek = expr.next_token();
            if cx (peek.kind == token::comma) {
                return parse_array_literal(state_after(State, peek), elems..., expr.result);
            } else if cx (peek.kind == token::square_r) {
                return parse_array_literal(state_after(State, expr), elems..., expr.result);
            } else {
                return error<
                    "Expected a comma `,` or closing square bracket `]` following array entry",
                    peek.tstring_in(State)>();
            }
        }
    }
}

}  // namespace smstr::dsl::detail
