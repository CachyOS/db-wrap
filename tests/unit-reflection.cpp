#include "doctest_compatibility.h"

#include <db_wrap/details/reflect.hpp>
#include <db_wrap/table_traits.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

using namespace std::string_view_literals;

// -----------------------------------------------------------------------------
// Simple legacy-shaped struct — default `table_traits` specialization kicks
// in via the `LegacyScheme` concept.
// -----------------------------------------------------------------------------
namespace {
struct ReflectUser {
    static constexpr std::string_view kName = "__test.reflect_users";

    std::int64_t id;
    std::string  name;
    std::string  email;
    std::string  display_name;
    std::string  password;
};
}  // namespace

// -----------------------------------------------------------------------------
// Struct with column_overrides so the "with_column_name" helper has something
// to translate.
// -----------------------------------------------------------------------------
namespace {
struct ReflectOverride {
    std::int64_t user_id;
    std::string  full_name;
    std::string  email_addr;
};
}  // namespace

template <>
struct db::table_traits<ReflectOverride> {
    static constexpr std::string_view table_name  = "accounts";
    static constexpr std::string_view primary_key = "user_id";

    using primary_key_type = std::int64_t;

    static constexpr std::array<std::pair<std::string_view, std::string_view>, 2>
        column_overrides{{{"full_name", "name"}, {"email_addr", "email"}}};

    static constexpr std::array<std::string_view, 0> insert_exclude{};
    static constexpr std::array<std::string_view, 0> update_exclude{};
};

TEST_CASE("reflect::fields_count matches the number of struct members")
{
    static_assert(db::reflect::fields_count<ReflectUser>() == 5);
    static_assert(db::reflect::fields_count<ReflectOverride>() == 3);
}

TEST_CASE("reflect::field_names returns struct member names in order")
{
    constexpr auto names = db::reflect::field_names<ReflectUser>();
    static_assert(names.size() == 5);
    static_assert(names[0] == "id"sv);
    static_assert(names[1] == "name"sv);
    static_assert(names[2] == "email"sv);
    static_assert(names[3] == "display_name"sv);
    static_assert(names[4] == "password"sv);
}

TEST_CASE("reflect::for_each_field_with_name visits every field with its raw name")
{
    ReflectUser u{.id = 7, .name = "a", .email = "b", .display_name = "c", .password = "d"};

    std::size_t visits = 0;
    std::string concatenated_names;
    db::reflect::for_each_field_with_name(u, [&](std::string_view name, auto& /*field*/) {
        ++visits;
        concatenated_names += name;
        concatenated_names += '|';
    });

    CHECK_EQ(visits, 5);
    CHECK_EQ(concatenated_names, "id|name|email|display_name|password|");
}

TEST_CASE("reflect::for_each_field_with_column_name applies column overrides")
{
    ReflectOverride r{.user_id = 1, .full_name = "x", .email_addr = "y"};

    std::string concatenated_columns;
    db::reflect::for_each_field_with_column_name<ReflectOverride>(r, [&](std::string_view column, auto& /*field*/) {
        concatenated_columns += column;
        concatenated_columns += '|';
    });

    // `user_id` has no override and is passed through; `full_name` and
    // `email_addr` are translated via table_traits.
    CHECK_EQ(concatenated_columns, "user_id|name|email|");
}

TEST_CASE("reflect::for_each_field_with_column_name on a legacy struct is identity")
{
    ReflectUser u{};

    std::string concatenated_columns;
    db::reflect::for_each_field_with_column_name<ReflectUser>(u, [&](std::string_view column, auto& /*field*/) {
        concatenated_columns += column;
        concatenated_columns += '|';
    });

    CHECK_EQ(concatenated_columns, "id|name|email|display_name|password|");
}

// -----------------------------------------------------------------------------
// The std::meta backend compiles only under the `bloomberg/clang-p2996`
// fork. When `DB_WRAP_HAS_STD_REFLECTION` is defined by the build system
// the dispatcher routes these exact calls through `std::meta`. In that
// configuration the assertions above must still hold byte-for-byte —
// both backends agree on field order and identifier text.
//
// This block is kept here purely for documentation: it references the
// macro but does not introduce new tests, since the body of the backend
// is transparent to callers. If a future change adds backend-specific
// behavior, the test suite can exercise it here under the guard.
// -----------------------------------------------------------------------------
#if DB_WRAP_REFLECT_BACKEND_STD
TEST_CASE("std::meta backend identity: field_names stays stable")
{
    constexpr auto names = db::reflect::field_names<ReflectUser>();
    static_assert(names.size() == 5);
    static_assert(names[0] == "id"sv);
}
#endif
