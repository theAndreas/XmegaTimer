// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A Bit of a signed integer, whose top bit as a mask would be a negative
// number.
//
// Expected: Bit: Integer is an unsigned integer other than bool

#include <stdint.h>

#include "xmega_timer/Bit.h"

int8_t Case() { return xmega_timer::Bit<int8_t>{}.value(); }
