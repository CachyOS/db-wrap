# Database Module (`db` Namespace)

The Database Module provides the core functionalities for interacting with PostgreSQL databases.

## Data Retrieval

- **`db::find_by_id<Scheme>(connection&, IdType&& id)`:**
    - Retrieves a single record by its unique ID from the table specified by `Scheme`.
    - Returns `std::nullopt` if no matching record is found.
    - Example:
        ```cpp
        auto user = db::find_by_id<User>(conn, 1);
        ```
- **`db::get_all_records<Scheme>(connection&)`:**
    - Retrieves all records from the table specified by `Scheme`.
    - Returns `std::nullopt` if no records found.
    - Example:
        ```cpp
        auto users = db::get_all_records<User>(conn);
        ```

## Data Manipulation

- **`db::insert_record<T>(connection&, const Scheme&)`:**
    - Inserts a new record into the table based on the data in the object `Scheme`.
    - Example:
        ```cpp
        User new_user{1, "John Doe", "john.doe@example.com"};
        db::insert_record(conn, new_user);
        ```

- **`db::update_fields<Scheme, Fields>(connection&, const Scheme&)`:**
    - Updates specified fields of a record by its unique ID in the table specified by `Scheme`.
    - Example:
        ```cpp
        User user_to_update{1, "Updated Name", "updated.email@example.com"};
        db::update_fields<User, "name", "email">(conn, user_to_update);
        ```

- **`db::update_record<Scheme>(connection&, const Scheme&)`:**
    - Updates all fields (except "id") of a record by its unique ID in the table specified by `Scheme`.
    - Example:
        ```cpp
        User user{1, "John Doe", "john.doe@example.com"};
        user.email = "john.updated@example.com";
        db::update_record<User>(conn, user);
        ```
- **`db::delete_record_by_id<Scheme>(connection&, IdType&&)`:**
    - Deletes a record from a database table by its unique ID.
    - Example:
        ```cpp
        db::delete_record_by_id<User>(conn, 1);
        ```

## Utility Functions

- **`db::utils::one_row_as<T>(connection&, std::string_view, Args&&...)`:**
    - Retrieves a single row from the database and converts it to the specified type.
    - Returns `std::nullopt` if no rows are found.
    - Example:
        ```cpp
        auto user = db::utils::one_row_as<User>(conn, "SELECT * FROM users WHERE id = $1", 1);
        ```
- **`db::utils::as_set_of<T>(connection&, std::string_view, Args&&...)`:**
    - Retrieves all rows from the database and converts them to a vector of the specified type.
    - Returns `std::nullopt` if no rows are found.
    - Example:
        ```cpp
        auto users = db::utils::as_set_of<Product>(conn, "SELECT * FROM products WHERE price > $1", 10.0);
        ```
- **`db::utils::exec_affected(connection&, std::string_view, Args&&...)`:**
    - Executes a SQL query and returns the number of affected rows.
    - Example:
        ```cpp
        auto deleted_rows = db::utils::exec_affected(conn, "DELETE FROM users WHERE name = $1", "John Doe");
        ```
- **`db::utils::exec_affected<Scheme>(connection&, std::string_view, const Scheme&)`:**
    - Executes a query and returns the number of affected rows, automatically unpacking the fields of a record for parameter binding.
    - Example:
        ```cpp
        Product product{1, "Example Product", 19.99};
        auto rows_affected = db::utils::exec_affected<Product>(conn, "UPDATE products SET name = $2, price = $3 WHERE id = $1;", product);
        ```
