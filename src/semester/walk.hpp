#pragma once

#include "./data.hpp"

#include "./get.hpp"
#include "./traits.hpp"

#include <neo/concepts.hpp>
#include <neo/fwd.hpp>
#include <neo/iterator_concepts.hpp>

#include <stdexcept>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

namespace smstr {

// clang-format off

namespace detail {

inline thread_local std::string walk_path;

template <typename Func, typename = void>
struct inspect_visitor {};

template <typename Func>
struct inspect_visitor<Func, std::void_t<decltype(&Func::operator())>>
    : inspect_visitor<decltype(&Func::operator())> {};

template <typename>
constexpr bool vst_accepts_one_arg_v = false;

template <typename Owner, typename Ret, typename Arg>
constexpr bool vst_accepts_one_arg_v<Ret(Owner::*)(Arg)> = true;

template <typename Owner, typename Ret, typename Arg>
constexpr bool vst_accepts_one_arg_v<Ret(Owner::*)(Arg) const> = true;

template <typename MemPtr>
    requires (
        std::is_member_function_pointer_v<MemPtr> &&
        vst_accepts_one_arg_v<MemPtr>
    )
struct inspect_visitor<MemPtr, void> {
    template <typename Arg, typename Owner, typename Ret>
    static Arg _get(Ret(Owner::*)(Arg) const);

    template <typename Arg, typename Owner, typename Ret>
    static Arg _get(Ret(Owner::*)(Arg));

    using arg_type = decltype(_get(MemPtr()));
};

template <typename Ret, typename Arg>
struct inspect_visitor<Ret(*)(Arg)> {
    using arg_type = Arg;
};

template <typename T>
using vst_arg_t = std::remove_cvref_t<typename inspect_visitor<std::remove_cvref_t<T>>::arg_type>;

template <typename T>
concept vst_not_generic = requires { typename vst_arg_t<T>; };

template <typename T>
concept vst_is_generic = !vst_not_generic<T>;

template <typename Func, typename Data>
concept vst_invocable_for =
    neo::invocable<Func, Data> ||
    requires (Data dat) {
        &std::remove_cvref_t<Func>::operator();
    };

struct ident_project {
    template <typename T>
    constexpr T&& operator()(T&& t) const noexcept {
        return NEO_FWD(t);
    }
};

}  // namespace detail

struct walk_error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct walk_reject {
    std::string message;
};
inline constexpr struct walk_accept_t {
} walk_accept;
inline constexpr struct walk_pass_t {
} walk_pass;

class walk_result {
    using variant_type = std::variant<walk_pass_t, walk_reject, walk_accept_t>;
    variant_type _var;

public:
    template <neo::convertible_to<variant_type> T>
    walk_result(T&& t) : _var(NEO_FWD(t)) {}

    bool operator==(walk_accept_t) const noexcept {
        return std::holds_alternative<walk_accept_t>(_var);
    }

    bool operator==(walk_pass_t) const noexcept {
        return std::holds_alternative<walk_pass_t>(_var);
    }

    bool rejected() const noexcept {
        return std::holds_alternative<walk_reject>(_var);
    }

    auto& rejection() const noexcept {
        return std::get<walk_reject>(_var);
    }

    template <typename E = walk_error>
    void throw_if_rejected() const {
        if (rejected()) {
            throw E(rejection().message);
        }
    }
};

inline namespace walk_ops {

/**
 * `walk_seq` is the basis for many operations. It takes an ordered list of visitors,
 * and attempts to pass the data to each. If a visitor has a definite argument type,
 * then the visitor will only be matched if the data currently contains that type.
 * If a visitor is generic, then the visitor will always be applied.
 *
 * If a visitor returns a walk_reject or walk_accept_t, then the walk_seq does
 * not continue and returns that result.
 *
 * If a visitor returns a walk_pass, then we continue down through the list.
 *
 * If no visitor either accepts or rejects the data, then we throw an exception.
 */
template <typename... Handlers>
class walk_seq {
    std::tuple<Handlers...> _hs;

    /**
     * Base case: No visitors matched, so we will throw.
     */
    template <typename Data>
    walk_result _try_next(const Data&) const {
        throw walk_error(detail::walk_path + ": No matching handler in walk_seq<> data visitor");
    }

    /**
     * Recursive case. Attempt to invoke `c` with `dat`.
     */
    template <typename Data, typename Cand, typename... Tail>
    walk_result _try_next(Data&& dat, Cand&& c, Tail&&... tail) {
        if constexpr (bool(neo::invocable<Cand, Data>)) {
            // The visitor accepts the data object directly without conversion
            static_assert(
                neo::convertible_to<std::invoke_result_t<Cand, Data>, walk_result>,
                "Handler must return a walk_result");
            auto result = std::invoke(c, NEO_FWD(dat));
            if (result == walk_pass) {
                // If the visitor passes, continue on
                return _try_next(NEO_FWD(dat), tail...);
            }
            return result;
        } else if constexpr (bool(detail::vst_not_generic<Cand>)) {
            // The visitor expects one exact type.
            using arg_type = detail::vst_arg_t<Cand>;
            using std::get_if;
            static_assert(
                neo::convertible_to<std::invoke_result_t<Cand, arg_type>, walk_result>,
                "Handler must return a walk_result");
            // Get a pointer to the contained value, if that is currently valid:
            auto val_ref = try_get<arg_type>(dat);
            if (val_ref) {
                // The data is holding the correct type. Invoke the visitor:
                walk_result result = std::invoke(c, *val_ref);
                if (result == walk_pass) {
                    // If the visitor passes, continue on
                    return _try_next(NEO_FWD(dat), tail...);
                }
                // We're done. Return the immediate result.
                return result;
            }
            // The data does not hold the type that this visitor wants. Pass on.
            return _try_next(NEO_FWD(dat), tail...);
        } else {
            static_assert(std::is_void_v<Cand>, "walk_seq handler cannot accept any argument type");
        }
    }

public:
    constexpr explicit walk_seq(Handlers&&... h) noexcept
        : _hs(NEO_FWD(h)...) {}

    template <typename Data>
        requires ((detail::vst_invocable_for<Handlers, Data> || ...))
    walk_result invoke(Data&& dat) {
        return std::apply([&](auto&&... hs) {
            return _try_next(NEO_FWD(dat), hs...); },
        _hs);
    }

    template <typename Data>
        requires ((detail::vst_invocable_for<Handlers, Data> || ...))
    walk_result operator()(Data&& dat) {
        return invoke(NEO_FWD(dat));
    }
};

template <typename... Hs>
walk_seq(Hs&&...) -> walk_seq<Hs...>;

// clang-format on

/**
 * put_into will insert the value from the given object through a reference.
 * Optionally, provide a projection function. The projection function can
 * accept a typed param, which will result in a cast before the projection
 * is invoked.
 */
template <typename Target, typename Project = std::monostate>
struct put_into {
    Target  _target;
    Project _project;

    template <typename Iter, typename = void>
    struct _get_value_type {
        using type = neo::iter_value_t<Iter>;
    };

    template <typename Iter>
    struct _get_value_type<Iter, std::void_t<typename Iter::container_type::value_type>> {
        using type = Iter::container_type::value_type;
    };

    template <typename Dest, typename Value>
    walk_result _put(Dest& into, Value&& val) const {
        using std::get_if;
        if constexpr (neo::assignable_from<Dest&, Value>) {
            into = NEO_FWD(val);
        } else if constexpr (supports_alternative<Value, Dest>) {
            auto ref = try_get<Dest>(val);
            if (!ref) {
                throw walk_error(detail::walk_path + ": Incorrect type to put-into a value");
            }
            into = *ref;
        } else {
            static_assert(std::is_void_v<Dest>,
                          "put_into cannot possibly receive the given data type");
        }
        return walk_accept;
    }

    // XXX: Should use indirectly_readable/writable with proper C++20 support.
    // GCC requires this 1337 h4ck
    template <typename Dest, typename Value>
    requires requires(Dest d) {
        *d = *d;
    }
    walk_result _put(Dest& into, Value&& val) const {
        using dest_type = _get_value_type<Dest>::type;
        if constexpr (neo::assignable_from<Dest&, Value>) {
            into = NEO_FWD(val);
        } else if constexpr (neo::assignable_from<Dest&, dest_type>) {
            auto ref = try_get<dest_type>(val);
            if (!ref) {
                throw walk_error("Incorrect type to put-into a value");
            }
            into = *ref;
        } else if constexpr (neo::assignable_from<dest_type&, Value>) {
            *into = NEO_FWD(val);
        } else {
            auto ref = try_get<dest_type>(val);
            if (!ref) {
                throw walk_error("Incorrect type to put-into a value");
            }
            *into = *ref;
        }
        return walk_accept;
    }

public:
    explicit put_into(Target&& t)
        : _target(NEO_FWD(t)) {}
    explicit put_into(Target&& t, Project&& pr)
        : _target(NEO_FWD(t))
        , _project(NEO_FWD(pr)) {}

    template <typename Data>
    walk_result operator()(Data&& dat) {
        if constexpr (neo::same_as<Project, std::monostate>) {
            return _put(_target, NEO_FWD(dat));
        } else if constexpr (detail::vst_is_generic<Project>) {
            auto&& proj = _project(NEO_FWD(dat));
            return _put(_target, NEO_FWD(proj));
        } else {
            using arg_type        = detail::vst_arg_t<Project>;
            constexpr bool inv_ok = neo::invocable<Project, Data>;
            if constexpr (inv_ok) {
                auto&& proj = _project(NEO_FWD(dat));
                return _put(_target, NEO_FWD(proj));
            } else {
                static_assert(supports_alternative<Data, arg_type>,
                              "projection function cannot handle the argument we wish to give it");
                auto&& proj = _project(smstr::get<arg_type>(NEO_FWD(dat)));
                return _put(_target, NEO_FWD(proj));
            }
        }
    }
};

template <typename T>
put_into(T&& t) -> put_into<T>;

template <typename T, typename Func>
put_into(T&& t, Func&& fn) -> put_into<T, Func>;

template <typename T, typename Proj = std::monostate>
struct put_into_pass : put_into<T, Proj> {
    using put_into_pass::put_into::put_into;

    template <typename Data>
    constexpr walk_result operator()(Data&& dat) {
        auto result = put_into_pass::put_into::operator()(NEO_FWD(dat));
        if (result.rejected()) {
            return result;
        }
        return walk_pass;
    }
};

template <typename T>
put_into_pass(T&& t) -> put_into_pass<T>;

template <typename T, typename Func>
put_into_pass(T&& t, Func&& fn) -> put_into_pass<T, Func>;

template <typename... KeyFuncs>
class mapping {
    std::tuple<KeyFuncs...> _funcs;

    template <typename F>
    void _forget(F&&) {}

    template <typename F>
    void _forget(F&& fn) requires requires {
        fn.forget();
    }
    { fn.forget(); }

    void _forget() {
        std::apply([&](auto&&... fns) { (_forget(fns), ...); }, _funcs);
    }

    template <typename F>
    auto _check_unsatisfied(F&&) {
        return std::nullopt;
    }

    template <typename Fn>
    std::optional<std::string> _check_unsatisfied(Fn&& fn) requires requires {
        fn.unsat_message();
    }
    { return fn.unsat_message(); }

    auto _find_unsatisfied_key_1() { return std::nullopt; }

    template <typename Fn, typename... Tail>
    std::optional<std::string> _find_unsatisfied_key_1(Fn&& fn, Tail&&... tail) {
        std::optional<std::string> unsat = _check_unsatisfied(fn);
        if (unsat) {
            return unsat;
        }
        return _find_unsatisfied_key_1(tail...);
    }

    std::optional<std::string> _find_unsatisfied_key() {
        return std::apply([&](auto&&... fns) { return _find_unsatisfied_key_1(fns...); }, _funcs);
    }

    template <typename Key, typename Data>
    walk_result _try_key(const Key& k, const Data&) const {
        // No key matched. Ignore it.
        return walk_reject{detail::walk_path + ": Unhandled key: " + std::string(k)};
    }

    template <typename Key, typename Data, typename Head, typename... Tail>
    walk_result _try_key(const Key& k, Data&& dat, Head&& h, Tail&&... tail) const {
        auto prev_path = detail::walk_path;
        detail::walk_path += "/" + std::string(k);
        walk_result key_res = std::invoke(h, k, NEO_FWD(dat));
        detail::walk_path   = prev_path;
        if (key_res == walk_pass) {
            return _try_key(k, NEO_FWD(dat), tail...);
        }
        return key_res;
    }

public:
    explicit mapping(KeyFuncs&&... ks) noexcept
        : _funcs(NEO_FWD(ks)...) {}

    template <supports_maps Data>
    walk_result operator()(Data&& dat) {
        _forget();
        auto&& map = as_map(dat);
        for (const auto& [key, value] : map) {
            walk_result partial_result
                = std::apply([&](auto&&... fns) { return _try_key(key, value, fns...); }, _funcs);
            if (partial_result.rejected()) {
                return partial_result;
            }
        }
        auto unsat_message = _find_unsatisfied_key();
        if (unsat_message) {
            return walk_reject{detail::walk_path
                               + ": Missing required key/property: " + *unsat_message};
        }
        return walk_accept;
    }
};

template <typename... Args>
mapping(Args&&...) -> mapping<Args...>;

template <typename... Funcs>
class if_key : walk_seq<Funcs...> {
    std::string _key;

public:
    explicit if_key(std::string k, Funcs&&... fns) noexcept
        : if_key::walk_seq(NEO_FWD(fns)...)
        , _key(std::move(k)) {}

    template <typename Key, typename Data>
    walk_result operator()(Key&& k, Data&& dat) {
        if (_key == k) {
            return this->invoke(NEO_FWD(dat));
        }
        return walk_pass;
    }
};

template <typename... Fs>
if_key(std::string, Fs&&...) -> if_key<Fs...>;

template <typename... Funcs>
class required_key : walk_seq<Funcs...> {
    std::string _key;
    std::string _message;
    bool        _satisfied = false;

public:
    explicit required_key(std::string k, std::string message, Funcs&&... fns) noexcept
        : required_key::walk_seq(NEO_FWD(fns)...)
        , _key(std::move(k))
        , _message(std::move(message)) {}

    void forget() { _satisfied = false; }

    std::optional<std::string> unsat_message() const noexcept {
        return _satisfied ? std::nullopt : std::make_optional(_message);
    }

    template <typename Key, typename Data>
    walk_result operator()(Key&& k, Data&& dat) {
        if (_key == k) {
            _satisfied = true;
            return this->invoke(NEO_FWD(dat));
        }
        return walk_pass;
    }
};

template <typename... Fs>
required_key(std::string, std::string, Fs&&...) -> required_key<Fs...>;

template <typename Type, typename... Fns>
class if_type_fn : walk_seq<Fns...> {
public:
    using if_type_fn::walk_seq::walk_seq;

    template <typename Data>
    walk_result operator()(Data&& dat) {
        if (smstr::holds_alternative<Type>(dat)) {
            return this->invoke(NEO_FWD(dat));
        }
        return walk_pass;
    }
};

template <typename Type, typename... Funcs>
auto if_type(Funcs&&... fns) noexcept {
    return if_type_fn<Type, Funcs...>(NEO_FWD(fns)...);
}

template <typename T>
struct require_type {
    std::string message;

    explicit require_type(std::string m) noexcept
        : message(std::move(m)) {}

    template <typename Data>
    walk_result operator()(Data&& dat) const noexcept {
        if (smstr::holds_alternative<T>(dat)) {
            return walk_pass;
        }
        return walk_reject{detail::walk_path + ": " + message};
    }
};

constexpr inline auto just_accept = [](auto&&...) -> walk_result { return walk_accept; };

constexpr inline auto reject_with = [](std::string message) {
    return [message](auto&&...) -> walk_result {
        return walk_reject{detail::walk_path + ": " + message};
    };
};

template <typename... Fns>
struct if_mapping : walk_seq<Fns...> {
    using if_mapping::walk_seq::walk_seq;

    walk_result operator()(supports_maps auto&& dat) {
        if (is_map(dat)) {
            return this->invoke(NEO_FWD(dat));
        }
        return walk_pass;
    }
};

template <typename... Fs>
if_mapping(Fs&&...) -> if_mapping<Fs...>;

template <typename... Fns>
struct if_array : walk_seq<Fns...> {
    using if_array::walk_seq::walk_seq;

    walk_result operator()(supports_arrays auto&& dat) {
        if (is_array(dat)) {
            return this->invoke(NEO_FWD(dat));
        }
        return walk_pass;
    }
};

template <typename... Fs>
if_array(Fs&&...) -> if_array<Fs...>;

template <typename... Funcs>
class for_each : walk_seq<Funcs...> {
public:
    using for_each::walk_seq::walk_seq;

    walk_result operator()(supports_arrays auto&& dat) {
        if (!is_array(dat)) {
            return walk_reject{detail::walk_path + ": Expected an array"};
        }
        auto&& arr   = as_array(dat);
        int    index = 0;
        for (decltype(auto) element : arr) {
            auto prev_path = detail::walk_path;
            detail::walk_path += "[" + std::to_string(index) + "]";
            walk_result interim = this->invoke(element);
            detail::walk_path   = prev_path;
            if (interim.rejected()) {
                return interim;
            }
            ++index;
        }
        return walk_accept;
    }
};

template <typename... Fs>
for_each(Fs&&...) -> for_each<Fs...>;

}  // namespace walk_ops

inline constexpr struct walk_fn {
    static inline walk_result accept = walk_accept;
    static inline walk_result pass   = walk_pass;

    static walk_result reject(std::string str) { return walk_reject{NEO_FWD(str)}; }
    template <typename Data, typename... Handlers>
    [[nodiscard]] constexpr decltype(auto) try_walk(Data&& dat, Handlers&&... hs) const {
        if (detail::walk_path.empty()) {
            detail::walk_path = "<root>";
        }
        walk_seq seq(NEO_FWD(hs)...);
        return seq(NEO_FWD(dat));
    }

    template <typename Data, typename... Handlers>
    constexpr decltype(auto) operator()(Data&& dat, Handlers&&... hs) const {
        auto res = try_walk(NEO_FWD(dat), NEO_FWD(hs)...);
        res.throw_if_rejected();
    }

    std::string_view path() const noexcept { return detail::walk_path; }
} walk;

namespace walk_ops {
using smstr::walk;
}

}  // namespace smstr