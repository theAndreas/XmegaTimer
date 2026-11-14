// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Bit: single bits and groups of bits of an unsigned integer.

#include "xmega_timer/Bit.h"

#include <gtest/gtest.h>
#include <stdint.h>

#include <type_traits>

namespace xmega_timer {
namespace {

// As large as its integer, and copied as one.
static_assert(sizeof(Bit<uint8_t>) == 1);
static_assert(sizeof(Bit<uint32_t>) == 4);
static_assert(std::is_trivially_copyable_v<Bit<uint8_t>>);

// Made only from its integer, never from another by accident.
static_assert(!std::is_convertible_v<uint8_t, Bit<uint8_t>>);

// The compiler computes what does not write.
constexpr Bit<uint8_t> kEmpty{};
constexpr Bit<uint8_t> kFive{0x05};
static_assert(kEmpty.value() == 0x00);
static_assert(kFive.value() == 0x05);
static_assert(kFive[0] && !kFive[1] && kFive[2]);
static_assert(Bit<uint8_t>::ToMask(7) == 0x80);
static_assert(Bit<uint8_t>::Combine(0x01, 0x80) == 0x81);
static_assert(Bit<uint8_t>::SetBit(0x01, 7) == 0x81);
static_assert(Bit<uint8_t>::ClearBit(0x81, 7) == 0x01);
static_assert(Bit<uint8_t>::ToggleBit(0x01, 0) == 0x00);
static_assert(Bit<uint8_t>::WriteBit(0x00, 3, true) == 0x08);
static_assert(Bit<uint8_t>::ReadBit(0x08, 3));
static_assert(Bit<uint8_t>::ReadBitGroup(0x3C, 0x0F) == 0x0C);
static_assert(Bit<uint8_t>::ReadBitGroup(0x3C, 0x03, 4) == 0x03);

TEST(BitTest, StartsWithEveryBitCleared) {
  Bit<uint16_t> const bits;

  EXPECT_EQ(bits.value(), 0x0000);
}

TEST(BitTest, KeepsTheValueItIsMadeWith) {
  Bit<uint16_t> const bits{0xA5A5};

  EXPECT_EQ(bits.value(), 0xA5A5);
}

TEST(BitTest, ReplacesEveryBitBySetValue) {
  Bit<uint8_t> bits{0xFF};

  bits.set_value(0x12);

  EXPECT_EQ(bits.value(), 0x12);
}

TEST(BitTest, ReadsABitByIndex) {
  Bit<uint8_t> const bits{0x81};

  EXPECT_TRUE(bits[0]);
  EXPECT_FALSE(bits[1]);
  EXPECT_FALSE(bits[6]);
  EXPECT_TRUE(bits[7]);
}

TEST(BitTest, ReadsABitByIndexForWriting) {
  Bit<uint8_t> bits{0x80};

  bool const top{bits[7]};
  bool const bottom{bits[0]};

  EXPECT_TRUE(top);
  EXPECT_FALSE(bottom);
}

TEST(BitTest, SetsABitByIndex) {
  Bit<uint8_t> bits{0x01};

  bits[7] = true;

  EXPECT_EQ(bits.value(), 0x81);
}

TEST(BitTest, ClearsABitByIndex) {
  Bit<uint8_t> bits{0xFF};

  bits[7] = false;

  EXPECT_EQ(bits.value(), 0x7F);
}

TEST(BitTest, CopiesABitByIndex) {
  Bit<uint8_t> bits{0x02};

  bits[0] = bits[1];
  bits[1] = bits[2];

  EXPECT_EQ(bits.value(), 0x01);
}

TEST(BitTest, KeepsABitAssignedToItself) {
  Bit<uint8_t> bits{0x08};
  Bit<uint8_t>::Reference third{bits[3]};
  Bit<uint8_t>::Reference const& same{third};

  third = same;
  bits[0] = bits[0];

  EXPECT_EQ(bits.value(), 0x08);
}

TEST(BitTest, CopiesABitOfAnotherByIndex) {
  Bit<uint8_t> source{0x04};
  Bit<uint8_t> target{0x00};

  target[0] = source[2];

  EXPECT_EQ(target.value(), 0x01);
  EXPECT_EQ(source.value(), 0x04);
}

TEST(BitTest, MovesABitByIndex) {
  Bit<uint8_t> bits{0x08};

  bits[0] = Bit<uint8_t>::Reference{bits[3]};

  EXPECT_EQ(bits.value(), 0x09);
}

TEST(BitTest, ChainsAWriteByIndex) {
  Bit<uint8_t> bits{0x00};

  bits[0] = bits[1] = true;

  EXPECT_EQ(bits.value(), 0x03);
}

TEST(BitTest, KeepsAReferenceToTheSameBitWhenCopied) {
  Bit<uint8_t> bits{0x00};
  Bit<uint8_t>::Reference const first{bits[4]};
  Bit<uint8_t>::Reference second{first};

  second = true;

  EXPECT_TRUE(first);
  EXPECT_EQ(bits.value(), 0x10);
}

TEST(BitTest, MakesTheMaskOfTheLowestBit) {
  EXPECT_EQ(Bit<uint8_t>::ToMask(0), 0x01);
  EXPECT_EQ(Bit<uint64_t>::ToMask(0), 0x01u);
}

TEST(BitTest, MakesTheMaskOfTheHighestBit) {
  EXPECT_EQ(Bit<uint8_t>::ToMask(7), 0x80);
  EXPECT_EQ(Bit<uint16_t>::ToMask(15), 0x8000);
  EXPECT_EQ(Bit<uint32_t>::ToMask(31), 0x80000000uL);
  EXPECT_EQ(Bit<uint64_t>::ToMask(63), 0x8000000000000000uLL);
}

TEST(BitTest, CombinesTwoMasks) {
  EXPECT_EQ(Bit<uint8_t>::Combine(0x01, 0x80), 0x81);
  EXPECT_EQ(Bit<uint8_t>::Combine(0x0F, 0x3C), 0x3F);
}

TEST(BitTest, CombinesFourMasks) {
  EXPECT_EQ(Bit<uint16_t>::Combine(0x0001, 0x0010, 0x0100, 0x8000), 0x8111);
}

TEST(BitTest, CombinesNoBitToNone) {
  EXPECT_EQ(Bit<uint8_t>::Combine(0x00, 0x00), 0x00);
}

TEST(BitTest, SetsABitAndLeavesTheOthers) {
  EXPECT_EQ(Bit<uint8_t>::SetBit(0x10, 0), 0x11);
  EXPECT_EQ(Bit<uint8_t>::SetBit(0x10, 4), 0x10);
}

TEST(BitTest, ClearsABitAndLeavesTheOthers) {
  EXPECT_EQ(Bit<uint8_t>::ClearBit(0xFF, 7), 0x7F);
  EXPECT_EQ(Bit<uint8_t>::ClearBit(0x7F, 7), 0x7F);
  EXPECT_EQ(Bit<uint16_t>::ClearBit(0xFFFF, 15), 0x7FFF);
}

TEST(BitTest, TogglesABit) {
  EXPECT_EQ(Bit<uint8_t>::ToggleBit(0x00, 7), 0x80);
  EXPECT_EQ(Bit<uint8_t>::ToggleBit(0x80, 7), 0x00);
}

TEST(BitTest, WritesABit) {
  EXPECT_EQ(Bit<uint8_t>::WriteBit(0x00, 2, true), 0x04);
  EXPECT_EQ(Bit<uint8_t>::WriteBit(0xFF, 2, false), 0xFB);
}

TEST(BitTest, ReadsABit) {
  EXPECT_TRUE(Bit<uint32_t>::ReadBit(0x80000000uL, 31));
  EXPECT_FALSE(Bit<uint32_t>::ReadBit(0x7FFFFFFFuL, 31));
  EXPECT_TRUE(Bit<uint32_t>::ReadBit(0x00000001uL, 0));
}

TEST(BitTest, ReadsAGroupInPlace) {
  EXPECT_EQ(Bit<uint8_t>::ReadBitGroup(0xB6, 0x30), 0x30);
  EXPECT_EQ(Bit<uint8_t>::ReadBitGroup(0xB6, 0x0F), 0x06);
}

TEST(BitTest, ReadsAGroupShiftedDown) {
  EXPECT_EQ(Bit<uint8_t>::ReadBitGroup(0xB6, 0x03, 4), 0x03);
  EXPECT_EQ(Bit<uint8_t>::ReadBitGroup(0xB6, 0x07, 0), 0x06);
}

TEST(BitTest, ReadsTheHighestGroupShiftedDown) {
  EXPECT_EQ(Bit<uint8_t>::ReadBitGroup(0xC0, 0x03, 6), 0x03);
  EXPECT_EQ(Bit<uint16_t>::ReadBitGroup(0xF000, 0x0F, 12), 0x0F);
  EXPECT_EQ(Bit<uint8_t>::ReadBitGroup(0x80, 0x01, 7), 0x01);
}

TEST(BitTest, SetsBitsOfATargetAndLeavesTheOthers) {
  uint8_t volatile target{0x81};

  Bit<uint8_t>::SetBits(target, 0x0C);

  EXPECT_EQ(target, 0x8D);
}

TEST(BitTest, ClearsBitsOfATargetAndLeavesTheOthers) {
  uint8_t volatile target{0xFF};

  Bit<uint8_t>::ClearBits(target, 0x81);

  EXPECT_EQ(target, 0x7E);
}

TEST(BitTest, WritesAGroupOfATargetAndLeavesTheOthers) {
  uint8_t volatile target{0xA5};

  Bit<uint8_t>::WriteBitGroup(target, 0x0F, 0x03);

  EXPECT_EQ(target, 0xA3);
}

TEST(BitTest, WritesNoBitOfAGroupOutsideItsMask) {
  uint8_t volatile target{0x00};

  Bit<uint8_t>::WriteBitGroup(target, 0x0F, 0xFF);

  EXPECT_EQ(target, 0x0F);
}

}  // namespace
}  // namespace xmega_timer
