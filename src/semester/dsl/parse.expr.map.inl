namespace smstr::dsl::detail {

template <auto S, auto Lookup>
cxauto parse_simple_mapping_key() {
    cxauto tok = token::lex(S);
    if cx (tok.kind == tok.ident) {
        return parse_result{lit_string<SM_STR(tok.spelling)>{}, tok.tail};
    } else if cx (tok.kind == tok.string) {
        cxauto key = realize_string_literal<tok.tstring_tok(S)>();
        return parse_result{lit_string<SM_STR(key)>{}, tok.tail};
    } else {
        return undefined_t{};
    }
}

template <auto S, auto Lookup = 0>
cxauto parse_mapping_key() {
    cxauto simple = parse_simple_mapping_key<S, Lookup>();
    if cx (simple != undefined_t{}) {
        return simple;
    } else {
        cxauto tok = token::lex(S);
        if cx (tok.spelling == "...") {
            return parse_result{anything{}, tok.tail};
        } else {
            return error<"Expected a mapping key", tok.tstring_tok(S)>();
        }
    }
}

template <auto S, typename... Types>
cxauto parse_mapping_type_next(neo::tag<Types...>) {
    cxauto peek = token::lex(S);
    if cx (peek.spelling == "}") {
        return parse_result{mapping<Types...>{}, peek.tail};
    } else {
        cxauto key = parse_mapping_key<S>();
        if cx (key.is_error) {
            return key;
        } else {
            cxauto colon = token::lex(key.tail);
            if cx (colon.spelling != ":") {
                return error<"Expected a colon ':' following mapping key definition",
                             colon.tstring_tok(S)>();
            } else {
                static_assert(colon.spelling == ":", "Expected a colon ':' following mapping key");
                cxauto val = parse_type_definition<tailof(colon)>();
                if cx (val.is_error) {
                    return val;
                } else {
                    using parsed_kv = kv_pair<decltype(key.result), decltype(val.result)>;
                    cxauto next     = token::lex(val.tail);
                    if cx (next.spelling == ",") {
                        return parse_mapping_type_next<tailof(next)>(
                            neo::tag_v<Types..., parsed_kv>);
                    } else if cx (next.spelling == "}") {
                        return parse_result{mapping<Types..., parsed_kv>{}, next.tail};
                    } else {
                        return error<
                            "Expected a comma `,` or closing brace `}` following mapping value "
                            "type entry",
                            next.tstring_tok(S)>();
                    }
                }
            }
        }
    }
}

template <auto S>
cxauto parse_mapping_type() {
    cxauto tok = token::lex(S);
    static_assert(tok.spelling == "{");

    return parse_mapping_type_next<tailof(tok)>(neo::tag_v<>);
}

}  // namespace smstr::dsl::detail
