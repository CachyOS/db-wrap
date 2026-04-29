#include "doctest_compatibility.h"

#include <db_wrap/db_cursor.hpp>
#include <db_wrap/db_utils.hpp>
#include <db_wrap/table_traits.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include <pqxx/pqxx>

using namespace std::string_view_literals;

namespace {

inline constexpr auto CONNECTION_URL = "postgresql://postgres:password@localhost:15432/testdb"sv;

inline constexpr auto kCreateTable = R"~(
CREATE SCHEMA IF NOT EXISTS __pgtest;
DROP TABLE IF EXISTS __pgtest.cursor_users;
CREATE TABLE __pgtest.cursor_users (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL
);
INSERT INTO __pgtest.cursor_users (id, name)
SELECT i, 'user_' || i::text FROM generate_series(1, 1000) AS i;
)~";

inline constexpr auto kDropTable = "DROP TABLE IF EXISTS __pgtest.cursor_users;"sv;

struct CursorUser {
    static constexpr std::string_view kName = "__pgtest.cursor_users";

    std::int64_t id;
    std::string  name;
};

auto run_ddl(pqxx::connection& conn, std::string_view ddl) -> bool {
    try {
        pqxx::work txn{conn};
        auto r = txn.exec(ddl);
        (void)r;
        txn.commit();
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

}  // namespace

TEST_CASE("cursor iterates all rows in a parameterless SELECT")
{
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE(cx.is_open());
    REQUIRE(run_ddl(cx, kCreateTable));

    {
        pqxx::work txn{cx};
        auto       cur = db::open_cursor<CursorUser>(
            txn, "SELECT id, name FROM __pgtest.cursor_users ORDER BY id",
            db::cursor_options{.batch_size = 100});

        std::vector<CursorUser> collected;
        for (const auto& row : cur) {
            collected.push_back(row);
        }
        txn.commit();

        REQUIRE_EQ(collected.size(), 1000);
        for (std::size_t i = 0; i < collected.size(); ++i) {
            CHECK_EQ(collected[i].id, static_cast<std::int64_t>(i + 1));
            CHECK_EQ(collected[i].name, "user_" + std::to_string(i + 1));
        }
    }

    REQUIRE(run_ddl(cx, kDropTable));
}

TEST_CASE("cursor substitutes $N parameters via inline quoting")
{
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE(cx.is_open());
    REQUIRE(run_ddl(cx, kCreateTable));

    {
        pqxx::work txn{cx};
        auto       cur = db::open_cursor<CursorUser>(
            txn,
            "SELECT id, name FROM __pgtest.cursor_users WHERE id > $1 ORDER BY id",
            db::cursor_options{.batch_size = 50},
            std::int64_t{500});

        std::vector<CursorUser> collected;
        for (const auto& row : cur) {
            collected.push_back(row);
        }
        txn.commit();

        REQUIRE_EQ(collected.size(), 500);
        CHECK_EQ(collected.front().id, 501);
        CHECK_EQ(collected.back().id, 1000);
    }

    REQUIRE(run_ddl(cx, kDropTable));
}

TEST_CASE("two cursors in the same transaction are independent")
{
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE(cx.is_open());
    REQUIRE(run_ddl(cx, kCreateTable));

    {
        pqxx::work txn{cx};

        auto cur_a = db::open_cursor<CursorUser>(
            txn, "SELECT id, name FROM __pgtest.cursor_users WHERE id <= 500 ORDER BY id",
            db::cursor_options{.batch_size = 100});

        auto cur_b = db::open_cursor<CursorUser>(
            txn, "SELECT id, name FROM __pgtest.cursor_users WHERE id > 500 ORDER BY id",
            db::cursor_options{.batch_size = 100});

        std::vector<CursorUser> a_rows;
        std::vector<CursorUser> b_rows;

        auto it_a = cur_a.begin();
        auto it_b = cur_b.begin();
        while (it_a != cur_a.end() || it_b != cur_b.end()) {
            if (it_a != cur_a.end()) {
                a_rows.push_back(*it_a);
                ++it_a;
            }
            if (it_b != cur_b.end()) {
                b_rows.push_back(*it_b);
                ++it_b;
            }
        }
        txn.commit();

        REQUIRE_EQ(a_rows.size(), 500);
        REQUIRE_EQ(b_rows.size(), 500);
        CHECK_EQ(a_rows.front().id, 1);
        CHECK_EQ(a_rows.back().id, 500);
        CHECK_EQ(b_rows.front().id, 501);
        CHECK_EQ(b_rows.back().id, 1000);
    }

    REQUIRE(run_ddl(cx, kDropTable));
}

TEST_CASE("early-destroyed cursor does not poison the transaction")
{
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE(cx.is_open());
    REQUIRE(run_ddl(cx, kCreateTable));

    {
        pqxx::work txn{cx};

        {
            auto cur = db::open_cursor<CursorUser>(
                txn, "SELECT id, name FROM __pgtest.cursor_users ORDER BY id",
                db::cursor_options{.batch_size = 10});
            auto it = cur.begin();
            for (int read = 0; read < 3 && it != cur.end(); ++read, ++it) {
                (void)*it;
            }
        }

        auto r = txn.exec("SELECT COUNT(*) FROM __pgtest.cursor_users"sv);
        REQUIRE_EQ(r[0][0].as<std::int64_t>(), 1000);
        txn.commit();
    }

    REQUIRE(run_ddl(cx, kDropTable));
}

TEST_CASE("small batch size drains in full + partial batches")
{
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE(cx.is_open());
    REQUIRE(run_ddl(cx, kCreateTable));

    {
        pqxx::work txn{cx};
        auto       cur = db::open_cursor<CursorUser>(
            txn,
            "SELECT id, name FROM __pgtest.cursor_users WHERE id <= $1 ORDER BY id",
            db::cursor_options{.batch_size = 1},
            std::int64_t{3});

        std::vector<CursorUser> collected;
        for (const auto& row : cur) {
            collected.push_back(row);
        }
        txn.commit();

        REQUIRE_EQ(collected.size(), 3);
        CHECK_EQ(collected[0].id, 1);
        CHECK_EQ(collected[1].id, 2);
        CHECK_EQ(collected[2].id, 3);
    }

    REQUIRE(run_ddl(cx, kDropTable));
}

TEST_CASE("empty result set is immediately at end()")
{
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE(cx.is_open());
    REQUIRE(run_ddl(cx, kCreateTable));

    {
        pqxx::work txn{cx};
        auto       cur = db::open_cursor<CursorUser>(
            txn, "SELECT id, name FROM __pgtest.cursor_users WHERE id < 0",
            db::cursor_options{.batch_size = 100});

        CHECK(cur.begin() == cur.end());
        txn.commit();
    }

    REQUIRE(run_ddl(cx, kDropTable));
}

TEST_CASE("explicit fetch_next yields the same rows as iteration")
{
    pqxx::connection cx(CONNECTION_URL.data());
    REQUIRE(cx.is_open());
    REQUIRE(run_ddl(cx, kCreateTable));

    {
        pqxx::work txn{cx};
        auto       cur = db::open_cursor<CursorUser>(
            txn, "SELECT id, name FROM __pgtest.cursor_users WHERE id <= 250 ORDER BY id",
            db::cursor_options{.batch_size = 100});

        auto first  = cur.fetch_next(100);
        auto second = cur.fetch_next(100);
        auto third  = cur.fetch_next(100);
        auto fourth = cur.fetch_next(100);
        txn.commit();

        CHECK_EQ(first.size(), 100);
        CHECK_EQ(second.size(), 100);
        CHECK_EQ(third.size(), 50);
        CHECK_EQ(fourth.size(), 0);
        CHECK_EQ(first.front().id, 1);
        CHECK_EQ(third.back().id, 250);
    }

    REQUIRE(run_ddl(cx, kDropTable));
}
