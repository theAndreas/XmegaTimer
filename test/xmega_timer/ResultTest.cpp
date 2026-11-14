// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xmega_timer/Result.h"

#include <gtest/gtest.h>
#include <stdint.h>

#include "test/printers.h"  // IWYU pragma: keep
#include "xmega_timer/Error.h"

namespace xmega_timer {
namespace {

TEST(ResultTest, HoldsAValue) {
  Result<uint32_t> const result{uint32_t{42u}};

  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(result.value(), 42u);
  EXPECT_EQ(result.status(), Error::kOk);
}

TEST(ResultTest, HoldsAnErrorAndAValueOfZero) {
  Result<uint32_t> const result{Error::kNotStarted};

  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.value(), 0u);
  EXPECT_EQ(result.status(), Error::kNotStarted);
}

TEST(ResultTest, IsMadeWhenCompiling) {
  static_assert(Result<uint32_t>{uint32_t{7u}}.value() == 7u, "a value");
  static_assert(!Result<uint32_t>{Error::kNotBegun}.IsOk(), "an error");
}

}  // namespace
}  // namespace xmega_timer
