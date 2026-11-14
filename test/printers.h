// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// How GoogleTest prints an Error and a Status that an expectation did not
// meet: by the text the library has for it, rather than as the bytes of an
// object.

#ifndef XMEGATIMER_TEST_PRINTERS_H_
#define XMEGATIMER_TEST_PRINTERS_H_

#include <ostream>

#include "xmega_timer/Error.h"
#include "xmega_timer/Status.h"

namespace xmega_timer {

// The text of a status, as the library has it in flash, which the host reads
// as RAM.
inline char const* TextOf(Status status) {
  // The type F() of the core makes, back to the chars it points to.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return reinterpret_cast<char const*>(status.ErrorText());
}

// GoogleTest finds these by their name, in the namespace of the type.
// NOLINTNEXTLINE(readability-identifier-naming)
inline void PrintTo(Error error, std::ostream* out) {
  *out << TextOf(Status{error});
}

// NOLINTNEXTLINE(readability-identifier-naming)
inline void PrintTo(Status status, std::ostream* out) {
  *out << TextOf(status);
}

}  // namespace xmega_timer

#endif  // XMEGATIMER_TEST_PRINTERS_H_
