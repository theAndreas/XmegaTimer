// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xmega_timer/Status.h"

#include <avr/pgmspace.h>
#include <stddef.h>
#include <stdint.h>

#include "xmega_timer/Error.h"
#include "xmega_timer/Utility/array.h"

namespace xmega_timer {
namespace {

// The texts and the table of them lie in flash (AVR-1): a switch that returned
// F("...") for each error would be turned by avr-gcc into a table of pointers
// in RAM.
char const kTextOk[] PROGMEM = "ok";
char const kTextNotBegun[] PROGMEM = "not begun";
char const kTextBegunAlready[] PROGMEM = "begun already";
char const kTextNotStarted[] PROGMEM = "not started";
char const kTextNotStopped[] PROGMEM = "not stopped";
char const kTextPeriodOutOfRange[] PROGMEM = "period out of range";
char const kTextDutyOutOfRange[] PROGMEM = "duty out of range";
char const kTextNoCallback[] PROGMEM = "no callback";
char const kTextUnknownError[] PROGMEM = "unknown error";

// In the order of Error.
constexpr internal::array<char const*, 8u> kTexts PROGMEM{{
    kTextOk,
    kTextNotBegun,
    kTextBegunAlready,
    kTextNotStarted,
    kTextNotStopped,
    kTextPeriodOutOfRange,
    kTextDutyOutOfRange,
    kTextNoCallback,
}};

static_assert(kTexts.size() == (static_cast<size_t>(Error::kNoCallback) + 1u),
              "every error has a text, in the order of Error");

}  // namespace

__FlashStringHelper const* Status::ErrorText() const {
  uint8_t const index{static_cast<uint8_t>(error_)};
  void const* text{kTextUnknownError};
  // The address of an entry only once the index is known to be one: beyond
  // the table, even forming it would be undefined.
  if (index < kTexts.size()) {
    text = pgm_read_ptr(&kTexts[index]);
  }
  // The type Serial.print() takes for a text in flash, as F() makes it.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return reinterpret_cast<__FlashStringHelper const*>(text);
}

}  // namespace xmega_timer
