@page db_module_cursor Server-side cursors
@tableofcontents

The Cursor Module wraps a PostgreSQL `DECLARE CURSOR` / `FETCH` /
`CLOSE` lifecycle behind a C++20 input range. Reach for it when a result
set does not fit comfortably in memory, or when latency for the first
row matters more than peak throughput.

All entries on this page belong to the @ref db_cursor group.

## Lifecycle

A @ref db::cursor "cursor<Scheme>" is bound to an outer
`pqxx::work` transaction:

- The cursor borrows the transaction; the caller keeps the `pqxx::work`
  alive for at least as long as the cursor.
- Construction issues `DECLARE <name> CURSOR FOR <query>`.
- Iteration issues `FETCH <batch_size> FROM <name>` repeatedly.
- The destructor issues `CLOSE <name>` and swallows server errors so
  that destruction during stack unwinding is safe.

The cursor never commits or aborts the caller's transaction.

## Iterating

The intended idiom is range-based `for`:

```cpp
pqxx::work txn{conn};
auto cur = db::open_cursor<User>(
    txn,
    "SELECT id, name FROM users WHERE id > $1 ORDER BY id",
    db::cursor_options{.batch_size = 500},
    std::int64_t{100});

for (const User& u : cur) {
    std::cout << u.id << ' ' << u.name << '\n';
}
txn.commit();
```

The iterator is single-pass and `operator*` returns a `const Scheme&`
that is invalidated by the next `++`. Callers who want to keep a row
across iteration steps must copy it.

## Explicit batching

For workflows that want to drive batching themselves — e.g. to apply
back-pressure or to interleave fetches with other I/O — use
@ref db::cursor::fetch_next directly:

```cpp
auto cur = db::open_cursor<User>(txn, "SELECT * FROM users ORDER BY id");
while (auto batch = cur.fetch_next(1000); !batch.empty()) {
    process(batch);
}
```

A returned vector smaller than the requested `n` signals exhaustion.

## Parameterized queries

`DECLARE CURSOR` in libpqxx 8 cannot accept bind parameters, so the
parameterized constructor of @ref db::cursor inline-quotes its
arguments via `pqxx::work::quote` and substitutes them for `$N` tokens
in the query at construction time. The textual substitution is handled
by an internal helper (`db::details::substitute_params`).

@warning The substitution scanner does not skip `$N` tokens that appear
inside SQL string literals. If you need a literal `$N` in your query
text (rare), pre-process the string before handing it to
`open_cursor`.

## Tuning

@ref db::cursor_options carries the knobs:

- `batch_size` — how many rows each `FETCH` round-trip pulls. Larger
  batches amortize round-trip cost; smaller batches reduce peak memory
  and improve first-row latency.

`WITH HOLD` cursors and named cursors that survive the enclosing
transaction are not supported in v1.
