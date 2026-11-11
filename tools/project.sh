#!/bin/bash
#
# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
#
# What the scripts in tools/ need to know of this project. tools/common.sh sources it after
# its own defaults, so what is defined here wins. The template wrote this file once; it is the
# project's, and `copier update` leaves it alone. What it may define:
#
#   NAMESPACE            the library's namespace; tools/docs.sh lets gaps in
#                        NAMESPACE::internal pass
#   FQBN                 the board the examples are compiled for
#   ARDUINO_CORE         the core, pinned, as arduino-cli installs it
#   ARDUINO_BOARD_URLS   the index of a third-party core, for arduino-cli core update-index
#   SIMAVR_MCU           what simavr runs examples/SelfCheck as; empty: no run in simavr
#   SIMAVR_FREQUENCY     its clock, in Hz
#   DATABASE_EXAMPLE     the example whose compiler flags tools/tidy.sh takes for the rest
#   SELF_CHECK_EXAMPLE   the example tools/self_check.sh runs
#   VARIANTS             the builds every example is compiled in, see variant_flags()
#   TIDY_VARIANTS        the variants clang-tidy and cppcheck read: those whose defines change
#                        the code. The first is named default: clangd reads its database
#   DOCS_INTERNAL_API    a regex of the files in NAMESPACE::internal that are API after all,
#                        such as a base a public class takes members from
#   CPPCHECK             0 if cppcheck cannot parse the library, with the reason; else 1
#   HOST_CODE            folders of code that runs on the host beside the library, as a
#                        simulator, built by CMakeLists.txt: tools/lint.sh formats and checks
#                        them as src/, tools/tidy.sh reads them with the tests
#
#   variant_flags VARIANT   prints the compiler flags of a variant, one per line
#   prepare                 builds what compiling needs beyond the core, such as a plugin
#   check_elf VARIANT ELF   fails if a compiled example is wrong in a way no warning says

# shellcheck disable=SC2034  # read by the scripts that source this file

NAMESPACE="xmega_timer"
FQBN="XMegaForArduino:avr:xmega32e5"
ARDUINO_CORE="XMegaForArduino:avr@0.9.2"
# XMegaForArduino's index lies only on its master branch; the version of the core is pinned above.
ARDUINO_BOARD_URLS="https://raw.githubusercontent.com/XMegaForArduino/IDE/master/package_XMegaForArduino_index.json"
# simavr does not know the ATxmega E5.
SIMAVR_MCU=""
SIMAVR_FREQUENCY="32000000"

# One build of each example, as avr-gcc builds it by default. A variant whose defines take
# other branches of #if, a switch for checks the library makes only on request, is added here
# and in TIDY_VARIANTS, its flags in variant_flags():
#
#   VARIANTS=(default checked)
#   TIDY_VARIANTS=(default checked)
#   variant_flags() {
#     case "$1" in
#       checked) echo "-DXMEGATIMER_CHECK_ACCESS=1" ;;
#     esac
#   }
# tcc4 switches on TCC4, which the core's tone() and analogWrite() use: a sketch that uses tone()
# does not link with it, as both define TCC4_OVF_vect, but Basics does not.
VARIANTS=(default tcc4)
TIDY_VARIANTS=(default tcc4)
variant_flags() {
  case "$1" in
    tcc4) echo "-DTIMER_USE_TCC4=1u" ;;
  esac
}

# XMegaForArduino brings no compiler of its own: its platform.txt takes avr-gcc from the tools of
# the Arduino AVR core, 7.3.0 in 1.8.8, which knows -mmcu=atxmega32e5. Without that core,
# arduino-cli calls /bin/avr-g++.
prepare() {
  arduino-cli core install arduino:avr@1.8.8
}
