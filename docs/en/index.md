@mainpage db-wrap

`db-wrap` is a type-safe, compile-time-checked PostgreSQL wrapper built on
top of [libpqxx](https://pqxx.org/). It turns plain C++ structs into
queryable rows by deriving the SQL it needs from a small `table_traits`
specialization (or, for legacy code, from a `kName` + `id` convention).

## Modules

- @subpage db_module_database — high-level CRUD on the `db` namespace
- @subpage db_module_sql_utilities — compile-time query construction
- @subpage db_module_cursor — server-side SQL cursors for streaming reads
- @subpage db_module_uuid_utilities — UUID helpers under `db::uuids`

The matching API-Groups taxonomy is generated from
`docs/en/cpp/def_groups.hpp` and is reachable from the "API Groups" tab:

- @ref db_api — high-level CRUD entry points
- @ref db_utils — row-mapping and exec helpers
- @ref db_sql — `consteval` SQL builders
- @ref db_cursor — `cursor<Scheme>` and `open_cursor`
- @ref db_uuids — UUID utilities
- @ref db_traits — `table_traits<T>` customization point

## Key features

- **Type safety.** Every public API is gated by the @ref db::DbTable
  concept and rejects untyped queries at compile time.
- **No runtime SQL string construction.** Helpers like
  @ref db::sql::utils::create_update_query produce
  `db::details::static_string` values during constant evaluation.
- **Boost.PFR-based row mapping.** Struct fields and SQL columns are
  matched by name; `column_overrides` lets you override individual
  mappings without touching the struct.
- **Optional PostgreSQL cursors** for streaming over large result sets
  in batches.

## Quick start

```cpp
#include <db_wrap/db_api.hpp>
```

A minimal CRUD round-trip looks like this — the snippet is pulled from
the runnable example program `examples/example_basic.cpp`:

@snippet example_basic.cpp full

## Examples

Three full programs ship under `examples/` and show up under the
"Examples" tab:

- `example_basic.cpp` — `insert_record`, `find_by_id`, `get_all_records`.
- `example_motivating.cpp` — mixing @ref db::utils::one_row_as with
  high-level CRUD.
- `example_sql_gen.cpp` — driving `as_set_of` with a `consteval`-built
  query.
