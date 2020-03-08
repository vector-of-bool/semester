#pragma once

#include <neo/concepts.hpp>
#include <neo/fwd.hpp>

#include <cassert>
#include <functional>
#include <optional>
#include <tuple>
#include <variant>

namespace semester {

template <typename T>
concept supports_arrays = requires {
    typename T::array_type;
};

template <typename T>
concept supports_mappings = requires {
    typename T::mapping_type;
};

struct dc_reject_t {
    std::string message;
};
inline struct dc_accept_t {
} dc_accept;
inline struct dc_pass_t {
} dc_pass;

using dc_result_t = std::variant<dc_reject_t, dc_accept_t, dc_pass_t>;

namespace detail {

struct nocopy {
    nocopy()              = default;
    nocopy(const nocopy&) = delete;
    nocopy& operator=(const nocopy&) = delete;
};

}  // namespace detail

inline namespace decompose_ops {

/**
 * Given a sequence of visitors, try each visitor on the data until one of them
 * either accepts or rejects.
 */
template <typename... Handlers>
class try_seq : detail::nocopy {
    std::tuple<Handlers&...> _hs;

    static_assert((std::is_reference_v<Handlers> && ...),
                  "All class template arguments to try_seq must be references");

    template <typename Data>
    dc_result_t _try_next(const Data&) const noexcept {
        return dc_pass;
    }

    template <typename Data, typename Head, typename... Tail>
    requires neo::invocable<Head, Data>
        dc_result_t _try_next(Data&& dat, Head& h, Tail&... tail) const {
        dc_result_t cur_res = std::invoke(h, dat);
        if (std::holds_alternative<dc_pass_t>(cur_res)) {
            return _try_next(NEO_FWD(dat), tail...);
        }
        return cur_res;
    }

public:
    explicit try_seq(Handlers&&... h) noexcept
        : _hs(std::tie(h...)) {}

    template <typename Data>
    constexpr static bool nothrow_for = (std::is_nothrow_invocable_v<Handlers, Data> && ...);

    template <typename Data>
    dc_result_t invoke(Data&& dat) const noexcept(nothrow_for<Data>) {
        static_assert((std::is_invocable_v<Handlers, Data> && ...));
        return std::apply([&](auto&... hs) { return _try_next(NEO_FWD(dat), hs...); }, _hs);
    }

    template <typename Data>
    dc_result_t operator()(Data&& dat) const noexcept(nothrow_for<Data>) {
        return invoke(dat);
    }
};

template <typename... Ts>
try_seq(Ts&&...) -> try_seq<Ts&&...>;

template <typename... Funcs>
class if_key : try_seq<Funcs...> {
    std::string_view _key;

public:
    explicit if_key(std::string_view k, Funcs&&... fns) noexcept
        : if_key::try_seq(NEO_FWD(fns)...)
        , _key(k) {}

    template <typename Key, typename Data>
    dc_result_t operator()(const Key& k, const Data& dat) const
        noexcept(if_key::template nothrow_for<Data>) {
        if (_key == k) {
            return this->invoke(dat);
        }
        return dc_pass;
    }
};

template <typename... Fs>
if_key(std::string_view, Fs&&...) -> if_key<Fs&&...>;

template <typename Type, typename... Funcs>
class if_type_fn : try_seq<Funcs...> {
public:
    using if_type_fn::try_seq::try_seq;

    template <typename Data>
    dc_result_t operator()(const Data& dat) const noexcept(if_type_fn::template nothrow_for<Data>) {
        using std::holds_alternative;
        if (holds_alternative<Type>(dat)) {
            return this->invoke(dat);
        }
        return dc_pass;
    }
};

template <typename Type, typename... Funcs>
auto if_type(Funcs&&... fns) noexcept {
    return if_type_fn<Type, Funcs&&...>(NEO_FWD(fns)...);
}

template <typename T>
struct require_type {
    std::string_view message;

    explicit require_type(std::string_view m) noexcept
        : message(m) {}

    template <typename Data>
    dc_result_t operator()(const Data& dat) const noexcept {
        using std::holds_alternative;
        if (holds_alternative<T>(dat)) {
            return dc_pass;
        }
        return dc_reject_t{std::string(message)};
    }
};

/**
 * Decompose predicate to accept a mapping data value.
 */
template <typename... Funcs>
class if_mapping : try_seq<Funcs...> {
public:
    using if_mapping::try_seq::try_seq;

    template <supports_mappings Data>
    dc_result_t operator()(const Data& dat) const noexcept(if_mapping::template nothrow_for<Data>) {
        using std::holds_alternative;
        if (holds_alternative<typename Data::mapping_type>(dat)) {
            return this->invoke(dat);
        }
        return dc_pass;
    }
};

template <typename... Fs>
if_mapping(Fs&&...) -> if_mapping<Fs&&...>;

/**
 * Decompose predicate to accept an array data value
 */
template <typename... Funcs>
class if_array : try_seq<Funcs...> {
public:
    using if_array::try_seq::try_seq;

    template <supports_arrays Data>
    dc_result_t operator()(const Data& dat) const noexcept(if_array::template nothrow_for<Data>) {
        using std::holds_alternative;
        if (holds_alternative<typename Data::array_type>(dat)) {
            return this->invoke(dat);
        }
        return dc_pass;
    }
};

template <typename... Fs>
if_array(Fs&&...) -> if_array<Fs&&...>;

template <typename... Funcs>
class for_each : try_seq<Funcs...> {
    template <typename Arr>
    static decltype(auto) _first_elem(Arr&& arr) noexcept {
        for (decltype(auto) elem : arr) {
            return elem;
        }
    }

    template <typename Data>
    constexpr static bool _noexcept = noexcept(
        std::declval<for_each&>().invoke(std::declval<typename Data::array_type::value_type>()));

public:
    using for_each::try_seq::try_seq;

    template <typename Data>
    requires supports_arrays<std::decay_t<Data>> dc_result_t operator()(Data&& dat) const
        noexcept(_noexcept<std::decay_t<Data>>) {
        using std::get;
        using std::holds_alternative;
        using array_type = typename std::decay_t<Data>::array_type;
        if (!holds_alternative<array_type>(dat)) {
            return dc_reject_t{"Expected an array"};
        }
        decltype(auto) arr = get<array_type>(NEO_FWD(dat));
        for (decltype(auto) element : arr) {
            dc_result_t interim = this->invoke(element);
            if (holds_alternative<dc_reject_t>(interim)) {
                return interim;
            }
        }
        return dc_accept;
    }
};

template <typename... Fs>
for_each(Fs&&...) -> for_each<Fs&&...>;

/**
 * `mapping` will decompose a mapping object based on its keys.
 */
template <typename... Keys>
class mapping : detail::nocopy {
    std::tuple<Keys&...> _decomp;

    template <typename Key, typename Data>
    dc_result_t _try_key(const Key&, const Data&) const noexcept {
        // No key matched. Ignore it.
        return dc_pass;
    }

    template <typename Key, typename Data, typename Head, typename... Tail>
    dc_result_t _try_key(const Key& k, Data&& dat, Head& h, Tail&... tail) const  //
        noexcept(                                                                 //
            std::is_nothrow_invocable_v<Head, Data>&&                             //
            noexcept(_try_key(k, NEO_FWD(dat), tail...))                          //
        ) {
        dc_result_t key_res = std::invoke(h, k, NEO_FWD(dat));
        if (std::holds_alternative<dc_pass_t>(key_res)) {
            return _try_key(k, NEO_FWD(dat), tail...);
        }
        return key_res;
    }

    template <typename Key, typename Value, typename Handler>
    constexpr static bool _noexcept_for
        = noexcept(std::declval<Handler&>()(std::declval<Key>(), std::declval<Value>()));

    template <typename Data>
    constexpr static bool _all_noexcept = (_noexcept_for<typename Data::mapping_type::key_type,
                                                         typename Data::mapping_type::mapped_type,
                                                         Keys> && ...);

public:
    explicit mapping(Keys&&... ks) noexcept
        : _decomp(std::tie(ks...)) {}

    template <typename Data>
    requires supports_mappings<std::decay_t<Data>> dc_result_t operator()(Data&& dat) const
        noexcept(_all_noexcept<std::decay_t<Data>>) {
        using mapping_type = typename std::decay_t<Data>::mapping_type;
        using std::get;
        using std::holds_alternative;
        if (!holds_alternative<mapping_type>(dat)) {
            return dc_reject_t{"Data is not a mapping"};
        }
        const mapping_type& map = get<mapping_type>(NEO_FWD(dat));
        for (const auto& [key, value] : map) {
            dc_result_t part_result
                = std::apply([&](auto&&... key_fn) { return _try_key(key, value, key_fn...); },
                             _decomp);
            if (holds_alternative<dc_reject_t>(part_result)) {
                return part_result;
            }
        }
        return dc_accept;
    }
};

template <typename... Args>
mapping(Args&&...) -> mapping<Args&&...>;

/**
 * `put_into` will place the decomposed value into a destination object. The
 * destination object must be of a type (or optional thereof) that is directly
 * supported by the data begin decomposed.
 */
template <typename T>
class put_into : detail::nocopy {
    T& _target;

    template <typename Type, typename Data, typename Target>
    dc_result_t _try_put_1(Data&& dat, Target& t) const
        noexcept(noexcept(t = std::declval<Type>())) {
        static_assert(std::decay_t<Data>::template supports<Type>,
                      "The destination of a put_into() is not of a "
                      "type supported by the decomposee data");
        using std::get;
        using std::holds_alternative;
        if (holds_alternative<Type>(dat)) {
            t = get<Type>(NEO_FWD(dat));
            return dc_accept;
        } else {
            return dc_reject_t{"No valid conversion from the destination type"};
        }
    }

    template <typename Data, typename Target>
    dc_result_t _try_put(Data&& dat, Target& t) const
        noexcept(noexcept(_try_put_1<Target>(NEO_FWD(dat), t))) {
        return _try_put_1<Target>(NEO_FWD(dat), t);
    }

    template <typename Data, typename Target>
    dc_result_t _try_put(Data&& dat, std::optional<Target>& opt_t) const
        noexcept(noexcept(_try_put_1<Target>(NEO_FWD(dat), opt_t))) {
        return _try_put_1<Target>(NEO_FWD(dat), opt_t);
    }

public:
    explicit put_into(T& r) noexcept
        : _target(r) {}

    template <typename Data>
    dc_result_t operator()(Data&& dat) const noexcept(noexcept(_try_put(NEO_FWD(dat), _target))) {
        return _try_put(NEO_FWD(dat), _target);
    }
};

template <typename T>
put_into(T) -> put_into<T>;

/**
 * A handler that assigns items through an output iterator
 */
template <typename Iterator>
class write_to {
    Iterator _iter;

    template <typename Iter, typename = void>
    struct _get_value_type {
        using type = typename std::iterator_traits<Iter>::value_type;
    };

    template <typename Iter>
    struct _get_value_type<Iter, std::void_t<typename Iter::container_type::value_type>> {
        using type = typename Iter::container_type::value_type;
    };

public:
    write_to(Iterator it) noexcept(std::is_nothrow_move_constructible_v<Iterator>)
        : _iter(std::move(it)) {}

    template <typename Data>
    dc_result_t operator()(Data&& dat) noexcept(                                        //
        noexcept(*_iter = std::declval<typename _get_value_type<Iterator>::type>()) &&  //
        noexcept(++_iter)                                                               //
    ) {
        using value_type = typename _get_value_type<Iterator>::type;
        using std::get;
        using std::holds_alternative;
        if (!holds_alternative<value_type>(dat)) {
            return dc_reject_t{"Data has incorrect type"};
        }
        *_iter = get<value_type>(NEO_FWD(dat));
        ++_iter;
        return dc_accept;
    }
};

/**
 * A handler that will reject its input and return a specific message
 */
struct reject_with : detail::nocopy {
    std::string_view message;

    explicit reject_with(std::string_view s) noexcept
        : message(s) {}

    template <typename Data>
    dc_result_t operator()(const Data&) const noexcept {
        return dc_reject_t{std::string(message)};
    }
};

/**
 * A handler that simply accepts the input without doing anything with it
 */
inline constexpr struct just_accept_fn : detail::nocopy {
    template <typename Data>
    constexpr dc_result_t operator()(const Data&) const noexcept {
        return dc_accept;
    }
} just_accept;

}  // namespace decompose_ops

/**
 * Decompose a data structure using the given decomposition visitor
 */
template <typename Data, typename Visitor>
requires neo::invocable<Visitor, Data> constexpr decltype(auto)
decompose(Data&& dat, Visitor&& vis) noexcept(noexcept(std::invoke(NEO_FWD(vis), NEO_FWD(dat)))) {
    return std::invoke(NEO_FWD(vis), NEO_FWD(dat));
}

}  // namespace semester
