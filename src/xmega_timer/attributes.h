// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Attributes of a later standard, as macros for every standard, and
 *        Discard() for a result one of them marks.
 *
 * The library is written for C++11 and later. Where the compiler has an
 * attribute of a later standard, the macro is that attribute; where not, the
 * compiler's own where GCC and clang have one, else nothing.
 */

#ifndef XMEGATIMER_SRC_XMEGA_TIMER_ATTRIBUTES_H_
#define XMEGATIMER_SRC_XMEGA_TIMER_ATTRIBUTES_H_

/*!
 * \def XMEGATIMER_NODISCARD
 * \brief Marks a function whose result must not be ignored.
 *
 * Stands in front of the declaration: `XMEGATIMER_NODISCARD int Read();`. A
 * call that drops the result is a warning, and an error in CI.
 *
 * \note For a function only. On a type, as a class whose every result must be
 *       used, `[[nodiscard]]` works, GCC's attribute, which the macro falls
 *       back to, does not.
 */
/*
 * By __has_cpp_attribute, not by __cplusplus: GCC, clang and avr-gcc 7.3 have
 * [[nodiscard]] in C++11 as well, and GCC's warn_unused_result made every
 * example of MaxMatrix 10 to 30 bytes of flash larger on the Uno, where
 * [[nodiscard]] cost nothing. clang reports [[nodiscard]] before C++17 only
 * under -Wpedantic, which the template does not turn on. Two #if, as a
 * preprocessor without __has_cpp_attribute cannot read it in a condition.
 */
#ifdef __has_cpp_attribute
#if __has_cpp_attribute(nodiscard)
#define XMEGATIMER_NODISCARD [[nodiscard]]
#endif
#endif
#ifndef XMEGATIMER_NODISCARD
#ifdef __GNUC__
// GCC, avr-gcc and clang, which defines __GNUC__ as well.
#define XMEGATIMER_NODISCARD __attribute__((warn_unused_result))
#else
#define XMEGATIMER_NODISCARD
#endif
#endif

namespace xmega_timer {

/*!
 * \brief Drops a result that must not be ignored, where dropping it is meant.
 *
 * For a result XMEGATIMER_NODISCARD marks: the call says that it is
 * dropped on purpose, and a comment beside it why:
 * `Discard(Send(frame));  // sent again on the next tick`. Not a cast to
 * `void`, which does not say that the drop is meant: and where the macro is
 * GCC's `warn_unused_result`, GCC warns through one and clang does not, so a
 * cast that passes on the host fails the build for the target.
 *
 * \tparam Value The type of the result.
 */
template <typename Value>
void Discard(Value const& /*value*/) noexcept {}

}  // namespace xmega_timer

#endif  // XMEGATIMER_SRC_XMEGA_TIMER_ATTRIBUTES_H_
