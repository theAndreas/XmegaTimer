# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The tests on the host: the library against a mock of the Arduino core, on GoogleTest. The
# library is built as gnu++11, as avr-gcc builds it for Arduino; only the tests are C++17,
# which GoogleTest needs. The warnings and the presets' instrumentation come from
# cmake/checks.cmake.
#
#   cmake --workflow --preset default    # the tests
#   cmake --workflow --preset sanitize   # under ASan and UBSan
#   tools/coverage.sh                    # the tests, and what they reach

cmake_minimum_required(VERSION 3.25)
project(XmegaTimer LANGUAGES CXX)
enable_testing()
# For clangd, which reads it through .clangd.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/checks.cmake)
find_package(GTest REQUIRED)
include(GoogleTest)

# Recursive, as a module has a folder of its own (docs/CodingStyle.md F-12). A test is named as
# the class it tests and Test, or a family of functions and _test (Testing.md TS-5); the mock
# and the cases of what must not compile are no tests of their own.
# The library's sources lie in src/ itself until it moves into the namespace xmega_timer (OIL 3).
file(GLOB_RECURSE library_sources CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS test/*Test.cpp test/*_test.cpp)
list(FILTER test_sources EXCLUDE REGEX "/test/(mock|compile_fail)/")

# One library and one test program per variant: a switch that changes what is compiled makes
# a build of its own. A variant is added with its defines, as add_variant(tcc4 DEFINES
# TIMER_USE_TCC4=1u), and in tools/project.sh for the examples. F_CPU is the 32 MHz of
# XMegaForArduino's ATxmega32E5, as its board defines it.
function(add_variant name)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "DEFINES")

  add_library(xmega_timer_${name} STATIC ${library_sources} test/mock/avr/io.cpp)
  target_include_directories(xmega_timer_${name} PUBLIC src test/mock)
  target_compile_definitions(xmega_timer_${name} PUBLIC F_CPU=32000000uL ${arg_DEFINES})
  target_compile_options(xmega_timer_${name} PRIVATE -std=gnu++11 ${CHECK_WARNINGS}
                                                  ${CHECK_INSTRUMENT})
  target_link_options(xmega_timer_${name} PUBLIC ${CHECK_INSTRUMENT})

  add_executable(xmega_timer_test_${name} ${test_sources})
  target_compile_features(xmega_timer_test_${name} PRIVATE cxx_std_17)
  # The root, so that a test includes its helpers as "test/...", as Google wants an include
  # to name the directory.
  target_include_directories(xmega_timer_test_${name} PRIVATE ${CMAKE_SOURCE_DIR})
  target_compile_options(xmega_timer_test_${name} PRIVATE ${CHECK_WARNINGS}
                                                       ${CHECK_INSTRUMENT})
  target_link_libraries(xmega_timer_test_${name} PRIVATE xmega_timer_${name}
                                                      GTest::gtest_main)
  # Each test in a process of its own, as ctest runs them: Timer is a singleton that init()
  # takes once.
  gtest_discover_tests(xmega_timer_test_${name} TEST_PREFIX ${name}/)
endfunction()

add_variant(default)
# TCC4, which the core's tone() and analogWrite() use, switched on: its four channels.
add_variant(tcc4 DEFINES TIMER_USE_TCC4=1u)

add_compile_fail_tests(DIRECTORY ${CMAKE_SOURCE_DIR}/test/compile_fail STANDARD gnu++11
                       INCLUDES ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/test/mock)
