/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

#include <db_wrap/details/pfr_utils.hpp>
#include <db_wrap/details/static_string.hpp>
#include <db_wrap/details/string_utils.hpp>

#include <cstdint>

#include <algorithm>    // for all_of, find
#include <array>        // for array
#include <concepts>     // for convertible_to
#include <ranges>       // for ranges::*
#include <string>       // for string
#include <string_view>  // for string_view
#include <vector>       // for vector

namespace db::sql::details {

/// @brief Concept that checks if a type has a static member `kName`
///        representing a database table name.
///
/// The `kName` member must be convertible to `std::string_view` and
/// should not be empty. This concept is used to ensure that a type
/// can provide a valid table name for database operations.
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
/// @param name The name of the field.
/// @param i A reference to the current index, which is used to determine the
///          parameter index for the assignment expression.
/// @param max_size The total number of fields being processed.
/// @param dest A reference to the destination string where the formatted
///             expression is appended.
constexpr void interpret_name(std::string_view name, std::int32_t& i, std::int32_t max_size, auto& dest) noexcept {
    using namespace std::string_view_literals;

    std::array<char, 10> buf{};
    utils::itoa_d(i + 2, buf.data());

    dest += name;
    dest += " = $"sv;
    dest += buf.data();

    if (i + 1 < max_size) {
        dest += ", "sv;
    }
    ++i;
}

/// @brief Generates an SQL UPDATE query string based on the provided scheme
///        and field names.
///
/// This function constructs an SQL query string for updating records in a
/// database table. It uses the `kName` member of the `Scheme` type to
/// determine the table name and the provided `Fields` to specify the
/// columns to be updated. The query assumes that the table has an "id"
/// column used for filtering the update operation.
///
/// @tparam Scheme The type representing the database table scheme.
///                Must satisfy the `HasName` concept.
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
template <HasName Scheme, ::db::details::static_string... Fields>
constexpr void update_query_str(auto&& dest) noexcept {
    using namespace std::string_view_literals;

    constexpr auto kStatementBegin = "UPDATE "sv;
    constexpr auto kStatementEnd   = " WHERE id = $1;"sv;

    constexpr std::int32_t size = sizeof...(Fields);

    std::int32_t i{};
    // clang<=18 does not support constexpr std::string constructor for std::string_view arg
    dest += kStatementBegin;
    dest += Scheme::kName;
    dest += " SET "sv;
    (details::interpret_name(Fields, i, size, dest), ...);

    dest += kStatementEnd;
}

/// @brief Validates that the provided field names are valid members of
///        the given SQL scheme structure.
///
/// This function checks if all the field names provided in `Fields` are
/// present as members in the structure represented by `sql_scheme_struct`.
/// It utilizes `utils::get_struct_names` to retrieve the member names
/// of the structure.
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
    constexpr auto fields_count = sizeof...(Fields);

    std::vector<std::string_view> fields_str{};
    fields_str.reserve(fields_count);
    (fields_str.push_back(std::string_view{Fields}), ...);

    return std::ranges::all_of(fields_str,
        [&valid_fields](auto&& field_name) { return std::ranges::find(valid_fields, field_name) != valid_fields.end(); });
}

/// @brief Generates an SQL UPDATE query string to update all fields (except "id")
///        in a table based on the provided scheme.
///
/// This function constructs an SQL query string for updating all fields of a
/// database table, excluding the "id" field, which is assumed to be used in the
/// WHERE clause for identifying the record to update. It uses the `kName` member
/// of the `Scheme` type to determine the table name and automatically generates
/// the SET clause based on the structure of the `Scheme` type.
///
/// The generated query string is appended to the provided `dest` string.
///
/// @tparam Scheme The type representing the database table scheme.
///                Must satisfy the `HasName` concept.
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
template <HasName Scheme>
constexpr void update_query_all_str(auto&& dest) noexcept {
    using namespace std::string_view_literals;

    constexpr auto kStatementBegin = "UPDATE "sv;
    constexpr auto kStatementEnd   = " WHERE id = $1;"sv;

    constexpr auto valid_fields = utils::get_struct_names<std::remove_cvref_t<Scheme>>();

    constexpr std::int32_t names_size = [&]() {
        if (std::ranges::find(valid_fields, "id"sv) != valid_fields.end()) {
            return valid_fields.size() - 1;
        }
        return valid_fields.size();
    }();

    std::int32_t i{};
    // clang<=18 does not support constexpr std::string constructor for std::string_view arg
    dest += kStatementBegin;
    dest += Scheme::kName;
    dest += " SET "sv;

    // run for each field except ID
    for (auto&& valid_field : valid_fields) {
        if (valid_field == "id"sv) {
            continue;
        }
        details::interpret_name(valid_field, i, names_size, dest);
    }

    dest += kStatementEnd;
}

/// @brief Generates an SQL INSERT query string to insert all fields
///        of a record into a table based on the provided scheme.
///
/// This function constructs an SQL query string for inserting all fields of
/// a record into a database table. It uses the `kName` member of the
/// `Scheme` type to determine the table name and automatically generates the
/// column list and values placeholders based on the structure of the `Scheme`
/// type.
///
/// The generated query string is appended to the provided `dest` string.
///
/// @tparam Scheme The type representing the database table scheme.
///                Must satisfy the `HasName` concept.
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
template <HasName Scheme>
constexpr void insert_query_all_str(auto&& dest) noexcept {
    using namespace std::string_view_literals;

    constexpr auto kStatementBegin = "INSERT INTO "sv;

    constexpr auto valid_fields       = utils::get_struct_names<std::remove_cvref_t<Scheme>>();
    constexpr std::int32_t names_size = valid_fields.size();

    // generate beginning
    {
        std::int32_t i{};
        // clang<=18 does not support constexpr std::string constructor for std::string_view arg
        dest += kStatementBegin;
        dest += Scheme::kName;
        dest += " ("sv;

        // run for each field
        // TODO(vnepogodin): refactor that later
        for (auto&& valid_field : valid_fields) {
            dest += valid_field;
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

        // run for each field
        // TODO(vnepogodin): refactor that later
        for (auto&& field_idx : std::ranges::views::iota(0, names_size)) {
            std::array<char, 10> buf{};
            utils::itoa_d(field_idx + 1, buf.data());

            dest += "$"sv;
            dest += buf.data();
            if (field_idx + 1 < names_size) {
                dest += ", "sv;
            }
        }
        dest += ");"sv;
    }
}

}  // namespace db::sql::details
