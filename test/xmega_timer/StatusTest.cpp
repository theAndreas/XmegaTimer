// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xmega_timer/Status.h"

#include <gtest/gtest.h>
#include <stdint.h>

#include <ostream>
#include <string>

#include "test/printers.h"
#include "xmega_timer/Error.h"

namespace xmega_timer {
namespace {

TEST(StatusTest, IsOkWithoutAnError) {
  Status const status;

  EXPECT_TRUE(status.IsOk());
  EXPECT_EQ(status.error(), Error::kOk);
}

TEST(StatusTest, CarriesItsError) {
  Status const status{Error::kNotStopped};

  EXPECT_FALSE(status.IsOk());
  EXPECT_EQ(status.error(), Error::kNotStopped);
}

TEST(StatusTest, EqualsTheStatusOfTheSameErrorOnly) {
  Status const status{Error::kNotStopped};

  EXPECT_TRUE(status == Error::kNotStopped);
  EXPECT_FALSE(status != Error::kNotStopped);
  EXPECT_FALSE(status == Error::kNotStarted);
  EXPECT_TRUE(status != Error::kNotStarted);
}

TEST(StatusTest, IsMadeWhenCompiling) {
  constexpr Status kOk{};
  constexpr Status kNoCallback{Error::kNoCallback};

  static_assert(kOk.IsOk(), "ok without an error");
  static_assert(kNoCallback.error() == Error::kNoCallback, "carries its error");
  static_assert(kNoCallback == Error::kNoCallback, "compares when compiling");
}

struct TextCase {
  char const* name;
  Error error;
  char const* text;
};

// By its name, as ctest names the test after it.
// NOLINTNEXTLINE(readability-identifier-naming)
void PrintTo(TextCase const& test_case, std::ostream* out) {
  *out << test_case.name;
}

class StatusTextTest : public ::testing::TestWithParam<TextCase> {};

TEST_P(StatusTextTest, HasATextInFlashForEachError) {
  Status const status{GetParam().error};

  EXPECT_EQ(std::string{TextOf(status)}, GetParam().text);
}

INSTANTIATE_TEST_SUITE_P(
    Errors, StatusTextTest,
    ::testing::Values(
        TextCase{"Ok", Error::kOk, "ok"},
        TextCase{"NotBegun", Error::kNotBegun, "not begun"},
        TextCase{"BegunAlready", Error::kBegunAlready, "begun already"},
        TextCase{"NotStarted", Error::kNotStarted, "not started"},
        TextCase{"NotStopped", Error::kNotStopped, "not stopped"},
        TextCase{"PeriodOutOfRange", Error::kPeriodOutOfRange,
                 "period out of range"},
        TextCase{"DutyOutOfRange", Error::kDutyOutOfRange, "duty out of range"},
        TextCase{"NoCallback", Error::kNoCallback, "no callback"}),
    [](::testing::TestParamInfo<TextCase> const& case_info) {
      return std::string{case_info.param.name};
    });

TEST(StatusTest, HasATextForAByteThatIsNoError) {
  // A byte that is none of the values of Error, as one read from memory may
  // be (CodingStyle T-15).
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  Status const status{static_cast<Error>(200)};

  EXPECT_EQ(std::string{TextOf(status)}, "unknown error");
}

}  // namespace
}  // namespace xmega_timer
