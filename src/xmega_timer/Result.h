// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Result: what a call of a timer returns when it has a value to return.
 */

#ifndef XMEGATIMER_SRC_XMEGA_TIMER_RESULT_H_
#define XMEGATIMER_SRC_XMEGA_TIMER_RESULT_H_

#include "xmega_timer/Error.h"
#include "xmega_timer/Status.h"
#include "xmega_timer/attributes.h"

namespace xmega_timer {

/*!
 * \brief A value, or the Error why there is none.
 *
 * \code{.cpp}
 * xmega_timer::Result<uint32_t> const elapsed{xmega_timer::tcc5.Read()};
 * if (elapsed.IsOk()) {
 *   Serial.println(elapsed.value());
 * } else {
 *   Serial.println(elapsed.status().ErrorText());
 * }
 * \endcode
 *
 * As large as `Value` and a byte, with the padding `Value` asks for.
 *
 * \tparam Value The value, a number; made of zero where there is an error.
 */
template <typename Value>
class Result {
 public:
  /*!
   * \brief Makes a result of `value`.
   *
   * \param[in] value The value.
   */
  // NOLINTNEXTLINE(google-explicit-constructor): a call returns its value so
  constexpr Result(Value value)  // NOLINT(runtime/explicit)
      : value_{value}, status_{} {}

  /*!
   * \brief Makes a result of `error`, whose value is zero.
   *
   * \param[in] error The error.
   */
  // NOLINTNEXTLINE(google-explicit-constructor): a call returns Error::kX so
  constexpr Result(Error error)  // NOLINT(runtime/explicit)
      : value_{}, status_{error} {}

  /*! \brief Returns whether there is a value, and no error. */
  XMEGATIMER_NODISCARD constexpr bool IsOk() const { return status_.IsOk(); }

  /*! \brief Returns the value, zero where there is an error. */
  XMEGATIMER_NODISCARD constexpr Value value() const { return value_; }

  /*! \brief Returns the status: ok, or the error why there is no value. */
  XMEGATIMER_NODISCARD constexpr Status status() const { return status_; }

 private:
  Value value_;
  Status status_;
};

}  // namespace xmega_timer

#endif  // XMEGATIMER_SRC_XMEGA_TIMER_RESULT_H_
