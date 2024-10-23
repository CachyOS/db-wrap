/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

#include <db_wrap/db_utils.hpp>
#include <db_wrap/details/pfr_utils.hpp>
#include <db_wrap/sql_utils.hpp>

#include <optional>  // for optional

#include <pqxx/pqxx>

namespace db {

/// @brief Finds a record in a database table by its unique ID.
///
/// This function constructs and executes a SELECT query to retrieve a
/// single record from the table specified by `Scheme::kName` that matches
/// the provided `id`. The result is then converted to an object of type
/// `Scheme` using the `db::utils::one_row_as` function.
///
/// The function assumes that the table has a unique "id" column that is
/// used for the lookup. The type of the `id` parameter is inferred from
/// the `id` member of the `Scheme` structure.
///
/// @tparam Scheme The type representing the database table scheme. Must
///                satisfy the `sql::details::HasSchemeAndId` concept.
/// @tparam IdType The type of the ID field in the `Scheme` structure.
///               Deduced automatically from `Scheme::id`.
/// @param conn The pqxx::connection object representing the database connection.
/// @param id The unique ID to search for.
/// @return An optional `Scheme` object representing the matching record.
///         Returns `std::nullopt` if no matching record is found.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   int id;
///   std::string name;
///   std::string email;
/// };
///
/// auto user = db::find_by_id<User>(conn, 1);
/// if (user) {
///   std::cout << "User found: " << user->name << std::endl;
/// } else {
///   std::cout << "User not found!" << std::endl;
/// }
template <sql::details::HasSchemeAndId Scheme, typename IdType = Scheme::id>
auto find_by_id(pqxx::connection& conn, IdType&& id) -> std::optional<Scheme> {
    constexpr auto kSelectQuery = sql::utils::construct_query_from_condition<Scheme, "id = $1">();
    return db::utils::one_row_as<Scheme>(conn, kSelectQuery, id);
}

/// @brief Retrieves all records from a table as an optional vector.
///
/// This function constructs and executes a SELECT query to retrieve all
/// records from the table specified by `Scheme::kName`. If the table contains
/// records, they are converted to a vector of objects of type `Scheme` using
/// the `db::utils::as_set_of` function.
///
/// The function returns an empty optional (`std::nullopt`) if the table
/// contains no records.
///
/// @tparam Scheme The type representing the database table scheme. Must
///                satisfy the `sql::details::HasSchemeAndId` concept.
/// @param conn The pqxx::connection object representing the database connection.
/// @return A `std::optional<std::vector<Scheme>>` containing the results.
///         The optional will contain a vector of `Scheme` objects if the
///         table has records; otherwise, it will be empty.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   int id;
///   std::string name;
///   std::string email;
/// };
///
/// auto all_users = db::get_all_records<User>(conn);
/// if (all_users) {
///   for (auto&& user : *all_users) {
///     std::cout << "User: " << user.name << std::endl;
///   }
/// } else {
///   std::cout << "Users not found!" << std::endl;
/// }
template <sql::details::HasSchemeAndId Scheme>
auto get_all_records(pqxx::connection& conn) -> std::optional<std::vector<Scheme>> {
    constexpr auto kSelectAllQuery = sql::utils::construct_select_all_query<Scheme>();
    return db::utils::as_set_of<Scheme>(conn, kSelectAllQuery);
}

/// @brief Updates specified fields of a record in the database.
///
/// This function constructs and executes an SQL UPDATE query to modify
/// the specified fields (`Fields`) of a record in the table defined by
/// the `Scheme` type. The record to update is identified by the "id"
/// field of the provided `record` object.
///
/// The function uses `sql::utils::create_update_query` to generate
/// the SQL query string based on the `Scheme` and the `Fields` to be
/// updated. It then uses `utils::get_field_by_name` to extract the
/// values of the specified fields from the `record` object and binds
/// them to the query parameters.
///
/// @tparam Scheme The type representing the database table scheme. Must
///                satisfy the `sql::details::HasSchemeAndId` concept.
/// @tparam Fields A pack of `db::details::static_string` representing the names
///                of the fields to update.
/// @param conn The pqxx::connection object representing the database connection.
/// @param record The object containing the data to update. The "id" field
///               of this object is used to identify the record to update.
/// @return The number of rows affected by the UPDATE query.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   int id;
///   std::string name;
///   std::string email;
/// };
///
/// User user_to_update{1, "Updated Name", "updated.email@example.com"};
/// auto affected_rows = db::update_fields<User, "name", "email">(conn, user_to_update);
/// std::cout << "Rows affected: " << affected_rows << std::endl;
template <sql::details::HasSchemeAndId Scheme, ::db::details::static_string... Fields>
auto update_fields(pqxx::connection& conn, const Scheme& record) -> std::size_t {
    static_assert(sql::details::validate_fields<Fields...>(Scheme{}), "non existent field detected!");

    constexpr auto kUpdateQuery = sql::utils::create_update_query<Scheme, Fields...>();
    return db::utils::exec_affected(conn, kUpdateQuery, record.id, db::utils::get_field_by_name<Fields>(record)...);
}

/// @brief Deletes a record from a database table by its unique ID.
///
/// This function constructs and executes a DELETE query to remove a
/// record from the table specified by Scheme::kName that matches the
/// provided id.
///
/// The function assumes that the table has a unique "id" column that is
/// used for identifying the record to be deleted.
///
/// @tparam Scheme The type representing the database table scheme. Must
///                satisfy the `sql::details::HasSchemeAndId` concept.
/// @tparam IdType The type of the ID field in the Scheme structure.
///               Deduced automatically from `Scheme::id`.
/// @param conn The pqxx::connection object representing the database connection.
/// @param id The unique ID of the record to be deleted.
/// @return The number of rows affected by the DELETE query, which should
///         typically be 1 if the record was found and deleted successfully.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   int id;
///   std::string name;
///   std::string email;
/// };
///
/// std::size_t rows_affected = db::delete_record_by_id<User>(conn, 1);
/// if (rows_affected == 1) {
///   std::cout << "User deleted successfully!" << std::endl;
/// } else {
///   std::cout << "Failed to delete user (rows affected: " << rows_affected << ")" << std::endl;
/// }
template <sql::details::HasSchemeAndId Scheme, typename IdType = Scheme::id>
auto delete_record_by_id(pqxx::connection& conn, IdType&& id) -> std::size_t {
    constexpr auto kDeleteQuery = sql::utils::construct_delete_query_from_condition<Scheme, "id = $1">();
    return db::utils::exec_affected(conn, kDeleteQuery, id);
}

/// @brief Updates a record in a database table by its ID, modifying all fields
///        except for the ID itself.
///
/// This function constructs and executes an SQL UPDATE query to modify the
/// record in the table specified by `Scheme::kName` that matches the ID
/// contained in the provided `record` object. It uses the
/// `sql::utils::create_update_all_query` function to generate the UPDATE query
/// and `db::utils::exec_affected` to execute the query and retrieve the number
/// of affected rows.
///
/// The function automatically handles the binding of all fields from the
/// `record` object as parameters to the query.
///
/// @tparam Scheme The type representing the database table scheme. Must
///                satisfy the `sql::details::HasSchemeAndId` concept.
/// @param conn The pqxx::connection object representing the database connection.
/// @param record The object containing the updated data for the record. The
///               "id" field of this object will be used to identify the record
///               to update, but its value will not be used to modify the "id"
///               column in the database.
/// @return The number of rows affected by the UPDATE query, which should
///         typically be 1 if the record was found and updated successfully.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   int id;
///   std::string name;
///   std::string email;
/// };
///
/// User user{1, "John Doe", "john.doe@example.com"};
/// user.name = "Updated Name";
/// user.email = "updated.email@example.com";
/// std::size_t rows_affected = db::update_record<User>(conn, user);
/// if (rows_affected == 1) {
///   std::cout << "User updated successfully!" << std::endl;
/// } else {
///   std::cout << "Failed to update user (rows affected: " << rows_affected << ")" << std::endl;
/// }
template <sql::details::HasSchemeAndId Scheme>
auto update_record(pqxx::connection& conn, const Scheme& record) -> std::size_t {
    constexpr auto kUpdateAllQuery = sql::utils::create_update_all_query<Scheme>();
    return db::utils::exec_affected<Scheme>(conn, kUpdateAllQuery, record);
}

/// @brief Inserts a new record into a database table and returns the number of rows affected.
///
/// This function executes a pre-constructed SQL INSERT query to insert a new
/// record into the database. The query is expected to be generated using
/// `db::sql::utils::create_insert_all_query`, which ensures that all fields of the
/// `record` object are included in the insert operation.
///
/// @tparam Scheme The type representing the database table scheme.
///                Must satisfy the `sql::details::HasSchemeAndId` concept.
/// @param conn The pqxx::connection object representing the database connection.
/// @param record The object containing the data to be inserted into the table.
/// @return The number of rows affected by the insert operation. Typically, this
///         will be 1 if the insert was successful.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   int id;
///   std::string name;
///   int age;
/// };
///
/// User new_user{1, "Bob", 30};
/// std::size_t rows_affected = db::insert_record(conn, new_user);
/// // rows_affected should be 1 if the insert was successful
template <sql::details::HasSchemeAndId Scheme>
auto insert_record(pqxx::connection& conn, const Scheme& record) -> std::size_t {
    constexpr auto kInsertAllQuery = sql::utils::create_insert_all_query<Scheme>();
    return db::utils::exec_affected<Scheme>(conn, kInsertAllQuery, record);
}

}  // namespace db
