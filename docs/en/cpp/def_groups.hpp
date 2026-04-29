/// @file def_groups.hpp
/// @brief Doxygen group taxonomy for db-wrap.
///
/// This file is documentation-only; it is parsed by doxygen but never
/// compiled. Each `@defgroup` here defines a navigable section under the
/// "API Groups" tab of the generated HTML.

/// @defgroup db_api High-level CRUD
///
/// @brief Type-safe CRUD entry points operating on table-mapped structs.
///
/// Functions in this group accept a user struct `Scheme` together with a
/// `pqxx::connection&` and translate it into a single SQL round-trip.
/// Mapping between C++ field names and SQL column names is driven by
/// `db::table_traits<Scheme>` (see @ref db_traits).

/// @defgroup db_utils Row mapping and exec helpers
///
/// @brief Lower-level utilities under the `db::utils` namespace.
///
/// These helpers convert between `pqxx::row` and user structs and execute
/// raw SQL with parameter binding. They are the building blocks the
/// @ref db_api functions are written against and are also exposed for
/// hand-written queries.

/// @defgroup db_sql Compile-time SQL builders
///
/// @brief `consteval` helpers under `db::sql` and `db::sql::utils`.
///
/// Functions in this group construct SQL fragments at compile time from
/// the same `table_traits` metadata used by @ref db_api. The resulting
/// `static_string` values can be passed directly to `db::utils::as_set_of`
/// or `db::utils::one_row_as`.

/// @defgroup db_cursor Server-side cursors
///
/// @brief Streaming over large result sets via PostgreSQL cursors.
///
/// `db::cursor::make_cursor` returns an input range that issues
/// `FETCH` round-trips lazily, bounded by `cursor_options::batch_size`.
/// Use this when a result set does not fit comfortably in memory.

/// @defgroup db_uuids UUID utilities
///
/// @brief Helpers under `db::uuids` for converting between SQL UUIDs and
/// `boost::uuids::uuid`.

/// @defgroup db_traits Table traits
///
/// @brief Customization point: `db::table_traits<T>` specializations.
///
/// Specialize `db::table_traits<YourStruct>` to map a C++ struct onto a
/// SQL table. Required members are `table_name`, `primary_key`, and
/// `primary_key_type`; optional `column_overrides`, `insert_exclude` and
/// `update_exclude` arrays let you diverge from defaults.
