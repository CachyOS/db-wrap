/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

/// @file db_wrap/db_api.hpp
/// @brief High-level CRUD entry points for table-mapped structs.
/// @ingroup db_api

#include <db_wrap/db_utils.hpp>
#include <db_wrap/details/pfr_utils.hpp>
#include <db_wrap/sql_utils.hpp>
#include <db_wrap/table_traits.hpp>

#include <optional>  // for optional

#include <boost/pfr/core.hpp>

#include <pqxx/pqxx>

/// @example example_basic.cpp
/// CRUD round-trip: `insert_record`, `find_by_id`, `get_all_records`.

/// @example example_motivating.cpp
/// Mixed `db::utils::one_row_as` + `insert_record` workflow.

/// @example example_sql_gen.cpp
/// Compile-time `construct_query_from_condition` driving `as_set_of`.

namespace db {

/// @ingroup db_api
/// @brief Find a record by its primary key.
///
/// Constructs and executes a `SELECT` against
/// `table_traits<Scheme>::table_name` matching `id`. The row is converted
/// to `Scheme` via @ref db::utils::one_row_as. The primary-key column
/// name and its C++ type come from @ref db::table_traits "table_traits<Scheme>".
///
/// Legacy schemes that expose a `kName` member and an `id` data member are
/// picked up automatically by the default `table_traits` specialization,
/// so existing call sites keep working without changes.
///
/// @tparam Scheme Must satisfy @ref db::DbTable.
/// @tparam IdType Primary-key C++ type. Defaults to
///                `table_traits<Scheme>::primary_key_type`.
/// @param conn Open libpqxx connection.
/// @param id Primary-key value to look up.
/// @return Matching record, or `std::nullopt` if not found.
/// @throws pqxx::sql_error if the SELECT fails on the server.
/// @throws pqxx::broken_connection if the connection drops mid-query.
///
/// @snippet example_basic.cpp find_by_id
template <DbTable Scheme, typename IdType = typename table_traits<Scheme>::primary_key_type>
auto find_by_id(pqxx::connection& conn, IdType&& id) -> std::optional<Scheme> {
    constexpr auto kSelectQuery = sql::utils::construct_find_by_pk_query<Scheme>();
    return db::utils::one_row_as<Scheme>(conn, kSelectQuery, id);
}

/// @ingroup db_api
/// @brief Retrieve all rows from a table.
///
/// Executes `SELECT *` against `table_traits<Scheme>::table_name` and maps
/// each row to `Scheme` via @ref db::utils::as_set_of. Returns
/// `std::nullopt` when the table is empty.
///
/// @tparam Scheme Must satisfy @ref db::DbTable.
/// @param conn Open libpqxx connection.
/// @return Vector of records, or `std::nullopt` for an empty result set.
/// @throws pqxx::sql_error on SQL execution failure.
///
/// @snippet example_basic.cpp get_all_records
template <DbTable Scheme>
auto get_all_records(pqxx::connection& conn) -> std::optional<std::vector<Scheme>> {
    constexpr auto kSelectAllQuery = sql::utils::construct_select_all_query<Scheme>();
    return db::utils::as_set_of<Scheme>(conn, kSelectAllQuery);
}

/// @ingroup db_api
/// @brief Update a chosen subset of fields on a record.
///
/// Builds an `UPDATE ... SET <fields> WHERE <pk>=$1` statement at compile
/// time via @ref db::sql::utils::create_update_query, then binds the
/// primary-key value and the named fields from `record` and executes the
/// statement.
///
/// `Fields` are C++ struct field names. Any column-name remapping declared
/// in `table_traits<Scheme>::column_overrides` is applied automatically
/// when the SQL is emitted, so callers always reference C++ names.
///
/// @tparam Scheme Must satisfy @ref db::DbTable.
/// @tparam Fields Pack of `db::details::static_string` C++ field names.
/// @param conn Open libpqxx connection.
/// @param record Source object; its primary-key field locates the row to
///               update.
/// @return Number of rows affected (typically 1).
/// @throws pqxx::sql_error on SQL execution failure.
/// @note A field name not present in `Scheme` triggers a `static_assert`
///       at compile time.
///
/// ```cpp
/// User user_to_update{1, "Updated Name", "updated.email@example.com"};
/// db::update_fields<User, "name", "email">(conn, user_to_update);
/// ```
template <DbTable Scheme, ::db::details::static_string... Fields>
auto update_fields(pqxx::connection& conn, const Scheme& record) -> std::size_t {
    static_assert(sql::details::validate_fields<Fields...>(Scheme{}), "non existent field detected!");

    constexpr auto kUpdateQuery = sql::utils::create_update_query<Scheme, Fields...>();
    constexpr auto kPkIdx       = ::db::get_pk_field_idx<Scheme>();
    return db::utils::exec_affected(conn, kUpdateQuery,
        boost::pfr::get<kPkIdx>(record),
        db::utils::get_field_by_name<Fields>(record)...);
}

/// @ingroup db_api
/// @brief Delete a record by its primary key.
///
/// Executes `DELETE FROM <table> WHERE <pk>=$1`. The primary-key column
/// name and its C++ type are resolved through
/// @ref db::table_traits "table_traits<Scheme>".
///
/// @tparam Scheme Must satisfy @ref db::DbTable.
/// @tparam IdType Primary-key C++ type. Defaults to
///                `table_traits<Scheme>::primary_key_type`.
/// @param conn Open libpqxx connection.
/// @param id Primary-key value identifying the row to delete.
/// @return Number of rows affected (1 on success, 0 if the row was missing).
/// @throws pqxx::sql_error on SQL execution failure.
template <DbTable Scheme, typename IdType = typename table_traits<Scheme>::primary_key_type>
auto delete_record_by_id(pqxx::connection& conn, IdType&& id) -> std::size_t {
    constexpr auto kDeleteQuery = sql::utils::construct_delete_by_pk_query<Scheme>();
    return db::utils::exec_affected(conn, kDeleteQuery, id);
}

/// @ingroup db_api
/// @brief Update every field of a record except the primary key.
///
/// Builds an `UPDATE` statement that sets all eligible columns at once,
/// using the primary-key value carried in `record` to locate the row.
/// Fields listed in `table_traits<Scheme>::update_exclude` are omitted
/// from the SET clause.
///
/// @tparam Scheme Must satisfy @ref db::DbTable.
/// @param conn Open libpqxx connection.
/// @param record Source object. Its primary-key field locates the row;
///               that field is never written.
/// @return Number of rows affected (typically 1).
/// @throws pqxx::sql_error on SQL execution failure.
/// @see db::update_fields for partial updates.
template <DbTable Scheme>
auto update_record(pqxx::connection& conn, const Scheme& record) -> std::size_t {
    constexpr auto kUpdateAllQuery = sql::utils::create_update_all_query<Scheme>();
    return db::utils::exec_affected<Scheme>(conn, kUpdateAllQuery, record);
}

/// @ingroup db_api
/// @brief Insert a new record into a table.
///
/// Builds an `INSERT INTO <table> (<columns>) VALUES (...)` statement at
/// compile time via @ref db::sql::utils::create_insert_all_query.
/// Columns listed in `table_traits<Scheme>::insert_exclude` are omitted
/// (typical use: an auto-incrementing primary key).
///
/// @tparam Scheme Must satisfy @ref db::DbTable.
/// @param conn Open libpqxx connection.
/// @param record Object whose fields become the inserted row.
/// @return Number of rows affected (typically 1).
/// @throws pqxx::sql_error on SQL execution failure (constraint violation,
///         duplicate key, etc.).
///
/// @snippet example_basic.cpp insert_record
template <DbTable Scheme>
auto insert_record(pqxx::connection& conn, const Scheme& record) -> std::size_t {
    constexpr auto kInsertAllQuery = sql::utils::create_insert_all_query<Scheme>();
    return db::utils::exec_affected<Scheme>(conn, kInsertAllQuery, record);
}

}  // namespace db
