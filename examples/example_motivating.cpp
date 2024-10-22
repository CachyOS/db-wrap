#include <iostream>
#include <string>

#include <db_wrap/db_api.hpp>

#include <pqxx/pqxx>

struct UserInfo {
    /// `kName` is name table in DB
    static constexpr auto kName = "user_infos";
    std::int64_t id;
    std::string name, email, login;
};

namespace {

auto ask_user_for_friend(UserInfo&& info) -> UserInfo {
    // just for demo
    std::cout << "Asking for " << std::move(info).name << " friend!\n";
    return UserInfo{2, "abc", "abc@example.com", "abc"};
}

auto retrieve_friend(pqxx::connection& conn, std::string_view name) -> UserInfo {
    auto info = db::utils::one_row_as<UserInfo>(
        conn,
        "SELECT id, name, email, login FROM user_infos WHERE name = $1",
        name
    );

    if (!info.has_value()) {
        throw std::runtime_error("User not found");
    }

    auto friend_info = ask_user_for_friend(std::move(*info));
    db::insert_record(conn, friend_info);

    return friend_info;
}

} // namespace

inline constexpr auto kCreateTable = R"~(
CREATE TEMPORARY TABLE IF NOT EXISTS user_infos (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  email TEXT NOT NULL,
  login TEXT NOT NULL
);
)~";
inline constexpr auto CONNECTION_URL = "postgresql://postgres:password@localhost:15432/testdb";

int main() {
    // Create a connection object
    pqxx::connection conn(CONNECTION_URL);

    db::utils::exec_affected(conn, kCreateTable);
    UserInfo new_user{0, "John Doe", "johndoe@example.com", "johndoe"};
    db::insert_record(conn, new_user);

    // Retrieve friend info and insert
    try {
        auto friend_info = retrieve_friend(conn, "John Doe");
        std::cout << "Friend info retrieved and inserted successfully!\n";
        std::cout << "Name: " << friend_info.name << ", Email: " << friend_info.email
                  << ", Login: " << friend_info.login << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
