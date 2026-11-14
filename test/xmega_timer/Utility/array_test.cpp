// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xmega_timer/Utility/array.h"

#include <gtest/gtest.h>
#include <stdint.h>

namespace xmega_timer::internal {
namespace {

TEST(ArrayTest, HasTheSizeItIsMadeWith) {
  array<uint16_t, 3> const values{};

  EXPECT_EQ(values.size(), 3u);
}

TEST(ArrayTest, HasItsSizeWhenCompiled) {
  constexpr array<uint16_t, 2> kValues{};

  static_assert(kValues.size() == 2u, "the size is known when compiled");
}

TEST(ArrayTest, IsZeroWhenValueInitialized) {
  array<uint16_t, 2> const values{};

  EXPECT_EQ(values[0], 0u);
  EXPECT_EQ(values[1], 0u);
}

TEST(ArrayTest, IsInitializedAsACArray) {
  array<uint16_t, 3> const values{{7, 8, 9}};

  EXPECT_EQ(values[0], 7u);
  EXPECT_EQ(values[1], 8u);
  EXPECT_EQ(values[2], 9u);
}

TEST(ArrayTest, WritesAnElementByIndexAndLeavesTheOthers) {
  array<uint16_t, 3> values{};

  values[1] = 1023u;

  EXPECT_EQ(values[0], 0u);
  EXPECT_EQ(values[1], 1023u);
  EXPECT_EQ(values[2], 0u);
}

}  // namespace
}  // namespace xmega_timer::internal
