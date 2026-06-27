# ---------------------------------------------------------------------------
# gansi unit tests — a trun (TestRunner v3) shared library.
#
# Built as `gansitests` (libgansitests.{dylib,so}); load it with:
#     trun --sequential ./libgansitests.dylib            # macOS
#     trun --sequential ./libgansitests.so               # Linux
#
# Links only `gansi` — the tests stay as standalone as the library itself.
# trun is a test *framework*, not an editor dependency.
# ---------------------------------------------------------------------------

# Locate the trun headers (testinterface.h). Same probe the editor uses:
# Linux installs to /usr/include, macOS to /usr/local/include. NO_DEFAULT_PATH
# forces an explicit lookup after the macOS SDK upgrade dropped /usr/local/include
# from the default search path. (Cache var — a no-op if the parent already found it.)
find_path(TRUN_INCLUDE_DIR
    NAMES testinterface.h
    PATHS /usr/local/include /usr/include
    NO_DEFAULT_PATH
)
if (NOT TRUN_INCLUDE_DIR)
    message(WARNING "gansi tests: trun headers not found — gansitests will not build. Install the trun package.")
    return()
endif()

list(APPEND gansitest_src
    test_gansi.cpp
    test_cellgrid.cpp
    test_ansiencoder.cpp
    test_mockio.cpp
    test_inputparser.cpp
    test_caps.cpp
    test_capabilities.cpp
    test_terminal.cpp
    test_posixio.cpp
)

add_library(gansitests SHARED ${gansitest_src})
target_link_libraries(gansitests PRIVATE gansi)
target_compile_options(gansitests PRIVATE -fPIC)
# CMake strips /usr/local/include from include dirs on macOS (treats it as an implicit
# system path); the raw -I bypasses that dedup — the only reliable way to force it.
target_compile_options(gansitests PRIVATE -I${TRUN_INCLUDE_DIR})

# Land the lib at the top of the build tree (next to libutests) so trun finds it
# from the build dir regardless of this subfolder's nesting.
set_target_properties(gansitests PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    EXCLUDE_FROM_ALL TRUE)
