# ---------------------------------------------------------------------------
# gvt — terminal emulation library: streaming VT parser (+ screen model, later).
#
# Standalone unit: depends on NOTHING in the editor (same rules as gansi). Produces a
# static library target `gvt` (namespace gnilk::vt). The editor pulls it in via
# add_subdirectory(src/ext/gvt). Buildable on its own:
#
#   cmake -S src/ext/gvt -B build-gvt && cmake --build build-gvt --target gvttests
#   cd build-gvt && trun --sequential ./libgvttests.so
#
# Plan: docs/terminal-vt.md (in the editor repo).
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.28)
project(gvt LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GVT_BUILD_TESTS "Build gvt unit tests (trun shared lib)" ON)

list(APPEND gvt_src
    src/Version.cpp
    src/Parser.cpp
)

add_library(gvt STATIC ${gvt_src})
target_include_directories(gvt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(gvt PUBLIC cxx_std_20)
# PIC: linked into shared test libs (and later the editor's utests)
set_property(TARGET gvt PROPERTY POSITION_INDEPENDENT_CODE ON)
add_library(gnilk::vt ALIAS gvt)

if (GVT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
