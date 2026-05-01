/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Vladislav Nepogodin <vnepogodin@cachyos.org>
 */
#pragma once

// Central macro that selects the reflection backend.
// 1 = C++26 std::meta (P2996), 0 = Boost.PFR fallback.
#if defined(DB_WRAP_HAS_STD_REFLECTION) && __has_include(<meta>) && (defined(__cpp_lib_reflection) || defined(__cpp_impl_reflection))
#define DB_WRAP_REFLECT_BACKEND_STD 1
#else
#define DB_WRAP_REFLECT_BACKEND_STD 0
#endif
