// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The registers of TCC4, TCC5, PORTC and the PMIC of the ATxmega32E5, for the
// tests on the host: in the layout of avr-libc's iox32e5.h, each a byte or a
// word the tests read and set, with the masks and group configurations by
// avr-libc's names. CTRLGSET carries the commands, and records them; its
// direction bit is the counter's. Nothing counts: a test sets CNT and the
// direction as the moment it reads them in.

#ifndef XMEGATIMER_TEST_MOCK_AVR_IO_H_
#define XMEGATIMER_TEST_MOCK_AVR_IO_H_

#include <stdint.h>

#include <vector>

// avr-libc's names, which are macros and enums there; constants and enums by
// the same names are all the library needs.
// NOLINTBEGIN(readability-identifier-naming,performance-enum-size,cppcoreguidelines-use-enum-class)
using register8_t = uint8_t volatile;
using register16_t = uint16_t volatile;

constexpr uint8_t TC4_CLKSEL_gm{0x0F};
constexpr uint8_t TC4_WGMODE_gm{0x07};
constexpr uint8_t TC4_CCAMODE_gm{0x03};
constexpr uint8_t TC4_OVFINTLVL_gm{0x03};
constexpr uint8_t TC4_CMD_gm{0x0C};
constexpr uint8_t TC4_DIR_bm{0x01};
constexpr uint8_t TC4_OVFIF_bm{0x01};
constexpr uint8_t TC4_BYTEM_gm{0xC0};
constexpr uint8_t TC5_CLKSEL_gm{0x0F};
constexpr uint8_t TC5_WGMODE_gm{0x07};
constexpr uint8_t TC5_CCAMODE_gm{0x03};
constexpr uint8_t TC5_OVFINTLVL_gm{0x03};
constexpr uint8_t TC5_CMD_gm{0x0C};
constexpr uint8_t TC5_DIR_bm{0x01};
constexpr uint8_t TC5_OVFIF_bm{0x01};
constexpr uint8_t PMIC_LOLVLEN_bm{0x01};

enum TC45_CLKSEL_t {
  TC45_CLKSEL_OFF_gc = 0x00,
  TC45_CLKSEL_DIV1_gc = 0x01,
  TC45_CLKSEL_DIV2_gc = 0x02,
  TC45_CLKSEL_DIV4_gc = 0x03,
  TC45_CLKSEL_DIV8_gc = 0x04,
  TC45_CLKSEL_DIV64_gc = 0x05,
  TC45_CLKSEL_DIV256_gc = 0x06,
  TC45_CLKSEL_DIV1024_gc = 0x07
};

enum TC45_WGMODE_t {
  TC45_WGMODE_NORMAL_gc = 0x00,
  TC45_WGMODE_DSBOTTOM_gc = 0x07
};

enum TC45_CCAMODE_t {
  TC45_CCAMODE_DISABLE_gc = 0x00,
  TC45_CCAMODE_COMP_gc = 0x01
};

enum TC45_OVFINTLVL_t {
  TC45_OVFINTLVL_OFF_gc = 0x00,
  TC45_OVFINTLVL_LO_gc = 0x01
};

enum TC45_CMD_t {
  TC45_CMD_NONE_gc = 0x00,
  TC45_CMD_UPDATE_gc = 0x04,
  TC45_CMD_RESTART_gc = 0x08,
  TC45_CMD_RESET_gc = 0x0C
};
// NOLINTEND(readability-identifier-naming,performance-enum-size,cppcoreguidelines-use-enum-class)

// CTRLGSET: a write of a command records it, and RESTART and RESET clear the
// direction, as on the board; a read gives the direction bit.
class MockCommandRegister {
 public:
  // Records the command of `value`; a set direction bit counts down.
  MockCommandRegister& operator=(uint8_t value);
  // The direction bit, TC4_DIR_bm while counting down.
  operator uint8_t() const;  // NOLINT(google-explicit-constructor)

  // Sets the direction, for a test.
  void SetCountingDown(bool down) { counting_down_ = down; }
  // Every command written since Clear(), in order, TC45_CMD_NONE_gc left out.
  // No [[nodiscard]]: the mock is compiled as gnu++11, as the library is.
  // NOLINTNEXTLINE(modernize-use-nodiscard)
  std::vector<uint8_t> const& Commands() const { return commands_; }
  // Forgets the commands and counts up, as after a reset of the board.
  void Clear();

 private:
  bool counting_down_{false};
  std::vector<uint8_t> commands_;
};

// The registers of a TC4 or TC5, in the order of iox32e5.h without its
// reserved bytes: the library relies only on CCA..CCD and on CCABUF..CCDBUF
// following one another, as they do there.
// NOLINTBEGIN(readability-identifier-naming)
struct MockTimer {
  register8_t CTRLA{};
  register8_t CTRLB{};
  register8_t CTRLC{};
  register8_t CTRLD{};
  register8_t CTRLE{};
  register8_t CTRLF{};
  register8_t INTCTRLA{};
  register8_t INTCTRLB{};
  register8_t CTRLGCLR{};
  MockCommandRegister CTRLGSET;
  register8_t CTRLHCLR{};
  register8_t CTRLHSET{};
  // What was last written to INTFLAGS: on the board a 1 clears its flag, so a
  // test sees which flags were cleared.
  register8_t INTFLAGS{};
  register16_t CNT{};
  register16_t PER{};
  register16_t CCA{};
  register16_t CCB{};
  register16_t CCC{};
  register16_t CCD{};
  register16_t PERBUF{};
  register16_t CCABUF{};
  register16_t CCBBUF{};
  register16_t CCCBUF{};
  register16_t CCDBUF{};
};

// A TC5 has no channels C and D; the mock has their words all the same, so
// that a write to them by mistake is seen there rather than in a neighbour.
using TC4_t = MockTimer;
using TC5_t = MockTimer;

struct PORT_t {
  // What was last written to DIRSET: on the board a 1 makes its pin an output.
  register8_t DIRSET{};
};

struct PMIC_t {
  register8_t CTRL{};
};

// avr-libc's names of the peripherals, globals as they are on the board.
extern TC4_t TCC4;
extern TC5_t TCC5;
extern PORT_t PORTC;
extern PMIC_t PMIC;
// NOLINTEND(readability-identifier-naming)

// Every register of the mock as after a reset of the board, for a test to
// start from.
void MockReset();

#endif  // XMEGATIMER_TEST_MOCK_AVR_IO_H_
