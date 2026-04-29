/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

/// @file db_wrap/db_transaction.hpp
/// @brief Transaction type alias plus quick-start examples.
/// @ingroup db_transaction
///
/// ## Quick start
///
/// ```cpp
/// pqxx::connection cx{"postgresql://..."};
///
/// {
///     db::transaction txn{cx};
///     db::insert_record(txn, User{1, "Alice", "a@x"});
///     db::insert_record(txn, User{2, "Bob",   "b@x"});
///     txn.commit();           // both rows become visible atomically
/// }                           // if commit() is skipped, txn auto-aborts
/// ```
///
/// Every `db::*` and `db::utils::*` entry point that takes a
/// `pqxx::connection&` also has an overload taking `pqxx::work&` (a.k.a.
/// `db::transaction&`) that does **not** commit. Use the work-based
/// overload to batch multiple operations into one atomic unit, and call
/// `txn.commit()` once you are done. Letting the `transaction` go out of
/// scope without committing rolls everything back, so exception-induced
/// aborts are safe by default.
///
/// Savepoints / nested transactions (`pqxx::subtransaction`) are not yet
/// supported.

#include <pqxx/pqxx>

namespace db {

/// @ingroup db_transaction
/// @brief A db-wrap transaction.
///
/// Currently a type alias for `pqxx::work`. Open one with
/// `db::transaction txn{conn};`, pass it to any db-wrap function, then
/// `txn.commit()`. If the `transaction` is destroyed without
/// `commit()`, libpqxx aborts the in-flight transaction automatically.
using transaction = pqxx::work;

}  // namespace db
