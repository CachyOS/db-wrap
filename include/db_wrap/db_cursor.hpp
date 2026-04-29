/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

// Server-side SQL cursors
//
// ## Quick start
//
// ```cpp
// struct User {
//   static constexpr std::string_view kName = "users";
//   std::int64_t id;
//   std::string  name;
// };
//
// pqxx::connection cx{"postgresql://..."};
// pqxx::work       txn{cx};
//
// auto cur = db::open_cursor<User>(
//     txn,
//     "SELECT id, name FROM users WHERE id > $1 ORDER BY id",
//     db::cursor_options{.batch_size = 500},
//     std::int64_t{100});
//
// for (const User& u : cur) {
//     std::cout << u.id << ' ' << u.name << '\n';
// }
// txn.commit();
// ```

#include <db_wrap/db_utils.hpp>
#include <db_wrap/table_traits.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <pqxx/pqxx>

namespace db {

/// @brief Tunables for a `db::cursor`.
///
/// @example
/// db::cursor_options opts{.batch_size = 500};
/// auto cur = db::open_cursor<User>(txn, "SELECT * FROM users", opts);
struct cursor_options {
    std::size_t batch_size = 100;
};

namespace details {

/// @brief Monotonic cursor-name counter.
///
/// Cursor names are scoped to the enclosing transaction, so a plain
/// process-wide atomic counter is sufficient to guarantee uniqueness
/// no random suffix is necessary. Two cursors opened in the same txn
/// receive distinct names because they observe different counter values.
inline auto cursor_name_counter() noexcept -> std::atomic_uint64_t& {
    static std::atomic_uint64_t counter{0};
    return counter;
}

/// @brief Substitute `$1`, `$2`, ... in `query` with the pre-quoted SQL
///        literals from `quoted`.
///
/// `DECLARE CURSOR` does not accept bind parameters in libpqxx 8, so
/// parameterized cursors must inline-escape their arguments at
/// construction time via `pqxx::work::quote`. This helper performs the
/// textual substitution.
///
/// The scanner is intentionally narrow: it only recognizes `$<digits>`
/// token sequences and replaces them in-place, leaving the surrounding
/// text alone. It does not attempt to skip placeholders embedded inside
/// string literals. callers are expected to supply well-formed SQL
/// where `$N` only appears as a real parameter placeholder.
///
/// @param query The SQL text containing `$N` placeholders.
/// @param quoted Pre-quoted SQL literals, indexed 0-based (so `$1` maps
///               to `quoted[0]`).
/// @return The fully substituted SQL text.
///
/// @example
/// std::vector<std::string> q = {"'alice'", "42"};
/// auto out = substitute_params("SELECT * FROM u WHERE name = $1 AND age > $2", q);
/// // out == "SELECT * FROM u WHERE name = 'alice' AND age > 42"
inline auto substitute_params(std::string_view query,
    const std::vector<std::string>& quoted) -> std::string {
    std::string result;
    result.reserve(query.size() + 32);
    for (std::size_t i = 0; i < query.size();) {
        if (query[i] == '$' && i + 1 < query.size() && query[i + 1] >= '0' && query[i + 1] <= '9') {
            std::size_t j   = i + 1;
            std::size_t idx = 0;
            while (j < query.size() && query[j] >= '0' && query[j] <= '9') {
                idx = idx * 10 + static_cast<std::size_t>(query[j] - '0');
                ++j;
            }
            if (idx >= 1 && idx <= quoted.size()) {
                result.append(quoted[idx - 1]);
                i = j;
                continue;
            }
        }
        result.push_back(query[i]);
        ++i;
    }
    return result;
}

}  // namespace details

/// @brief RAII server-side SQL cursor with a C++20 input-range interface.
///
/// A `cursor` owns a server-side PostgreSQL cursor for the lifetime of
/// its enclosing `pqxx::work` transaction. It exposes a single-pass
/// input iterator that lazily fetches rows in batches (`DECLARE CURSOR`
/// + repeated `FETCH`) and converts each batch into
/// `std::vector<Scheme>` via `db::utils::extract_all_rows<Scheme>`.
///
/// The cursor closes itself in its destructor; the destructor swallows
/// any exceptions thrown by the server so that a destructor-during-
/// stack-unwind never calls `std::terminate`.
///
/// ## Ownership model
///
///   - The cursor borrows a `pqxx::work&`. The caller is responsible
///     for keeping the transaction alive at least as long as the cursor.
///   - The cursor never `commit()`s or `abort()`s the caller's
///     transaction.
///   - Copy is disabled; move transfers ownership and leaves the source
///     in a valid-but-closed state.
///
/// ## Iterator semantics
///
///   - Single-pass input iterator (`std::input_iterator_tag`).
///   - `operator*` returns `const Scheme&`. The reference is invalidated
///     by the next `++` because it points into the current batch buffer.
///     Callers who want to keep a row across iteration steps must copy it.
///   - Once `begin()` has been called, calling it again returns a fresh
///     iterator. the cursor is still single-pass and will not rewind.
///
/// ## Limitations (v1)
///
///   - `DECLARE CURSOR` in libpqxx 8 cannot take bind parameters, so the
///     parameterized constructor inline-escapes its arguments via
///     `pqxx::work::quote`. This is safe against SQL injection (quoting
///     is handled by libpqxx) but callers should not embed `$N` tokens
///     inside string literals in the query. they will be substituted
///     along with real placeholders.
///   - `WITH HOLD` cursors are not supported; see `cursor_options`.
///
/// @tparam Scheme The row type each fetched row is converted to. Must
///                satisfy `db::DbTable`.
///
/// @example
/// struct User {
///   static constexpr std::string_view kName = "users";
///   std::int64_t id;
///   std::string  name;
/// };
///
/// pqxx::work txn{conn};
/// auto cur = db::open_cursor<User>(
///     txn,
///     "SELECT id, name FROM users WHERE id > $1 ORDER BY id",
///     db::cursor_options{.batch_size = 500},
///     std::int64_t{100});
///
/// for (const User& u : cur) {
///     std::cout << u.id << ' ' << u.name << '\n';
/// }
/// txn.commit();
template <DbTable Scheme>
class cursor {
 public:
    /// @brief Open an unparameterized cursor over `query`.
    ///
    /// Issues `DECLARE <name> CURSOR FOR <query>` on the enclosing
    /// transaction.
    ///
    /// @param txn  The enclosing `pqxx::work`. Must outlive the cursor.
    /// @param query The SELECT statement to iterate over.
    /// @param opts Tunables (batch size, etc.). Defaults are sensible.
    ///
    /// @example
    /// pqxx::work txn{conn};
    /// db::cursor<User> cur{txn, "SELECT * FROM users ORDER BY id"};
    cursor(pqxx::work& txn, std::string_view query, cursor_options opts = {})
      : m_txn{&txn}, m_name{"db_wrap_cursor_" + std::to_string(details::cursor_name_counter().fetch_add(1, std::memory_order_relaxed))}, m_batch{opts.batch_size} {
        std::string stmt;
        stmt.reserve(query.size() + m_name.size() + 32);
        stmt.append("DECLARE ").append(m_name).append(" CURSOR FOR ").append(query);
        m_txn->exec(stmt);
    }

    /// @brief Open a parameterized cursor.
    ///
    /// Parameters are inline-quoted via `pqxx::work::quote` and
    /// substituted for `$1`, `$2`, ... in `query` at construction time.
    /// libpqxx 8 does not accept bind parameters on `DECLARE CURSOR`,
    /// so this textual substitution is the idiomatic workaround.
    ///
    /// @tparam Args Parameter pack; must be non-empty (the unparameterized
    ///              constructor handles the zero-arg case).
    /// @param txn   The enclosing `pqxx::work`.
    /// @param query The SELECT statement with `$N` placeholders.
    /// @param opts  Tunables.
    /// @param params The values to substitute. Their order corresponds
    ///               to `$1`, `$2`, ...
    ///
    /// @example
    /// auto cur = db::open_cursor<User>(
    ///     txn,
    ///     "SELECT * FROM users WHERE id > $1 AND name LIKE $2",
    ///     db::cursor_options{.batch_size = 250},
    ///     std::int64_t{100}, std::string{"alice%"});
    template <typename... Args>
        requires(sizeof...(Args) > 0)
    cursor(pqxx::work& txn, std::string_view query, cursor_options opts, Args&&... params)
      : m_txn{&txn}, m_name{"db_wrap_cursor_" + std::to_string(details::cursor_name_counter().fetch_add(1, std::memory_order_relaxed))}, m_batch{opts.batch_size} {
        std::vector<std::string> quoted;
        quoted.reserve(sizeof...(Args));
        (quoted.emplace_back(m_txn->quote(std::forward<Args>(params))), ...);

        const std::string substituted = details::substitute_params(query, quoted);

        std::string stmt;
        stmt.reserve(substituted.size() + m_name.size() + 32);
        stmt.append("DECLARE ").append(m_name).append(" CURSOR FOR ").append(substituted);
        m_txn->exec(stmt);
    }

    /// @brief Closes the server-side cursor. Swallows exceptions so that
    ///        destruction during stack unwinding is safe.
    ~cursor() noexcept {
        close_quietly();
    }

    cursor(const cursor&)            = delete;
    cursor& operator=(const cursor&) = delete;

    /// @brief Move constructor. Transfers cursor ownership; the source
    ///        becomes closed and no longer issues a `CLOSE` in its
    ///        destructor.
    cursor(cursor&& other) noexcept
      : m_txn{other.m_txn}, m_name{std::move(other.m_name)}, m_batch{other.m_batch}, m_closed{other.m_closed} {
        other.m_closed = true;
    }

    /// @brief Move assignment. Closes the currently-held cursor (if any)
    ///        and takes ownership of `other`.
    cursor& operator=(cursor&& other) noexcept {
        if (this != &other) {
            close_quietly();
            m_txn          = other.m_txn;
            m_name         = std::move(other.m_name);
            m_batch        = other.m_batch;
            m_closed       = other.m_closed;
            other.m_closed = true;
        }
        return *this;
    }

    /// @brief Single-pass input iterator over the cursor's rows.
    ///
    /// Models `std::input_iterator`. Advancing with `++` may trigger a
    /// new batch fetch; the reference returned by `*` is invalidated by
    /// that fetch. The iterator compares equal to
    /// `std::default_sentinel_t` when the cursor is exhausted.
    ///
    /// @example
    /// auto cur = db::open_cursor<User>(txn, "SELECT * FROM users");
    /// for (auto it = cur.begin(); it != cur.end(); ++it) {
    ///     const User& u = *it;
    ///     // `u` is valid until the next `++it`.
    /// }
    class iterator {
     public:
        using value_type        = Scheme;
        using difference_type   = std::ptrdiff_t;
        using reference         = const Scheme&;
        using pointer           = const Scheme*;
        using iterator_category = std::input_iterator_tag;

        iterator() noexcept = default;

        explicit iterator(cursor* owner)
          : m_owner{owner} {
            fetch_batch();
            if (m_buf.empty()) {
                m_owner = nullptr;
            }
        }

        auto operator*() const -> const Scheme& {
            return m_buf[m_pos];
        }

        auto operator->() const -> const Scheme* {
            return &m_buf[m_pos];
        }

        auto operator++() -> iterator& {
            ++m_pos;
            if (m_pos >= m_buf.size()) {
                if (m_partial) {
                    m_owner = nullptr;
                } else {
                    fetch_batch();
                    if (m_buf.empty()) {
                        m_owner = nullptr;
                    }
                }
            }
            return *this;
        }

        void operator++(int) {
            ++*this;
        }

        friend auto operator==(const iterator& lhs, std::default_sentinel_t) noexcept -> bool {
            return lhs.m_owner == nullptr;
        }

     private:
        void fetch_batch() {
            m_buf     = m_owner->fetch_next(m_owner->m_batch);
            m_pos     = 0;
            m_partial = m_buf.size() < m_owner->m_batch;
        }

        cursor* m_owner = nullptr;
        std::vector<Scheme> m_buf{};
        std::size_t m_pos = 0;
        bool m_partial    = false;
    };

    /// @brief Returns an input iterator positioned at the first row of
    ///        the cursor. Calling `begin()` triggers the first `FETCH`
    ///        from the server.
    auto begin() -> iterator {
        return iterator{this};
    }

    /// @brief Returns the sentinel that compares equal to an exhausted
    ///        iterator.
    auto end() -> std::default_sentinel_t {
        return {};
    }

    /// @brief Explicit batch fetch. pulls up to `n` rows and returns
    ///        them as a `std::vector<Scheme>`.
    ///
    /// This is an alternative to range-based iteration for callers that
    /// prefer explicit control over batching. A returned vector smaller
    /// than `n` signals that the cursor is exhausted; a subsequent call
    /// returns an empty vector.
    ///
    /// @param n Maximum number of rows to fetch in this batch.
    /// @return The converted rows.
    ///
    /// @example
    /// auto cur = db::open_cursor<User>(txn, "SELECT * FROM users ORDER BY id");
    /// while (auto batch = cur.fetch_next(1000); !batch.empty()) {
    ///     process(batch);
    /// }
    auto fetch_next(std::size_t n) -> std::vector<Scheme> {
        std::string stmt;
        stmt.reserve(m_name.size() + 32);
        stmt.append("FETCH ").append(std::to_string(n)).append(" FROM ").append(m_name);

        pqxx::result raw = m_txn->exec(stmt);
        return db::utils::extract_all_rows<Scheme>(std::move(raw));
    }

 private:
    void close_quietly() noexcept {
        if (m_closed) {
            return;
        }
        m_closed = true;
        try {
            std::string stmt;
            stmt.reserve(m_name.size() + 8);
            stmt.append("CLOSE ").append(m_name);
            m_txn->exec(stmt);
        } catch (...) {
        }
    }

    pqxx::work* m_txn{};
    std::string m_name{};
    std::size_t m_batch{100};
    bool m_closed{false};
};

/// @brief Convenience factory that opens a cursor inside a transaction,
///        optionally forwarding parameters for `$N` substitution.
///
/// Picks the right `cursor<Scheme>` constructor at compile time based on
/// whether any parameters are supplied. Most call sites should prefer
/// this helper over constructing a `cursor` directly.
///
/// @tparam Scheme The row type (must satisfy `db::DbTable`).
/// @tparam Args   Parameter pack for `$N` substitution.
/// @param txn     Enclosing transaction.
/// @param query   SELECT statement, optionally with `$N` placeholders.
/// @param opts    Cursor tunables.
/// @param args    Values for `$1`, `$2`, ...
/// @return A ready-to-iterate `cursor<Scheme>`.
///
/// @example
/// // Unparameterized. iterate the whole table.
/// auto cur1 = db::open_cursor<User>(txn, "SELECT * FROM users");
///
/// // Parameterized. inline-quoted at construction time.
/// auto cur2 = db::open_cursor<User>(
///     txn,
///     "SELECT * FROM users WHERE id > $1",
///     db::cursor_options{.batch_size = 250},
///     std::int64_t{42});
template <DbTable Scheme, typename... Args>
auto open_cursor(pqxx::work& txn, std::string_view query,
    cursor_options opts = {}, Args&&... args) -> cursor<Scheme> {
    if constexpr (sizeof...(Args) == 0) {
        return cursor<Scheme>{txn, query, opts};
    } else {
        return cursor<Scheme>{txn, query, opts, std::forward<Args>(args)...};
    }
}

}  // namespace db
