#pragma once

#include <cstdint>

#include <algorithm>
#include <string_view>

namespace db::details {

/// @brief A compile-time string class with fixed maximum size.
///
/// This class provides a way to store and manipulate strings at compile time.
/// It is similar to `std::string_view`, but it owns the underlying character
/// array, allowing it to be used in constexpr contexts.
///
/// The maximum size of the string is determined by the template parameter `N`.
/// If you attempt to store a string longer than `N` (including the null
/// terminator), the behavior is undefined.
template <std::size_t N>
struct static_string {
    /// @brief Create empty string.
    constexpr static_string() noexcept = default;

    /// @brief Construct from a string literal.
    /// @param str The string literal to copy from.
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr static_string(const char (&str)[N]) noexcept : len(N - 1) {
        std::copy_n(str, N, value);
    }

    /// @brief Convert string to a std::string_view.
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr operator std::string_view() const noexcept { return {value, len}; }

    /// @brief Compare with a std::string_view.
    friend constexpr bool operator==(const static_string& lhs,
        std::string_view rhs) noexcept {
        return std::string_view{lhs} == rhs;
    }
    /// @brief Compare with a std::string_view.
    friend constexpr bool operator==(std::string_view rhs,
        const static_string& lhs) noexcept {
        return lhs == std::string_view{rhs};
    }

    /// @brief Append a std::string_view to the string.
    /// @param str The string_view to append.
    constexpr auto operator+=(std::string_view str) noexcept {
        // assert(str.size() + len < N);

        std::copy_n(str.data(), str.size(), value + len);
        len += str.size();
        return *this;
    }

    /// @brief Get string size.
    [[nodiscard]] constexpr std::size_t size() const noexcept { return len; }

    /// @brief Is the string empty?
    [[nodiscard]] constexpr bool empty() const noexcept { return len == 0; }

    /// @brief Get pointer to data.
    [[nodiscard]] constexpr const char* data() const noexcept { return value; }

    std::size_t len{};
    char value[N]{};
};

}  // namespace db::details

/// @brief Concatenate two static_string objects.
/// @param first The first static_string.
/// @param second The second static_string.
/// @return A new static_string containing the concatenation of the two input strings.
template <std::size_t NF, std::size_t NS>
constexpr auto operator+(const db::details::static_string<NF>& first, const db::details::static_string<NS>& second) noexcept {
    db::details::static_string<NF + NS> res;
    res += first;
    res += second;
    return res;
}
