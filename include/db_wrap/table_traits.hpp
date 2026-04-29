/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

/// @file db_wrap/table_traits.hpp
/// @brief Customization point: `db::table_traits<T>` and column-routing helpers.
/// @ingroup db_traits

#include <algorithm>    // for contains, find_if
#include <array>        // for array
#include <concepts>     // for convertible_to
#include <cstddef>      // for size_t
#include <ranges>       // for ranges::*
#include <string_view>  // for string_view
#include <type_traits>  // for remove_cvref_t
#include <utility>      // for declval, pair

#include <boost/pfr/core_name.hpp>

namespace db {

/// @ingroup db_traits
/// @brief Primary template for table metadata traits (Rust-trait style).
///
/// Users plug compile-time table metadata into the library by specializing
/// `db::table_traits<T>` for their domain struct `T`. A specialization
/// must expose at least:
///
///   - `static constexpr std::string_view table_name`  — the SQL table
///     this struct maps to.
///   - `static constexpr std::string_view primary_key` — the SQL column
///     name of the primary key.
///   - `using primary_key_type = ...`                  — the C++ type of
///     that primary key.
///
/// Optional members with sensible defaults:
///
///   - `static constexpr std::array<std::pair<std::string_view, std::string_view>, N>
///        column_overrides` — maps C++ struct field name → SQL column
///     name. Use this when your struct field naming diverges from your
///     schema (e.g. `full_name` in C++ ↔ `name` in SQL).
///   - `static constexpr std::array<std::string_view, N> insert_exclude`
///      — struct fields that must NOT appear in an INSERT column list
///     (e.g. auto-generated timestamps).
///   - `static constexpr std::array<std::string_view, N> update_exclude`
///      — struct fields that must NOT appear in an UPDATE SET clause
///     (e.g. read-only audit columns).
///
/// The library ships a default specialization (below) which auto-derives
/// these values from the legacy contract (`T::kName` + `T::id` member),
/// so existing user code that predates `table_traits` keeps working
/// unchanged.
///
/// ```cpp
/// struct Account {
///     std::int64_t user_id;
///     std::string  full_name;
///     std::string  email_addr;
///     std::string  created_at;
/// };
///
/// template <>
/// struct db::table_traits<Account> {
///     static constexpr std::string_view table_name  = "accounts";
///     static constexpr std::string_view primary_key = "user_id";
///
///     using primary_key_type = std::int64_t;
///
///     static constexpr std::array<std::pair<std::string_view, std::string_view>, 2>
///         column_overrides{{{"full_name",  "name"},
///                           {"email_addr", "email"}}};
///
///     static constexpr std::array<std::string_view, 1> insert_exclude{"created_at"};
///     static constexpr std::array<std::string_view, 1> update_exclude{"created_at"};
/// };
///
/// // Now all db::* APIs work on `Account` transparently:
/// auto found = db::find_by_id<Account>(conn, std::int64_t{42});
/// auto rows  = db::get_all_records<Account>(conn);
/// db::insert_record(conn, acct);
/// ```
template <typename T>
struct table_traits;

namespace details {

/// @brief Legacy concept. Detects the pre-traits struct contract
/// (`static T::kName` convertible to `string_view` plus a data member `id`).
/// Used only to gate the default `table_traits` specialization.
template <typename T>
concept LegacyScheme = requires(T t) {
    { T::kName } -> std::convertible_to<std::string_view>;
    t.id;
};

}  // namespace details

/// @brief Default `table_traits` specialization for legacy schemes.
///
/// If a user has not specialized `table_traits<T>` but `T` satisfies the
/// original `kName` + `id` contract, this specialization kicks in and
/// derives everything from those members. The emitted SQL matches what the
/// pre-traits implementation produced byte-for-byte.
template <details::LegacyScheme T>
struct table_traits<T> {
    static constexpr std::string_view table_name  = T::kName;
    static constexpr std::string_view primary_key = "id";

    using primary_key_type = std::remove_cvref_t<decltype(std::declval<T>().id)>;

    static constexpr std::array<std::pair<std::string_view, std::string_view>, 0> column_overrides{};
    static constexpr std::array<std::string_view, 0> insert_exclude{};
    static constexpr std::array<std::string_view, 0> update_exclude{};
};

/// @ingroup db_traits
/// @brief Concept gating every high-level `db::*` API.
///
/// `T` models `DbTable` iff a `table_traits<T>` specialization exposes the
/// required `table_name`, `primary_key` and `primary_key_type` members.
template <typename T>
concept DbTable = requires {
    { table_traits<T>::table_name } -> std::convertible_to<std::string_view>;
    { table_traits<T>::primary_key } -> std::convertible_to<std::string_view>;
    typename table_traits<T>::primary_key_type;
};

/// @brief Translate a C++ struct field name to its SQL column name for `T`.
///
/// Looks up `field_name` in `table_traits<T>::column_overrides`. Returns
/// the mapped value if present, otherwise returns `field_name` unchanged.
/// `consteval` so all callers pay zero runtime cost.
template <DbTable T>
constexpr auto column_of(std::string_view field_name) noexcept -> std::string_view {
    const auto& overrides = table_traits<T>::column_overrides;
    const auto it         = std::ranges::find_if(overrides,
        [field_name](auto&& p) { return p.first == field_name; });
    return it != overrides.end() ? it->second : field_name;
}

/// @brief True if `field_name` should be excluded from a generated INSERT
///        column list for `T` (either the primary key for auto-generated PKs
///        is NOT excluded here.
template <DbTable T>
constexpr auto is_insert_excluded(std::string_view field_name) noexcept -> bool {
    return std::ranges::contains(table_traits<T>::insert_exclude, field_name);
}

/// @brief True if `field_name` should be excluded from a generated UPDATE
///        SET clause for `T`. The primary key is always excluded from SET
///        regardless of this list.
template <DbTable T>
constexpr auto is_update_excluded(std::string_view field_name) noexcept -> bool {
    return std::ranges::contains(table_traits<T>::update_exclude, field_name);
}

/// @brief True if a struct field with member name `field_name` should be
///        emitted in a generated UPDATE SET clause for `T`. Skips the
///        primary-key column and anything in `update_exclude`.
template <DbTable T>
constexpr auto is_in_update_set(std::string_view field_name) noexcept -> bool {
    return column_of<T>(field_name) != std::string_view{table_traits<T>::primary_key}
    && !is_update_excluded<T>(field_name);
}

/// @brief True if a struct field with member name `field_name` should be
///        emitted in a generated INSERT column list for `T`.
template <DbTable T>
constexpr auto is_in_insert_columns(std::string_view field_name) noexcept -> bool {
    return !is_insert_excluded<T>(field_name);
}

/// @brief Compile-time index of the struct field that maps to the primary
///        key column under `table_traits<T>::column_overrides`.
///
/// Walks the boost::pfr field name list, applies `column_of<T>` to each,
/// and returns the index whose mapped column name equals
/// `table_traits<T>::primary_key`. Returns `fields_count<T>()` (i.e. one
/// past the end) if no match is found. this is a hard compile-time error
/// if the caller later uses the index with `boost::pfr::get<idx>`, which
/// is the desired behavior.
template <DbTable T>
consteval auto get_pk_field_idx() noexcept -> std::size_t {
    constexpr auto names = boost::pfr::names_as_array<std::remove_cvref_t<T>>();
    constexpr auto kPk   = std::string_view{table_traits<T>::primary_key};
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (column_of<T>(names[i]) == kPk) {
            return i;
        }
    }
    return names.size();
}

}  // namespace db
