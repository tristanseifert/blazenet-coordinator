####################################################################################################
cmake_minimum_required(VERSION 3.21 FATAL_ERROR)
project(coordinatord VERSION 0.1 LANGUAGES C CXX)

include(ExternalProject)
include(FetchContent)

###############
# Set warning levels and language version
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wmissing-declarations -Wformat=2 -fdiagnostics-color=always
    -ftls-model=initial-exec -Wno-undef -Wcast-qual -Wwrite-strings -Wno-format-nonliteral)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-Werror -Wimplicit-fallthrough -Wno-deprecated-copy -Wno-address-of-packed-member
        -Wno-expansion-to-defined -Wno-unused-private-field -Wno-deprecated-volatile)
endif()

###############
# Configuration options

# Should dependencies be pulled via fetch content, or do we expect they're on the system already?
option(EXPECT_DEPENDENCIES_LOCAL "Get dependencies from the system instead of fetching" OFF)
# Should tests be built?
option(BUILD_TESTS "Build tests" OFF)

###############
# Find our dependencies
find_package(PkgConfig REQUIRED)

# libraries always expected to be installed on the system
find_package(SQLite3 REQUIRED)

pkg_search_module(PKG_LIBCBOR REQUIRED libcbor)
link_directories(${PKG_LIBCBOR_LIBRARY_DIRS})

pkg_search_module(PKG_LIBEVENT REQUIRED libevent)
link_directories(${PKG_LIBEVENT_LIBRARY_DIRS})

# these libraries may be externally provided
if(${EXPECT_DEPENDENCIES_LOCAL})
    find_package(fmt REQUIRED)
    find_package(plog REQUIRED)
    find_package(CryptoPP REQUIRED)
else()
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY  https://github.com/fmtlib/fmt.git
        # 9.1.0
        GIT_TAG         a33701196adfad74917046096bf5a2aa0ab0bb50
    )
    FetchContent_Declare(
        plog
        GIT_REPOSITORY  https://github.com/SergiusTheBest/plog.git
        # 1.1.8
        GIT_TAG         89ac49396ae6978a056034d1e34bb170bfd3de33
    )
    FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG        v3.2.0
    )
    FetchContent_Declare(
        CryptoPP
        GIT_REPOSITORY  https://github.com/abdes/cryptopp-cmake.git
    )

    FetchContent_MakeAvailable(fmt plog tomlplusplus CryptoPP)
endif()

###############
# Create version file
find_package(Git REQUIRED)
execute_process(
    COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(VERSION_FILE ${CMAKE_CURRENT_BINARY_DIR}/version.c)
configure_file(${CMAKE_CURRENT_LIST_DIR}/Sources/version.c.in ${VERSION_FILE} @ONLY)

###############
# main daemon (blazed) target
add_executable(daemon
    ${VERSION_FILE}
    Sources/Main.cpp
    Sources/Config/Reader.cpp
    Sources/Support/EventLoop.cpp
    Sources/Support/Logging.cpp
    Sources/Support/Watchdog.cpp
)

target_include_directories(daemon PRIVATE ${CMAKE_CURRENT_LIST_DIR}/Sources)
target_include_directories(daemon PRIVATE ${CMAKE_CURRENT_LIST_DIR}/Include)
target_link_libraries(daemon PRIVATE SQLite::SQLite3 plog::plog fmt::fmt
    tomlplusplus::tomlplusplus cryptopp::cryptopp)

target_include_directories(daemon PRIVATE ${PKG_LIBEVENT_INCLUDE_DIRS} ${PKG_LIBCBOR_INCLUDE_DIRS})
target_link_libraries(daemon PRIVATE ${PKG_LIBEVENT_LIBRARIES} ${PKG_LIBCBOR_LIBRARIES})

# add systemd support on linux
if(UNIX AND NOT APPLE)
    pkg_search_module(PKG_SYSTEMD libsystemd)
    link_directories(${PKG_SYSTEMD_LIBRARY_DIRS})

    target_include_directories(daemon PRIVATE ${PKG_SYSTEMD_INCLUDE_DIRS})
    target_link_libraries(daemon PUBLIC ${PKG_SYSTEMD_LIBRARIES})

    target_compile_definitions(daemon PRIVATE -DCONFIG_WITH_SYSTEMD)
endif()

# installation target
set_target_properties(daemon PROPERTIES OUTPUT_NAME blazed)
INSTALL(TARGETS daemon RUNTIME DESTINATION /usr/sbin)

###############
# Include tests, if enabled
if(${BUILD_TESTS})
    # fetch testing library (catch2)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.1.0
    )
    FetchContent_MakeAvailable(Catch2)

    # set up catch2 and test targets
    include(CTest)
    include(Catch)
endif()