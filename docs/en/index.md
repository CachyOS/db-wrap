# Introduction to the C++20 Meta DB Library

`db-wrap` is a powerful and flexible library designed to simplify and streamline database interactions in C++.

The library offers a high-level abstraction layer built on top of the libpqxx library, providing a more intuitive and type-safe way to interact with PostgreSQL databases.

## Key Features

- **Type-Safety:**  Leverages C++20 concepts and type traits to enforce compile-time checks, ensuring data integrity and reducing runtime errors.
- **Simplicity:** Provides convenient helper functions for common database operations, minimizing boilerplate code and improving code readability.
- **Flexibility:** Designed with extensibility in mind, allowing for potential integration with other database libraries in the future.
- **Boost.PFR Integration:**  Utilizes Boost.PFR for seamless conversion between database rows and user-defined data structures.

## Library Structure

The library is organized into the following main modules:

- **Database Module (`db` namespace):** Contains core functionalities for executing queries, retrieving data, and performing data manipulation operations (insert, update, delete).
  @ref docs/en/database.md
- **SQL Utilities Module (`db::sql::utils` namespace):** Provides helper functions for constructing SQL queries at compile time, simplifying query generation and improving type safety.
  @ref docs/en/sql_utilities.md
- **UUID Utilities Module (`db::uuids` namespace):** Offers utilities for working with UUIDs, including conversion between string representations and UUID objects.
  @ref docs/en/uuid_utilities.md

## Getting Started

To use the library, include the main header file:

```cpp
#include <db_wrap/db_api.hpp>
```

Refer to the specific documentation for each module for detailed information on available functions, usage examples, and advanced features.
