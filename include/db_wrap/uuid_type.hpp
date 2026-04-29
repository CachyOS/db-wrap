/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

/// @file db_wrap/uuid_type.hpp
/// @brief Fixed-size UUID storage and conversions under `db::uuids`.
/// @ingroup db_uuids

#include <cstdint>

#include <algorithm>    // for copy_n
#include <array>        // for array
#include <string_view>  // for string_view

namespace db::uuids {

/// @ingroup db_uuids
/// @brief Length of a canonical UUID string in characters (36, including hyphens).
inline constexpr std::size_t UUID_LEN = 36UL;

/// @ingroup db_uuids
/// @brief Fixed-size character storage for a 36-character UUID string.
using uuid = std::array</*std::uint8_t*/ char, UUID_LEN>;

/// @ingroup db_uuids
/// @brief Copy a 36-character UUID string into a `uuid` array.
///
/// @param id_str Source string. Must be exactly 36 characters long.
/// @return A `uuid` populated with the copied bytes.
/// @warning No format validation is performed; only the length and the
///          first 36 characters are read.
///
/// ```cpp
/// constexpr std::string_view uuid_str = "877dae4c-0a31-499d-9f81-521532024f53";
/// auto uuid_obj = db::uuids::convert_to_uuid(uuid_str);
/// ```
constexpr auto convert_to_uuid(std::string_view id_str) noexcept -> db::uuids::uuid {
    db::uuids::uuid uuid_obj{};
    std::copy_n(id_str.begin(), uuid_obj.size(), uuid_obj.data());
    return uuid_obj;
}

/// @ingroup db_uuids
/// @brief Borrow a `uuid` as a non-owning `std::string_view`.
///
/// @param uuid Source array.
/// @return A view spanning the 36 stored characters; valid for as long as
///         `uuid` is alive.
constexpr auto convert_from_uuid(const db::uuids::uuid& uuid) noexcept -> std::string_view {
    return std::string_view{uuid.begin(), uuid.size()};
}

}  // namespace db::uuids
