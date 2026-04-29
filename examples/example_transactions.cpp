#include <iostream>
#include <stdexcept>
#include <string_view>

#include <db_wrap/db_api.hpp>
#include <db_wrap/db_transaction.hpp>

#include <pqxx/pqxx>

struct User {
    /// `kName` is name of the table in DB
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
    pqxx::connection conn(CONNECTION_URL);
    db::utils::exec_affected(conn, kCreateTable);

    //! [atomic_inserts]
    // Two inserts that must either both succeed or both be rolled back.
    {
        db::transaction txn{conn};
        db::insert_record(txn, User{1, "Alice", "alice@example.com"});
        db::insert_record(txn, User{2, "Bob", "bob@example.com"});
        txn.commit();  // both rows visible atomically
    }
    //! [atomic_inserts]

    //! [auto_rollback]
    // Letting the transaction go out of scope without commit() rolls back.
    try {
        db::transaction txn{conn};
        db::insert_record(txn, User{3, "Carol", "carol@example.com"});
        throw std::runtime_error("something went wrong");
        // txn.commit() is never reached -> destructor aborts
    } catch (const std::exception& ex) {
        std::cerr << "rolled back: " << ex.what() << '\n';
    }
    //! [auto_rollback]

    auto users = db::get_all_records<User>(conn);
    if (users) {
        for (auto&& u : *users) {
            std::cout << "User: " << u.name << " (" << u.email << ")\n";
        }
    }
}
//! [full]
