#include <iostream>
#include <string_view>

#include <db_wrap/db_api.hpp>

#include <pqxx/pqxx>

struct Product {
    /// `kName` is name table in DB
    static constexpr std::string_view kName = "products";

    int id;
    std::string name;
    double price;
};

inline constexpr auto kCreateTable = R"~(
CREATE TEMPORARY TABLE IF NOT EXISTS products (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  price REAL NOT NULL
);
)~";
inline constexpr auto CONNECTION_URL = "postgresql://postgres:password@localhost:15432/testdb";

int main() {
    // Create a connection object
    pqxx::connection conn(CONNECTION_URL);

    db::utils::exec_affected(conn, kCreateTable);

    // Construct a compile-time SELECT query
    constexpr auto kSelectQuery = db::sql::utils::construct_query_from_condition<Product, "price > 10.0">();
    std::cout << "Generated SQL Query: " << std::string_view{kSelectQuery} << std::endl;

    // Use the query with as_set_of
    auto products = db::utils::as_set_of<Product>(conn, kSelectQuery);
    if (products) {
        for (auto&& product : *products) {
            std::cout << "Product: " << product.name << " (Price: " << product.price << ")" << std::endl;
        }
    } else {
        std::cout << "No products found with price > 10.0" << std::endl;
    }
}
