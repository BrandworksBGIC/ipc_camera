cmake_minimum_required(VERSION 3.22)

project(cmos_sensors C)

set(CHIP_ID "RTS3917" CACHE STRING "select a chip id")

if(NOT DEFINED CONFIG_CMOS_SENSOR_TARGET_OS)
    set(CONFIG_CMOS_SENSOR_TARGET_OS "Linux" CACHE STRING "select a os")
endif()

if(${CONFIG_CMOS_SENSOR_TARGET_OS} STREQUAL Zephyr)
    include(zephyr_lib.cmake)
else()
    include(linux_lib.cmake)
endif()
