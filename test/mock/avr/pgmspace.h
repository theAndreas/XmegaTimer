// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// avr-libc's access to flash, for the tests on the host, which has no flash of
// its own: PROGMEM is nothing, and a read from flash is a read.

#ifndef XMEGATIMER_TEST_MOCK_AVR_PGMSPACE_H_
#define XMEGATIMER_TEST_MOCK_AVR_PGMSPACE_H_

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PROGMEM

// avr-libc's name, which the library calls, for the one kind of pointer it
// reads from flash: one to a text.
// NOLINTNEXTLINE(readability-identifier-naming)
inline void const* pgm_read_ptr(char const* const* address) { return *address; }

#endif  // XMEGATIMER_TEST_MOCK_AVR_PGMSPACE_H_
