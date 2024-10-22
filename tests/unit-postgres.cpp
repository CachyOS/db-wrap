#include "doctest_compatibility.h"

#include <db_wrap/db_utils.hpp>
#include <db_wrap/db_api.hpp>

#include <string_view>
#include <ranges>
#include <algorithm>

using namespace std::string_view_literals;

inline constexpr auto CONNECTION_URL = "postgresql://postgres:password@localhost:15432/testdb"sv;

inline constexpr auto kCreateTable = R"~(
CREATE SCHEMA IF NOT EXISTS __pgtest;
CREATE TABLE IF NOT EXISTS __pgtest.users (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  email TEXT
);
)~";

inline constexpr auto kInsertUsers = R"~(
INSERT INTO __pgtest.users (id, name, email) VALUES
  (1, 'user1', 'user1@example.com'),
  (2, 'user2', NULL),
  (3, 'user3', 'user3@example.com');
)~";

inline constexpr auto kDropUsers = R"~(
DROP SCHEMA IF EXISTS __pgtest CASCADE;
)~";

inline constexpr auto kTestQueryAll = "SELECT * FROM __pgtest.users";
inline constexpr auto kTestDeleteAll = "DELETE FROM __pgtest.users";
inline constexpr auto kTestInsertUser = "INSERT INTO __pgtest.users VALUES ($1, $2, $3)";

namespace {

struct UserScheme {
  static constexpr std::string_view kName = "__pgtest.users";

  std::int64_t id;
  std::string name;
  std::optional<std::string> email;

  constexpr bool operator==(const UserScheme&) const = default;
};

// helper function for test case
auto execute_query(pqxx::connection& conn, std::string_view query) noexcept -> bool {
    try {
        pqxx::work txn(conn);
        pqxx::result result = txn.exec(query);
        txn.commit();
    } catch (const std::exception& ex) {
        return false;
    }
    return true;
}

// helper function to setup table for test case
inline auto setup_scheme_data(pqxx::connection& conn) noexcept -> bool {
    REQUIRE(execute_query(conn, kCreateTable));
    REQUIRE(execute_query(conn, kInsertUsers));

    return true;
}

// helper function to cleanup test env table for test case
inline auto drop_scheme_data(pqxx::connection& conn) noexcept -> bool {
    return execute_query(conn, kDropUsers);
}

} // namespace

TEST_CASE("db utils")
{
  SECTION("basic row query test")
  {
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE_EQ(cx.is_open(), true);

    pqxx::work tx{cx};

    struct testscheme { std::int64_t a,d; std::string b; double c; };
    auto res = tx.exec("SELECT 1 AS a, 'abc'::text AS b, 1.2 AS c, 3 AS d");
    REQUIRE_EQ(res.empty(), false);

    auto scheme_val = db::utils::from_row<testscheme>(res[0]);
    REQUIRE_EQ(scheme_val.a, 1);
    REQUIRE_EQ(scheme_val.b, "abc");
    REQUIRE_EQ(scheme_val.c, 1.2);
    REQUIRE_EQ(scheme_val.d, 3);

    // test from columns
    CHECK_THROWS_AS(db::utils::from_columns<testscheme>(res[0]), pqxx::conversion_error);

    struct testscheme_ordered { std::int64_t a; std::string b; double c; std::int64_t d; };
    {
      auto scheme_val_col = db::utils::from_columns<testscheme_ordered>(res[0]);
      REQUIRE_EQ(scheme_val_col.a, 1);
      REQUIRE_EQ(scheme_val_col.b, "abc");
      REQUIRE_EQ(scheme_val_col.c, 1.2);
      REQUIRE_EQ(scheme_val_col.d, 3);
    }

    res = tx.exec("SELECT 1, 'abc'::text, 1.2, 3");
    REQUIRE_EQ(res.empty(), false);
    auto scheme_val_col = db::utils::from_columns<testscheme_ordered>(res[0]);
    REQUIRE_EQ(scheme_val_col.a, 1);
    REQUIRE_EQ(scheme_val_col.b, "abc");
    REQUIRE_EQ(scheme_val_col.c, 1.2);
    REQUIRE_EQ(scheme_val_col.d, 3);
  }
  SECTION("structure result query pg_tables test")
  {
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE_EQ(cx.is_open(), true);

    pqxx::work tx{cx};

    struct pgtables_scheme { std::optional<std::string> schemaname, tablename, tableowner, tablespace; bool hasindexes, hasrules, hastriggers, rowsecurity; };
    auto res = tx.exec("SELECT * FROM pg_tables");
    REQUIRE_EQ(res.empty(), false);

    auto scheme_vals = db::utils::extract_all_rows<pgtables_scheme>(std::move(res));
    REQUIRE_EQ(scheme_vals.size(), 68);

    auto packages_table = std::ranges::find(scheme_vals, std::optional<std::string>{"pg_database"}, &pgtables_scheme::tablename);
    REQUIRE_NE(packages_table, std::ranges::end(scheme_vals));

    REQUIRE_EQ(*packages_table->schemaname, "pg_catalog");
    REQUIRE_EQ(*packages_table->tablename, "pg_database");
    REQUIRE_EQ(*packages_table->tableowner, "postgres");
    REQUIRE_EQ(packages_table->tablespace, "pg_global");
    REQUIRE_EQ(packages_table->hasindexes, true);
    REQUIRE_EQ(packages_table->hasrules, false);
    REQUIRE_EQ(packages_table->hastriggers, false);
    REQUIRE_EQ(packages_table->rowsecurity, false);
  }
  SECTION("extract one row from result as structure test")
  {
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE_EQ(cx.is_open(), true);

    constexpr auto kSelectQuerybyId = "SELECT * FROM __pgtest.users WHERE id = $1";
    // create testing table
    REQUIRE(execute_query(cx, kCreateTable));

    // test before testing data
    auto user = db::utils::one_row_as<UserScheme>(cx, kSelectQuerybyId, 1);
    REQUIRE_EQ(user, std::nullopt);

    // insert testing data
    REQUIRE(execute_query(cx, kInsertUsers));

    // test after testing data
    user = db::utils::one_row_as<UserScheme>(cx, kSelectQuerybyId, 1);
    REQUIRE_EQ(user.has_value(), true);
    REQUIRE_EQ(user->id, 1);
    REQUIRE_EQ(user->name, "user1");
    REQUIRE_EQ(user->email, "user1@example.com");

    user = db::utils::one_row_as<UserScheme>(cx, kSelectQuerybyId, 2);
    REQUIRE_EQ(user.has_value(), true);
    REQUIRE_EQ(user->id, 2);
    REQUIRE_EQ(user->name, "user2");
    REQUIRE_EQ(user->email, std::nullopt);

    user = db::utils::one_row_as<UserScheme>(cx, kSelectQuerybyId, 3);
    REQUIRE_EQ(user.has_value(), true);
    REQUIRE_EQ(user->id, 3);
    REQUIRE_EQ(user->name, "user3");
    REQUIRE_EQ(user->email, "user3@example.com");

    user = db::utils::one_row_as<UserScheme>(cx, kSelectQuerybyId, 4);
    REQUIRE_EQ(user, std::nullopt);

    // drop testing data
    REQUIRE(drop_scheme_data(cx));
  }
  SECTION("as set of scheme test")
  {
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE_EQ(cx.is_open(), true);

    // insert testing data
    REQUIRE(setup_scheme_data(cx));

    auto users = db::utils::as_set_of<UserScheme>(cx, kTestQueryAll);
    REQUIRE_EQ(users.has_value(), true);
    REQUIRE_EQ(users->size(), 3);

    auto users_vec = *users;
    REQUIRE_EQ(users_vec[0].id, 1);
    REQUIRE_EQ(users_vec[0].name, "user1");
    REQUIRE_EQ(users_vec[0].email, "user1@example.com");
    REQUIRE_EQ(users_vec[1].id, 2);
    REQUIRE_EQ(users_vec[1].name, "user2");
    REQUIRE_EQ(users_vec[1].email, std::nullopt);
    REQUIRE_EQ(users_vec[2].id, 3);
    REQUIRE_EQ(users_vec[2].name, "user3");
    REQUIRE_EQ(users_vec[2].email, "user3@example.com");

    // must be equal
    auto users_rec = db::get_all_records<UserScheme>(cx);
    REQUIRE_EQ(users_rec, users);

    auto affected_rows = db::utils::exec_affected(cx, kTestDeleteAll);
    REQUIRE_EQ(affected_rows, 3);

    users = db::utils::as_set_of<UserScheme>(cx, kTestQueryAll);
    REQUIRE_EQ(users, std::nullopt);

    // drop testing data
    REQUIRE(drop_scheme_data(cx));
  }
  SECTION("exec affected test")
  {
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE_EQ(cx.is_open(), true);

    // insert testing data
    REQUIRE(setup_scheme_data(cx));

    auto users_rec = db::get_all_records<UserScheme>(cx);
    REQUIRE_EQ(users_rec->size(), 3);

    auto affected_rows = db::utils::exec_affected(cx, "DELETE FROM __pgtest.users WHERE email IS NULL");
    REQUIRE_EQ(affected_rows, 1);

    affected_rows = db::utils::exec_affected(cx, "DELETE FROM __pgtest.users WHERE name IS NOT NULL");
    REQUIRE_EQ(affected_rows, 2);

    users_rec = db::get_all_records<UserScheme>(cx);
    REQUIRE_EQ(users_rec, std::nullopt);

    auto new_user_f = UserScheme{.id = 4, .name = "user4", .email = "user4@example.com"};
    affected_rows = db::utils::exec_affected<UserScheme>(cx, kTestInsertUser, new_user_f);
    REQUIRE_EQ(affected_rows, 1);

    auto new_user_s = UserScheme{.id = 5, .name = "user5", .email = std::nullopt};
    affected_rows = db::utils::exec_affected<UserScheme>(cx, kTestInsertUser, new_user_s);
    REQUIRE_EQ(affected_rows, 1);

    users_rec = db::get_all_records<UserScheme>(cx);
    REQUIRE_EQ(users_rec->size(), 2);
    REQUIRE_EQ(users_rec->at(0), new_user_f);
    REQUIRE_EQ(users_rec->at(1), new_user_s);

    // drop testing data
    REQUIRE(drop_scheme_data(cx));
  }
}

TEST_CASE("db api")
{
  SECTION("get all records as structure test")
  {
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE_EQ(cx.is_open(), true);

    // insert testing data
    REQUIRE(setup_scheme_data(cx));

    // before removal
    auto users_rec = db::get_all_records<UserScheme>(cx);
    REQUIRE_EQ(users_rec->size(), 3);

    auto affected_rows = db::utils::exec_affected(cx, kTestDeleteAll);
    REQUIRE_EQ(affected_rows, 3);

    // after removal
    users_rec = db::get_all_records<UserScheme>(cx);
    REQUIRE_EQ(users_rec, std::nullopt);

    // drop testing data
    REQUIRE(drop_scheme_data(cx));
  }
  SECTION("update fields test")
  {
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE_EQ(cx.is_open(), true);

    // insert testing data
    REQUIRE(setup_scheme_data(cx));

    auto updated_user = UserScheme{.id = 2, .email = "user2@example.com"};
    auto affected_rows = db::update_fields<UserScheme, "email">(cx, updated_user);
    REQUIRE_EQ(affected_rows, 1);

    // make sure the field is not NULL, e.g should remove all 3 rows
    affected_rows = db::utils::exec_affected(cx, "DELETE FROM __pgtest.users WHERE name IS NOT NULL");
    REQUIRE_EQ(affected_rows, 3);

    auto users_rec = db::get_all_records<UserScheme>(cx);
    REQUIRE_EQ(users_rec, std::nullopt);

    // drop testing data
    REQUIRE(drop_scheme_data(cx));
  }
  SECTION("delete records by id test")
  {
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE_EQ(cx.is_open(), true);

    // insert testing data
    REQUIRE(setup_scheme_data(cx));

    auto affected_rows = db::delete_record_by_id<UserScheme>(cx, 1);
    REQUIRE_EQ(affected_rows, 1);
    affected_rows = db::delete_record_by_id<UserScheme>(cx, 2);
    REQUIRE_EQ(affected_rows, 1);
    affected_rows = db::delete_record_by_id<UserScheme>(cx, 3);
    REQUIRE_EQ(affected_rows, 1);

    // make sure all removed
    auto users_rec = db::get_all_records<UserScheme>(cx);
    REQUIRE_EQ(users_rec, std::nullopt);

    // drop testing data
    REQUIRE(drop_scheme_data(cx));
  }
  SECTION("update record test")
  {
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE_EQ(cx.is_open(), true);

    // insert testing data
    REQUIRE(setup_scheme_data(cx));

    auto users_rec = db::get_all_records<UserScheme>(cx);
    REQUIRE_EQ(users_rec->size(), 3);
    auto users_vec = *users_rec;
    REQUIRE_EQ(users_vec[0].id, 1);
    REQUIRE_EQ(users_vec[0].name, "user1");
    REQUIRE_EQ(users_vec[0].email, "user1@example.com");
    REQUIRE_EQ(users_vec[1].id, 2);
    REQUIRE_EQ(users_vec[1].name, "user2");
    REQUIRE_EQ(users_vec[1].email, std::nullopt);
    REQUIRE_EQ(users_vec[2].id, 3);
    REQUIRE_EQ(users_vec[2].name, "user3");
    REQUIRE_EQ(users_vec[2].email, "user3@example.com");

    auto updated_user_f = UserScheme{.id = 1, .name = "user1-updated", .email = std::nullopt};
    auto affected_rows = db::update_record(cx, updated_user_f);
    REQUIRE_EQ(affected_rows, 1);

    auto updated_user_s = UserScheme{.id = 2, .name = "user2-updated", .email = "user2-updated@example-updated.com"};
    affected_rows = db::update_record(cx, updated_user_s);
    REQUIRE_EQ(affected_rows, 1);

    users_rec = db::get_all_records<UserScheme>(cx);
    REQUIRE_EQ(users_rec->size(), 3);

    users_vec = *users_rec;
    std::ranges::sort(users_vec, {}, &UserScheme::id);

    REQUIRE_EQ(users_vec.size(), 3);
    REQUIRE_EQ(users_vec[0].id, updated_user_f.id);
    REQUIRE_EQ(users_vec[0].name, updated_user_f.name);
    REQUIRE_EQ(users_vec[0].email, updated_user_f.email);
    REQUIRE_EQ(users_vec[1].id, updated_user_s.id);
    REQUIRE_EQ(users_vec[1].name, updated_user_s.name);
    REQUIRE_EQ(users_vec[1].email, updated_user_s.email);
    REQUIRE_EQ(users_vec[2].id, 3);
    REQUIRE_EQ(users_vec[2].name, "user3");
    REQUIRE_EQ(users_vec[2].email, "user3@example.com");

    // drop testing data
    REQUIRE(drop_scheme_data(cx));
  }
  SECTION("insert record test")
  {
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE_EQ(cx.is_open(), true);

    // insert testing data
    REQUIRE(setup_scheme_data(cx));

    auto users_rec = db::get_all_records<UserScheme>(cx);
    REQUIRE_EQ(users_rec->size(), 3);

    auto new_user = UserScheme{.id = 4, .name = "user4", .email = "user4@example.com"};
    auto affected_rows = db::insert_record(cx, new_user);
    REQUIRE_EQ(affected_rows, 1);

    users_rec = db::get_all_records<UserScheme>(cx);
    REQUIRE_EQ(users_rec->size(), 4);

    auto users_vec = *users_rec;
    std::ranges::sort(users_vec, {}, &UserScheme::id);

    REQUIRE_EQ(users_vec.size(), 4);
    REQUIRE_EQ(users_vec[3].id, new_user.id);
    REQUIRE_EQ(users_vec[3].name, new_user.name);
    REQUIRE_EQ(users_vec[3].email, new_user.email);

    // drop testing data
    REQUIRE(drop_scheme_data(cx));
  }
}
