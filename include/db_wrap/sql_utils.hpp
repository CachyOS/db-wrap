/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

#include <db_wrap/details/sql_impl.hpp>
#include <db_wrap/details/static_string.hpp>
#include <db_wrap/table_traits.hpp>

#include <cstdint>

#include <algorithm>    // for find
#include <ranges>       // for ranges::*
#include <string>       // for string
#include <string_view>  // for string_view

namespace db::sql::utils {

/// @brief Creates an SQL UPDATE query string at compile time.
///
/// This function generates an SQL query string for updating records in a
/// database table based on the provided `Scheme` and `Fields`. The query
/// is constructed at compile time, resulting in a `db::details::static_string`
/// that can be used directly in your code.
///
/// @see https://godbolt.org/z/K71ecnoa6 for a demonstration.
///
/// @tparam Scheme The type representing the database table scheme.
///                Must satisfy the `db::DbTable` concept.
/// @tparam Fields A pack of `db::details::static_string` representing the
///                names of the fields to be updated.
/// @return A `db::details::static_string` containing the generated SQL query.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   int id;
///   std::string name;
///   int age;
/// };
///
/// constexpr auto query = db::sql::utils::create_update_query<User, "name", "age">();
/// static_assert(query == "UPDATE users SET name = $2, age = $3 WHERE id = $1;");
/// // This will compile successfully as the query is generated correctly.
template <::db::DbTable Scheme, ::db::details::static_string... Fields>
consteval auto create_update_query() noexcept {
    constexpr auto static_size = []() {
        std::string res{};
        details::update_query_str<Scheme, Fields...>(res);
        return res.size() + 1;
    }();
    ::db::details::static_string<static_size> res{};
    details::update_query_str<Scheme, Fields...>(res);
    return res;
}

/// @brief Constructs a SELECT query with a WHERE clause at compile time.
///
/// This function generates a SQL SELECT query string at compile time,
/// retrieving all columns (*) from a table specified by
/// `table_traits<Scheme>::table_name`. The query includes a WHERE clause
/// based on the provided `query` condition.
///
/// The function uses `db::details::static_string` to construct the query string
/// at compile time, ensuring that the query is readily available for use
/// without runtime overhead.
///
/// @tparam Scheme The type representing the database table scheme. It must
///                satisfy the `db::DbTable` concept.
/// @tparam query A `db::details::static_string` representing the WHERE clause
///              condition.
/// @return A `db::details::static_string` containing the constructed SELECT query.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   // ... other members ...
/// };
///
/// constexpr auto query = db::sql::utils::construct_query_from_condition<User, "id = 1">();
/// static_assert(query == "SELECT * FROM users WHERE id = 1;");
template <::db::DbTable Scheme, ::db::details::static_string query>
consteval auto construct_query_from_condition() noexcept {
    constexpr auto kDbName = []() {
        constexpr std::string_view db_name_str = ::db::table_traits<Scheme>::table_name;
        ::db::details::static_string<db_name_str.size()> res{};
        res += db_name_str;
        return res;
    }();

    constexpr auto kStatementBegin = ::db::details::static_string("SELECT * FROM ");
    constexpr auto kSelectWhere    = kStatementBegin + kDbName + ::db::details::static_string(" WHERE ");
    return kSelectWhere + query + ::db::details::static_string(";");
}

/// @brief Creates an SQL UPDATE query string at compile time to update all fields
///        (except the primary key, and any field listed in `update_exclude`)
///        in a table based on the provided scheme.
///
/// This function generates an SQL query string for updating all eligible
/// fields of a database table, excluding the primary-key column (which
/// is used in the WHERE clause to identify the record to update) and any
/// fields declared in `table_traits<Scheme>::update_exclude`. It utilizes
/// `table_traits<Scheme>::table_name` to determine the table name and
/// leverages the structure of the `Scheme` type to create the SET clause.
///
/// The generated query string is returned as a `db::details::static_string`, suitable
/// for use in compile-time contexts.
///
/// @tparam Scheme The type representing the database table scheme.
///                Must satisfy the `db::DbTable` concept.
/// @return A `db::details::static_string` containing the generated SQL query string.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   int id;
///   std::string name;
///   int age;
/// };
///
/// constexpr auto query = db::sql::utils::create_update_all_query<User>();
/// static_assert(query == "UPDATE users SET name = $2, age = $3 WHERE id = $1;");
template <::db::DbTable Scheme>
consteval auto create_update_all_query() noexcept {
    // NOTE: for now we hardcode generating where clause for id field
    // static_assert(details::validate_fields<"id">(Scheme{}), "'id' field is required!");

    constexpr auto static_size = []() {
        std::string res{};
        details::update_query_all_str<Scheme>(res);
        return res.size() + 1;
    }();
    ::db::details::static_string<static_size> res{};
    details::update_query_all_str<Scheme>(res);
    return res;
}

/// @brief Constructs a SQL SELECT query to retrieve all rows from a table.
///
/// This function generates a compile-time SQL SELECT query string that
/// selects all columns (*) from the table specified by
/// `table_traits<Scheme>::table_name`.
///
/// The generated query string is returned as a `db::details::static_string`, suitable
/// for use in compile-time contexts.
///
/// @tparam Scheme The type representing the database table scheme.
///                Must satisfy the `db::DbTable` concept.
/// @return A `db::details::static_string` containing the generated SQL query string.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   // ... other members ...
/// };
///
/// constexpr auto query = db::sql::utils::construct_select_all_query<User>();
/// static_assert(query == "SELECT * FROM users;");
template <::db::DbTable Scheme>
consteval auto construct_select_all_query() noexcept {
    constexpr auto kDbName = []() {
        constexpr std::string_view db_name_str = ::db::table_traits<Scheme>::table_name;
        ::db::details::static_string<db_name_str.size()> res{};
        res += db_name_str;
        return res;
    }();

    constexpr auto kStatementBegin = ::db::details::static_string("SELECT * FROM ");
    return kStatementBegin + kDbName + ::db::details::static_string(";");
}

/// @brief Constructs a SQL DELETE query with a WHERE clause at compile time.
///
/// This function generates a SQL DELETE query string at compile time,
/// removing records from the table specified by
/// `table_traits<Scheme>::table_name`. The query includes a WHERE clause
/// based on the provided `query` condition.
///
/// @tparam Scheme The type representing the database table scheme. It must
///                satisfy the `db::DbTable` concept.
/// @tparam query A `db::details::static_string` representing the WHERE clause
///              condition.
/// @return A `db::details::static_string` containing the constructed DELETE query.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   // ... other members ...
/// };
///
/// constexpr auto query = db::sql::utils::construct_delete_query_from_condition<User, "name = 'John Doe'">();
/// static_assert(query == "DELETE FROM users WHERE name = 'John Doe';");
template <::db::DbTable Scheme, ::db::details::static_string query>
consteval auto construct_delete_query_from_condition() noexcept {
    constexpr auto kDbName = []() {
        constexpr std::string_view db_name_str = ::db::table_traits<Scheme>::table_name;
        ::db::details::static_string<db_name_str.size()> res{};
        res += db_name_str;
        return res;
    }();

    constexpr auto kStatementBegin = ::db::details::static_string("DELETE FROM ");
    constexpr auto kSelectWhere    = kStatementBegin + kDbName + ::db::details::static_string(" WHERE ");
    return kSelectWhere + query + ::db::details::static_string(";");
}

/// @brief Creates an SQL INSERT query string at compile time to insert all fields
///        of a record into a table based on the provided scheme.
///
/// This function generates an SQL query string for inserting a new record into
/// a database table. It uses `table_traits<Scheme>::table_name` to determine
/// the table name and walks the structure of the `Scheme` type to construct
/// the column list and values placeholders. Fields declared in
/// `table_traits<Scheme>::insert_exclude` are omitted from the column list
/// and from the value placeholders, and the placeholder indices are
/// renumbered accordingly.
///
/// @tparam Scheme The type representing the database table scheme.
///                Must satisfy the `db::DbTable` concept.
/// @return A `db::details::static_string` containing the generated SQL query string.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   int id;
///   std::string name;
///   int age;
/// };
///
/// constexpr auto query = db::sql::utils::create_insert_all_query<User>();
/// static_assert(query == "INSERT INTO users (id, name, age) VALUES ($1, $2, $3);");
template <::db::DbTable Scheme>
consteval auto create_insert_all_query() noexcept {
    constexpr auto static_size = []() {
        std::string res{};
        details::insert_query_all_str<Scheme>(res);
        return res.size() + 1;
    }();
    ::db::details::static_string<static_size> res{};
    details::insert_query_all_str<Scheme>(res);
    return res;
}

/// @brief Constructs a `SELECT * FROM <table> WHERE <pk> = $1;` query at
///        compile time, where `<pk>` is `table_traits<Scheme>::primary_key`.
///
/// This is the trait-aware companion to `construct_query_from_condition`,
/// used by `db::find_by_id` so that the WHERE-clause column name and the
/// `$1` placeholder reflect the table's actual primary-key column.
///
/// @tparam Scheme The type representing the database table scheme.
///                Must satisfy the `db::DbTable` concept.
/// @return A `db::details::static_string` containing the generated SQL query string.
template <::db::DbTable Scheme>
consteval auto construct_find_by_pk_query() noexcept {
    constexpr auto kDbName = []() {
        constexpr std::string_view db_name_str = ::db::table_traits<Scheme>::table_name;
        ::db::details::static_string<db_name_str.size()> res{};
        res += db_name_str;
        return res;
    }();
    constexpr auto kPkName = []() {
        constexpr std::string_view pk_str = ::db::table_traits<Scheme>::primary_key;
        ::db::details::static_string<pk_str.size()> res{};
        res += pk_str;
        return res;
    }();

    constexpr auto kStatementBegin = ::db::details::static_string("SELECT * FROM ");
    constexpr auto kSelectWhere    = kStatementBegin + kDbName + ::db::details::static_string(" WHERE ");
    return kSelectWhere + kPkName + ::db::details::static_string(" = $1;");
}

/// @brief Constructs a `DELETE FROM <table> WHERE <pk> = $1;` query at
///        compile time, where `<pk>` is `table_traits<Scheme>::primary_key`.
///
/// This is the trait-aware companion to
/// `construct_delete_query_from_condition`, used by
/// `db::delete_record_by_id` so that the WHERE clause references the
/// table's actual primary-key column rather than a hard-coded `id`.
///
/// @tparam Scheme The type representing the database table scheme.
///                Must satisfy the `db::DbTable` concept.
/// @return A `db::details::static_string` containing the generated SQL query string.
template <::db::DbTable Scheme>
consteval auto construct_delete_by_pk_query() noexcept {
    constexpr auto kDbName = []() {
        constexpr std::string_view db_name_str = ::db::table_traits<Scheme>::table_name;
        ::db::details::static_string<db_name_str.size()> res{};
        res += db_name_str;
        return res;
    }();
    constexpr auto kPkName = []() {
        constexpr std::string_view pk_str = ::db::table_traits<Scheme>::primary_key;
        ::db::details::static_string<pk_str.size()> res{};
        res += pk_str;
        return res;
    }();

    constexpr auto kStatementBegin = ::db::details::static_string("DELETE FROM ");
    constexpr auto kDeleteWhere    = kStatementBegin + kDbName + ::db::details::static_string(" WHERE ");
    return kDeleteWhere + kPkName + ::db::details::static_string(" = $1;");
}

}  // namespace db::sql::utils
