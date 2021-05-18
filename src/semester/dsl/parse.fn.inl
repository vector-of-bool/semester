
namespace smstr::dsl::detail {

/**
 * @brief Parse a parameter list. Expects `S` to start _after_ the opening paren
 *
 * @tparam S
 * @tparam Params
 * @return cxauto
 */
template <auto S, typename... Params>
cxauto parse_param_list() {
    cxauto tok = token::lex(S);
    if cx (tok.kind != tok.ident || tok.spelling == "with") {
        return parse_result{scoped_names<Params...>{}, S};
    } else if cx (cxauto after_name = tok.next(); after_name.kind != after_name.colon) {
        using param_name = name<SM_STR(tok.spelling)>;
        return parse_param_list<tok.tstring_tail(S), Params..., param_name>();
    } else {
        cxauto type           = parse_type_definition<after_name.tstring_tail(S)>();
        using param_with_type = typed_name<SM_STR(tok.spelling), decltype(type)>;
        return parse_param_list<type.tstring_tail(S), Params..., param_with_type>();
    }
}

template <auto S, typename Lookup>
cxauto parse_fn_expression() {
    guard(params, parse_param_list<S>()) {
        using new_scope = Lookup::template push_front<decltype(params.result)>;
        guard(body, parse_expression<params.tstring_tail(S), new_scope>()) {
            return parse_result{fn_expr{params.result, body.result}, body.tail};
        }
    }
}

}  // namespace smstr::dsl::detail
