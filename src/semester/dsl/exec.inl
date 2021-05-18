namespace smstr::dsl::detail {

cx auto do_eval(fn_expr<auto, auto> fn, auto, auto&&) { return function{fn.params, fn.body}; }

template <int Scope, int Idx, typename Type>
cx auto do_eval(param_ref<Scope, Idx, Type>, auto, auto&& ctx) {
    auto&  scope = std::get<Scope>(ctx);
    auto&& arg   = NEO_FWD(std::get<Idx>(scope));
    return NEO_FWD(arg);
}

cx auto do_eval(binop_expr<auto, auto, auto> binop, auto traits, auto&& ctx) {
    auto&& left  = do_eval(binop.left, traits, ctx);
    auto&& right = do_eval(binop.right, traits, ctx);
    return binop.op(left, right);
}

cx auto do_eval(lit_number_c auto num, auto traits, auto&&) { return traits.number(num.str); }

cx decltype(auto) evaluate_expression(auto traits, auto expr, auto&& ctx) {
    return do_eval(expr, traits, ctx);
}

}  // namespace smstr::dsl::detail
