/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

/// @file db_wrap/sql_utils.hpp
/// @brief Compile-time SQL builders under `db::sql::utils`.
/// @ingroup db_sql

#include <db_wrap/details/sql_impl.hpp>
#include <db_wrap/details/static_string.hpp>
#include <db_wrap/table_traits.hpp>

#include <cstdint>

#include <algorithm>    // for find
#include <ranges>       // for ranges::*
#include <string>       // for string
#include <string_view>  // for string_view

namespace db::sql::utils {

/// @ingroup db_sql
/// @brief Build a compile-time `UPDATE` statement for a chosen field set.
///
/// Produces `UPDATE <table> SET <f2>=$2, <f3>=$3, ... WHERE <pk>=$1;`.
/// Field names are C++ struct member names; column-name remapping
/// declared in `table_traits<Scheme>::column_overrides` is applied.
///
/// @tparam Scheme Must satisfy @ref db::DbTable.
/// @tparam Fields Pack of `db::details::static_string` C++ field names.
/// @return A `db::details::static_string` holding the rendered SQL.
/// @see https://godbolt.org/z/K71ecnoa6 for a live demonstration.
///
/// ```cpp
/// struct User {
///   static constexpr std::string_view kName = "users";
///   int id;
///   std::string name;
///   int age;
/// };
///
/// constexpr auto query = db::sql::utils::create_update_query<User, "name", "age">();
/// static_assert(query == "UPDATE users SET name = $2, age = $3 WHERE id = $1;");
/// ```
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

/// @ingroup db_sql
/// @brief Build a compile-time `SELECT * FROM <table> WHERE <cond>;`.
///
/// `<cond>` is a free-form `static_string` template argument, so callers
/// can embed any literal predicate (e.g. `"id = 1"`, `"price > 10.0"`).
/// For parameterized lookups by primary key, prefer
/// @ref construct_find_by_pk_query.
///
/// @tparam Scheme Must satisfy @ref db::DbTable.
/// @tparam query WHERE-clause body as a `static_string`.
/// @return A `db::details::static_string` holding the rendered SQL.
///
/// @snippet example_sql_gen.cpp construct_query_from_condition
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

/// @ingroup db_sql
/// @brief Build a compile-time `UPDATE` statement covering every eligible field.
///
/// Sets every column of `Scheme` except the primary key (which lives in
/// the WHERE clause) and any field declared in
/// `table_traits<Scheme>::update_exclude`.
///
/// @tparam Scheme Must satisfy @ref db::DbTable.
/// @return A `db::details::static_string` holding the rendered SQL.
///
/// ```cpp
/// constexpr auto query = db::sql::utils::create_update_all_query<User>();
/// static_assert(query == "UPDATE users SET name = $2, age = $3 WHERE id = $1;");
/// ```
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

/// @ingroup db_sql
/// @brief Build a compile-time `SELECT * FROM <table>;`.
///
/// @tparam Scheme Must satisfy @ref db::DbTable.
/// @return A `db::details::static_string` holding the rendered SQL.
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

/// @ingroup db_sql
/// @brief Build a compile-time `DELETE FROM <table> WHERE <cond>;`.
///
/// Free-form WHERE-clause counterpart of
/// @ref construct_query_from_condition. For parameterized deletion by
/// primary key, prefer @ref construct_delete_by_pk_query.
///
/// @tparam Scheme Must satisfy @ref db::DbTable.
/// @tparam query WHERE-clause body as a `static_string`.
/// @return A `db::details::static_string` holding the rendered SQL.
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

/// @ingroup db_sql
/// @brief Build a compile-time `INSERT INTO <table> (<cols>) VALUES (...);`.
///
/// Uses `table_traits<Scheme>::table_name` for the target and walks the
/// fields of `Scheme` for the column list and `$N` placeholders. Fields
/// declared in `table_traits<Scheme>::insert_exclude` are dropped from
/// both the column list and the value list, and remaining placeholders
/// are renumbered accordingly.
///
/// @tparam Scheme Must satisfy @ref db::DbTable.
/// @return A `db::details::static_string` holding the rendered SQL.
///
/// ```cpp
/// constexpr auto query = db::sql::utils::create_insert_all_query<User>();
/// static_assert(query == "INSERT INTO users (id, name, age) VALUES ($1, $2, $3);");
/// ```
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

/// @ingroup db_sql
/// @brief Build a compile-time `SELECT * FROM <table> WHERE <pk> = $1;`.
///
/// Trait-aware companion of @ref construct_query_from_condition used by
/// @ref db::find_by_id, so the WHERE-clause column name and the `$1`
/// placeholder reflect the table's actual primary-key column instead of
/// a hard-coded `id`.
///
/// @tparam Scheme Must satisfy @ref db::DbTable.
/// @return A `db::details::static_string` holding the rendered SQL.
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

/// @ingroup db_sql
/// @brief Build a compile-time `DELETE FROM <table> WHERE <pk> = $1;`.
///
/// Trait-aware companion of @ref construct_delete_query_from_condition
/// used by @ref db::delete_record_by_id, so the WHERE clause references
/// the table's actual primary-key column rather than a hard-coded `id`.
///
/// @tparam Scheme Must satisfy @ref db::DbTable.
/// @return A `db::details::static_string` holding the rendered SQL.
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
