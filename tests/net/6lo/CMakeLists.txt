# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(6lo)

target_include_directories(app PRIVATE $ENV{ZEPHYR_BASE}/subsys/net/ip)
FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
