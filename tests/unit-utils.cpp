#include "doctest_compatibility.h"

#include <db_wrap/sql_utils.hpp>
#include <db_wrap/details/static_string.hpp>
#include <db_wrap/details/string_utils.hpp>
#include <db_wrap/details/pfr_utils.hpp>

#include <string_view>

using namespace std::string_view_literals;
using namespace db;

struct TestUserScheme {
  static constexpr std::string_view kName = "__test.users";

  std::int64_t id;
  std::string name;
  std::string email;
  std::string display_name;
  std::string password;
};

TEST_CASE("static_string")
{
  SECTION("empty string")
  {
    db::details::static_string<10> input;
    REQUIRE_FALSE(!input.empty());
    REQUIRE_EQ(input.size(), 0);
    REQUIRE_EQ(std::string_view{input}, std::string_view{});
  }
  SECTION("non empty string")
  {
    db::details::static_string<10> input;
    input += "abcd"sv;
    REQUIRE_FALSE(input.empty());
    REQUIRE_EQ(input.size(), 4);
    REQUIRE_EQ(std::string_view{input}, "abcd"sv);
  }
  SECTION("string init constexpr")
  {
    static_assert(db::details::static_string("one") != "two"sv);
    static_assert(db::details::static_string("two") == "two"sv);
  }
  SECTION("string addition")
  {
    db::details::static_string<10> input;
    input += "one"sv;
    REQUIRE_FALSE(input.empty());

    REQUIRE_EQ(input.size(), 3);
    REQUIRE_EQ(input, "one"sv);
    REQUIRE_EQ(std::string_view{input}, "one"sv);

    input += "two"sv;
    REQUIRE_EQ(input.size(), 6);
    REQUIRE_EQ(input, "onetwo"sv);
    REQUIRE_EQ(std::string_view{input}, "onetwo"sv);
  }
  SECTION("string concat")
  {
    static_assert(db::details::static_string("one") + db::details::static_string("two") == "onetwo"sv);
    static_assert(db::details::static_string("hello, ") + db::details::static_string("world!") == "hello, world!"sv);
  }
}

TEST_CASE("itoa_d")
{
  SECTION("zero")
  {
    char output[10];
    utils::itoa_d(0, output);
    REQUIRE_EQ(std::string_view{output}, "0"sv);
  }
  SECTION("9")
  {
    char output[10];
    utils::itoa_d(9, output);
    REQUIRE_EQ(std::string_view{output}, "9"sv);
  }
  SECTION("678109823")
  {
    char output[10];
    utils::itoa_d(678109823, output);
    REQUIRE_EQ(std::string_view{output}, "678109823"sv);
  }
  SECTION("10m")
  {
    char output[10];
    utils::itoa_d(10000000, output);
    REQUIRE_EQ(std::string_view{output}, "10000000"sv);
  }
}

TEST_CASE("get struct names")
{
  SECTION("simple")
  {
    struct simple { int32_t one; int64_t two; };
    constexpr auto struct_names = utils::get_struct_names<simple>();
    static_assert(struct_names.size() == 2);
    static_assert(utils::get_fields_count<simple>() == struct_names.size());
    static_assert(struct_names[0] == "one"sv);
    static_assert(struct_names[1] == "two"sv);
  }
  SECTION("multiple types")
  {
    struct multiple_types { int32_t one, two, tree; std::string_view seven, eight, nine; };
    constexpr auto struct_names = utils::get_struct_names<multiple_types>();
    static_assert(struct_names.size() == 6);
    static_assert(utils::get_fields_count<multiple_types>() == struct_names.size());
    static_assert(struct_names[0] == "one"sv);
    static_assert(struct_names[1] == "two"sv);
    static_assert(struct_names[2] == "tree"sv);
    static_assert(struct_names[3] == "seven"sv);
    static_assert(struct_names[4] == "eight"sv);
    static_assert(struct_names[5] == "nine"sv);
  }
}

TEST_CASE("validate struct names")
{
  SECTION("simple")
  {
    struct simple { int32_t one; int64_t two; };
    static_assert(sql::details::validate_fields<"one", "two">(simple{}));
    static_assert(!sql::details::validate_fields<"one", "to">(simple{}));
  }
  SECTION("multiple types")
  {
    struct multiple_types { int32_t one, two, tree; std::string_view seven, eight, nine; };
    static_assert(sql::details::validate_fields<"one", "two", "tree", "seven", "eight", "nine">(multiple_types{}));
    static_assert(!sql::details::validate_fields<"one", "two", "tree", "seven", "eght", "nine">(multiple_types{}));
  }
}

TEST_CASE("get field by name")
{
  SECTION("simple")
  {
    struct simple { int32_t one; int64_t two; };
    static constexpr simple input{ 1, 2 };
    static_assert(utils::get_field_by_name<"one">(input) == 1);
    static_assert(utils::get_field_by_name<"two">(input) == 2);
  }
  SECTION("multiple types")
  {
    struct multiple_types { int32_t one, two, tree; std::string_view seven, eight, nine; };
    static constexpr multiple_types input{ 1, 2, 3, "seven"sv, "eight"sv, "nine"sv };
    static_assert(utils::get_field_by_name<"one">(input) == 1);
    static_assert(utils::get_field_by_name<"two">(input) == 2);
    static_assert(utils::get_field_by_name<"tree">(input) == 3);
    static_assert(utils::get_field_by_name<"seven">(input) == "seven"sv);
    static_assert(utils::get_field_by_name<"eight">(input) == "eight"sv);
    static_assert(utils::get_field_by_name<"nine">(input) == "nine"sv);
  }
}

TEST_CASE("get fields count")
{
  SECTION("simple")
  {
    struct simple { int32_t one; int64_t two; };
    static_assert(utils::get_fields_count<simple>() == 2);
  }
  SECTION("multiple types")
  {
    struct multiple_types { int32_t one, two, tree; std::string_view seven, eight, nine; };
    static_assert(utils::get_fields_count<multiple_types>() == 6);
  }
}

TEST_CASE("static sql update all")
{
  SECTION("multiple fields")
  {
    constexpr auto kUpdateQuery = sql::utils::create_update_all_query<TestUserScheme>();
    static_assert(kUpdateQuery == "UPDATE __test.users SET name = $2, email = $3, display_name = $4, password = $5 WHERE id = $1;"sv);
  }
}

TEST_CASE("static sql update")
{
    SECTION("single field")
    {
        static_assert(sql::utils::create_update_query<TestUserScheme, "packages">() == "UPDATE __test.users SET packages = $2 WHERE id = $1;"sv);
        static_assert(sql::utils::create_update_query<TestUserScheme, "name">() == "UPDATE __test.users SET name = $2 WHERE id = $1;"sv);
    }
    SECTION("multiple fields")
    {
        static_assert(sql::utils::create_update_query<TestUserScheme, "packages", "version">() == "UPDATE __test.users SET packages = $2, version = $3 WHERE id = $1;"sv);
        static_assert(sql::utils::create_update_query<TestUserScheme, "name", "updated">() == "UPDATE __test.users SET name = $2, updated = $3 WHERE id = $1;"sv);
    }
    SECTION("many fields")
    {
        static_assert(sql::utils::create_update_query<TestUserScheme, "pkgbase", "packages", "status", "skip_reason", "repository", "march",
            "version", "repo_version", "build_time_start", "updated", "hash", "last_version_build",
            "last_verified", "debug_symbols", "max_rss", "u_time", "s_time", "io_in", "io_out", "tag_rev">()
        == "UPDATE __test.users SET pkgbase = $2, packages = $3, status = $4, skip_reason = $5, repository = $6, march = $7, version = $8, repo_version = $9, build_time_start = $10, updated = $11, hash = $12, last_version_build = $13, last_verified = $14, debug_symbols = $15, max_rss = $16, u_time = $17, s_time = $18, io_in = $19, io_out = $20, tag_rev = $21 WHERE id = $1;"sv);
    }
}

TEST_CASE("static sql insert all")
{
  SECTION("multiple fields")
  {
    constexpr auto kInsertQuery = sql::utils::create_insert_all_query<TestUserScheme>();
    static_assert(kInsertQuery == "INSERT INTO __test.users (id, name, email, display_name, password) VALUES ($1, $2, $3, $4, $5);"sv);
  }
}

TEST_CASE("static sql query")
{
    SECTION("basic select all query")
    {
        static_assert(sql::utils::construct_select_all_query<TestUserScheme>() == "SELECT * FROM __test.users;"sv);
    }
    SECTION("single field")
    {
        static_assert(sql::utils::construct_query_from_condition<TestUserScheme, "name = $1">() == "SELECT * FROM __test.users WHERE name = $1;"sv);
        static_assert(sql::utils::construct_query_from_condition<TestUserScheme, "another = $1">() == "SELECT * FROM __test.users WHERE another = $1;"sv);
        static_assert(sql::utils::construct_delete_query_from_condition<TestUserScheme, "another = $1">() == "DELETE FROM __test.users WHERE another = $1;"sv);
    }
    SECTION("multiple fields")
    {
        static_assert(sql::utils::construct_query_from_condition<TestUserScheme, "name = $1 AND age = $2">() == "SELECT * FROM __test.users WHERE name = $1 AND age = $2;"sv);
        static_assert(sql::utils::construct_query_from_condition<TestUserScheme, "another = $1 OR smth = $2">() == "SELECT * FROM __test.users WHERE another = $1 OR smth = $2;"sv);
        static_assert(sql::utils::construct_delete_query_from_condition<TestUserScheme, "another = $1 OR smth = $2">() == "DELETE FROM __test.users WHERE another = $1 OR smth = $2;"sv);
    }
    SECTION("many fields")
    {
        static_assert(sql::utils::construct_query_from_condition<TestUserScheme, "name = $1 AND age = $2 OR paid = $3 AND wallet <> $4">() == "SELECT * FROM __test.users WHERE name = $1 AND age = $2 OR paid = $3 AND wallet <> $4;"sv);
        static_assert(sql::utils::construct_delete_query_from_condition<TestUserScheme, "name = $1 AND age = $2 OR paid = $3 AND wallet <> $4">() == "DELETE FROM __test.users WHERE name = $1 AND age = $2 OR paid = $3 AND wallet <> $4;"sv);
    }
}
