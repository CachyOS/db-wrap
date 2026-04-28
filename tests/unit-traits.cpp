#include "doctest_compatibility.h"

#include <db_wrap/sql_utils.hpp>
#include <db_wrap/table_traits.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

using namespace std::string_view_literals;

struct LegacyUserScheme {
    static constexpr std::string_view kName = "__test.legacy_users";

    std::int64_t id;
    std::string  name;
    std::string  email;
};

struct CustomPkUser {
    std::int64_t user_id;
    std::string  full_name;
    std::string  email_addr;
};

template <>
struct db::table_traits<CustomPkUser> {
    static constexpr std::string_view table_name  = "accounts";
    static constexpr std::string_view primary_key = "user_id";

    using primary_key_type = std::int64_t;

    static constexpr std::array<std::pair<std::string_view, std::string_view>, 2>
        column_overrides{{{"full_name", "name"}, {"email_addr", "email"}}};

    static constexpr std::array<std::string_view, 0> insert_exclude{};
    static constexpr std::array<std::string_view, 0> update_exclude{};
};

struct WithInsertExclude {
    std::int64_t id;
    std::string  name;
    std::string  created_at;
};

template <>
struct db::table_traits<WithInsertExclude> {
    static constexpr std::string_view table_name  = "events";
    static constexpr std::string_view primary_key = "id";

    using primary_key_type = std::int64_t;

    static constexpr std::array<std::pair<std::string_view, std::string_view>, 0> column_overrides{};
    static constexpr std::array<std::string_view, 1> insert_exclude{"created_at"};
    static constexpr std::array<std::string_view, 0> update_exclude{};
};

struct WithUpdateExclude {
    std::int64_t id;
    std::string  name;
    std::string  created_at;
};

template <>
struct db::table_traits<WithUpdateExclude> {
    static constexpr std::string_view table_name  = "audited";
    static constexpr std::string_view primary_key = "id";

    using primary_key_type = std::int64_t;

    static constexpr std::array<std::pair<std::string_view, std::string_view>, 0> column_overrides{};
    static constexpr std::array<std::string_view, 0> insert_exclude{};
    static constexpr std::array<std::string_view, 1> update_exclude{"created_at"};
};

TEST_CASE("DbTable concept recognises legacy and trait-specialized structs")
{
    static_assert(db::DbTable<LegacyUserScheme>);
    static_assert(db::DbTable<CustomPkUser>);
    static_assert(db::DbTable<WithInsertExclude>);
    static_assert(db::DbTable<WithUpdateExclude>);
}

TEST_CASE("default traits derive table name and pk from the legacy contract")
{
    using traits = db::table_traits<LegacyUserScheme>;
    static_assert(traits::table_name == "__test.legacy_users"sv);
    static_assert(traits::primary_key == "id"sv);
    static_assert(std::same_as<typename traits::primary_key_type, std::int64_t>);
}

TEST_CASE("column_of translates struct field names through overrides")
{
    static_assert(db::column_of<CustomPkUser>("full_name") == "name"sv);
    static_assert(db::column_of<CustomPkUser>("email_addr") == "email"sv);
    // Unmapped names fall through unchanged.
    static_assert(db::column_of<CustomPkUser>("user_id") == "user_id"sv);
    // Legacy structs have no overrides; every lookup is identity.
    static_assert(db::column_of<LegacyUserScheme>("name") == "name"sv);
}

TEST_CASE("get_pk_field_idx returns the index of the PK within the struct")
{
    // user_id is the first field of CustomPkUser.
    static_assert(db::get_pk_field_idx<CustomPkUser>() == 0);
    // id is the first field of LegacyUserScheme.
    static_assert(db::get_pk_field_idx<LegacyUserScheme>() == 0);
}

TEST_CASE("custom PK + column overrides drive UPDATE-all query generation")
{
    constexpr auto kQuery = db::sql::utils::create_update_all_query<CustomPkUser>();
    static_assert(std::string_view{kQuery}
        == "UPDATE accounts SET name = $2, email = $3 WHERE user_id = $1;"sv);
}

TEST_CASE("custom PK + column overrides drive partial UPDATE query generation")
{
    constexpr auto kQuery = db::sql::utils::create_update_query<CustomPkUser, "full_name">();
    static_assert(std::string_view{kQuery}
        == "UPDATE accounts SET name = $2 WHERE user_id = $1;"sv);

    constexpr auto kQuery2 = db::sql::utils::create_update_query<CustomPkUser, "full_name", "email_addr">();
    static_assert(std::string_view{kQuery2}
        == "UPDATE accounts SET name = $2, email = $3 WHERE user_id = $1;"sv);
}

TEST_CASE("custom PK + column overrides drive INSERT-all query generation")
{
    constexpr auto kQuery = db::sql::utils::create_insert_all_query<CustomPkUser>();
    static_assert(std::string_view{kQuery}
        == "INSERT INTO accounts (user_id, name, email) VALUES ($1, $2, $3);"sv);
}

TEST_CASE("SELECT-all, find-by-pk and delete-by-pk use the trait table name / PK")
{
    constexpr auto kSelectAll = db::sql::utils::construct_select_all_query<CustomPkUser>();
    static_assert(std::string_view{kSelectAll} == "SELECT * FROM accounts;"sv);

    constexpr auto kFind = db::sql::utils::construct_find_by_pk_query<CustomPkUser>();
    static_assert(std::string_view{kFind}
        == "SELECT * FROM accounts WHERE user_id = $1;"sv);

    constexpr auto kDelete = db::sql::utils::construct_delete_by_pk_query<CustomPkUser>();
    static_assert(std::string_view{kDelete}
        == "DELETE FROM accounts WHERE user_id = $1;"sv);
}

TEST_CASE("insert_exclude omits the column and renumbers placeholders")
{
    constexpr auto kQuery = db::sql::utils::create_insert_all_query<WithInsertExclude>();
    static_assert(std::string_view{kQuery}
        == "INSERT INTO events (id, name) VALUES ($1, $2);"sv);
}

TEST_CASE("update_exclude omits the column from the SET list")
{
    constexpr auto kQuery = db::sql::utils::create_update_all_query<WithUpdateExclude>();
    // `created_at` is excluded; only `name` remains on the SET clause.
    static_assert(std::string_view{kQuery}
        == "UPDATE audited SET name = $2 WHERE id = $1;"sv);
}

TEST_CASE("legacy default-traits UPDATE output is byte-identical to pre-refactor")
{
    // Sanity: the existing byte-identical shape from unit-utils.cpp is also
    // reachable from this test file via the same code paths.
    constexpr auto kUpdateAll = db::sql::utils::create_update_all_query<LegacyUserScheme>();
    static_assert(std::string_view{kUpdateAll}
        == "UPDATE __test.legacy_users SET name = $2, email = $3 WHERE id = $1;"sv);
}
