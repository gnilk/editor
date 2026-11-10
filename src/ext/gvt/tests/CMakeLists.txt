# ---------------------------------------------------------------------------
# gvt unit tests — a trun (TestRunner v3) shared library, `gvttests`:
#     cd <build> && trun --sequential ./libgvttests.so        (.dylib on macOS)
#
# Links only `gvt` - plus libvterm when it is installed: the `oracle` module compares gvt with libvterm
# (libvterm-dev / brew install libvterm). Without it that module is simply not built. libvterm is a TEST
# reference only; nothing in gvt or the editor links it.
# ---------------------------------------------------------------------------

# Locate the trun headers (testinterface.h). Same probe the editor and gansi use.
find_path(TRUN_INCLUDE_DIR
    NAMES testinterface.h
    PATHS /usr/local/include /usr/include
    NO_DEFAULT_PATH
)
if (NOT TRUN_INCLUDE_DIR)
    message(WARNING "gvt tests: trun headers not found — gvttests will not build. Install the trun package.")
    return()
endif()

list(APPEND gvttest_src
    test_gvt.cpp
    test_parser.cpp
    test_terminal.cpp
)

find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_check_modules(VTERM IMPORTED_TARGET vterm)
endif()
if (VTERM_FOUND)
    message(STATUS "gvt tests: libvterm ${VTERM_VERSION} found - oracle module enabled")
    list(APPEND gvttest_src test_oracle.cpp)
else()
    message(STATUS "gvt tests: libvterm not found - oracle module disabled (install libvterm-dev / brew install libvterm)")
endif()

add_library(gvttests SHARED ${gvttest_src})
target_link_libraries(gvttests PRIVATE gvt)
if (VTERM_FOUND)
    target_link_libraries(gvttests PRIVATE PkgConfig::VTERM)
endif()
target_compile_options(gvttests PRIVATE -fPIC)
# Raw -I: CMake drops /usr/local/include as an implicit system path on macOS (see gansi/tests)
target_compile_options(gvttests PRIVATE -I${TRUN_INCLUDE_DIR})

# Next to libutests / libgansitests at the top of the build tree, so trun finds it from the build dir
set_target_properties(gvttests PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    EXCLUDE_FROM_ALL TRUE)

# Byte-stream fixtures -> <build>/gvt-fixtures (tests open them relative to the working directory)
file(GLOB gvt_fixtures CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/*.bin)
add_custom_target(gvt-fixtures
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/gvt-fixtures
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${gvt_fixtures} ${CMAKE_BINARY_DIR}/gvt-fixtures
    DEPENDS ${gvt_fixtures}
    COMMENT "Copying gvt fixtures")
add_dependencies(gvttests gvt-fixtures)
