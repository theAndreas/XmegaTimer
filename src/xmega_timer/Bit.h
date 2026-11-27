// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Single bits and groups of bits of an unsigned integer.
 *
 * The one place where the library's bits are read and written by bitwise
 * operators (docs/CodingStyle.md H-9). Two ways in, for two kinds of value. A
 * value that is kept, as a row of LEDs or a set of options, is held in a Bit
 * and indexed like an array: `bits[column] = true`. A value that is only
 * passed through, as a register, a byte read off a bus or a field of a stored
 * format, goes through the static functions: `Bit<uint8_t>::ReadBit(value, 3)`.
 */

#ifndef XMEGATIMER_SRC_XMEGA_TIMER_BIT_H_
#define XMEGATIMER_SRC_XMEGA_TIMER_BIT_H_

#include <stdint.h>

#include "xmega_timer/attributes.h"

namespace xmega_timer {

/*!
 * \brief An unsigned integer read and written bit by bit, kept as a value or
 *        passed through the static functions.
 *
 * As large as `Integer`, 1 byte for a `Bit<uint8_t>`. A mask is as wide as
 * `Integer`, or as an `unsigned int` where `Integer` is narrower, and never
 * wider: on the AVR a shift of 64 bits is a call into the library, for a byte.
 *
 * A position counts from 0, the least significant bit. A group of bits is
 * given by its mask as it stands in the value, as Combine() of the masks of
 * its bits, and is read back in place; ReadBitGroup() with a position is for a
 * mask written down unshifted, with the position of its lowest bit, as a
 * datasheet gives a field.
 *
 * \code{.cpp}
 * xmega_timer::Bit<uint8_t> row;
 * row[3] = true;  // row.value() is 0x08
 *
 * // Bits 4 and 5 of a byte read from a sensor, as 0 to 3.
 * uint8_t const mode{xmega_timer::Bit<uint8_t>::ReadBitGroup(reading, 0x03,
 * 4)};
 *
 * xmega_timer::Bit<uint8_t>::SetBits(DDRB, _BV(DDB5));  // pin 13 an output
 * \endcode
 *
 * A register is changed in place by SetBits(), ClearBits() and
 * WriteBitGroup(), on a volatile reference, not through `bits[position]`,
 * which would hide a read-modify-write behind an assignment: see the warning
 * at SetBits().
 *
 * \tparam Integer The unsigned integer, as `uint8_t` or `uint16_t`; not `bool`.
 * \pre Each `position` lies below the width of `Integer`; a shift by as much is
 *      undefined.
 */
template <typename Integer>
class Bit {
  // -1 converted is the largest value of an unsigned type only: in a signed
  // one it stays -1, and in a bool it is true, as 1 is.
  static_assert(static_cast<Integer>(-1) > static_cast<Integer>(1),
                "Bit: Integer is an unsigned integer other than bool");

 public:
  /*!
   * \brief A bit of a Bit, as operator[] gives it for writing: it reads as a
   *        `bool` and writes through to the value.
   *
   * As `std::bitset::reference`: assigning a `bool` sets or clears the bit,
   * and assigning another Reference copies the bit it reads, so that
   * `bits[0] = bits[1]` means what it says.
   */
  class Reference {
   public:
    /*! \brief Deleted: a Reference is always of a bit. */
    Reference() = delete;

    /*!
     * \brief Makes the bit at `position` of `value`.
     *
     * \param[in,out] value The integer the bit lies in.
     * \param[in] position The position of the bit.
     */
    constexpr Reference(Integer& value, uint8_t position)
        : value_{&value}, position_{position} {}

    /*! \brief Makes a second Reference of the same bit. */
    constexpr Reference(Reference const&) = default;

    /*! \brief Makes a second Reference of the same bit. */
    constexpr Reference(Reference&&) = default;

    /*! \brief Trivial: a Reference owns nothing. */
    ~Reference() = default;

    /*!
     * \brief Writes the bit that `other` reads into this one.
     *
     * \param[in] other The bit to copy, of this value or of another.
     * \return This bit.
     */
    // Assigned to itself, it rewrites its own bit. Without the `&` of C-17, as
    // `bits[3] = bits[4]` assigns to the temporary that operator[] returns:
    // NOLINTNEXTLINE(bugprone-unhandled-self-assignment,cert-oop54-cpp,house-copy-move-signature)
    Reference& operator=(Reference const& other) {
      // Not = default, which would point value_ at the integer of other and
      // leave both bits as they were. No test of this == &other, which the
      // check asks for: nothing is released, and a Reference assigned to
      // itself reads its bit and writes it back unchanged, as a test shows.
      return *this = static_cast<bool>(other);
    }

    /*! \copydoc operator=(Reference const&) */
    // Without the `&` of C-17, as above:
    // NOLINTNEXTLINE(house-copy-move-signature)
    Reference& operator=(Reference&& other) noexcept {
      return *this = static_cast<bool>(other);
    }

    /*!
     * \brief Sets the bit to `bit`.
     *
     * \param[in] bit True to set the bit, false to clear it.
     * \return This bit.
     */
    Reference& operator=(bool bit) {
      *value_ = WriteBit(*value_, position_, bit);
      return *this;
    }

    /*! \brief Returns whether the bit is set. */
    constexpr operator bool() const {  // NOLINT(runtime/explicit)
      // Implicit, as in the standard, so that the bit reads as a bool.
      return ReadBit(*value_, position_);
    }

   private:
    // A pointer, not a reference_wrapper as FN-15 asks: the skeleton brings
    // none, and a Reference is made only by operator[], from an integer, so
    // that it is never null.
    Integer* value_;
    uint8_t position_;
  };

  /*! \brief Makes a value with every bit cleared. */
  constexpr Bit() = default;

  /*!
   * \brief Makes a value with the bits of `value`.
   *
   * \param[in] value The bits to start with.
   */
  constexpr explicit Bit(Integer value) : value_{value} {}

  /*! \brief Returns all the bits as one integer. */
  XMEGATIMER_NODISCARD constexpr Integer value() const { return value_; }

  /*!
   * \brief Sets all the bits at once.
   *
   * \param[in] value The new bits.
   */
  void set_value(Integer value) { value_ = value; }

  /*!
   * \brief Returns whether the bit at `position` is set.
   *
   * \param[in] position The position of the bit.
   * \return True if the bit is set.
   */
  constexpr bool operator[](uint8_t position) const {
    return ReadBit(value_, position);
  }

  /*!
   * \brief Returns the bit at `position` for writing: assigning a `bool` to it
   *        sets or clears that bit.
   *
   * \param[in] position The position of the bit.
   * \return The bit, which reads as a `bool` and writes through to the value.
   */
  Reference operator[](uint8_t position) { return Reference{value_, position}; }

  /*!
   * \brief Returns the mask of the bit at `position`.
   *
   * \param[in] position The position of the bit.
   * \return An integer with that bit set and every other bit cleared.
   */
  static constexpr Integer ToMask(uint8_t position) {
    return static_cast<Integer>(static_cast<ShiftType>(1u) << position);
  }

  /*!
   * \brief Returns the masks or'ed together: the mask of a group of bits from
   *        the masks of its parts.
   *
   * What a `|` of named masks does in a `constexpr`, the one place it may
   * stand outside this class (docs/CodingStyle.md H-9); this anywhere.
   *
   * \code{.cpp}
   * uint8_t const fault{Bit<uint8_t>::Combine(kOverheat, kUndervoltage)};
   * \endcode
   *
   * \param[in] first The one mask.
   * \param[in] second The other.
   * \param[in] third A third, none by default.
   * \param[in] fourth A fourth, none by default.
   * \return Every bit that is set in one of the masks.
   */
  static constexpr Integer Combine(Integer first, Integer second,
                                   Integer third = Integer{0u},
                                   Integer fourth = Integer{0u}) {
    // In ShiftType from the first operand on: or'ed as they are, two uint8_t
    // would give an int, a signed operand of the next |.
    return static_cast<Integer>(static_cast<ShiftType>(first) | second | third |
                                fourth);
  }

  /*!
   * \brief Returns `value` with the bit at `position` set.
   *
   * \param[in] value The integer to change.
   * \param[in] position The position of the bit.
   * \return The changed integer.
   */
  static constexpr Integer SetBit(Integer value, uint8_t position) {
    return static_cast<Integer>(value | ToMask(position));
  }

  /*!
   * \brief Returns `value` with the bit at `position` cleared.
   *
   * \param[in] value The integer to change.
   * \param[in] position The position of the bit.
   * \return The changed integer.
   */
  static constexpr Integer ClearBit(Integer value, uint8_t position) {
    return ReadBitGroup(value, Invert(ToMask(position)));
  }

  /*!
   * \brief Returns `value` with the bit at `position` inverted.
   *
   * \param[in] value The integer to change.
   * \param[in] position The position of the bit.
   * \return The changed integer.
   */
  static constexpr Integer ToggleBit(Integer value, uint8_t position) {
    return static_cast<Integer>(value ^ ToMask(position));
  }

  /*!
   * \brief Returns `value` with the bit at `position` set to `bit`.
   *
   * \param[in] value The integer to change.
   * \param[in] position The position of the bit.
   * \param[in] bit True to set the bit, false to clear it.
   * \return The changed integer.
   */
  static constexpr Integer WriteBit(Integer value, uint8_t position, bool bit) {
    return bit ? SetBit(value, position) : ClearBit(value, position);
  }

  /*!
   * \brief Returns whether the bit at `position` of `value` is set.
   *
   * \param[in] value The integer to read.
   * \param[in] position The position of the bit.
   * \return True if the bit is set.
   */
  static constexpr bool ReadBit(Integer value, uint8_t position) {
    return ReadBitGroup(value, ToMask(position)) != Integer{0u};
  }

  /*!
   * \brief Returns the bits of `value` that `mask` selects, where they stand.
   *
   * \param[in] value The integer to read.
   * \param[in] mask The group as it stands in the value.
   * \return The bits of the group in place, every other bit cleared.
   */
  static constexpr Integer ReadBitGroup(Integer value, Integer mask) {
    return static_cast<Integer>(value & mask);
  }

  /*!
   * \brief Returns the group of bits of `value` that starts at `position`,
   *        shifted down to bit 0.
   *
   * \param[in] value The integer to read.
   * \param[in] mask The group written down unshifted, from bit 0 on.
   * \param[in] position The position of the group's lowest bit in `value`.
   * \return The bits of the group from bit 0 on, every other bit cleared.
   */
  static constexpr Integer ReadBitGroup(Integer value, Integer mask,
                                        uint8_t position) {
    return ReadBitGroup(ShiftDown(value, position), mask);
  }

  /*!
   * \brief Sets the bits of `mask` in `target`, the other bits left as they
   *        were.
   *
   * \param[in,out] target The register or variable to change.
   * \param[in] mask The bits to set.
   * \warning Each of SetBits(), ClearBits() and WriteBitGroup() reads the
   *          target, changes it and writes it back, as `|=` on a register does,
   *          and named so that it says so. That rules out two kinds of register
   *          of the ATmega328P, which share a type with all the others, so that
   *          nothing but this note keeps them away: a register of flags such as
   *          `TIFR1` or `PCIFR`, where writing back a 1 that was read clears a
   *          flag nobody asked to clear; and `PINB` and its like, where a 1
   *          written toggles the pin. Nor is any of it atomic: a target that an
   *          interrupt writes as well needs the caller's `ATOMIC_BLOCK`.
   */
  static void SetBits(Integer volatile& target, Integer mask) {
    target = static_cast<Integer>(target | mask);
  }

  /*!
   * \brief Clears the bits of `mask` in `target`, the other bits left as they
   *        were.
   *
   * \param[in,out] target The register or variable to change.
   * \param[in] mask The bits to clear.
   * \warning It reads the target, changes it and writes it back, and not
   *          atomically; see SetBits() for the registers that rules out.
   */
  static void ClearBits(Integer volatile& target, Integer mask) {
    target = ReadBitGroup(target, Invert(mask));
  }

  /*!
   * \brief Writes `group` into the bits of `target` that `mask` selects, the
   *        other bits left as they were.
   *
   * \param[in,out] target The register or variable to change.
   * \param[in] mask The group as it stands in the target.
   * \param[in] group The new bits of the group, in place; bits outside `mask`
   *                  are ignored.
   * \warning It reads the target, changes it and writes it back, and not
   *          atomically; see SetBits() for the registers that rules out.
   */
  static void WriteBitGroup(Integer volatile& target, Integer mask,
                            Integer group) {
    Integer const kept{ReadBitGroup(target, Invert(mask))};
    target = static_cast<Integer>(kept | ReadBitGroup(group, mask));
  }

 private:
  /*
   * What Integer is shifted in: Integer itself when it is at least an
   * unsigned int, unsigned int otherwise. Shifting a uint8_t or a uint16_t
   * as it is would promote it to int, which on the AVR has 16 bits and a sign.
   */
  using ShiftType = decltype(Integer{} | 0u);

  // ~ of an Integer narrower than int is a negative int, so cast back.
  static constexpr Integer Invert(Integer mask) {
    return static_cast<Integer>(~mask);
  }

  static constexpr Integer ShiftDown(Integer value, uint8_t position) {
    return static_cast<Integer>(static_cast<ShiftType>(value) >> position);
  }

  Integer value_{0u};
};

}  // namespace xmega_timer

#endif  // XMEGATIMER_SRC_XMEGA_TIMER_BIT_H_
