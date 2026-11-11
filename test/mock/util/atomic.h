// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// avr-libc's ATOMIC_BLOCK(), for the tests on the host: no interrupt runs
// between two statements there, so the block runs once, as it stands.

#ifndef XMEGATIMER_TEST_MOCK_UTIL_ATOMIC_H_
#define XMEGATIMER_TEST_MOCK_UTIL_ATOMIC_H_

// avr-libc's names, which are macros there; for ATOMIC_RESTORESTATE a
// constant by the same name is all the library needs.
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr int ATOMIC_RESTORESTATE{0};

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define ATOMIC_BLOCK(type)                                      \
  for (bool atomic_block_once{(static_cast<void>(type), true)}; \
       atomic_block_once; atomic_block_once = false)
// NOLINTEND(cppcoreguidelines-macro-usage)

#endif  // XMEGATIMER_TEST_MOCK_UTIL_ATOMIC_H_
