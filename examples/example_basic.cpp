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

inline constexpr auto kCreateTable   = R"~(
CREATE TEMPORARY TABLE IF NOT EXISTS users (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  email TEXT NOT NULL
);
)~";
inline constexpr auto CONNECTION_URL = "postgresql://postgres:password@localhost:15432/testdb";

//! [full]
int main() {
    // Create a connection object
    pqxx::connection conn(CONNECTION_URL);

    db::utils::exec_affected(conn, kCreateTable);

    //! [insert_record]
    User new_user{0, "Alice", "alice@example.com"};
    db::insert_record(conn, new_user);
    //! [insert_record]

    //! [find_by_id]
    auto user = db::find_by_id<User>(conn, 1);
    if (user) {
        std::cout << "User found: " << user->name << std::endl;
    } else {
        std::cout << "User not found!" << std::endl;
    }
    //! [find_by_id]

    //! [get_all_records]
    auto users = db::get_all_records<User>(conn);
    if (users) {
        for (auto&& user : *users) {
            std::cout << "User: " << user.name << " (" << user.email << ")" << std::endl;
        }
    } else {
        std::cout << "No users found!" << std::endl;
    }
    //! [get_all_records]
}
//! [full]
