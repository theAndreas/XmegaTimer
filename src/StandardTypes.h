// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The return type, the switches and the bit group functions that
 *        XmegaTimer shares with the author's older libraries.
 *
 * The same file comes with those libraries, under the same guard, so that a
 * sketch that includes two of them gets its definitions once. Of what it held
 * there, only what XmegaTimer uses is left. A later version replaces it by
 * types of the namespace xmega_timer.
 */

// The guard the other copies of this file have; renamed, a sketch with one of
// them would define E_OK twice.
#ifndef _STANDARD_TYPES_H_  // NOLINT(build/header_guard)
// NOLINTNEXTLINE(bugprone-reserved-identifier)
#define _STANDARD_TYPES_H_

#include <stdint.h>

/*! \brief A switch of the configuration that is on. */
#define STD_ON 1u
/*! \brief A switch of the configuration that is off. */
#define STD_OFF 0u

/*! \brief Whether a function did what it was asked. */
enum StdReturnType {
  E_OK = 0u,    /*!< It did. */
  E_NOT_OK = 1u /*!< It refused, and changed nothing unless it says so. */
};

/*!
 * \brief The bits `BitGroupMask` of `Var`, in place.
 * \param Var the variable or register
 * \param BitGroupMask the bits of the group, in place
 * \return `Var` with every bit outside the group cleared
 */
template <typename VarType, typename MaskType>
static inline VarType readBitGroup(VarType Var, MaskType BitGroupMask) {
  return Var & (static_cast<VarType>(BitGroupMask));
}

/*!
 * \brief Sets the bits `BitGroupMask` of `Var` to those of `Value`, and leaves
 *        the others as they are.
 * \param Var the variable or register
 * \param BitGroupMask the bits of the group, in place
 * \param Value the value of the group, in place
 */
template <typename VarType, typename MaskType, typename ValType>
static inline void writeBitGroup(VarType& Var, MaskType BitGroupMask,
                                 ValType Value) {
  Var = (Var & ~(static_cast<VarType>(BitGroupMask))) |
        (static_cast<VarType>(Value & BitGroupMask));
}

#endif  // _STANDARD_TYPES_H_
