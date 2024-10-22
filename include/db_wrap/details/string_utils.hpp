#pragma once

#include <cstdint>

#include <algorithm>

namespace db::utils {

/// @brief Converts a 32-bit integer to a null-terminated string.
///
/// This function converts the given integer `in_num` to its string
/// representation in base-10 and stores it in the provided character
/// buffer `out_str`. The buffer must be large enough to hold the
/// resulting string, including the null terminator.
///
/// If the input number is less than or equal to 0, the function
/// will store "0" in the buffer.
///
/// @param in_num The 32-bit integer to convert.
/// @param out_str The character buffer to store the resulting string.
constexpr void itoa_d(const std::int32_t in_num, char* out_str) noexcept {
    // in case input is less or equal to zero -> return zero
    if (in_num <= 0) {
        out_str[0] = '0';
        out_str[1] = '\0';
        return;
    }

    // Process individual digits
    std::int32_t i = 0;
    for (std::int32_t num = in_num; num != 0; num /= 10, ++i) {
        const std::int32_t rem = num % 10;
        out_str[i]             = static_cast<char>((rem > 9) ? (rem - 10) + 'a' : rem + '0');
    }

    // Append string terminator
    out_str[i] = '\0';

    // Reverse the string
    std::reverse(out_str, out_str + i);
}

}  // namespace db::utils
