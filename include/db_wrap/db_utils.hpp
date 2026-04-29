/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024-2026 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

/// @file db_wrap/db_utils.hpp
/// @brief Row-mapping and exec helpers under the `db::utils` namespace.
/// @ingroup db_utils

#include <db_wrap/details/reflect.hpp>
#include <db_wrap/details/sql_impl.hpp>
#include <db_wrap/details/unpack_fields_impl.hpp>
#include <db_wrap/table_traits.hpp>

#include <optional>     // for optional
#include <ranges>       // for ranges::*
#include <string_view>  // for string_view
#include <vector>       // for vector

#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>

#include <pqxx/pqxx>

namespace db::utils {

/// @ingroup db_utils
/// @brief Convert a `pqxx::row_ref` into a user struct.
///
/// Routes through the `db::reflect::*` dispatcher to walk the fields of
/// `T` and pull each value out of the row by column name.
///
/// When `T` satisfies @ref db::DbTable, the lookup uses the SQL column
/// names produced by `db::column_of<T>`, so any `column_overrides`
/// declared on `table_traits<T>` are applied consistently with INSERT /
/// UPDATE emission. When `T` does not model `DbTable` (ad-hoc structs,
/// unit tests), the lookup falls back to an identity mapping from struct
/// member name to column name.
///
/// @tparam T Destination struct type.
/// @param row Source row.
/// @return A fully populated `T`.
/// @throws pqxx::conversion_error if a column cannot be converted to the
///         corresponding C++ field type.
template <typename T>
constexpr T from_row(pqxx::row_ref&& row) noexcept {
    T obj{};
    if constexpr (::db::DbTable<T>) {
        // Route the SELECT-result extraction through the reflection
        // dispatcher so that column overrides declared in
        // `table_traits<T>` are applied consistently with INSERT /
        // UPDATE emission.
        ::db::reflect::for_each_field_with_column_name<T>(obj, [&](std::string_view column, auto& field) {
            field = row[pqxx::zview(column)].as<std::decay_t<decltype(field)>>();
        });
    } else {
        // Fallback for non-`DbTable` types (unit tests, ad-hoc structs):
        // identity mapping from struct member name to column name.
        ::db::reflect::for_each_field_with_name(obj, [&](std::string_view field_name, auto& field) {
            field = row[pqxx::zview(field_name)].as<std::decay_t<decltype(field)>>();
        });
    }
    return obj;
}

/// @ingroup db_utils
/// @brief Convert a `pqxx::row_ref` into a user struct by column index.
///
/// Walks the fields of `T` via Boost.PFR and pulls each value out of the
/// row by 0-based positional index. Useful when the SQL projection order
/// matches the struct field order exactly (e.g. `SELECT a,b,c FROM t`).
///
/// @tparam T Destination struct type.
/// @param row Source row.
/// @return A fully populated `T`.
/// @warning Column ordering is implicit. If you reorder struct fields
///          without updating the SQL, every column shifts silently.
///          Prefer @ref from_row whenever a column name lookup is
///          acceptable.
template <typename T>
constexpr T from_columns(pqxx::row_ref&& row) {
    T obj{};
    boost::pfr::for_each_field(obj, [&](auto& field, std::size_t index) {
        field = row[static_cast<pqxx::row_ref::size_type>(index)].as<std::decay_t<decltype(field)>>();
    });
    return obj;
}

/// @ingroup db_utils
/// @brief Materialize every row of a result into a `std::vector<T>`.
///
/// Iterates the result and converts each row through @ref from_row.
///
/// @tparam T Destination struct type.
/// @param result Result set to consume. Moved-from on return.
/// @return Vector with one element per row.
template <typename T>
constexpr auto extract_all_rows(pqxx::result&& result) noexcept -> std::vector<T> {
    return result
        | std::views::transform([](auto&& row) { return utils::from_row<T>(std::move(row)); })
        | std::ranges::to<std::vector<T>>();
}

/// @ingroup db_utils
/// @brief Run a parameterized query and return the first row as `T`.
///
/// Opens a `pqxx::work`, executes `query` with `args` bound to
/// `$1`..`$N`, and converts the first row via @ref from_row. Returns
/// `std::nullopt` if the result set is empty.
///
/// @tparam T Destination struct type.
/// @tparam Args Query parameter types; must be `pqxx`-bindable.
/// @param conn Open libpqxx connection.
/// @param query SQL with `$1`-style placeholders.
/// @param args Values bound to the placeholders, in order.
/// @return First row as `T`, or `std::nullopt` if no rows.
/// @throws pqxx::sql_error on SQL execution failure.
///
/// @snippet example_motivating.cpp one_row_as
template <typename T, typename... Args>
auto one_row_as(pqxx::connection& conn, std::string_view query, Args&&... args) -> std::optional<T> {
    pqxx::work txn(conn);
    pqxx::result result = txn.exec(query.data(), pqxx::params(std::forward<Args>(args)...));

    if (result.empty()) {
        return std::nullopt;
    }
    // end our transaction and return
    txn.commit();
    return utils::from_row<T>(result[0]);
}

/// @ingroup db_utils
/// @brief Run a parameterized query and return all rows as `std::vector<T>`.
///
/// Opens a `pqxx::work`, executes `query` with `args` bound to
/// `$1`..`$N`, and converts every row via @ref from_row. Returns
/// `std::nullopt` if the result set is empty (so the caller can
/// distinguish "no rows" from "empty vector").
///
/// @tparam T Destination struct type.
/// @tparam Args Query parameter types; must be `pqxx`-bindable.
/// @param conn Open libpqxx connection.
/// @param query SQL with `$1`-style placeholders.
/// @param args Values bound to the placeholders, in order.
/// @return Vector of rows, or `std::nullopt` if no rows.
/// @throws pqxx::sql_error on SQL execution failure.
///
/// @snippet example_sql_gen.cpp as_set_of
template <typename T, typename... Args>
auto as_set_of(pqxx::connection& conn, std::string_view query, Args&&... args) -> std::optional<std::vector<T>> {
    pqxx::work txn(conn);
    pqxx::result result = txn.exec(query.data(), pqxx::params(std::forward<Args>(args)...));

    if (result.empty()) {
        return std::nullopt;
    }
    // end our transaction and return
    txn.commit();
    return utils::extract_all_rows<T>(std::move(result));
}

/// @ingroup db_utils
/// @brief Run a parameterized statement and return the affected-row count.
///
/// Opens a `pqxx::work`, executes `query` with `args` bound to
/// `$1`..`$N`, commits, and returns `pqxx::result::affected_rows()`.
///
/// @tparam Args Query parameter types; must be `pqxx`-bindable.
/// @param conn Open libpqxx connection.
/// @param query SQL with `$1`-style placeholders.
/// @param args Values bound to the placeholders, in order.
/// @return Number of rows affected.
/// @throws pqxx::sql_error on SQL execution failure.
template <typename... Args>
auto exec_affected(pqxx::connection& conn, std::string_view query, Args&&... args) -> std::size_t {
    pqxx::work txn(conn);
    pqxx::result result = txn.exec(query.data(), pqxx::params(std::forward<Args>(args)...));

    // end our transaction and return
    txn.commit();
    return static_cast<std::size_t>(result.affected_rows());
}

/// @ingroup db_utils
/// @brief Overload that unpacks every field of `record` into the parameter pack.
///
/// Convenience wrapper for INSERT/UPDATE statements whose `$1`..`$N`
/// placeholders correspond exactly to the fields of `record` (in
/// declaration order). The fields are extracted via
/// `db::utils::unpack_fields` and forwarded to the variadic
/// @ref exec_affected overload.
///
/// @tparam Scheme Must satisfy `sql::details::HasName`.
/// @param conn Open libpqxx connection.
/// @param query SQL with one `$N` placeholder per field of `Scheme`.
/// @param record Source object whose fields become the bound parameters.
/// @return Number of rows affected.
/// @throws pqxx::sql_error on SQL execution failure.
template <sql::details::HasName Scheme>
auto exec_affected(pqxx::connection& conn, std::string_view query, const Scheme& record) -> std::size_t {
    // Unroll fields using unpack_fields
    auto&& unroll_func = [&conn, &query, &record](const auto&... fields) {
        return db::utils::exec_affected(conn, query, fields...);
    };
    return db::utils::unpack_fields(std::move(unroll_func), record);
}

}  // namespace db::utils
