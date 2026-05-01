/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024-2026 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

#include <db_wrap/details/backend_detect.hpp>
#include <db_wrap/details/static_string.hpp>

#include <algorithm>    // for find
#include <iterator>     // for distance
#include <type_traits>  // for remove_cvref_t

#if DB_WRAP_REFLECT_BACKEND_STD
#include <array>
#include <meta>
#include <string_view>
#else
#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>
#endif

namespace db::utils {

#if DB_WRAP_REFLECT_BACKEND_STD

template <typename T>
consteval auto get_struct_names() noexcept {
    auto members = std::meta::nonstatic_data_members_of(^^std::remove_cvref_t<T>,
                       std::meta::access_context::unchecked());

    std::array<std::string_view, std::meta::nonstatic_data_members_of(^^std::remove_cvref_t<T>,
                   std::meta::access_context::unchecked()).size()> out{};
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = std::meta::identifier_of(members[i]);
    }
    return out;
}

template <typename T>
consteval auto get_fields_count() noexcept -> std::size_t {
    return std::meta::nonstatic_data_members_of(^^std::remove_cvref_t<T>,
               std::meta::access_context::unchecked()).size();
}

template <db::details::static_string field_name, typename T>
consteval auto get_field_idx_by_name() noexcept -> std::size_t {
    constexpr auto struct_fields = utils::get_struct_names<std::remove_cvref_t<T>>();
    return std::distance(struct_fields.begin(), std::find(struct_fields.begin(), struct_fields.end(), field_name));
}

template <std::size_t idx>
constexpr auto get_field_by_idx(auto&& val) noexcept {
    using T = std::remove_cvref_t<decltype(val)>;
    static constexpr auto members = std::define_static_array(
        std::meta::nonstatic_data_members_of(^^T,
            std::meta::access_context::unchecked()));
    return val.[:members[idx]:];
}

template <db::details::static_string field_name>
constexpr auto get_field_by_name(auto&& val) noexcept {
    constexpr auto field_idx = utils::get_field_idx_by_name<field_name, std::remove_cvref_t<decltype(val)>>();
    return utils::get_field_by_idx<field_idx>(val);
}

#else

/// @brief Retrieves the names of the members of a structure as a
///        `boost::pfr::flat_names_array`.
///
/// This function uses Boost.PFR to extract the names of the members
/// of the given structure type `T` and returns them as a
/// `boost::pfr::flat_names_array`.
///
/// @tparam T The structure type.
/// @return A `boost::pfr::flat_names_array` containing the member names.
template <typename T>
consteval auto get_struct_names() noexcept {
    return boost::pfr::names_as_array<std::remove_cvref_t<T>>();
}

/// @brief Retrieves the number of fields in a structure at compile time.
///
/// This function uses Boost.PFR to determine the number of fields in the
/// structure type `T` at compile time. It's useful for scenarios where
/// the number of fields needs to be known during compilation, such as
/// generating SQL queries or performing static assertions.
///
/// @tparam T The structure type.
/// @return The number of fields in the structure.
template <typename T>
consteval auto get_fields_count() noexcept -> std::size_t {
    return boost::pfr::tuple_size_v<std::remove_cvref_t<T>>;
}

/// @brief Gets the index of a field in a structure by its name.
///
/// This function searches for the field with the given name `field_name`
/// in the structure type `T` and returns its index.
///
/// @tparam field_name The name of the field to search for (as a
///                   `db::details::static_string`).
/// @tparam T The structure type.
/// @return The index of the field if found, otherwise the size of
///         the structure (indicating that the field was not found).
template <db::details::static_string field_name, typename T>
consteval auto get_field_idx_by_name() noexcept -> std::size_t {
    constexpr auto struct_fields = utils::get_struct_names<std::remove_cvref_t<T>>();
    // NOTE: apparently ranges::find cannot handle array of std::common_type_t at compile time
    return std::distance(struct_fields.begin(), std::find(struct_fields.begin(), struct_fields.end(), field_name));
}

/// @brief Gets a field from a structure by its index.
///
/// This function retrieves the field at the given index `idx` from the
/// structure `val`.
///
/// @tparam idx The index of the field to retrieve.
/// @param val The structure object.
/// @return The value of the field at the given index.
template <std::size_t idx>
constexpr auto get_field_by_idx(auto&& val) noexcept {
    return boost::pfr::get<idx>(val);
}

/// @brief Gets a field from a structure by its name.
///
/// This function retrieves the field with the given name `field_name`
/// from the structure `val`.
///
/// @tparam field_name The name of the field to retrieve (as a
///                   `db::details::static_string`).
/// @param val The structure object.
/// @return The value of the field with the given name.
template <db::details::static_string field_name>
constexpr auto get_field_by_name(auto&& val) noexcept {
    constexpr auto field_idx = utils::get_field_idx_by_name<field_name, std::remove_cvref_t<decltype(val)>>();
    return utils::get_field_by_idx<field_idx>(val);
}

#endif

}  // namespace db::utils
