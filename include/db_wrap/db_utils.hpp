/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

#include <db_wrap/details/sql_impl.hpp>
#include <db_wrap/details/unpack_fields_impl.hpp>

#include <algorithm>    // for transform
#include <optional>     // for optional
#include <ranges>       // for ranges::*
#include <string_view>  // for string_view
#include <vector>       // for vector

#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>

#include <pqxx/pqxx>

namespace db::utils {

/// @brief Helper function to convert a pqxx::row to a user-defined type.
///
/// This function utilizes Boost.PFR to iterate over the fields of the
/// user-defined type `T` and extract the corresponding values from the
/// `pqxx::row`.
///
/// @tparam T The type to convert the row to.
/// @param row The pqxx::row to convert.
/// @return An object of type `T` filled with the data from the row.
template <typename T>
constexpr T from_row(pqxx::row&& row) noexcept {
    T obj{};
    boost::pfr::for_each_field_with_name(obj, [&](std::string_view field_name, auto& field) {
        field = row[pqxx::zview(field_name)].as<std::decay_t<decltype(field)>>();
    });
    return obj;
}

/// @brief Helper function to convert a pqxx::row to a user-defined type.
///
/// This function utilizes Boost.PFR to iterate over the fields of the
/// user-defined type `T` and extract the corresponding values from the
/// `pqxx::row`. It assumes that the order of fields in `T` matches the
/// order of columns in the row.
///
/// @tparam T The type to convert the row to.
/// @param row The pqxx::row to convert.
/// @return An object of type `T` filled with the data from columns.
template <typename T>
constexpr T from_columns(pqxx::row&& row) {
    T obj{};
    boost::pfr::for_each_field(obj, [&](auto& field, std::size_t index) {
        field = row[index].as<std::decay_t<decltype(field)>>();
    });
    return obj;
}

/// @brief Extracts all rows from a pqxx::result and converts them to a
///        vector of the specified type.
///
/// This function iterates through all rows in the `pqxx::result` and uses
/// the `from_row` function to convert each row to an object of type `T`.
/// The converted objects are stored in a vector.
///
/// @tparam T The type to convert each row to.
/// @param result The pqxx::result containing the rows.
/// @return A vector of objects of type `T` representing the extracted rows.
template <typename T>
constexpr auto extract_all_rows(pqxx::result&& result) noexcept -> std::vector<T> {
    std::vector<T> rows{};
    rows.reserve(static_cast<std::size_t>(result.size()));
    std::ranges::transform(result, std::back_inserter(rows),
        [](auto&& row) { return utils::from_row<T>(std::move(row)); });
    return rows;
}

/// @brief Retrieves a single row from the database and converts it to the
///        specified type.
///
/// This function executes the provided SQL query and retrieves the first row
/// of the result set. It then uses the `utils::from_row` function to convert
/// the row data into an object of the specified type `T`.
///
/// @tparam T The type to convert the database row to. The type `T` must be
///           compatible with the `utils::from_row` function, which uses
///           Boost.PFR to perform the conversion.
/// @tparam Args The types of the query parameters.
/// @param conn The pqxx::connection object representing the database connection.
/// @param query The SQL query to execute.
/// @param args The parameters for the SQL query. The number and types of
///             parameters must match the placeholders in the query string.
/// @return An optional object of type `T` containing the data from the
///         database row. Returns `std::nullopt` if no rows are found.
///
/// @example
/// struct User {
///   int id;
///   std::string name;
///   std::string email;
/// };
///
/// // Retrieve a user with ID 1
/// auto user = db::utils::one_row_as<User>(conn, "SELECT * FROM users WHERE id = $1", 1);
///
/// if (user) {
///   std::cout << "User ID: " << user->id << std::endl;
///   std::cout << "User Name: " << user->name << std::endl;
///   std::cout << "User Email: " << user->email << std::endl;
/// } else {
///   std::cout << "User not found!" << std::endl;
/// }
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

/// @brief Executes a query and retrieves all rows as an optional vector of the specified type.
///
/// This function executes the provided SQL query, retrieves all rows from the
/// result set, and converts each row into an object of the specified type `T`
/// using the `utils::from_row` function. The converted objects are stored in
/// a `std::vector`, which is then wrapped in a `std::optional`.
///
/// If the query returns no rows, the function returns `std::nullopt`. Otherwise,
/// it returns the vector of converted objects.
///
/// @tparam T The type to convert each database row to. This type must be
///           compatible with the `utils::from_row` function, which uses
///           Boost.PFR to perform the conversion.
/// @tparam Args The types of the query parameters.
/// @param conn The pqxx::connection object representing the database connection.
/// @param query The SQL query to execute.
/// @param args The parameters for the SQL query. The number and types of
///             parameters must match the placeholders in the query string.
/// @return A `std::optional<std::vector<T>>` containing the results of the query.
///         If the query returns rows, the optional contains a vector of objects
///         of type `T`, each representing a row from the result set. If the query
///         returns no rows, the optional is empty (`std::nullopt`).
///
/// @example
/// struct Product {
///   int id;
///   std::string name;
///   double price;
/// };
///
/// // Retrieve all products with a price greater than 10.0
/// constexpr auto kSelectQuery = "SELECT * FROM products WHERE price > $1";
/// auto products = db::utils::as_set_of<Product>(conn, kSelectQuery, 10.0);
/// if (products) {
///   for (auto&& product : *products) {
///     std::cout << "Product: " << product.name << " (Price: " << product.price << ")" << std::endl;
///   }
/// } else {
///   std::cout << "Products not found!" << std::endl;
/// }
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

/// @brief Executes a SQL query and returns the number of affected rows.
///
/// This function takes a database connection (`conn`) and a SQL query string
/// (`query`), executes the query, and returns the number of rows affected by
/// the query. It uses a `pqxx::work` transaction to ensure that the query
/// is executed within a transaction context.
///
/// @param conn A reference to a `pqxx::connection` object representing the
///             database connection.
/// @param query A `std::string_view` containing the SQL query to execute.
/// @param args A parameter pack of arguments to be bound to the placeholders
///             in the SQL query.
/// @return The number of rows affected by the executed query.
///
/// @example
/// // Delete all users with the name "John Doe"
/// constexpr auto kDeleteQuery = "DELETE FROM users WHERE name = $1";
/// auto deleted_rows = db::utils::exec_affected(conn, kDeleteQuery, "John Doe");
/// std::cout << "Deleted " << deleted_rows << " rows." << std::endl;
template <typename... Args>
auto exec_affected(pqxx::connection& conn, std::string_view query, Args&&... args) -> std::size_t {
    pqxx::work txn(conn);
    pqxx::result result = txn.exec(query.data(), pqxx::params(std::forward<Args>(args)...));

    // end our transaction and return
    txn.commit();
    return static_cast<std::size_t>(result.affected_rows());
}

/// @brief Executes a query and returns the number of affected rows,
///        automatically unpacking the fields of a record for parameter binding.
///
/// This function executes the provided SQL query using `exec` and
/// returns the number of rows affected by the query. It simplifies the process
/// of binding parameters to the query by automatically unpacking the fields of
/// a record object using the `utils::unpack_fields` function.
///
/// @tparam Scheme The type representing the database table scheme. Must
///                satisfy the `sql::details::HasName` concept.
/// @param conn The pqxx::connection object representing the database connection.
/// @param query The SQL query to execute. This query should contain placeholders
///              ($1, $2, etc.) that correspond to the fields of the `record` object.
/// @param record The object containing the data to be used as parameters for the
///               query. The fields of this object are unpacked and passed to
///               `exec` in the order they are defined in the structure.
/// @return The number of rows affected by the executed query.
///
/// @example
/// struct Product {
///   static constexpr std::string_view kName = "products";
///   int id;
///   std::string name;
///   double price;
/// };
///
/// Product product{1, "Example Product", 19.99};
/// constexpr auto kUpdateQuery = "UPDATE products SET name = $2, price = $3 WHERE id = $1;";
/// std::size_t rows_affected = db::utils::exec_affected<Product>(conn, kUpdateQuery, product);
/// std::cout << "Rows affected: " << rows_affected << std::endl;
template <sql::details::HasName Scheme>
auto exec_affected(pqxx::connection& conn, std::string_view query, const Scheme& record) -> std::size_t {
    // Unroll fields using unpack_fields
    auto&& unroll_func = [&conn, &query, &record](const auto&... fields) {
        return db::utils::exec_affected(conn, query, fields...);
    };
    return db::utils::unpack_fields(std::move(unroll_func), record);
}

}  // namespace db::utils
