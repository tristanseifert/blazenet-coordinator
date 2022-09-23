####################################################################################################
# BlazeNet Coordinator RF Firmware
#
# For EFR32FG23 SoC
####################################################################################################
###############
# Set up the CMake project and include some plugins
cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(blazenet-coordinator-rf VERSION 0.1 LANGUAGES ASM C CXX)


include(ExternalProject)
include(FetchContent)

###############
# Library configuration
# external embedded base lib
set(EFWB_TARGET "efr32xg23")
set(EFWB_GECKO_DEVICE "EFR32FG23B020F512IM48")
# XXX: figure out how to support floating point only mode
set(EFWB_CBOR_HAS_FLOAT NO)

set(FREERTOS_HEAP "${CMAKE_CURRENT_LIST_DIR}/Sources/Rtos/DummyAllocator.c" CACHE STRING "")
set(FREERTOS_CONFIG_FILE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/Includes/FreeRTOS/" CACHE STRING "")

# config path for the Gecko SDK files
set(EFWB_GECKO_CONFIG_DIR "${CMAKE_CURRENT_LIST_DIR}/Includes/gecko-config/" CACHE STRING "")
# config path for SPIFFS
set(EFWB_SPIFFS_CONFIG_DIR "${CMAKE_CURRENT_LIST_DIR}/Includes/" CACHE STRING "")

###############
# Get version information from Git and some additional build info
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
cmake_host_system_information(RESULT BUILD_HOSTNAME QUERY FQDN)
set(BUILD_USERNAME $ENV{USER})

# Generate a C++ file containing the build info
configure_file(${CMAKE_CURRENT_LIST_DIR}/Sources/BuildInfo.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/BuildInfo.cpp)
set(BuildInfoFile "${CMAKE_CURRENT_BINARY_DIR}/BuildInfo.cpp")

###############
# Set warning levels and language version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wmissing-declarations -Wformat=2 -fdiagnostics-color=always
    -ftls-model=initial-exec -Wundef -Wcast-qual -Wwrite-strings -Wno-error)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-Wimplicit-fallthrough -Wno-deprecated-copy -Wno-address-of-packed-member
        -Wno-expansion-to-defined -Wno-undef -Wno-unused-private-field -Wno-deprecated-volatile)
endif()

FetchContent_Declare(
    fw-base
    GIT_REPOSITORY https://github.com/tristanseifert/embedded-fw-base.git
    GIT_TAG main
)
FetchContent_MakeAvailable(fw-base)

link_libraries(embedded-fw-base::libc)

###############
# Set up the primary firmware target
add_firmware_executable(NAME firmware FILENAME "blazenet-coordinator-rf-firmware"
    LDSCRIPT ${CMAKE_CURRENT_LIST_DIR}/efr32fg32b0xxf512.ld
    LDMAP ${CMAKE_CURRENT_BINARY_DIR}/output.map
    SOURCES
    ${BuildInfoFile}
    Sources/Init/CppHeap.cpp
    Sources/Init/FaultHandlers.cpp
    Sources/Init/StackGuard.cpp
    Sources/Main.cpp
    Sources/Hw/Indicators.cpp
    Sources/Hw/Identity.cpp
    Sources/Log/Logger.cpp
    Sources/Radio/Init.cpp
    Sources/Radio/Task.cpp
    Sources/Radio/sl_rail_util_callbacks.c
    Sources/Radio/sl_rail_util_init.c
    Sources/Radio/rail_config.c
    Sources/Rtos/Idle.cpp
    Sources/Rtos/Memory.cpp
    Sources/Rtos/Start.cpp
    Sources/Fs/Init.cpp
    Sources/Fs/Flash.cpp
    Sources/Fs/FlashInfo.cpp
    Sources/Fs/NorFs.cpp
    Sources/HostIf/Init.cpp
    Sources/HostIf/IrqManager.cpp
    Sources/HostIf/Task.cpp
    Sources/HostIf/CommandHandlers.cpp
    Sources/Packet/Handler.cpp
    Sources/Drivers/sl_spidrv_init.c
    Sources/Drivers/sl_uartdrv_init.c
)

target_include_directories(firmware PUBLIC Includes)
target_include_directories(firmware PRIVATE Sources ${EFWB_GECKO_CONFIG_DIR})

# shut up some warnings per-file
set_source_files_properties(Sources/Drivers/sl_uartdrv_init.c PROPERTIES COMPILE_FLAGS
    -Wno-cast-qual)

####################################################################################################
# Configure and include various external components
target_link_libraries(firmware PRIVATE embedded-fw-base::tinycbor embedded-fw-base::spiffs)

# FreeRTOS
target_link_libraries(firmware PRIVATE embedded-fw-base::FreeRTOS)

# hardware drivers
target_link_libraries(firmware PRIVATE embedded-fw-base::efr32xg23-rail
    embedded-fw-base::efr32xg23-emdrv-spidrv embedded-fw-base::efr32xg23-emdrv-uartdrv
    embedded-fw-base::efr32xg23-emdrv-tempdrv)
