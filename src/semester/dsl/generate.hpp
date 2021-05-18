#pragma once

#include "../get.hpp"
#include "./lookup.hpp"
#include "./parse.hpp"

#include <neo/fixed_string.hpp>

#include <tuple>

namespace smstr::dsl {

using neo::tag;
using neo::tag_v;

namespace detail {

template <auto... Names>
struct param_list {
    constexpr static auto size = sizeof...(Names);
};

template <typename Data, int Scope, int NameIndex, typename Type>
constexpr decltype(auto) evaluate(param_ref<Scope, NameIndex, Type>, auto&& tup) {
    auto&  scope = std::get<Scope>(tup);
    auto&& arg   = NEO_FWD(std::get<NameIndex>(scope));
    return NEO_FWD(arg);
    // if constexpr (undefined<Type>) {
    //     return Data(NEO_FWD(arg));
    // } else {
    //     return Type()
    // }
}

template <typename Data>
constexpr auto evaluate(lit_string_c auto s, auto&&) {
    return typename Data::string_type(s.str.string_view());
}

template <typename Data, typename Left, typename Type>
constexpr auto evaluate(as_expr<Left, Type>, auto&& tup) {
    Data given = evaluate<Data>(Left{}, tup);
    using type = lookup_type_t<Data, Type>;
    static_assert(smstr::supports_alternative<Data, type>);
    return smstr::get<type>(given);
}

template <typename Data, typename Map, typename Key, typename Value>
constexpr void apply_generate_keys(Map& map, map_entry<Key, Value>, auto&& tup) {
    using pair_type                  = typename Map::value_type;
    auto&&         key               = evaluate<Data>(Key{}, tup);
    constexpr bool key_convert_check = std::convertible_to<decltype(key), typename Map::key_type>;
    static_assert(key_convert_check,
                  "Usage of a value-dependent dynamic expression as a key in a map literal "
                  "requires that the resulting value is convertible to the map's key type, but we "
                  "cannot prove that here. Use an `as <type>` suffix expression within the key "
                  "expression to ensure that the key is convertible to the key type of the map "
                  "(If your key type is a string, use `<expr> as string`).");
    if constexpr (key_convert_check) {
        auto&& value = evaluate<Data>(Value{}, tup);
        auto   pair  = pair_type(NEO_FWD(key), NEO_FWD(value));
        map.insert(NEO_FWD(pair));
    }
}

template <typename Data, typename... Elems>
constexpr Data evaluate(map_literal<Elems...>, auto&& tup) {
    auto map = typename Data::mapping_type{};
    (apply_generate_keys<Data>(map, Elems{}, tup), ...);
    return map;
}

// clang-format off
template <typename Data>
concept simple_basic_data_c =
    requires { typename Data::mapping_type; } ||
    requires { typename Data::array_type; } ||
    requires { typename Data::string_type; };
// clang-format on

}  // namespace detail

template <typename DataType, typename Body, typename... Args>
constexpr DataType apply_generator(Args&&... args);

template <typename Expression, typename ParamTuple>
struct evaluator {
    Expression _expr;
    ParamTuple _tup;

    template <detail::simple_basic_data_c Data>
    constexpr Data evaluate() {
        return detail::evaluate<Data>(_expr, std::tie(_tup));
    }

    template <detail::simple_basic_data_c Data>
    constexpr operator Data() && {
        return evaluate<Data>();
    }
};

template <std::size_t NParams, typename Body>
struct generator {
    template <typename... Args>
    constexpr auto operator()(Args&&... args) const noexcept requires(sizeof...(Args) == NParams) {
        return evaluator{Body{}, std::forward_as_tuple(NEO_FWD(args)...)};
    }

    template <typename Data, typename... Args>
    constexpr static Data apply(Args&&... args) requires(sizeof...(Args) == NParams) {
        return evaluator{Body{}, std::forward_as_tuple(NEO_FWD(args)...)}.template evaluate<Data>();
    }

    template <typename Data, typename... Args>
    constexpr Data operator()(neo::tag<Data>, Args&&... args) const
        requires(sizeof...(Args) == NParams) {
        return this->apply<Data>(NEO_FWD(args)...);
    }
};

}  // namespace smstr::dsl

#include "./parse.generate.inl"

namespace smstr::dsl {

namespace detail {

template <auto S>
constexpr auto parse_code() {
    constexpr auto tok = token::lex(S);
    if constexpr (tok.spelling == "generate") {
        return parse_generate_expr<S, scope_chain<>>();
    } else {
        return error<"Unknown top-level expression", tok.tstring_tok(S)>();
    }
}

}  // namespace detail

template <typename Data, typename Body, typename... Args>
constexpr Data apply_generator(Args&&... args) {
    return detail::evaluate<Data>(Body{}, std::make_tuple(std::forward_as_tuple(NEO_FWD(args)...)));
}

template <neo::basic_fixed_string S>
using code_t = decltype(detail::handle_error(detail::parse_code<neo::tstring_view_v<S>>()));

template <neo::basic_fixed_string S>
constexpr inline auto code = code_t<SM_S>{};

}  // namespace smstr::dsl
