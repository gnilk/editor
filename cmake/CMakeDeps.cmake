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
# gnklog — pinned SHA (own lib, no stable tag; 2026-05-19)
# ---------------------------------------------------------------------------
set(LOG_HAVE_FMT ON)
set(LOG_FMT_DIR ${fmt_SOURCE_DIR})
FetchContent_Declare(gnklog
    GIT_REPOSITORY https://github.com/gnilk/gnklog
    GIT_TAG        10f002a182c46cd0c95f715f3c83bd100fe7d1cc
    GIT_SHALLOW    FALSE
)
FetchContent_MakeAvailable(gnklog)
target_compile_options(gnklog PUBLIC -fPIC -Wno-unused-parameter)
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
# ---------------------------------------------------------------------------
find_path(TRUN_INCLUDE_DIR
    NAMES testinterface.h
    PATHS /usr/local/include
    NO_DEFAULT_PATH
)
if (NOT TRUN_INCLUDE_DIR)
    message(WARNING
        "trun headers not found in /usr/local/include — utests will not build correctly. "
        "Install the trun testrunner package to /usr/local.")
endif()
