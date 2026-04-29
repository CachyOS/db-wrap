@page db_module_database Database Module
@tableofcontents

The Database Module exposes the `db` namespace's high-level CRUD entry
points. Every function in this module is generic over a user struct
`Scheme` whose mapping to a SQL table is described by `db::table_traits`
(see @ref db::table_traits). For legacy structs that predate
`table_traits`, a default specialization picks up `kName` + `id`
automatically — no migration is needed.

All functions in this module belong to the @ref db_api group.

## Data retrieval

### find_by_id

Look up a single row by primary key (see @ref db::find_by_id). Returns
`std::nullopt` if no row matches.

@snippet example_basic.cpp find_by_id

### get_all_records

Materialize every row of the table into a `std::vector<Scheme>` (see
@ref db::get_all_records). Returns `std::nullopt` if the table is
empty.

@snippet example_basic.cpp get_all_records

## Data manipulation

### insert_record

Insert a new row built from `record` (see @ref db::insert_record).
Columns listed in `table_traits<Scheme>::insert_exclude` are dropped
from the column list, which is the standard escape hatch for
auto-incrementing primary keys.

@snippet example_basic.cpp insert_record

### update_fields

Update a chosen subset of fields, identified by their C++ struct member
names (see @ref db::update_fields). Field names are validated at
compile time; passing a name that does not exist on `Scheme` triggers a
`static_assert`.

```cpp
User user_to_update{1, "Updated Name", "updated.email@example.com"};
db::update_fields<User, "name", "email">(conn, user_to_update);
```

### update_record

Update every eligible field of a record at once (see
@ref db::update_record). The primary-key column is read from `record`
to locate the row, but its value is never written; fields listed in
`table_traits<Scheme>::update_exclude` are also skipped.

```cpp
User user{1, "John Doe", "john.updated@example.com"};
db::update_record<User>(conn, user);
```

### delete_record_by_id

Delete a row identified by its primary-key value (see
@ref db::delete_record_by_id).

```cpp
db::delete_record_by_id<User>(conn, 1);
```

## Lower-level helpers

For workflows that need raw SQL with parameter binding rather than
generated CRUD, drop down to the @ref db_utils group — see
@ref db::utils::one_row_as, @ref db::utils::as_set_of, and
@ref db::utils::exec_affected. The compile-time SQL builders that the
high-level API is written against live on the
@ref db_module_sql_utilities page.
