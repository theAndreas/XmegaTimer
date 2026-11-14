// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Status: what a call of a timer returns when it has no value to return.
 */

#ifndef XMEGATIMER_SRC_XMEGA_TIMER_STATUS_H_
#define XMEGATIMER_SRC_XMEGA_TIMER_STATUS_H_

#include "xmega_timer/Error.h"
#include "xmega_timer/attributes.h"

// The core's name, reserved as it is: ErrorText() returns this very type.
class __FlashStringHelper;  // NOLINT(bugprone-reserved-identifier)

namespace xmega_timer {

/*!
 * \brief What a call of a timer returns when it has no value to return: ok, or
 *        the Error why it refused.
 *
 * A sketch asks IsOk(), or compares with an error:
 *
 * \code{.cpp}
 * xmega_timer::Status const status{xmega_timer::tcc5.SetPeriod(5000000u)};
 * if (status == xmega_timer::Error::kPeriodOutOfRange) {
 *   Serial.println(status.ErrorText());
 * }
 * \endcode
 *
 * One byte, so that it returns in a register.
 *
 * \note It has no conversion to `bool`, on purpose: with `StdReturnType` of
 *       0.1.0, `E_OK` was 0, so that `if (timer.start())` was true on an error.
 */
class Status {
 public:
  /*!
   * \brief Makes a status of `error`, ok where none is given.
   *
   * \param[in] error The error, or Error::kOk.
   */
  // NOLINTNEXTLINE(runtime/explicit): a call returns an Error as it is
  constexpr Status(Error error = Error::kOk) : error_{error} {}

  /*! \brief Returns the error, Error::kOk where there is none. */
  XMEGATIMER_NODISCARD constexpr Error error() const { return error_; }

  /*! \brief Returns whether there is no error. */
  XMEGATIMER_NODISCARD constexpr bool IsOk() const {
    return error_ == Error::kOk;
  }

  /*!
   * \brief Returns the error as a text in flash, as `F()` makes one, for
   *        `Serial.println()`.
   *
   * "ok" where there is none, and "unknown error" for a byte that is none of
   * Error's values. The texts and their table take about 130 bytes of flash,
   * and only in a sketch that calls this.
   */
  XMEGATIMER_NODISCARD __FlashStringHelper const* ErrorText() const;

  /*!
   * \brief Returns whether `a` and `b` carry the same error.
   *
   * An Error converts, so that `status == Error::kNotBegun` reads as it says.
   *
   * \param[in] a The one status.
   * \param[in] b The other.
   */
  friend constexpr bool operator==(Status a, Status b) {
    return a.error_ == b.error_;
  }

  /*!
   * \brief Returns whether `a` and `b` carry different errors.
   *
   * \param[in] a The one status.
   * \param[in] b The other.
   */
  friend constexpr bool operator!=(Status a, Status b) { return !(a == b); }

 private:
  Error error_;
};

}  // namespace xmega_timer

#endif  // XMEGATIMER_SRC_XMEGA_TIMER_STATUS_H_
