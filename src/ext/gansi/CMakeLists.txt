# ---------------------------------------------------------------------------
# gansi — a small, modern-terminal (ANSI/VT) library.
#
# Standalone unit: depends on NOTHING in the editor. Produces a static library
# target `gansi` (namespace gnilk::ansi). The editor pulls it in via
# add_subdirectory(src/ext/gansi) and links the `gansi` target — it never lists
# these sources directly. The library is also buildable on its own:
#
#   cmake -S src/ext/gansi -B build-gansi && cmake --build build-gansi
#   cmake --build build-gansi --target gansitests   # trun shared lib
#
# Platform seam: everything is platform-independent except platform/PosixTerminalIO
# (the only file that touches <termios.h>/<poll.h>/signals).
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.28)
project(gansi LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# True when configured standalone (not add_subdirectory'd by the editor).
set(GANSI_STANDALONE OFF)
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(GANSI_STANDALONE ON)
endif()

option(GANSI_BUILD_TESTS "Build gansi unit tests (trun shared lib)" ON)

# ---------------------------------------------------------------------------
# Library sources. Phase 0 ships a version stamp only; later phases append the
# cell grid, encoder, input parser, caps builders and the Posix platform impl.
# ---------------------------------------------------------------------------
list(APPEND gansi_src
    src/Version.cpp
    src/CellGrid.cpp
    src/AnsiEncoder.cpp
    src/InputParser.cpp
    src/Caps.cpp
    src/Capabilities.cpp
    src/Terminal.cpp
    src/platform/PosixTerminalIO.cpp
)

add_library(gansi STATIC ${gansi_src})
target_include_directories(gansi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(gansi PUBLIC cxx_std_20)
# PIC: the editor links this into both `goatedit` and the shared `utests`/`gansitests` libs.
set_property(TARGET gansi PROPERTY POSITION_INDEPENDENT_CODE ON)
add_library(gnilk::ansi ALIAS gansi)

if (GANSI_BUILD_TESTS)
    add_subdirectory(tests)
endif()
