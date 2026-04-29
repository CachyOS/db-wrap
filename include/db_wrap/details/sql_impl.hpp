/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024-2026 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

#include <db_wrap/details/pfr_utils.hpp>
#include <db_wrap/details/static_string.hpp>
#include <db_wrap/table_traits.hpp>

#include <cstdint>

#include <algorithm>    // for count_if
#include <array>        // for array
#include <charconv>     // for to_chars
#include <concepts>     // for convertible_to
#include <ranges>       // for ranges::*
#include <string>       // for string
#include <string_view>  // for string_view

namespace db::sql::details {

/// @brief Append the base-10 representation of `value` to `dest` (which may
/// be a `std::string` or a `db::details::static_string<N>`).
constexpr void append_decimal(auto& dest, std::int32_t value) noexcept {
    std::array<char, 10> buf{};
    const auto end_ptr = std::to_chars(buf.data(), buf.data() + buf.size(), value).ptr;
    dest += std::string_view{buf.data(), end_ptr};
}

/// @brief Concept that checks if a type has a static member `kName`
///        representing a database table name.
///
/// The `kName` member must be convertible to `std::string_view` and
/// should not be empty. This concept is used to ensure that a type
/// can provide a valid table name for database operations.
///
/// Retained as a convenience alias for backwards compatibility. New
/// code should use `db::DbTable` from `db_wrap/table_traits.hpp`,
/// which is satisfied automatically by any type that meets this
/// legacy contract via the default `db::table_traits` specialization.
template <typename T>
concept HasName = requires(T t) {
    { T::kName } -> std::convertible_to<std::string_view>;
    !std::string_view{T::kName}.empty();
};

/// @brief Concept that checks if a type has an 'id' field.
///
/// This concept requires the type `T` to have a field named 'id'. This is
/// typically used to enforce that database table scheme types have an
/// identifier field.
///
/// Retained as a convenience alias for backwards compatibility. New
/// code should use `db::DbTable`.
template <typename T>
concept HasIdField = requires(T t) {
    t.id;
};

/// @brief Concept that checks if a type has an 'id' field
///        and satisfies the `HasName` concept.
///
/// This concept combines the requirements of `HasName` and `HasIdField`,
/// ensuring that the type `T` has a valid table name (`kName`) and an
/// 'id' field.
///
/// Retained as a convenience alias for backwards compatibility. New
/// code should use `db::DbTable`.
template <typename T>
concept HasSchemeAndId = details::HasName<T> && details::HasIdField<T>;

/// @brief Appends a formatted field assignment expression to a string.
///
/// This function constructs a string representing a field assignment expression
/// in the format "field_name = $parameter_index" and appends it to the
/// provided `dest` string. It also handles the addition of a comma separator
/// if it's not the last field in a sequence.
///
/// The function is designed to be used within loops that iterate over field
/// names to generate SQL query strings, particularly for UPDATE statements.
///
/// @param name The name of the field (already translated to its SQL column
///             name by the caller).
/// @param i A reference to the current index, which is used to determine the
///          parameter index for the assignment expression.
/// @param max_size The total number of fields being processed.
/// @param dest A reference to the destination string where the formatted
///             expression is appended.
constexpr void interpret_name(std::string_view name, std::int32_t& i, std::int32_t max_size, auto& dest) noexcept {
    using namespace std::string_view_literals;

    dest += name;
    dest += " = $"sv;
    details::append_decimal(dest, i + 2);

    if (i + 1 < max_size) {
        dest += ", "sv;
    }
    ++i;
}

/// @brief Generates an SQL UPDATE query string based on the provided scheme
///        and field names.
///
/// This function constructs an SQL query string for updating records in a
/// database table. It uses `table_traits<Scheme>::table_name` to determine
/// the table name, `table_traits<Scheme>::primary_key` to build the WHERE
/// clause, and the provided `Fields` to specify the columns to be updated.
/// `Fields` are interpreted as C++ struct member names; each is translated
/// to its SQL column name through `db::column_of<Scheme>` at emission time
/// so that any `column_overrides` declared by the caller are honored.
///
/// @tparam Scheme The type representing the database table scheme.
///                Must satisfy the `db::DbTable` concept.
/// @tparam Fields A pack of `db::details::static_string` representing the
///                names of the fields to be updated.
/// @param dest The destination string to which the generated query
///             string will be appended.
///
/// @example
/// struct MyScheme {
///   static constexpr std::string_view kName = "my_table";
/// };
///
/// int main() {
///   std::string query;
///   details::update_query_str<MyScheme, "field1", "field2">(query);
///   // query will be: "UPDATE my_table SET field1 = $2, field2 = $3 WHERE id = $1;"
/// }
template <::db::DbTable Scheme, ::db::details::static_string... Fields>
constexpr void update_query_str(auto&& dest) noexcept {
    using namespace std::string_view_literals;
    using traits = ::db::table_traits<Scheme>;

    constexpr auto kStatementBegin = "UPDATE "sv;
    constexpr auto kStatementEnd   = " WHERE "sv;

    constexpr std::int32_t size = sizeof...(Fields);

    std::int32_t i{};
    // clang<=18 does not support constexpr std::string constructor for std::string_view arg
    dest += kStatementBegin;
    dest += traits::table_name;
    dest += " SET "sv;
    (details::interpret_name(::db::column_of<Scheme>(std::string_view{Fields}), i, size, dest), ...);

    dest += kStatementEnd;
    dest += traits::primary_key;
    dest += " = $1;"sv;
}

/// @brief Validates that the provided field names are valid members of
///        the given SQL scheme structure.
///
/// This function checks if all the field names provided in `Fields` are
/// present as members in the structure represented by `sql_scheme_struct`.
/// It utilizes `utils::get_struct_names` to retrieve the member names
/// of the structure.
///
/// `Fields` are matched against C++ struct member names (not SQL column
/// names), so column overrides declared in `table_traits` do not affect
/// the result of this check.
///
/// @tparam Fields A pack of `db::details::static_string` representing the
///                field names to be validated.
/// @param sql_scheme_struct A structure representing the SQL scheme.
/// @return `true` if all field names are valid members of the structure,
///         `false` otherwise.
///
/// @example
/// struct User {
///   int id;
///   std::string name;
///   int age;
/// };
///
/// static_assert(details::validate_fields<"name", "age">(User{}));
/// // This will compile successfully as "name" and "age" are valid fields.
///
/// // static_assert(details::validate_fields<"name", "invalid_field">(User{}));
/// // This will result in a compile-time error as "invalid_field" is not a
/// // valid field in the User structure.
template <::db::details::static_string... Fields>
consteval auto validate_fields(auto&& sql_scheme_struct) noexcept -> bool {
    constexpr auto valid_fields = utils::get_struct_names<std::remove_cvref_t<decltype(sql_scheme_struct)>>();

    return (std::ranges::contains(valid_fields, std::string_view{Fields}) && ...);
}

/// @brief Generates an SQL UPDATE query string to update all fields
///        (except the primary key, and any field listed in
///        `update_exclude`) in a table based on the provided scheme.
///
/// This function constructs an SQL query string for updating all eligible
/// fields of a database table. The primary-key column is read from
/// `table_traits<Scheme>::primary_key` and used both to skip the column
/// in the SET clause and to build the WHERE clause (`WHERE <pk> = $1`).
/// Column overrides are applied when emitting each SET-clause column,
/// and any field listed in `table_traits<Scheme>::update_exclude` is
/// omitted entirely.
///
/// The generated query string is appended to the provided `dest` string.
///
/// @tparam Scheme The type representing the database table scheme.
///                Must satisfy the `db::DbTable` concept.
/// @param dest The destination string to which the generated query string
///             will be appended.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   int id;
///   std::string name;
///   int age;
/// };
///
/// int main() {
///   std::string query;
///   details::update_query_all_str<User>(query);
///   // query will be: "UPDATE users SET name = $2, age = $3 WHERE id = $1;"
/// }
template <::db::DbTable Scheme>
constexpr void update_query_all_str(auto&& dest) noexcept {
    using namespace std::string_view_literals;
    using traits = ::db::table_traits<Scheme>;

    constexpr auto kStatementBegin = "UPDATE "sv;
    constexpr auto kStatementEnd   = " WHERE "sv;

    constexpr auto valid_fields = utils::get_struct_names<std::remove_cvref_t<Scheme>>();

    constexpr std::int32_t names_size = static_cast<std::int32_t>(
        std::ranges::count_if(valid_fields, ::db::is_in_update_set<Scheme>));

    std::int32_t i{};
    // clang<=18 does not support constexpr std::string constructor for std::string_view arg
    dest += kStatementBegin;
    dest += traits::table_name;
    dest += " SET "sv;

    for (auto&& raw_name : valid_fields | std::views::filter(::db::is_in_update_set<Scheme>)) {
        details::interpret_name(::db::column_of<Scheme>(raw_name), i, names_size, dest);
    }

    dest += kStatementEnd;
    dest += std::string_view{traits::primary_key};
    dest += " = $1;"sv;
}

/// @brief Generates an SQL INSERT query string to insert all fields
///        of a record into a table based on the provided scheme.
///
/// This function constructs an SQL query string for inserting all fields of
/// a record into a database table. It uses `table_traits<Scheme>::table_name`
/// to determine the table name and walks the structure of the `Scheme`
/// type to construct the column list and values placeholders. Fields
/// declared in `table_traits<Scheme>::insert_exclude` are omitted from
/// both the column list and the placeholder list, and the placeholder
/// indices are renumbered accordingly. Column overrides declared in
/// `table_traits<Scheme>::column_overrides` are applied when emitting
/// each column name.
///
/// The generated query string is appended to the provided `dest` string.
///
/// @tparam Scheme The type representing the database table scheme.
///                Must satisfy the `db::DbTable` concept.
/// @param dest The destination string to which the generated query string
///             will be appended.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   int id;
///   std::string name;
///   int age;
/// };
///
/// int main() {
///   std::string query;
///   details::insert_query_all_str<User>(query);
///   // query will be: "INSERT INTO users (id, name, age) VALUES ($1, $2, $3);"
/// }
template <::db::DbTable Scheme>
constexpr void insert_query_all_str(auto&& dest) noexcept {
    using namespace std::string_view_literals;
    using traits = ::db::table_traits<Scheme>;

    constexpr auto kStatementBegin = "INSERT INTO "sv;
    constexpr auto valid_fields    = utils::get_struct_names<std::remove_cvref_t<Scheme>>();

    constexpr std::int32_t names_size = static_cast<std::int32_t>(
        std::ranges::count_if(valid_fields, ::db::is_in_insert_columns<Scheme>));

    // generate beginning
    {
        std::int32_t i{};
        // clang<=18 does not support constexpr std::string constructor for std::string_view arg
        dest += kStatementBegin;
        dest += traits::table_name;
        dest += " ("sv;

        for (auto&& raw_name : valid_fields | std::views::filter(::db::is_in_insert_columns<Scheme>)) {
            dest += ::db::column_of<Scheme>(raw_name);
            if (i + 1 < names_size) {
                dest += ", "sv;
            }
            ++i;
        }
        dest += ") VALUES"sv;
    }

    // generate params index
    {
        dest += " ("sv;

        for (auto&& field_idx : std::views::iota(1, names_size + 1)) {
            dest += "$"sv;
            details::append_decimal(dest, field_idx);
            if (field_idx < names_size) {
                dest += ", "sv;
            }
        }
        dest += ");"sv;
    }
}

}  // namespace db::sql::details
