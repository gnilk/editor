# cmake/CMakeBuildFlags.cmake — per-compiler warning flags, build-type options, sanitiser
# Must be included AFTER add_executable/add_library for ${CUR_TARGET}.
#
# Third-party noise is scoped to the relevant sources/includes rather than
# suppressed globally:
#   dukglue  — SYSTEM includes (suppresses header warnings at the compiler level)
#   yaml-cpp — SYSTEM includes
#   duktape  — set_source_files_properties on its vendored C translation units

# ---------------------------------------------------------------------------
# Base flags shared across all supported compilers
# ---------------------------------------------------------------------------
target_compile_options(${CUR_TARGET} PRIVATE
    -Wall -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-sign-compare
    -Wno-unused-but-set-variable
)

if (UNIX AND NOT APPLE)
    target_compile_options(${CUR_TARGET} PRIVATE -fPIC)
endif()

# ---------------------------------------------------------------------------
# Compiler-specific flags
# ---------------------------------------------------------------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${CUR_TARGET} PRIVATE
        -Wpedantic
        -Wno-unused-local-typedef
        -Wno-unneeded-internal-declaration
    )
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(${CUR_TARGET} PRIVATE
        -Wno-unused-local-typedefs
        -Wno-unused-but-set-parameter
        -Wno-unused-value
        -Wno-implicit-fallthrough
    )
endif()

# ---------------------------------------------------------------------------
# Build-type optimisation flags
# ---------------------------------------------------------------------------
target_compile_options(${CUR_TARGET} PRIVATE
    $<$<CONFIG:Debug>:-O0 -g>
    $<$<CONFIG:Release>:-O2 -DNDEBUG>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g -DNDEBUG>
)

# ---------------------------------------------------------------------------
# Sanitiser option (formalises the ad-hoc cmake-build-asan/ directory)
# Usage: cmake -DGEDIT_SANITIZE=address  or  -DGEDIT_SANITIZE=undefined
# ---------------------------------------------------------------------------
set(GEDIT_SANITIZE "" CACHE STRING "Enable a sanitiser: address or undefined (empty = off)")
if (GEDIT_SANITIZE STREQUAL "address")
    target_compile_options(${CUR_TARGET} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(${CUR_TARGET} PRIVATE -fsanitize=address)
elseif (GEDIT_SANITIZE STREQUAL "undefined")
    target_compile_options(${CUR_TARGET} PRIVATE -fsanitize=undefined)
    target_link_options(${CUR_TARGET} PRIVATE -fsanitize=undefined)
endif()

# ---------------------------------------------------------------------------
# Scope third-party warning suppression
# ---------------------------------------------------------------------------

# dukglue and yaml-cpp: SYSTEM includes suppress warnings from their headers.
# (dukglue's own header dir is kept out of the general includedirs list.)
# YAML_CPP_INCLUDE_DIR is set by find_package(yaml-cpp) in the parent CMakeLists.txt.
target_include_directories(${CUR_TARGET} SYSTEM PRIVATE
    ${DUKGLUE_HOME}/include
    ${YAML_CPP_INCLUDE_DIR}
)

# duktape: vendored C sources — suppress warnings on those TUs directly.
set_source_files_properties(
    ${DUKTAPE_HOME}/duktape.c
    ${DUKTAPE_EXTRAS}/module-node/duk_module_node.c
    PROPERTIES COMPILE_FLAGS
    "-Wno-shadow -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare -Wno-implicit-fallthrough -Wno-unused-parameter -Wno-unused-but-set-parameter"
)
