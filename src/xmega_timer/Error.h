// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Error: why a call of a timer did not do what it was asked.
 */

#ifndef XMEGATIMER_SRC_XMEGA_TIMER_ERROR_H_
#define XMEGATIMER_SRC_XMEGA_TIMER_ERROR_H_

#include <stdint.h>

namespace xmega_timer {

/*!
 * \brief Why a call of a timer did not do what it was asked; one byte, so that
 *        a Status returns in a register.
 *
 * A call that refuses leaves the timer as it was, unless its block says
 * otherwise.
 */
enum class Error : uint8_t {
  kOk = 0,           /*!< No error. */
  kNotBegun,         /*!< Begin() has not been called. */
  kBegunAlready,     /*!< Begin() has been called already. */
  kNotStarted,       /*!< Begun, but not started by Start(). */
  kNotStopped,       /*!< The timer was not stopped by Stop(). */
  kPeriodOutOfRange, /*!< The period is not in range, see SetPeriod(). */
  kDutyOutOfRange,   /*!< The duty is above the largest, see EnablePwm(). */
  kNoCallback        /*!< The callback is nullptr. */
};

}  // namespace xmega_timer

#endif  // XMEGATIMER_SRC_XMEGA_TIMER_ERROR_H_
