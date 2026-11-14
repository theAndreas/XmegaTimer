// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xmega_timer/attributes.h"

#include <gtest/gtest.h>

namespace xmega_timer {
namespace {

// A result that must not be dropped, and an effect that shows the call.
class Sender {
 public:
  XMEGATIMER_NODISCARD bool Send() {
    sent_ = true;
    return true;
  }

  XMEGATIMER_NODISCARD bool sent() const { return sent_; }

 private:
  bool sent_{false};
};

// That Discard() silences the warning is checked by the build itself, with
// warnings as errors under `make check`: where the macro is GCC's
// warn_unused_result, GCC warns through a cast to void.
TEST(DiscardTest, LeavesTheCallItsEffect) {
  Sender sender;

  Discard(sender.Send());

  EXPECT_TRUE(sender.sent());
}

}  // namespace
}  // namespace xmega_timer
