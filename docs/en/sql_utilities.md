# SQL Utilities Module (`db::sql::utils` Namespace)

The SQL Utilities Module provides helper functions for constructing SQL queries at compile time.

## Query Construction Functions

- **`db::sql::utils::construct_query_from_condition<Scheme, Condition>()`:**
    - Generates a compile-time SQL SELECT query string with a WHERE clause.
    - Example:
        ```cpp
        constexpr auto query = db::sql::utils::construct_query_from_condition<User, "id = 1">();
        ```

- **`db::sql::utils::construct_select_all_query<Scheme>()`:**
    - Generates a compile-time SQL SELECT query string to retrieve all rows from a table.
    - Example:
        ```cpp
        constexpr auto query = db::sql::utils::construct_select_all_query<User>();
        ```

## Concepts

- **`HasName`:** Ensures that a type has a static member `kName` for the table name.
- **`HasIdField`:** Checks if a type has an 'id' field.
- **`HasSchemeAndId`:** Combines `HasName` and `HasIdField` requirements.

## Helper Structures and Functions

- **`IdFieldType`:**  A helper struct to determine the type of the "id" field in a type.
- **`get_struct_names`:**  Retrieves the names of the members of a structure as a `boost::pfr::flat_names_array`.
- **`get_field_idx_by_name`:** Gets the index of a field in a structure by its name.
- **`get_field_by_name`:** Gets a field from a structure by its name.
