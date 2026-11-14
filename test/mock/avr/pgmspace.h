// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// avr-libc's access to flash, for the tests on the host, which has no flash of
// its own: PROGMEM is nothing, and a read from flash is a read.

#ifndef XMEGATIMER_TEST_MOCK_AVR_PGMSPACE_H_
#define XMEGATIMER_TEST_MOCK_AVR_PGMSPACE_H_

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PROGMEM

#include <stdint.h>

// avr-libc's names, which the library calls, for what it reads from flash: a
// byte, a word and a pointer to a text.
// NOLINTNEXTLINE(readability-identifier-naming)
inline uint8_t pgm_read_byte(uint8_t const* address) { return *address; }

// NOLINTNEXTLINE(readability-identifier-naming)
inline uint16_t pgm_read_word(uint16_t const* address) { return *address; }

// NOLINTNEXTLINE(readability-identifier-naming)
inline void const* pgm_read_ptr(char const* const* address) { return *address; }

#endif  // XMEGATIMER_TEST_MOCK_AVR_PGMSPACE_H_
