#pragma once

#include <neo/assert.hpp>
#include <neo/fixed_string.hpp>

#include <string_view>

namespace smstr::dsl {

using std::string_view;

constexpr bool is_space(char32_t c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

constexpr bool is_alpha(char32_t c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

constexpr bool is_digit(char32_t c) noexcept { return c >= '0' && c <= '9'; }
constexpr bool is_id_start(char32_t c) noexcept { return is_alpha(c) || c == '_' || c == '$'; }
constexpr bool is_id_char(char32_t c) noexcept { return is_id_start(c) || is_digit(c); }

/**
 * @brief Strip leading whitespace and comments from the given string. Returns a new string.
 */
constexpr string_view strip_front(string_view sv) noexcept {
    while (!sv.empty() && is_space(sv.front())) {
        sv.remove_prefix(1);
    }

    if (sv.empty() || sv.front() != '/') {
        return sv;
    }

    if (sv.starts_with("//")) {
        // Eat until the next newline
        auto nl_pos = sv.find('\n');
        if (nl_pos == sv.npos) {
            return sv.substr(sv.size());
        }
        return strip_front(sv.substr(nl_pos + 1));
    }

    if (sv.starts_with("/*")) {
        // Eat until the comment-end
        auto end_pos = sv.find("*/");
        if (end_pos == sv.npos) {
            return sv.substr(sv.size());
        }
        return strip_front(sv.substr(end_pos + 2));
    }

    return sv;
}

struct token {
    enum kind_t {
        invalid,

        square_l = '[',
        square_r = ']',

        paren_l = '(',
        paren_r = ')',

        brace_l = '{',
        brace_r = '}',

        comma = ',',
        colon = ':',

        question = '?',
        star     = '*',
        pipe     = '|',
        dot      = '.',
        slash    = '/',
        minus    = '-',
        plus     = '+',

        elipse = 999,
        ident,
        number,
        string,
        eof,
    };

    kind_t      kind{};
    string_view spelling{};
    string_view tail{};

    /// Obtain the next token following this token
    constexpr token next() const noexcept { return lex(tail); }

    constexpr auto tstring_tail(auto tstr) const noexcept {
        const auto offset = tail.data() - tstr.data();
        return tstr.substr(offset, tail.length());
    }

    constexpr auto tstring_tok(auto tstr) const noexcept {
        const auto offset = spelling.data() - tstr.data();
        return tstr.substr(offset, spelling.length());
    }

    /// Create a 'kind' token from the first 'len' bytes of 'str'
    constexpr static token split(kind_t kind, string_view str, std::ptrdiff_t len) noexcept {
        return token{kind, str.substr(0, len), str.substr(len)};
    }

    constexpr bool is(char c) const noexcept {
        return spelling.size() == 1 && spelling.front() == c;
    }

    /// Lex the token at the beginning of the given string
    constexpr static token lex(string_view sv) noexcept {
        sv = strip_front(sv);
        if (sv.empty()) {
            return split(eof, sv, 0);
        }

        auto c = sv.front();
        // Tokens that consist of a single character:
        constexpr std::string_view single_char_tokens = "[]{}():?,*|/-+";
        if (auto punct_idx = single_char_tokens.find(c); punct_idx != sv.npos) {
            return split(static_cast<kind_t>(single_char_tokens[punct_idx]), sv, 1);
        }

        if (c == '.') {
            if (sv.starts_with("...")) {
                return split(elipse, sv, 3);
            } else {
                return split(dot, sv, 1);
            }
        }

        if (is_id_start(c)) {
            auto c_it = sv.begin();
            while (c_it != sv.end() && is_id_char(*c_it)) {
                ++c_it;
            }
            auto len = c_it - sv.begin();
            return split(ident, sv, len);
        }

        if (is_digit(c)) {
            auto c_it = sv.begin();
            while (c_it != sv.end() && is_digit(*c_it)) {
                ++c_it;
            }
            auto len = c_it - sv.begin();
            return split(number, sv, len);
        }

        if (c == '"' || c == '\'') {
            bool       escaped = false;
            const auto quote   = c;
            for (auto c_it = sv.begin() + 1; c_it != sv.end(); ++c_it) {
                c = *c_it;
                if (escaped) {
                    escaped = false;
                    continue;
                }
                if (c == '\\') {
                    escaped = true;
                    continue;
                }
                if (c == quote) {
                    ++c_it;
                    return split(string, sv, c_it - sv.begin());
                }
            }
            return split(invalid, sv, sv.size());
        }

        return split(invalid, sv, 1);
    }
};

}  // namespace smstr::dsl