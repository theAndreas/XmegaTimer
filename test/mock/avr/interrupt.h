// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// avr-libc's ISR(), for the tests on the host: the handler is a function of C
// linkage by the vector's name, which a test calls in place of the
// interrupt.

#ifndef XMEGATIMER_TEST_MOCK_AVR_INTERRUPT_H_
#define XMEGATIMER_TEST_MOCK_AVR_INTERRUPT_H_

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ISR(vector) extern "C" void vector()

// The handlers of the overflows of TCC4 and TCC5, which a test calls; each is
// defined only where its timer is switched on.
// NOLINTBEGIN(readability-identifier-naming)
extern "C" void TCC4_OVF_vect();
extern "C" void TCC5_OVF_vect();
// NOLINTEND(readability-identifier-naming)

#endif  // XMEGATIMER_TEST_MOCK_AVR_INTERRUPT_H_
