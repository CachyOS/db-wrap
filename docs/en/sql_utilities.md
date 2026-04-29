@page db_module_sql_utilities SQL Utilities Module
@tableofcontents

The SQL Utilities Module exposes `consteval` helpers under `db::sql`
and `db::sql::utils` that build SQL fragments at compile time from the
same `db::table_traits` metadata used by the @ref db_module_database.
The output is a `db::details::static_string` that can be passed
straight to @ref db::utils::as_set_of or @ref db::utils::one_row_as.

All functions on this page belong to the @ref db_sql group.

## Query construction

### construct_query_from_condition

Build a `SELECT * FROM <table> WHERE <cond>;` at compile time, with a
free-form predicate. See @ref db::sql::utils::construct_query_from_condition.

@snippet example_sql_gen.cpp construct_query_from_condition

### construct_select_all_query

Build a `SELECT * FROM <table>;`. See @ref db::sql::utils::construct_select_all_query.

```cpp
constexpr auto query = db::sql::utils::construct_select_all_query<User>();
// "SELECT * FROM users;"
```

### construct_find_by_pk_query

Trait-aware `SELECT * FROM <table> WHERE <pk> = $1;`. Used internally
by @ref db::find_by_id; reach for it directly when you want the same
SQL shape but a hand-managed transaction. See
@ref db::sql::utils::construct_find_by_pk_query.

### construct_delete_by_pk_query

Trait-aware `DELETE FROM <table> WHERE <pk> = $1;`. Used internally by
@ref db::delete_record_by_id. See
@ref db::sql::utils::construct_delete_by_pk_query.

### construct_delete_query_from_condition

`DELETE FROM <table> WHERE <cond>;` — the free-form counterpart of
`construct_find_by_pk_query`. See
@ref db::sql::utils::construct_delete_query_from_condition.

## Mutation builders

### create_update_query

`UPDATE <table> SET <f2>=$2, ... WHERE <pk>=$1;` for an explicit field
list. See @ref db::sql::utils::create_update_query.

```cpp
constexpr auto query = db::sql::utils::create_update_query<User, "name", "age">();
// "UPDATE users SET name = $2, age = $3 WHERE id = $1;"
```

### create_update_all_query

`UPDATE` covering every column of `Scheme` except the primary key and
anything in `table_traits<Scheme>::update_exclude`. See
@ref db::sql::utils::create_update_all_query.

### create_insert_all_query

`INSERT INTO <table> (<cols>) VALUES (...);` covering every column of
`Scheme` except those in `table_traits<Scheme>::insert_exclude`. See
@ref db::sql::utils::create_insert_all_query.

## Concepts and traits

The customization point used by every helper above is documented on its
own page: see `db::table_traits`. The gate concept is @ref db::DbTable.
The @ref db_traits group lists the helpers that translate struct field
names to SQL column names: @ref db::column_of,
@ref db::is_in_update_set, @ref db::is_in_insert_columns,
@ref db::get_pk_field_idx.
