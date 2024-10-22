# db-wrap: A Modern C++ Database Wrapper Library

`db-wrap` is a lightweight, header-only C++20 library that simplifies database
interactions using modern C++ features and concepts. It provides a generic
interface for database operations, currently supports only (PostgreSQL) via
the libpqxx library.

motivating example: https://www.boost.org/doc/libs/1_86_0/doc/html/boost_pfr.html#boost_pfr.intro

## Features

- **Type-Safe SQL Queries:** Use C++ types directly in queries for better
  type safety and reduced errors.
- **Automatic Parameter Binding:**  Bind parameters to queries without manual
  placeholder management.
- **Row-to-Object Mapping:**  Easily convert database rows to user-defined
  objects using Boost.PFR.
- **Compile-Time Query Generation:**  Optionally generate SQL queries at
  compile time for performance optimization and validation.

## Requirements

- C++20 compatible compiler
- libpqxx library (for PostgreSQL support)
- Boost.PFR library

## Installation

`db-wrap` is a header-only library, so no installation is required. Simply
include the header files in your project and link against libpqxx and
Boost.PFR.

## Usage

Here's a basic example demonstrating how to use `db-wrap` with libpqxx:

```cpp
#include <iostream>
#include <string_view>
#include <db_wrap/db_api.hpp>

#include <pqxx/pqxx>

struct User {
    /// `kName` is name table in DB
    static constexpr std::string_view kName = "users";

    int id;
    std::string name;
    std::string email;
};

int main() {
    // Create a connection object
    pqxx::connection conn("postgresql://user:password@host:port/database");

    // Find a user by ID
    auto user = db::find_by_id<User>(conn, 1);
    if (user) {
        std::cout << "User found: " << user->name << std::endl;
    } else {
        std::cout << "User not found!" << std::endl;
    }

    // Retrieve all users
    auto users = db::get_all_records<User>(conn);
    if (users) {
        for (auto&& user : *users) {
            std::cout << "User: " << user.name << " (" << user.email << ")" << std::endl;
        }
    } else {
        std::cout << "No users found!" << std::endl;
    }
}
```

## Examples

More examples can be found in the `examples` directory.

## Testing

Unit tests are provided in the `tests` directory. You can run the tests using
CMake's CTest framework.

## Contributing

Contributions are welcome!

## License

This library is licensed under the [MIT License](LICENSE).

