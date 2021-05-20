#pragma once

namespace smstr {

constexpr inline struct null_t {
    constexpr null_t() = default;
    constexpr null_t(std::nullptr_t) noexcept {}

    constexpr bool operator==(null_t) const noexcept { return true; }
} null;

}  // namespace smstr
