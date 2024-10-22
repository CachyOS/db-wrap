#pragma once

#include <db_wrap/details/pfr_utils.hpp>

#include <cstdint>

#include <functional>  // for invoke
#include <utility>     // for integer_sequence, forward

namespace db::utils {

template <typename F, typename T, std::size_t... I>
constexpr decltype(auto) unpack_fields_impl(F&& f, const T& obj, std::index_sequence<I...>) {
    return std::invoke(std::forward<F>(f), utils::get_field_by_idx<I>(obj)...);
}

/// @brief Unpacks the fields of a structure and forwards them as arguments to
///        a callable object.
///
/// This function takes a callable object `f` and a structure `obj` and
/// recursively unpacks the fields of `obj`, forwarding them as individual
/// arguments to `f`.
///
/// @tparam F The type of the callable object.
/// @tparam T The type of the structure to unpack.
/// @param f The callable object to forward the fields to.
/// @param obj The structure to unpack.
/// @return The result of invoking the callable object with the unpacked fields.
template <typename F, typename T>
constexpr decltype(auto) unpack_fields(F&& f, const T& obj) {
    return unpack_fields_impl(
        std::forward<F>(f), obj,
        std::make_index_sequence<utils::get_fields_count<T>()>{});
}

}  // namespace db::utils
