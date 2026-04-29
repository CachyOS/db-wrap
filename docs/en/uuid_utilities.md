@page db_module_uuid_utilities UUID Utilities
@tableofcontents

The UUID Utilities Module ships a pair of conversions between
`std::string_view` and a fixed-size character array under
`db::uuids`. It is intentionally tiny: db-wrap does not own UUID
parsing — that is the application's responsibility — it only provides
storage and zero-copy borrowing for already-validated UUID strings.

All entries on this page belong to the @ref db_uuids group.

## Storage

`db::uuids::uuid` is `std::array<char, 36>`. The size constant
@ref db::uuids::UUID_LEN is exposed for callers that want to spell `36`
through the public API.

## Conversions

### convert_to_uuid

Copy a 36-character `std::string_view` into a `uuid` array. See
@ref db::uuids::convert_to_uuid.

```cpp
constexpr std::string_view uuid_str = "877dae4c-0a31-499d-9f81-521532024f53";
auto uuid_obj = db::uuids::convert_to_uuid(uuid_str);
```

@warning The function does not validate the input format; it only
copies the first 36 characters. Validate the source string before
calling if the input is untrusted.

### convert_from_uuid

Borrow a `uuid` as a non-owning `std::string_view`. The returned view
is valid only while the source array is alive. See
@ref db::uuids::convert_from_uuid.

```cpp
db::uuids::uuid u = ...;
std::string_view sv = db::uuids::convert_from_uuid(u);
```
