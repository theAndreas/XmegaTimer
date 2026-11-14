// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief tcc5, the TC5 of port C: its channels A and B on PC4 and PC5.
 */

#ifndef XMEGATIMER_SRC_XMEGA_TIMER_TCC5_H_
#define XMEGATIMER_SRC_XMEGA_TIMER_TCC5_H_

#include <avr/io.h>
#include <stdint.h>

#include "xmega_timer/Timer.h"

/*!
 * \def XMEGATIMER_USE_TCC5
 * \brief 1, the default, where the library offers TCC5 and defines its
 *        interrupt vector; 0 where a sketch uses TCC5 otherwise.
 *
 * The core leaves TCC5 alone. TCD5 is not offered at all: the core counts
 * millis() on it and defines its vector itself.
 */
#ifndef XMEGATIMER_USE_TCC5
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): T-6, a switch that #if reads
#define XMEGATIMER_USE_TCC5 1
#endif

namespace xmega_timer {

/*!
 * \brief What Timer needs to know of TCC5: its registers, its port and its two
 *        compare channels.
 *
 * Its channels sit on PC4 and PC5, the pins of SPI's SS and SCK.
 */
struct Tcc5Traits {
  /*! \brief Returns the registers of the timer. */
  static TC5_t& Registers() { return TCC5; }

  /*! \brief Returns the port of the compare channels' pins. */
  static PORT_t& Port() { return PORTC; }

  /*! \brief The number of compare channels: A and B. */
  static constexpr uint8_t kChannelCount{2u};

  /*! \brief Whether XMEGATIMER_USE_TCC5 switches the timer on. */
  static constexpr bool kEnabled{XMEGATIMER_USE_TCC5 != 0};

  /*!
   * \brief A compare channel: its pin, its mode in CTRLE and its registers.
   *
   * \tparam Which The channel, A or B.
   */
  template <Channel Which>
  struct CompareChannel;
};

/*! \brief Channel A of TCC5, on PC4. */
template <>
struct Tcc5Traits::CompareChannel<Channel::kA> {
  /*! \brief Its pin in PORTC. */
  static constexpr uint8_t kPinMask{PIN4_bm};
  /*! \brief Its mode in CTRLE. */
  static constexpr uint8_t kModeMask{TC5_CCAMODE_gm};
  /*! \brief The mode of a compare output. */
  static constexpr uint8_t kModeCompare{TC45_CCAMODE_COMP_gc};
  /*! \brief The mode of no output. */
  static constexpr uint8_t kModeDisable{TC45_CCAMODE_DISABLE_gc};
  /*! \brief Returns its compare register. */
  static register16_t& Compare() { return TCC5.CCA; }
  /*! \brief Returns the buffer of its compare register. */
  static register16_t& Buffer() { return TCC5.CCABUF; }
};

/*! \brief Channel B of TCC5, on PC5. */
template <>
struct Tcc5Traits::CompareChannel<Channel::kB> {
  /*! \brief Its pin in PORTC. */
  static constexpr uint8_t kPinMask{PIN5_bm};
  /*! \brief Its mode in CTRLE. */
  static constexpr uint8_t kModeMask{TC5_CCBMODE_gm};
  /*! \brief The mode of a compare output. */
  static constexpr uint8_t kModeCompare{TC45_CCBMODE_COMP_gc};
  /*! \brief The mode of no output. */
  static constexpr uint8_t kModeDisable{TC45_CCBMODE_DISABLE_gc};
  /*! \brief Returns its compare register. */
  static register16_t& Compare() { return TCC5.CCB; }
  /*! \brief Returns the buffer of its compare register. */
  static register16_t& Buffer() { return TCC5.CCBBUF; }
};

/*! \brief The type of tcc5. */
using Tcc5 = Timer<Tcc5Traits>;

/*! \brief TCC5, the one Timer of the TC5 of port C. */
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): M-5
extern Tcc5 tcc5;

}  // namespace xmega_timer

#endif  // XMEGATIMER_SRC_XMEGA_TIMER_TCC5_H_
