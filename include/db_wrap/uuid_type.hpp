/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

#include <cstdint>

#include <algorithm>    // for copy_n
#include <array>        // for array
#include <string_view>  // for string_view

namespace db::uuids {

/// @brief Constant representing the length of a UUID string.
inline constexpr std::size_t UUID_LEN = 36UL;

/// @brief Type alias for representing a UUID as a fixed-size character array.
using uuid = std::array</*std::uint8_t*/ char, UUID_LEN>;

/// @brief Converts a string view to a UUID object.
///
/// This function takes a string view representing a UUID and converts it to
/// a `db::uuids::uuid` object. The input string is expected to be exactly
/// 36 characters long (including hyphens).
///
/// @param id_str The string view representing the UUID.
/// @return A `db::uuids::uuid` object containing the converted UUID.
///
/// @example
/// constexpr std::string_view uuid_str = "877dae4c-0a31-499d-9f81-521532024f53";
/// auto uuid_obj = db::uuids::convert_to_uuid(uuid_str);
constexpr auto convert_to_uuid(std::string_view id_str) noexcept -> db::uuids::uuid {
    db::uuids::uuid uuid_obj{};
    std::copy_n(id_str.begin(), uuid_obj.size(), uuid_obj.data());
    return uuid_obj;
}

/// @brief Converts a UUID object to a string view.
///
/// This function takes a `db::uuids::uuid` object and returns a string view
/// representing the UUID.
///
/// @param uuid The `db::uuids::uuid` object to convert.
/// @return A string view representing the UUID.
constexpr auto convert_from_uuid(const db::uuids::uuid& uuid) noexcept -> std::string_view {
    return std::string_view{uuid.begin(), uuid.size()};
}

}  // namespace db::uuids
