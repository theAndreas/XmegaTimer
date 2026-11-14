// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief tcc4, the TC4 of port C: its channels A to D on PC0 to PC3, off by
 *        default.
 */

#ifndef XMEGATIMER_SRC_XMEGA_TIMER_TCC4_H_
#define XMEGATIMER_SRC_XMEGA_TIMER_TCC4_H_

#include <avr/io.h>
#include <stdint.h>

#include "xmega_timer/Timer.h"

/*!
 * \def XMEGATIMER_USE_TCC4
 * \brief 1 where the library offers TCC4 and defines its interrupt vector; 0,
 *        the default, where the core keeps it.
 *
 * The core drives tone() and analogWrite() with TCC4, and tone() defines its
 * vector itself, so that a sketch that uses tone() does not link with it on.
 * Its channels A and B sit on PC0 and PC1, the pins of TWI.
 */
#ifndef XMEGATIMER_USE_TCC4
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): T-6, a switch that #if reads
#define XMEGATIMER_USE_TCC4 0
#endif

namespace xmega_timer {

/*!
 * \brief What Timer needs to know of TCC4: its registers, its port and its four
 *        compare channels.
 *
 * PORTC.REMAP could move the channels to PC4 to PC7, which Timer does not do.
 */
struct Tcc4Traits {
  /*! \brief Returns the registers of the timer. */
  static TC4_t& Registers() { return TCC4; }

  /*! \brief Returns the port of the compare channels' pins. */
  static PORT_t& Port() { return PORTC; }

  /*! \brief The number of compare channels: A to D. */
  static constexpr uint8_t kChannelCount{4u};

  /*! \brief Whether XMEGATIMER_USE_TCC4 switches the timer on. */
  static constexpr bool kEnabled{XMEGATIMER_USE_TCC4 != 0};

  /*!
   * \brief A compare channel: its pin, its mode in CTRLE and its registers.
   *
   * \tparam Which The channel, A to D.
   */
  template <Channel Which>
  struct CompareChannel;
};

/*! \brief Channel A of TCC4, on PC0. */
template <>
struct Tcc4Traits::CompareChannel<Channel::kA> {
  /*! \brief Its pin in PORTC. */
  static constexpr uint8_t kPinMask{PIN0_bm};
  /*! \brief Its mode in CTRLE. */
  static constexpr uint8_t kModeMask{TC4_CCAMODE_gm};
  /*! \brief The mode of a compare output. */
  static constexpr uint8_t kModeCompare{TC45_CCAMODE_COMP_gc};
  /*! \brief The mode of no output. */
  static constexpr uint8_t kModeDisable{TC45_CCAMODE_DISABLE_gc};
  /*! \brief Returns its compare register. */
  static register16_t& Compare() { return TCC4.CCA; }
  /*! \brief Returns the buffer of its compare register. */
  static register16_t& Buffer() { return TCC4.CCABUF; }
};

/*! \brief Channel B of TCC4, on PC1. */
template <>
struct Tcc4Traits::CompareChannel<Channel::kB> {
  /*! \brief Its pin in PORTC. */
  static constexpr uint8_t kPinMask{PIN1_bm};
  /*! \brief Its mode in CTRLE. */
  static constexpr uint8_t kModeMask{TC4_CCBMODE_gm};
  /*! \brief The mode of a compare output. */
  static constexpr uint8_t kModeCompare{TC45_CCBMODE_COMP_gc};
  /*! \brief The mode of no output. */
  static constexpr uint8_t kModeDisable{TC45_CCBMODE_DISABLE_gc};
  /*! \brief Returns its compare register. */
  static register16_t& Compare() { return TCC4.CCB; }
  /*! \brief Returns the buffer of its compare register. */
  static register16_t& Buffer() { return TCC4.CCBBUF; }
};

/*! \brief Channel C of TCC4, on PC2. */
template <>
struct Tcc4Traits::CompareChannel<Channel::kC> {
  /*! \brief Its pin in PORTC. */
  static constexpr uint8_t kPinMask{PIN2_bm};
  /*! \brief Its mode in CTRLE. */
  static constexpr uint8_t kModeMask{TC4_CCCMODE_gm};
  /*! \brief The mode of a compare output. */
  static constexpr uint8_t kModeCompare{TC45_CCCMODE_COMP_gc};
  /*! \brief The mode of no output. */
  static constexpr uint8_t kModeDisable{TC45_CCCMODE_DISABLE_gc};
  /*! \brief Returns its compare register. */
  static register16_t& Compare() { return TCC4.CCC; }
  /*! \brief Returns the buffer of its compare register. */
  static register16_t& Buffer() { return TCC4.CCCBUF; }
};

/*! \brief Channel D of TCC4, on PC3. */
template <>
struct Tcc4Traits::CompareChannel<Channel::kD> {
  /*! \brief Its pin in PORTC. */
  static constexpr uint8_t kPinMask{PIN3_bm};
  /*! \brief Its mode in CTRLE. */
  static constexpr uint8_t kModeMask{TC4_CCDMODE_gm};
  /*! \brief The mode of a compare output. */
  static constexpr uint8_t kModeCompare{TC45_CCDMODE_COMP_gc};
  /*! \brief The mode of no output. */
  static constexpr uint8_t kModeDisable{TC45_CCDMODE_DISABLE_gc};
  /*! \brief Returns its compare register. */
  static register16_t& Compare() { return TCC4.CCD; }
  /*! \brief Returns the buffer of its compare register. */
  static register16_t& Buffer() { return TCC4.CCDBUF; }
};

/*! \brief The type of tcc4. */
using Tcc4 = Timer<Tcc4Traits>;

/*!
 * \brief TCC4, the one Timer of the TC4 of port C.
 *
 * Made when compiling, by the constexpr constructor of Timer, so that it needs
 * no code at start (CodingStyle M-5).
 */
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,bugprone-dynamic-static-initializers)
extern Tcc4 tcc4;

}  // namespace xmega_timer

#endif  // XMEGATIMER_SRC_XMEGA_TIMER_TCC4_H_
