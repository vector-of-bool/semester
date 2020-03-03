#pragma once

#include <semester/data.hpp>

#include <memory>

namespace semester {

template <typename Data>
struct s_expr_traits_base {
    using string_type = std::string;

    class pair_type {
        std::unique_ptr<Data> _left;
        std::unique_ptr<Data> _right;

    public:
        template <typename LeftArg, typename RightArg>
        pair_type(LeftArg&& left, RightArg&& right)
            : _left(std::make_unique<Data>(std::forward<LeftArg>(left)))
            , _right(std::make_unique<Data>(std::forward<RightArg>(right))) {}

        pair_type(const pair_type& other)
            : pair_type(*other._left, *other._right) {}

        pair_type(pair_type&&) = default;

        pair_type& operator=(const pair_type& other) {
            *_left  = other.left();
            *_right = other.right();
            return *this;
        }

        Data&       left() noexcept { return *_left; }
        Data&       right() noexcept { return *_right; }
        const Data& left() const noexcept { return *_left; }
        const Data& right() const noexcept { return *_right; }

        friend constexpr bool operator==(const pair_type& left, const pair_type& right) noexcept {
            return left.left() == right.left() && left.right() == right.right();
        }

        friend constexpr bool operator!=(const pair_type& left, const pair_type& right) noexcept {
            return !(left == right);
        }
    };

    using variant_type = std::variant<string_type, pair_type>;

    string_type convert(const char* s) { return s; }
};

struct s_expr_traits {
    template <typename Data>
    using traits = s_expr_traits_base<Data>;
};

using s_expr_data = basic_data<s_expr_traits>;

}  // namespace semester
