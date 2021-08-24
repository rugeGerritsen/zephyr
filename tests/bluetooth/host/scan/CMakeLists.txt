# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

if (NOT BOARD STREQUAL unit_testing)
  message(FATAL_ERROR "This project can only be used with '-DBOARD=unit_testing'.")
endif()

include_directories(
        src
        ${ZEPHYR_BASE}/tests/bluetooth/controller/mock_ctrl/include
        ${ZEPHYR_BASE}/tests/bluetooth/controller/common/include
        ${ZEPHYR_BASE}/include/bluetooth
        ${ZEPHYR_BASE}/subsys/bluetooth
)

FILE(GLOB SOURCES
  src/*.c
)

FILE(GLOB host_sources
  ${ZEPHYR_BASE}/subsys/bluetooth/host/scan.c
)

project(bluetooth_host_scan)
find_package(ZephyrUnittest HINTS $ENV{ZEPHYR_BASE})
target_sources(testbinary PRIVATE ${host_sources})

add_definitions(-include kconfig.h)
if(KCONFIG_OVERRIDE_FILE)
  add_definitions(-include ${KCONFIG_OVERRIDE_FILE})
endif()

add_definitions(-include ztest.h)
add_definitions(-include soc.h)