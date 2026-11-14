// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief std::array, which avr-gcc lacks, as far as XmegaTimer needs it.
 */

#ifndef XMEGATIMER_SRC_XMEGA_TIMER_UTILITY_ARRAY_H_
#define XMEGATIMER_SRC_XMEGA_TIMER_UTILITY_ARRAY_H_

#include <stddef.h>

namespace xmega_timer {
namespace internal {

/*!
 * \brief A C array of `Size` elements that knows its size.
 *
 * It follows std::array and keeps its names, so that it can give way to
 * std::array or etl::array where a target has one. A struct with a public
 * member, as std::array is: so it is an aggregate, and `{}` makes every
 * element 0.
 *
 * \tparam Element the type of the elements
 * \tparam Size the number of elements, at least 1
 *
 * \stddiff Only size() and operator[]: what XmegaTimer uses. No array of no
 *          elements.
 */
template <typename Element, size_t Size>
struct array {
  // A C array of no elements is ill-formed.
  static_assert(Size > 0, "an array needs at least one element");

  /*!
   * \brief The elements.
   *
   * Public, so that the array is an aggregate; use operator[] rather than this
   * member.
   */
  // The C array that T-20 asks to wrap: this class is the wrapper.
  Element elements[Size];  // NOLINT(modernize-avoid-c-arrays)

  /*! \brief Returns the number of elements, `Size`. */
  constexpr size_t size() const { return Size; }

  /*!
   * \brief Returns the element at `index`, unchecked.
   * \param[in] index the position, from 0
   * \pre `index` < `Size`; beyond the end the access is undefined.
   */
  Element& operator[](size_t index) { return elements[index]; }

  /*! \copydoc operator[](size_t) */
  constexpr Element const& operator[](size_t index) const {
    return elements[index];
  }
};

}  // namespace internal
}  // namespace xmega_timer

#endif  // XMEGATIMER_SRC_XMEGA_TIMER_UTILITY_ARRAY_H_
