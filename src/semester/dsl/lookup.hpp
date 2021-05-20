#pragma once

#include <algorithm>

namespace smstr {

template <typename Param, int Scope = -1, int Idx = -1>
struct resolved_name {
    constexpr static int scope_idx = Scope;
    constexpr static int name_idx  = Idx;
    using param_type               = Param;
};

template <typename... Variables>
struct scoped_names {
    constexpr static int count = sizeof...(Variables);

private:
    template <int N, auto Name>
    static constexpr auto _lookup() {
        return resolved_name<void>{};
    }

    template <int N, auto Name>
    static constexpr auto _lookup(auto head, auto... tail) {
        if constexpr (head.spelling == Name) {
            return resolved_name<decltype(head), -1, N>{};
        } else {
            return _lookup<N + 1, Name>(tail...);
        }
    }

    template <typename... Argv>
    constexpr static bool correct_argv_count = sizeof...(Argv) == sizeof...(Variables);

public:
    template <auto Name>
    constexpr static auto index_of() noexcept {
        return _lookup<0, Name>(Variables{}...);
    }

    static void check_compatible_argv(auto traits, auto&&... args) requires requires {
        requires correct_argv_count<decltype(args)...>;
        requires(requires { (Variables::bind_arg(traits, args)); } && ...);
    };
};

template <typename... Scopes>
struct scope_chain {
    template <int, auto>
    static constexpr auto _lookup() {
        return resolved_name<void>{};
    }

    template <int N, auto Name>
    static constexpr auto _lookup(auto head_scope, auto... tail_scopes) {
        constexpr auto found = head_scope.template index_of<Name>();
        if constexpr (found.name_idx < 0) {
            return _lookup<N + 1, Name>(tail_scopes...);
        } else {
            return resolved_name<typename decltype(found)::param_type, N, found.name_idx>{};
        }
    }

public:
    template <auto Name>
    constexpr static auto resolve() {
        return _lookup<0, Name>(Scopes{}...);
    }

    template <typename S>
    using push_front = scope_chain<S, Scopes...>;
};

}  // namespace smstr
