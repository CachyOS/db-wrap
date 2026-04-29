/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024-2026 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

#include <db_wrap/details/pfr_utils.hpp>
#include <db_wrap/table_traits.hpp>

#include <string_view>
#include <type_traits>

#if defined(DB_WRAP_HAS_STD_REFLECTION) && __has_include(<meta>) && (defined(__cpp_lib_reflection) || defined(__cpp_impl_reflection))
#include <meta>
#define DB_WRAP_REFLECT_BACKEND_STD 1
#else
#define DB_WRAP_REFLECT_BACKEND_STD 0
#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>
#endif

namespace db::reflect {

#if DB_WRAP_REFLECT_BACKEND_STD

/// @brief The number of non-static data members of `T`, computed at
///        compile time via `std::meta`.
template <typename T>
consteval auto fields_count() noexcept -> std::size_t {
    return std::meta::nonstatic_data_members_of(^^std::remove_cvref_t<T>).size();
}

/// @brief Array of field names of `T`, as `std::string_view`s.
template <typename T>
consteval auto field_names() noexcept {
    constexpr auto members = std::meta::nonstatic_data_members_of(^^std::remove_cvref_t<T>);
    constexpr auto N       = members.size();

    std::array<std::string_view, N> out{};
    for (std::size_t i = 0; i < N; ++i) {
        out[i] = std::meta::identifier_of(members[i]);
    }
    return out;
}

/// @brief Invoke `fn(field_name, field_ref)` for each non-static data
///        member of `obj`.
template <typename T, typename Fn>
constexpr void for_each_field_with_name(T&& obj, Fn&& fn) {
    using U                = std::remove_cvref_t<T>;
    constexpr auto members = std::meta::nonstatic_data_members_of(^^U);

    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (fn(std::string_view{std::meta::identifier_of(members[Is])},
             obj.[:members[Is]:]),
            ...);
    }(std::make_index_sequence<members.size()>{});
}

#else

/// @brief The number of fields in `T`, via `boost::pfr::tuple_size_v`.
template <typename T>
consteval auto fields_count() noexcept -> std::size_t {
    return boost::pfr::tuple_size_v<std::remove_cvref_t<T>>;
}

/// @brief Array of field names of `T`, as returned by
///        `boost::pfr::names_as_array`.
template <typename T>
consteval auto field_names() noexcept {
    return boost::pfr::names_as_array<std::remove_cvref_t<T>>();
}

/// @brief Invoke `fn(field_name, field_ref)` for each field of `obj`.
template <typename T, typename Fn>
constexpr void for_each_field_with_name(T&& obj, Fn&& fn) {
    boost::pfr::for_each_field_with_name(obj, [&](std::string_view name, auto& field) {
        fn(name, field);
    });
}

#endif

/// @brief Invoke `fn(sql_column_name, field_ref)` for each field of `obj`,
///        after translating the C++ struct name through
///        `db::column_of<T>`.
///
/// Call sites that write or read SQL (INSERT column lists, UPDATE SET
/// clauses, SELECT-result extraction) need the **SQL** column name, not
/// the raw struct member identifier. This helper centralizes the
/// translation so that column overrides declared in `table_traits<T>`
/// are consistently applied in every direction.
///
/// @tparam T   A type satisfying `db::DbTable`. Its `table_traits`
///             specialization drives the column-name translation.
/// @tparam Obj The object type being walked (usually deduced).
/// @tparam Fn  Callable type matching `void(std::string_view, auto&)`.
///
/// @example
/// struct User {
///     std::int64_t user_id;
///     std::string  full_name;  // maps to "name" in table_traits
/// };
///
/// User u{42, "alice"};
/// db::reflect::for_each_field_with_column_name<User>(u,
///     [](std::string_view sql_column, auto& value) {
///         // sql_column == "user_id", then "name"
///     });
template <::db::DbTable T, typename Obj, typename Fn>
constexpr void for_each_field_with_column_name(Obj&& obj, Fn&& fn) {
    reflect::for_each_field_with_name(obj, [&](std::string_view field_name, auto& field) {
        fn(::db::column_of<T>(field_name), field);
    });
}

}  // namespace db::reflect
