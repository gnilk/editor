# cmake/CMakeDeps.cmake — external source dependencies via FetchContent
#
# Fetched deps land in ${CMAKE_BINARY_DIR}/_deps/.
# Vendored deps (duktape, stb) stay in src/ext/ and are not touched here.
#
# To reuse a local clone instead of fetching from the network, pass e.g.:
#   cmake -DFETCHCONTENT_SOURCE_DIR_JSON=/path/to/json-clone ...
# (variable name is FETCHCONTENT_SOURCE_DIR_<UPPERCASE_DEP_NAME>)
#
# Transition helper: if an existing ext/<dep> clone is present, reuse it
# automatically so a fresh configure does not re-download what is already there.
foreach(_dep json gnklog dukglue fmt)
    string(TOUPPER ${_dep} _DEP)
    if (EXISTS ${CMAKE_SOURCE_DIR}/ext/${_dep} AND NOT DEFINED FETCHCONTENT_SOURCE_DIR_${_DEP})
        set(FETCHCONTENT_SOURCE_DIR_${_DEP} ${CMAKE_SOURCE_DIR}/ext/${_dep} CACHE PATH
            "Local ext/${_dep} clone — set to empty string to fetch from network")
        message(STATUS "FetchContent(${_dep}): reusing local ext/${_dep}")
    endif()
endforeach()
unset(_dep)
unset(_DEP)

include(FetchContent)

# ---------------------------------------------------------------------------
# fmt 12.1.0 — pinned by tag (gnklog depends on it; declare + make available first)
# ---------------------------------------------------------------------------
FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt
    GIT_TAG        12.1.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(fmt)
set_property(TARGET fmt PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(fmt PUBLIC FMT_EXCEPTIONS=0)
list(APPEND extlibs fmt)

# ---------------------------------------------------------------------------
# gnklog — pinned SHA (own lib, no stable tag; 2026-11-16: its warning options are PRIVATE; 2026-11-26: no <Windows.h>
# in its headers)
# ---------------------------------------------------------------------------
set(LOG_HAVE_FMT ON)
set(LOG_FMT_DIR ${fmt_SOURCE_DIR})
FetchContent_Declare(gnklog
    GIT_REPOSITORY https://github.com/gnilk/gnklog
    GIT_TAG        216d08b8a001e9a8d536f2a458a8cd1d5076a7a6
    GIT_SHALLOW    FALSE
)
FetchContent_MakeAvailable(gnklog)
if (MSVC)
    target_compile_options(gnklog PUBLIC -Wno-unused-parameter)
else()
    target_compile_options(gnklog PUBLIC -fPIC -Wno-unused-parameter)
endif()
list(APPEND extlibs gnklog)

# ---------------------------------------------------------------------------
# json (nlohmann) — pinned SHA (v3.11.3 + 6 commits; tag alone does not pin this tree)
# ---------------------------------------------------------------------------
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG        0457de21cffb298c22b629e538036bfeb96130b7
    GIT_SHALLOW    FALSE
)
FetchContent_MakeAvailable(json)
list(APPEND extlibs nlohmann_json::nlohmann_json)

# ---------------------------------------------------------------------------
# dukglue — gnilk fork (carries return-type + GCC-warning fixes over upstream Aloshi/dukglue)
# Header-only; SOURCE_SUBDIR points to a non-existent path so add_subdirectory is
# skipped (CMake 3.28+ behaviour — dukglue's CMakeLists.txt uses an old cmake_minimum_required
# that is a hard error in CMake 4.x; we only need its include directory anyway).
# ---------------------------------------------------------------------------
FetchContent_Declare(dukglue
    GIT_REPOSITORY https://github.com/gnilk/dukglue
    GIT_TAG        452d043bba2f480866204d2319d34eda6a408a77
    GIT_SHALLOW    FALSE
    SOURCE_SUBDIR  _no_cmake_build
)
FetchContent_MakeAvailable(dukglue)
set(DUKGLUE_HOME ${dukglue_SOURCE_DIR})

# ---------------------------------------------------------------------------
# trun (testrunner) headers — used only by the utests target.
# After a macOS SDK upgrade, /usr/local/include can fall off the compiler's
# default search path; find_path with NO_DEFAULT_PATH forces an explicit probe.
# The Linux trun package installs to /usr/include; macOS installs to
# /usr/local/include — probe both explicitly.
# ---------------------------------------------------------------------------
find_path(TRUN_INCLUDE_DIR
    NAMES testinterface.h
    PATHS /usr/local/include /usr/include
    NO_DEFAULT_PATH
)
if (NOT TRUN_INCLUDE_DIR)
    message(WARNING
        "trun headers not found in /usr/local/include or /usr/include — utests will not "
        "build correctly. Install the trun testrunner package.")
endif()

# ---------------------------------------------------------------------------
# Windows: SDL3 and yaml-cpp have no system package to find - fetched, pinned, built static (docs/windows-port.md §5).
# SDL3 is what the Mac (Homebrew) uses; yaml-cpp 0.9.0 too (Linux has 0.8.0, whose CMake file CMake 4 refuses).
# ---------------------------------------------------------------------------
if (WIN32)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.4.18
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(SDL3)

    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
    set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG        yaml-cpp-0.9.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(yaml-cpp)
    if (NOT TARGET yaml-cpp::yaml-cpp)
        add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
    endif()
endif()
